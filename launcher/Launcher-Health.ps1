param([ValidateSet('VirtualDesktop','SteamVR')][string]$Runtime,
 [string]$GamePath,[string]$SteamVRPath,[string]$VirtualDesktopPath,[switch]$ValidateOnly)

function Repair-LauncherTempDirectory {
 param([string[]]$Candidates=@([IO.Path]::GetTempPath(),(Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'DementiumVR/Diagnostics')))
 foreach($candidate in $Candidates){
  try{
   $null=[IO.Directory]::CreateDirectory($candidate)
   $probe=Join-Path $candidate ('.write-test-'+[guid]::NewGuid().ToString('N'))
   [IO.File]::WriteAllText($probe,'diagnostic probe');[IO.File]::Delete($probe)
   if([IO.Path]::GetFullPath($candidate).TrimEnd('\') -ne [IO.Path]::GetTempPath().TrimEnd('\')){
    $env:TEMP=$candidate;$env:TMP=$candidate
    Write-Host "REPAIRED: using writable temporary folder for this launcher process: $candidate"
   }
   return $candidate
  }catch{Write-Host "TEMP CHECK: $candidate : $($_.Exception.Message)" -ForegroundColor Yellow}
 }
 throw 'No writable diagnostic/temp directory. No game or repair started.'
}

function Enter-LauncherLock {
 param([string]$Folder)
 $canonical=[IO.Path]::GetFullPath($Folder).TrimEnd('\','/').ToUpperInvariant()
 $sha=[Security.Cryptography.SHA256]::Create()
 try{$key=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace('-','')}finally{$sha.Dispose()}
 $mutex=[Threading.Mutex]::new($false,('Local\DementiumVR-Launcher-'+$key))
 try{
  $acquired=$false
  try{$acquired=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$acquired=$true}
  if(!$acquired){throw 'Another launcher is already checking or starting this mod. Use that window, or wait for it to close before retrying.'}
  return $mutex
 }catch{$mutex.Dispose();throw}
}

function Get-LauncherFileHash {
 param([string]$LiteralPath,[string]$Algorithm='SHA256')
 $stream=[IO.File]::OpenRead($LiteralPath);$sha=[Security.Cryptography.SHA256]::Create()
 try { [pscustomobject]@{Hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')} }
 finally { $sha.Dispose();$stream.Dispose() }
}
function Assert-LocalModPath {
 param([string]$Folder,[string]$Path)
 $root=[IO.Path]::GetFullPath((Split-Path -Parent $Folder)).TrimEnd('\','/')
 $full=[IO.Path]::GetFullPath($Path)
 if(!$full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -and $full -ne $root){throw "Repair path escapes mod package: $Path"}
 $walk=$full
 while($walk -and $walk.Length -ge $root.Length){
  $item=Get-Item -LiteralPath $walk -Force -ErrorAction SilentlyContinue
  if($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Linked mod path requires manual relocation before repair: $walk"}
  if($walk -eq $root){break};$walk=Split-Path -Parent $walk
 }
}

function Repair-LauncherFiles {
 param([string]$Folder,[switch]$NoRepair)
 Assert-LocalModPath -Folder $Folder -Path $Folder
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 Assert-LocalModPath -Folder $Folder -Path (Join-Path $Folder 'Launcher-Recovery.json')
 Assert-LocalModPath -Folder $Folder -Path (Join-Path $Folder 'Launcher-Recovery.zip')
 $metadata=Get-Content -LiteralPath (Join-Path $Folder 'Launcher-Recovery.json') -Raw -ErrorAction Stop | ConvertFrom-Json
 $archive=Join-Path $Folder 'Launcher-Recovery.zip'
 if ((Get-LauncherFileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $metadata.sha256) { throw 'Recovery archive is damaged. Extract a fresh mod ZIP.' }
 $zip=[IO.Compression.ZipFile]::OpenRead($archive)
 try {
  $allowed=@('Launch VR.ps1','Install-Discovery.ps1','Launch SteamVR.cmd','Launch VirtualDesktop.cmd','DementiumOpenXR.dll','attach.exe','openxr_loader.dll','Startup-Checks.ps1','Game-Integrity.json','PillBottle.mesh')
  $required=$allowed
  foreach($file in $metadata.files) {
   if($required -contains $file.name){continue}
   if($file.name -notmatch '\A(SelectedAssets|ReloadSounds)/[A-Za-z0-9_ .()/\-]+\z' -or $file.name.Contains('..')){throw 'Unexpected recovery path. Extract a fresh mod ZIP.'}
   $allowed+= $file.name
  }
  if (@($metadata.files.name | Sort-Object -Unique).Count -ne @($metadata.files).Count -or @($required | Where-Object {$_ -notin $metadata.files.name}).Count) { throw 'Incomplete recovery manifest. Extract a fresh mod ZIP.' }
  foreach ($file in $metadata.files) {
   if ($allowed -notcontains $file.name) { throw 'Unexpected recovery file. Extract a fresh mod ZIP.' }
   $targetFolder=$Folder
   if ($metadata.layout -eq 'launchers-above-mod-files-v1' -and $file.name -in @('Launch SteamVR.cmd','Launch VirtualDesktop.cmd')) {
    if ((Split-Path -Leaf $Folder) -ne 'Mod Files') { throw 'Expected the Mod Files folder beside the launchers. Extract the full ZIP.' }
    $targetFolder=Split-Path -Parent $Folder
   }
   $path=Join-Path $targetFolder $file.name
   Assert-LocalModPath -Folder $Folder -Path $path
   $good=(Test-Path -LiteralPath $path -PathType Leaf) -and ((Get-LauncherFileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $file.sha256)
   if ($good) { Write-Host "OK: $($file.name)"; continue }
   Write-Host "PROBLEM: missing or changed $($file.name). Attempting verified repair." -ForegroundColor Yellow
   if ($NoRepair) { throw "Missing or damaged $($file.name). Close Dementium before repair." }
   $entry=$zip.GetEntry($file.name)
   if (!$entry) { throw "Recovery copy missing: $($file.name)" }
   $inputStream=$entry.Open(); $buffer=[IO.MemoryStream]::new()
   try { $inputStream.CopyTo($buffer); $bytes=$buffer.ToArray() } finally { $inputStream.Dispose(); $buffer.Dispose() }
   $sha=[Security.Cryptography.SHA256]::Create()
   try { $digest=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','') } finally { $sha.Dispose() }
   if ($digest -ne $file.sha256) { throw "Recovery checksum failed: $($file.name)" }
   # Only verified release files are replaced; settings and saves are never targets.
   $repairBackup=$path+'.before-repair-'+[guid]::NewGuid().ToString('N')
   $null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
   $temporary=$path+'.repair-'+[guid]::NewGuid().ToString('N')
   try {
    [IO.File]::WriteAllBytes($temporary,$bytes)
    if((Get-LauncherFileHash -LiteralPath $temporary).Hash -ne $file.sha256){throw "Repair verification failed: $($file.name)"}
    if([IO.File]::Exists($path)){[IO.File]::Replace($temporary,$path,$repairBackup)}else{[IO.File]::Move($temporary,$path)}
   }catch{throw "Could not replace $($file.name); original file preserved if replacement did not complete. Close programs using it and retry. Original error: $($_.Exception.Message)"}
   finally{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
   if ((Get-LauncherFileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) { throw "Repair verification failed: $($file.name)" }
   Write-Host "REPAIRED: $($file.name)"
  }
  if (@($metadata.files).Count -ne $allowed.Count -or @($metadata.files.name | Sort-Object -Unique).Count -ne $allowed.Count) { throw 'Incomplete recovery manifest. Extract a fresh mod ZIP.' }
 } finally { $zip.Dispose() }
}


function Install-VerifiedModUpdate {
 param([string]$Archive,[string]$ExpectedHash,[string]$Folder)
 if(Get-Process -Name Dementium -ErrorAction SilentlyContinue){throw 'Close Dementium before updating.'}
 if((Split-Path -Leaf $Folder) -ne 'Mod Files'){throw 'Wrong mod location: select Mod Files beside the two launchers.'}
 Assert-LocalModPath -Folder $Folder -Path $Folder
 if($ExpectedHash -notmatch '\A[0-9a-fA-F]{64}\z' -or (Get-LauncherFileHash $Archive).Hash -ne $ExpectedHash){throw 'Downloaded update checksum mismatch; no files replaced.'}
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $stage=Join-Path ([IO.Path]::GetTempPath()) ('Dementium-update-'+[guid]::NewGuid().ToString('N'))
 $null=[IO.Directory]::CreateDirectory($stage)
 $zip=[IO.Compression.ZipFile]::OpenRead($Archive)
 try{
  $total=0L
  foreach($entry in $zip.Entries){
   $name=$entry.FullName.Replace('\','/');$total+=$entry.Length
   if($total -gt 1073741824 -or $name.StartsWith('/') -or $name.Contains(':') -or $name.Split('/') -contains '..'){throw 'Unsafe update archive path or size.'}
   if($name.EndsWith('/')){continue}
   if($name -notmatch '\A(Mod Files/|Launch (SteamVR|VirtualDesktop)\.cmd\z)'){throw 'Unexpected update folder layout.'}
   $path=Join-Path $stage $name;$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
   [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path,$false)
  }
 }finally{$zip.Dispose()}
 $source=Join-Path $stage 'Mod Files'
 Repair-LauncherFiles -Folder $source -NoRepair
 $metadata=Get-Content (Join-Path $source 'Launcher-Recovery.json') -Raw|ConvertFrom-Json
 $names=@($metadata.files.name)+@('Launcher-Health.ps1','Launcher-Recovery.json','Launcher-Recovery.zip')
 $backup=Join-Path $Folder ('Update-Backups/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
 $plan=@()
 foreach($name in $names|Sort-Object -Unique){
  if($name -match '\.ini\z'){continue}
  $from=Join-Path $source $name;$to=Join-Path $Folder $name
  if($name -in @('Launch SteamVR.cmd','Launch VirtualDesktop.cmd')){$from=Join-Path $stage $name;$to=Join-Path (Split-Path -Parent $Folder) $name}
  if(!(Test-Path -LiteralPath $from -PathType Leaf)){throw "Update lacks $name"}
  Assert-LocalModPath -Folder $Folder -Path $to
  Assert-LocalModPath -Folder $Folder -Path $backup
  if(Test-Path -LiteralPath $to -PathType Container){throw "Update target is a folder instead of a file: $to. Nothing installed."}
  $exists=Test-Path -LiteralPath $to -PathType Leaf;$saved=Join-Path $backup $name
  if($exists){$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($saved));Copy-Item -LiteralPath $to -Destination $saved;if((Get-LauncherFileHash $saved).Hash -ne (Get-LauncherFileHash $to).Hash){throw 'Update backup verification failed'}}
  $plan+= [pscustomobject]@{From=$from;To=$to;Backup=$saved;Existed=$exists}
 }
 $written=@()
 try{foreach($item in $plan){$written+=$item;$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.To));Copy-Item -LiteralPath $item.From -Destination $item.To -Force;if((Get-LauncherFileHash $item.To).Hash -ne (Get-LauncherFileHash $item.From).Hash){throw 'Installed update checksum mismatch'}}}
 catch{
  $originalFailure=$_;$rollbackErrors=@()
  for($i=$written.Count-1;$i -ge 0;$i--){$item=$written[$i]
   try{
    if($item.Existed){
     if(!(Test-Path -LiteralPath $item.To -PathType Leaf) -or (Get-LauncherFileHash $item.To).Hash -ne (Get-LauncherFileHash $item.Backup).Hash){Copy-Item -LiteralPath $item.Backup -Destination $item.To -Force -ErrorAction Stop}
     if((Get-LauncherFileHash $item.To).Hash -ne (Get-LauncherFileHash $item.Backup).Hash){throw "Rollback verification failed: $($item.To)"}
    }else{if([IO.File]::Exists($item.To)){[IO.File]::Delete($item.To)}}
   }catch{$rollbackErrors+="$($item.To): $($_.Exception.Message)"}
  }
  if($rollbackErrors.Count){
   $failure=[InvalidOperationException]::new("Update failed: $($originalFailure.Exception.Message). Rollback needs attention: $($rollbackErrors -join '; '). Verified backups: $backup")
   $failure.Data['DementiumUnsafeUpdate']=$true
   throw $failure
  }
  throw $originalFailure
 }
 Write-Host "UPDATE INSTALLED: verified mod files restored. Backup: $backup. Saves and personal INI files preserved."
}
function Update-OfficialModFiles {
 param([string]$Folder)
 if(Get-Process -Name Dementium -ErrorAction SilentlyContinue){return}
 $current=[version]'0.1.6'
 try{
  [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
  $releases=Invoke-RestMethod -Uri 'https://api.github.com/repos/stevethetitan92/Dementium-The-Ward-VR/releases?per_page=20' -Headers @{'User-Agent'='Dementium-VR-Launcher'} -TimeoutSec 15
  $release=$releases | Where-Object {!$_.draft -and $_.tag_name -match '^v(\d+\.\d+\.\d+)(?:-|$)'} | Sort-Object -Property @{Expression={[version]([regex]::Match($_.tag_name,'^v(\d+\.\d+\.\d+)').Groups[1].Value)};Descending=$true} | Select-Object -First 1
  if(!$release){throw 'No versioned public mod release found'}
  if($release.tag_name -notmatch '^v(\d+\.\d+\.\d+)(?:-|$)'){throw 'Unrecognized official release version'}
  if([version]$Matches[1] -le $current){Write-Host 'Official update check: no newer version.';return}
  $assets=@($release.assets|Where-Object {$_.name -match '^Dementium.*\.zip$' -and $_.name -notmatch '(?i)backup' -and $_.digest -match '^sha256:[0-9a-fA-F]{64}$'})
  if($assets.Count -ne 1){throw 'Release has no unambiguous checksummed mod ZIP'}
  $asset=$assets[0];$prefix='https://github.com/stevethetitan92/Dementium-The-Ward-VR/releases/download/'
  if(!$asset.browser_download_url.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or $asset.size -le 0 -or $asset.size -gt 536870912){throw 'Unexpected update download URL or size'}
  $download=Join-Path ([IO.Path]::GetTempPath()) ('Dementium-update-'+[guid]::NewGuid().ToString('N')+'.zip')
  Write-Host "Downloading official update $($release.tag_name)..."
  Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $download -TimeoutSec 180
  if((Get-Item -LiteralPath $download).Length -ne $asset.size){throw 'Download is incomplete or has the wrong size; existing installation untouched.'}
  Install-VerifiedModUpdate -Archive $download -ExpectedHash $asset.digest.Substring(7) -Folder $Folder
 }catch{
  if($_.Exception.Data['DementiumUnsafeUpdate']){throw}
  Write-Host "UPDATE CHECK: $($_.Exception.Message) Local verified recovery will still run." -ForegroundColor Yellow
 }
}

if ($Runtime) {
 $ErrorActionPreference='Stop'
 $diagnostics=Repair-LauncherTempDirectory
 $log=Join-Path $diagnostics ('Dementium-launcher-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$PID+'.log')
 $launcherLock=$null
 try {
  $launcherLock=Enter-LauncherLock -Folder $PSScriptRoot
  Start-Transcript -LiteralPath $log -Force | Out-Null
  Write-Host "Launcher diagnostic log: $log"
  Write-Host "Time: $([DateTime]::UtcNow.ToString('o')) PID: $PID Runtime: $Runtime OS64: $([Environment]::Is64BitOperatingSystem) Process64: $([Environment]::Is64BitProcess)"
  Write-Host "Mod folder: $PSScriptRoot"
  if (![Environment]::Is64BitProcess) { throw 'Run the launcher using 64-bit Windows PowerShell.' }
  if(!$ValidateOnly){Update-OfficialModFiles -Folder $PSScriptRoot}
  Repair-LauncherFiles -Folder $PSScriptRoot -NoRepair:([bool](Get-Process -Name Dementium -ErrorAction SilentlyContinue))
  . (Join-Path $PSScriptRoot 'Startup-Checks.ps1')
  Test-ModStorage -Folder $PSScriptRoot
  Test-SaveHealth -Folder $PSScriptRoot
  Test-NativeSaveImport -ModFolder $PSScriptRoot
  & (Join-Path $PSScriptRoot 'Launch VR.ps1') -Runtime $Runtime -GamePath $GamePath -SteamVRPath $SteamVRPath -VirtualDesktopPath $VirtualDesktopPath -ValidateOnly:$ValidateOnly
 } catch {
  Write-Host "Startup check failed: $($_.Exception.Message)" -ForegroundColor Red
  if(Get-Command Write-LauncherFailureReport -ErrorAction SilentlyContinue){try{Write-LauncherFailureReport -Text ($_ | Out-String) -ModFolder $PSScriptRoot}catch{Write-Host "Could not save failure report: $_"}}
  Write-Host "Diagnostic log: $log"
  if (!$ValidateOnly) { $null=Read-Host 'Press Enter to close' }
  exit 1
 } finally { Stop-Transcript -ErrorAction SilentlyContinue | Out-Null; if($launcherLock){$launcherLock.ReleaseMutex();$launcherLock.Dispose()} }
}








