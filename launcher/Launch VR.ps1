param(
 [ValidateSet('VirtualDesktop','SteamVR')][string]$Runtime='VirtualDesktop',
 [string]$GamePath,[string]$SteamVRPath,[string]$VirtualDesktopPath,
 [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$initializationLog=''
try {
 . (Join-Path $PSScriptRoot 'Install-Discovery.ps1')
 $roots = @()
 foreach ($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam','HKLM:\SOFTWARE\Valve\Steam')) {
   $entry = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
   if ($entry) { if ($entry.SteamPath) { $roots += $entry.SteamPath }; if ($entry.InstallPath) { $roots += $entry.InstallPath } }
 }
 if (${env:ProgramFiles(x86)}) { $roots += Join-Path ${env:ProgramFiles(x86)} 'Steam' }
 $libraries = @(Get-SteamLibraries -Roots $roots)
 $game = Resolve-GameInstall -Explicit $GamePath -Candidates @(Find-SteamInstall $libraries '^Dementium\s*[-:]\s*The Ward$' 'Dementium.exe') -NonInteractive:$ValidateOnly -Diagnose { Find-GameWithDiagnostics -Libraries $libraries -ModFolder $PSScriptRoot }
 $loader = Join-Path $PSScriptRoot 'openxr_loader.dll'
 if ($Runtime -eq 'SteamVR') {
   $steamVR = Resolve-Install -Explicit $SteamVRPath -Candidates @(Find-SteamInstall $libraries '^SteamVR$' 'steamxr_win64.json') -RequiredFile 'steamxr_win64.json' -Option '-SteamVRPath'
   $manifest = Join-Path $steamVR 'steamxr_win64.json'
 }
 else {
   if (!$VirtualDesktopPath) {
     $vd = @()
     foreach ($root in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall','HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall')) {
       foreach ($key in @(Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue)) {
         $item=Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
         if ($item.DisplayName -like '*Virtual Desktop Streamer*' -and $item.InstallLocation) { $vd += $item.InstallLocation }
       }
     }
     if ($env:ProgramFiles) { $vd += Join-Path $env:ProgramFiles 'Virtual Desktop Streamer' }
     $vd = @($vd | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'OpenXR/virtualdesktop-openxr.json') -PathType Leaf } | ForEach-Object { (Resolve-Path -LiteralPath $_).ProviderPath.TrimEnd('\','/') } | Sort-Object -Unique)
     if ($vd.Count -ne 1) { throw 'Specify -VirtualDesktopPath with your Virtual Desktop Streamer installation folder.' }
     $VirtualDesktopPath=$vd[0]
   }
   $manifest=Join-Path $VirtualDesktopPath 'OpenXR/virtualdesktop-openxr.json'
 }
 $exe=Join-Path $game 'Dementium.exe'
 foreach ($path in @($exe,$loader,$manifest,(Join-Path $PSScriptRoot 'attach.exe'),(Join-Path $PSScriptRoot 'DementiumOpenXR.dll'))) {
   if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing launch dependency: $path" }
 }
 $exe=(Resolve-Path -LiteralPath $exe).ProviderPath
 $loader=(Resolve-Path -LiteralPath $loader).ProviderPath
 $manifest=(Resolve-Path -LiteralPath $manifest).ProviderPath
 $null=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
 Write-Output "Game: $exe" "OpenXR loader: $loader" "Runtime manifest: $manifest"
 Test-GameFiles -GameFolder $game -ModFolder $PSScriptRoot
 Test-GraphicsStartup -GameFolder $game -ModFolder $PSScriptRoot -Manifest $manifest
 if ($ValidateOnly) { Write-Output 'Paths validated; game not launched.'; exit 0 }
 if (Get-Process -Name Dementium -ErrorAction SilentlyContinue) { throw 'Close Dementium before launching another session.' }
 Repair-VRLaunchEnvironment
 $env:DEMENTIUM_GAME_EXE=$exe
 $env:DEMENTIUM_OPENXR_LOADER=$loader
 $env:DEMENTIUM_XR_RUNTIME_JSON=$manifest
 $env:XR_RUNTIME_JSON=$manifest
 $env:DEMENTIUM_PROFILE_NATIVE_GPU='0'
 $env:DEMENTIUM_TEST_UNCAPPED_MIRROR='1'
 $launchStarted=[datetime]::UtcNow
 Push-Location -LiteralPath $PSScriptRoot
 try { & (Join-Path $PSScriptRoot 'attach.exe') --launch 2>&1 | Tee-Object -Variable launchOutput | ForEach-Object { Write-Host $_ }; $status=$LASTEXITCODE; $initMatch=[regex]::Match(($launchOutput -join "`n"),'(?m)^initialization_log=(.+)$');if($initMatch.Success){$initializationLog=$initMatch.Groups[1].Value.Trim()}; if ($status -ne 0) { Write-BridgeFailureAdvice -Text ($launchOutput -join [Environment]::NewLine); throw "VR launcher returned $status" } }
 finally { Pop-Location }
 $pidMatch=[regex]::Match(($launchOutput -join "`n"),'(?m)^game_pid=(\d+)\s*$')
 if($pidMatch.Success){Watch-VRPresentation -GameProcessId ([int]$pidMatch.Groups[1].Value) -BridgeLog (Join-Path $PSScriptRoot 'bridge.log') -StartedUtc $launchStarted}
 else{Write-Host 'Could not identify the launched process; return the launcher log for diagnosis.' -ForegroundColor Yellow}
} catch {
 Write-Host $_ -ForegroundColor Red
 try{Write-LauncherFailureReport -Text ($_ | Out-String) -ModFolder $PSScriptRoot -InitLog $initializationLog}catch{Write-Host "Could not save failure report: $_"}
 if (!$ValidateOnly) { Read-Host 'Press Enter to close' }
 exit 1
}







