function Get-SteamLibraries {
    param([string[]]$Roots)
    $libraries = @()
    foreach ($root in $Roots) {
        if (!$root) { continue }
        $libraries += $root
        $vdf = Join-Path $root 'steamapps/libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf -PathType Leaf) {
            $text = Get-Content -LiteralPath $vdf -Raw -Encoding UTF8
            # Current Steam path fields and legacy numbered library entries.
            foreach ($match in [regex]::Matches($text, '"(?:path|\d+)"\s+"((?:[^"\\]|\\.)*)"')) {
                $value = $match.Groups[1].Value.Replace('\\','\').Replace('\"','"')
                if ([IO.Path]::IsPathRooted($value)) { $libraries += $value }
            }
        }
    }
    @($libraries | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Sort-Object -Unique)
}
function Find-SteamInstall {
    param([string[]]$Libraries,[string]$Name,[string]$RequiredFile)
    $found = @()
    foreach ($library in $Libraries) {
        $apps = Join-Path $library 'steamapps'
        foreach ($manifest in @(Get-ChildItem -LiteralPath $apps -Filter 'appmanifest_*.acf' -File -ErrorAction SilentlyContinue)) {
            $text = Get-Content -LiteralPath $manifest.FullName -Raw -Encoding UTF8
            $title = [regex]::Match($text,'"name"\s+"([^"]+)"').Groups[1].Value
            $install = [regex]::Match($text,'"installdir"\s+"([^"]+)"').Groups[1].Value
            if ($title -notmatch $Name -or !$install) { continue }
            $common = [IO.Path]::GetFullPath((Join-Path $apps 'common'))
            $folder = [IO.Path]::GetFullPath((Join-Path $common $install))
            if (!$folder.StartsWith($common + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { continue }
            if (Test-Path -LiteralPath (Join-Path $folder $RequiredFile) -PathType Leaf) { $found += $folder }
        }
    }
    @($found | Sort-Object -Unique)
}
function Resolve-Install {
    param([string]$Explicit,[string[]]$Candidates,[string]$RequiredFile,[string]$Option)
    if ($Explicit) {
        $resolved = (Resolve-Path -LiteralPath $Explicit -ErrorAction Stop).ProviderPath
        if (Test-Path -LiteralPath $resolved -PathType Leaf) {
            if ([IO.Path]::GetFileName($resolved) -ne $RequiredFile) { throw "Expected $RequiredFile for $Option" }
            $resolved = Split-Path -Parent $resolved
        }
        if (!(Test-Path -LiteralPath (Join-Path $resolved $RequiredFile) -PathType Leaf)) { throw "$Option does not contain $RequiredFile" }
        return $resolved
    }
    $unique = @($Candidates | Sort-Object -Unique)
    if ($unique.Count -eq 1) { return $unique[0] }
    if ($unique.Count -gt 1) { throw "Multiple installations found. Specify $Option. Candidates: $($unique -join '; ')" }
    throw "Installation not found. Specify $Option; no files will be moved."
}

function Get-GamePathDiagnostic {
    param([string]$Path)
    try {
        if (![IO.Path]::IsPathRooted($Path)) { return 'Paste the full file path, not a game name or web link.' }
        $root=[IO.Path]::GetPathRoot($Path)
        if (!(Test-Path -LiteralPath $root -ErrorAction Stop)) { return "Drive or network location is unavailable: $root" }
        $item=Get-Item -LiteralPath $Path -ErrorAction Stop
        if (!$item.PSIsContainer -and $item.Name -ne 'Dementium.exe') { return "Wrong file: $($item.Name). Select Dementium.exe, not a shortcut, ZIP, or mod launcher." }
        $exe=if ($item.PSIsContainer) { Join-Path $item.FullName 'Dementium.exe' } else { $item.FullName }
        if (!(Test-Path -LiteralPath $exe -PathType Leaf -ErrorAction Stop)) { return 'This folder exists, but does not contain Dementium.exe.' }
        $stream=[IO.File]::Open($exe,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        try { return 'Dementium.exe exists and is readable. See the original error above for the remaining failure.' } finally { $stream.Dispose() }
    } catch {
        return "Windows could not access the path: $($_.Exception.Message)"
    }
}
function Resolve-GameInstall {
    param([string]$Explicit,[string[]]$Candidates,[switch]$NonInteractive,
          [scriptblock]$Diagnose = {}, [scriptblock]$Prompt = { Read-Host 'Paste the full path to Dementium.exe (or its folder), then press Enter. Leave blank to cancel' })
    while ($true) {
        if ($Explicit) {
            $Explicit=$Explicit.Trim()
            # Windows Explorer's Copy as path wraps the path in quotes.
            if ($Explicit.Length -ge 2 -and (($Explicit.StartsWith('"') -and $Explicit.EndsWith('"')) -or ($Explicit.StartsWith("'") -and $Explicit.EndsWith("'")))) {
                $Explicit=$Explicit.Substring(1,$Explicit.Length-2)
            }
        }
        try { return Resolve-Install -Explicit $Explicit -Candidates $Candidates -RequiredFile 'Dementium.exe' -Option '-GamePath' }
        catch {
            $recovered=@(& $Diagnose)
            if ($recovered.Count -eq 1 -and !$Explicit) { return Resolve-Install -Explicit $recovered[0] -RequiredFile 'Dementium.exe' -Option '-GamePath' }
            if ($NonInteractive) { throw }
            Write-Host "Could not locate a usable Dementium installation: $($_.Exception.Message)" -ForegroundColor Yellow
            if ($Explicit) { Write-Host ('Quick path check: ' + (Get-GamePathDiagnostic -Path $Explicit)) -ForegroundColor Yellow }
            Write-Host 'In Steam: right-click Dementium > Manage > Browse local files. Right-click Dementium.exe and choose Copy as path. Paste it here.'
            $Explicit = & $Prompt
            if ([string]::IsNullOrWhiteSpace($Explicit)) { throw 'Launch cancelled. No game was started.' }
            $Candidates=@()
        }
    }
}


function Find-GameWithDiagnostics {
 param([string[]]$Libraries,[string]$ModFolder)
 Write-Host 'Checking game discovery...'
 $found=@()
 foreach ($library in $Libraries) {
  Write-Host "Steam library: $library"
  $apps=Join-Path $library 'steamapps'
  foreach ($acf in @(Get-ChildItem -LiteralPath $apps -Filter 'appmanifest_*.acf' -File -ErrorAction SilentlyContinue)) {
   try {
    $text=Get-Content -LiteralPath $acf.FullName -Raw -ErrorAction Stop
    if ($text -match '(?i)dementium') { Write-Host "Game manifest: $($acf.FullName)" }
   } catch { Write-Host "Cannot read manifest: $($_.Exception.Message)" }
  }
  # Bounded fallback: immediate Steam common children, no recursive drive scan.
  try {
   foreach ($folder in @(Get-ChildItem -LiteralPath (Join-Path $apps 'common') -Directory -ErrorAction Stop)) {
    if (Test-Path -LiteralPath (Join-Path $folder.FullName 'Dementium.exe') -PathType Leaf) { $found += $folder.FullName }
   }
  } catch { Write-Host "Cannot inspect library: $($_.Exception.Message)" }
 }
 if ($ModFolder -and (Test-Path -LiteralPath (Join-Path $ModFolder 'Dementium.exe') -PathType Leaf)) { $found += $ModFolder }
 $found=@($found | Sort-Object -Unique)
 if (!$Libraries.Count) { Write-Host 'No accessible registered Steam libraries were found.' }
 if (!$found.Count) { Write-Host 'Dementium.exe was not found in the checked libraries. The game may be elsewhere, uninstalled, or on an unavailable drive. Paste its path below.' }
 elseif ($found.Count -eq 1) { Write-Host "Recovered game location: $($found[0])" }
 else { Write-Host "Multiple game locations found: $($found -join '; '). Paste the one to use." }
 return $found
}
