
function Test-NativeSaveImport {
 param([string]$ModFolder,[string]$NativeFolder=(Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Dementium - The Ward/saves'))
 # Existing empty working sessions bypass Sessions::createWorking's profile seed.
 # Recover only that exact empty state, using this player's indexed native profile.
 $existing=Test-Path -LiteralPath (Join-Path $ModFolder 'save-slots/banks/slot-1/current')
 $emptyTarget=$null
 if($existing){
  $active=Join-Path $ModFolder 'save-slots/active'
  if(!(Test-Path -LiteralPath $active -PathType Leaf)){return}
  $value=(Get-Content -LiteralPath $active -Raw).Trim()
  if($value -notmatch '\A[012]\z'){return}
  $slot=Join-Path $ModFolder ('save-slots/slot-'+([int]$value+1))
  $pointer=Join-Path $slot 'working'
  if(!(Test-Path -LiteralPath $pointer -PathType Leaf)){return}
  $name=(Get-Content -LiteralPath $pointer -Raw).Trim()
  if($name -notmatch '\Asession-[0-9-]+\z'){return}
  $session=Join-Path $slot $name;$target=Join-Path $session 'Dementium - The Ward/saves'
  if(!(Test-Path -LiteralPath (Join-Path $session 'complete') -PathType Leaf) -or !(Test-Path -LiteralPath $target -PathType Container)){return}
  Assert-LocalModPath -Folder $ModFolder -Path $target
  if(@(Get-ChildItem -LiteralPath $target -Force).Count){return}
  $emptyTarget=$target
 }
 $index=Join-Path $NativeFolder '-saveindex'
 if(!(Test-Path -LiteralPath $index -PathType Leaf)){
  if($emptyTarget){
   $inventory=@()
   if(Test-Path -LiteralPath $NativeFolder -PathType Container){
    $inventory=@(Get-ChildItem -LiteralPath $NativeFolder -Force | ForEach-Object {
     [pscustomobject]@{name=$_.Name;directory=$_.PSIsContainer;bytes=$(if(!$_.PSIsContainer){$_.Length}else{0})}
    })
   }
   $report=Join-Path ([IO.Path]::GetTempPath()) ('Dementium-save-diagnosis-'+$PID+'-'+[guid]::NewGuid().ToString('N')+'.json')
   $diagnosis='Empty VR session and missing native save index: profile is uninitialized or incomplete, not proven corrupt.'
   try{
    [pscustomobject]@{time_utc=[datetime]::UtcNow.ToString('o');diagnosis=$diagnosis;native_folder=$NativeFolder;selected_session=$emptyTarget;native_files=$inventory;missing=@('-saveindex','Pickups.sav','Scores.sav')|Where-Object {!(Test-Path -LiteralPath (Join-Path $NativeFolder $_) -PathType Leaf)};repair='No verified donor; no save contents generated or overwritten.'}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $report -Encoding UTF8
    Write-Host "Save diagnostic report: $report"
   }catch{Write-Host "Could not write save diagnostic report: $($_.Exception.Message). $diagnosis" -ForegroundColor Yellow}
   Write-Host "SAVE DIAGNOSIS: $diagnosis" -ForegroundColor Yellow
   Write-Host "Native save folder: $NativeFolder"
   Write-Host 'No safe automatic repair source exists. Preserve both save folders. Start the unmodified game once and exit normally, then retry this launcher; if it still fails, return the save diagnostic report and both save folders.'
   throw 'Native save import blocked: empty selected VR session has no verified native save index or profile recovery source. No saves were reset; game was not launched.'
  }
  return
 }
 $entries=@(Read-NativeSaveIndex $index)
 foreach($entry in $entries){
  $path=Join-Path $NativeFolder $entry.Name
  if(!(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $entry.Size){
   throw "Native save import blocked: missing or incorrect $path (index expects $($entry.Size) bytes). Preserve the original save folder for recovery; no empty substitute was created."
  }
 }
 if(!$emptyTarget){return}
 $profiles=@($entries|Where-Object {$_.Name -in @('Pickups.sav','Scores.sav')})
 if(!$profiles.Count){return}
 foreach($entry in $profiles){
  $source=Join-Path $NativeFolder $entry.Name
  if($entry.Size -gt 1024 -or (Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Unsafe native profile recovery source: $source"}
 }
 if(Get-Process -Name Dementium -ErrorAction SilentlyContinue){throw 'Close Dementium before empty-session profile recovery.'}
 $backup=Backup-SaveHealth $ModFolder
 $staged=@();$committed=@()
 try{
  # Verify every staged copy before publishing either profile file.
  foreach($entry in $profiles){
   $source=Join-Path $NativeFolder $entry.Name;$destination=Join-Path $emptyTarget $entry.Name
   $hash=(Get-LauncherFileHash $source).Hash;$temp=$destination+'.repair-'+[guid]::NewGuid().ToString('N')
   $item=[pscustomobject]@{Temp=$temp;Destination=$destination;Hash=$hash};$staged+=$item
   Copy-Item -LiteralPath $source -Destination $temp -ErrorAction Stop
   if((Get-Item -LiteralPath $temp).Length -ne $entry.Size -or (Get-LauncherFileHash $temp).Hash -ne $hash){throw 'Native profile copy verification failed'}
  }
  foreach($item in $staged){[IO.File]::Move($item.Temp,$item.Destination);$committed+=$item}
 }catch{
  $failure=$_;$rollbackErrors=@()
  foreach($item in $committed){
   try{
    # Never remove a file changed by another writer after publication.
    if([IO.File]::Exists($item.Destination)){
     if((Get-LauncherFileHash $item.Destination).Hash -ne $item.Hash){throw 'Recovered file changed after publication'}
     [IO.File]::Delete($item.Destination)
    }
   }catch{$rollbackErrors+="$($item.Destination): $($_.Exception.Message)"}
  }
  if($rollbackErrors.Count){throw "Profile repair failed: $($failure.Exception.Message). Rollback needs attention: $($rollbackErrors -join '; '). Backup: $backup"}
  throw $failure
 }finally{
  foreach($item in $staged){if([IO.File]::Exists($item.Temp)){[IO.File]::Delete($item.Temp)}}
 }

 Write-Host "REPAIRED: restored indexed native profile files to the empty VR session. No run, inventory or save index copied. Backup: $backup"
}

function Read-BoundedSaveBytes {
 param([string]$Path,[int]$MaximumBytes)
 # Check the open handle before allocating; malformed files must not exhaust memory.
 $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 try{
  if($MaximumBytes -lt 0 -or $stream.Length -gt $MaximumBytes){throw "Save file exceeds allowed size ($MaximumBytes bytes): $Path"}
  $bytes=New-Object byte[] ([int]$stream.Length);$offset=0
  while($offset -lt $bytes.Length){$read=$stream.Read($bytes,$offset,$bytes.Length-$offset);if($read -le 0){throw "Save file read was incomplete: $Path"};$offset+=$read}
  return ,$bytes
 }finally{$stream.Dispose()}
}
function Read-NativeSaveIndex {
 param([string]$Path)
 $bytes=Read-BoundedSaveBytes -Path $Path -MaximumBytes 1048576
 if($bytes.Length -lt 21 -or $bytes.Length -gt 1048576 -or [BitConverter]::ToUInt32($bytes,0) -ne 0x7f000001 -or [BitConverter]::ToUInt32($bytes,4) -ne 1){throw "Unsupported or damaged save index: $Path"}
 $count=[BitConverter]::ToUInt32($bytes,17);if($count -gt 102){throw "Invalid save index count: $Path"}
 $offset=21;$seen=@{}
 for($i=0;$i -lt $count;$i++){
  if($offset+8 -gt $bytes.Length){throw "Truncated save index: $Path"}
  $length=[BitConverter]::ToUInt64($bytes,$offset);$offset+=8
  if($length -lt 2 -or $length -gt 64 -or $offset+$length+17 -gt $bytes.Length){throw "Invalid save index entry: $Path"}
  $name=[Text.Encoding]::ASCII.GetString($bytes,$offset,[int]$length);$offset+=[int]$length
  if($bytes[$offset] -ne 0 -or $name -notmatch '\A/(Global|Pickups|Scores|Options|InGame[0-9]{2})\.sav\z' -or $seen.ContainsKey($name)){throw "Unsupported save index filename: $Path"}
  $seen[$name]=$true;$offset++
  $size=[BitConverter]::ToUInt64($bytes,$offset);$offset+=16
  if($size -gt 8388608){throw "Invalid save entry size: $Path"}
  [pscustomobject]@{Name=$name.Substring(1);Size=$size}
 }
 if($offset -ne $bytes.Length){throw "Unexpected trailing save index bytes: $Path"}
}
function Repair-IndexedSaveComponents {
 param([string]$Folder)
 $root=Join-Path $Folder 'save-slots';$backup=$null;$errors=[Collections.Generic.List[string]]::new()
 foreach($slotNumber in 1..3){
  $bases=@((Join-Path $root "slot-$slotNumber"),(Join-Path $root "banks/slot-$slotNumber"))
  $targets=@()
  foreach($which in 0..1){
   $pointer=Join-Path $bases[$which] @('working','current')[$which]
   if(!(Test-Path -LiteralPath $pointer -PathType Leaf)){continue}
   $name=(Get-Content -LiteralPath $pointer -Raw).Trim();$pattern=@('\Asession-[0-9-]+\z','\Abank-[0-9-]+\z')[$which]
   if($name -notmatch $pattern){continue}
   $target=Join-Path $bases[$which] $name;if($which -eq 0){$target=Join-Path $target 'Dementium - The Ward/saves'}
   if(Test-Path -LiteralPath $target -PathType Container){$targets+=$target}
  }
  foreach($target in $targets){
   $index=Join-Path $target '-saveindex'
   if(!(Test-Path -LiteralPath $index -PathType Leaf)){continue}
   try{$entries=@(Read-NativeSaveIndex $index)}catch{$errors.Add($_.Exception.Message);continue}
   foreach($entry in $entries){
    # Options is intentionally global in the mod's routing, not part of a slot.
    if($entry.Name -eq 'Options.sav'){continue}
    $path=Join-Path $target $entry.Name
    if(Test-Path -LiteralPath $path -PathType Leaf){
     if((Get-Item -LiteralPath $path).Length -ne $entry.Size){$errors.Add("Save length differs from index: $path")};continue
    }
    if(!$backup){$backup=Backup-SaveHealth $Folder}
    $indexHash=(Get-LauncherFileHash $index).Hash;$donors=@()
    foreach($base in $bases){
     if(!(Test-Path -LiteralPath $base -PathType Container)){continue}
     foreach($generation in Get-ChildItem -LiteralPath $base -Directory){
      if($generation.Name -notmatch '\A(session|bank)-[0-9-]+\z' -or ($generation.Attributes -band [IO.FileAttributes]::ReparsePoint)){continue}
      if(!(Test-Path -LiteralPath (Join-Path $generation.FullName 'complete') -PathType Leaf)){continue}
      $candidate=$generation.FullName;if($generation.Name -like 'session-*'){$candidate=Join-Path $candidate 'Dementium - The Ward/saves'}
      $otherIndex=Join-Path $candidate '-saveindex';$other=Join-Path $candidate $entry.Name
      if(!(Test-Path -LiteralPath $otherIndex -PathType Leaf) -or !(Test-Path -LiteralPath $other -PathType Leaf)){continue}
      if((Get-LauncherFileHash $otherIndex).Hash -ne $indexHash -or (Get-Item -LiteralPath $other).Length -ne $entry.Size){continue}
      # Require all surviving indexed slot files to agree too; never mix progress.
      $same=$true
      foreach($peer in $entries){if($peer.Name -eq 'Options.sav'){continue};$a=Join-Path $target $peer.Name;$b=Join-Path $candidate $peer.Name
       if(Test-Path -LiteralPath $a -PathType Leaf){if(!(Test-Path -LiteralPath $b -PathType Leaf) -or (Get-LauncherFileHash $a).Hash -ne (Get-LauncherFileHash $b).Hash){$same=$false;break}}
      }
      if($same){$donors+=[pscustomobject]@{Path=$other;Hash=(Get-LauncherFileHash $other).Hash}}
     }
    }
    if($donors.Count -and @($donors.Hash|Sort-Object -Unique).Count -eq 1){
     $temp=$path+'.repair-'+[guid]::NewGuid().ToString('N')
     try{
      Copy-Item -LiteralPath $donors[0].Path -Destination $temp -ErrorAction Stop
      if((Get-LauncherFileHash $temp).Hash -ne $donors[0].Hash){throw 'Save recovery copy failed verification'}
      # Move without overwrite: a concurrent writer's file must remain intact.
      [IO.File]::Move($temp,$path)
     }finally{
      if([IO.File]::Exists($temp)){[IO.File]::Delete($temp)}
     }
     Write-Host "REPAIRED SAVE COMPONENT: $path from matching local save generation. Backup: $backup"
    }else{$errors.Add("Missing indexed save file: $path. No unambiguous matching local copy; no replacement invented.")}
   }
  }
 }
 if($errors.Count){foreach($message in $errors){Write-Host "SAVE ERROR: $message" -ForegroundColor Red};throw 'Save component validation failed; launch stopped before the native missing-file crash.'}
}

function Test-SaveHealth {
 param([string]$Folder,[switch]$Rechecked)
 $Folder=[IO.Path]::GetFullPath($Folder)
 Repair-IndexedSaveComponents -Folder $Folder
 $saveWarnings=0
 $root=Join-Path $Folder 'save-slots'
 $problems=[Collections.Generic.List[string]]::new()
 $active=Join-Path $root 'active'
 if(Test-Path -LiteralPath $active -PathType Leaf){if((Get-Content -LiteralPath $active -Raw).Trim() -notmatch '\A[012]\z'){$problems.Add("Invalid active-slot pointer: $active")}}
 foreach($index in 1..3){
  foreach($kind in @('working','current')){
   $slot=Join-Path $root "slot-$index";if($kind -eq 'current'){$slot=Join-Path $root "banks/slot-$index"}
   $pointer=Join-Path $slot $kind
   if(!(Test-Path -LiteralPath $pointer -PathType Leaf)){
    if(Test-Path -LiteralPath $slot -PathType Container){
     $prefix='session-*';if($kind -eq 'current'){$prefix='bank-*'}
     if(@(Get-ChildItem -LiteralPath $slot -Directory -Filter $prefix).Count){$problems.Add("Missing $kind pointer with existing save generations: $slot")}
    };continue
   }
   $name=(Get-Content -LiteralPath $pointer -Raw).Trim();$pattern='\Asession-[0-9-]+\z';if($kind -eq 'current'){$pattern='\Abank-[0-9-]+\z'}
   if($name -notmatch $pattern){$problems.Add("Invalid save pointer: $pointer");continue}
   $target=Join-Path $slot $name
   if(!(Test-Path -LiteralPath (Join-Path $target complete) -PathType Leaf)){$problems.Add("Incomplete or missing save generation: $target");continue}
   if((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint){$problems.Add("Unexpected linked save generation: $target");continue}
   if($kind -eq 'working' -and !(Test-Path -LiteralPath (Join-Path $target 'Dementium - The Ward/saves') -PathType Container)){$problems.Add("Missing native save folder inside $target")}
  }
 }
 if(!('DementiumSaveHash' -as [type])) {Add-Type -TypeDefinition 'public static class DementiumSaveHash { public static ulong Hash(byte[] b,int n) {unchecked {ulong h=14695981039346656037UL;for(int i=0;i<n;i++){h^=b[i];h*=1099511628211UL;}return h;}}}'}
 foreach($base in @($root,(Join-Path $Folder 'medicine-saves'))){
  if(!(Test-Path -LiteralPath $base -PathType Container)){continue}
  foreach($file in Get-ChildItem -LiteralPath $base -File -Recurse){
   if($file.Name -match '\Aingame[0-9]{2}\.sav\z' -and $file.Length -ne 61440){$saveWarnings++;Write-Host "SAVE WARNING: incomplete native save ($($file.Length) bytes, expected 61440): $($file.FullName)" -ForegroundColor Yellow}
   if($file.Name -like '*.vrmeds' -or $file.Name -like '*.vrmeds.equipment'){
    $size=96;$magic=0x44454d44;$offset=88
    if($file.Name -like '*.equipment'){$size=32;$magic=0x31514c44;$offset=24}
    if($file.Length -ne $size){$saveWarnings++;Write-Host "SAVE WARNING: invalid metadata size: $($file.FullName)" -ForegroundColor Yellow;continue}
    $bytes=Read-BoundedSaveBytes -Path $file.FullName -MaximumBytes $size
    if($bytes.Length -ne $size -or [BitConverter]::ToUInt32($bytes,0) -ne $magic){$saveWarnings++;Write-Host "SAVE WARNING: invalid metadata size/header: $($file.FullName)" -ForegroundColor Yellow;continue}
    if(($size -eq 96 -and [BitConverter]::ToUInt32($bytes,4) -ne 2) -or [DementiumSaveHash]::Hash($bytes,$offset) -ne [BitConverter]::ToUInt64($bytes,$offset)){$saveWarnings++;Write-Host "SAVE WARNING: damaged metadata checksum/version: $($file.FullName)" -ForegroundColor Yellow}
   }
  }
 }
 if(($problems.Count -or $saveWarnings) -and !$Rechecked){
  $backup=Backup-SaveHealth $Folder
  if($problems.Count -and (Repair-SavePointers $Folder)){Test-SaveHealth $Folder -Rechecked;return}
  Write-Host 'No safe content repair identified. Backup retained; original save contents unchanged.'
 }
 foreach($problem in $problems){Write-Host "SAVE ERROR: $problem" -ForegroundColor Red}
 if($problems.Count){throw 'Save selection is unsafe. Launch stopped to prevent falling back to older or empty progress. Back up save-slots and return this log for recovery; no save pointers were changed.'}
 Write-Host 'Save checks complete; existing bytes preserved. Warnings may concern historical files. Native save contents are not fully validated.'
}
function Test-ModStorage {
 param([string]$Folder)
 foreach($name in @('medicine-saves','save-slots')) {
  $path=Join-Path $Folder $name
  if(Test-Path -LiteralPath $path) {
   if(!(Test-Path -LiteralPath $path -PathType Container)){throw "$name is a file, but must be a folder. Move that file aside and retry; your saves have not been changed."}
  } else { $null=[IO.Directory]::CreateDirectory($path);Write-Host "REPAIRED: created missing $name folder (existing saves untouched)." }
  $probe=Join-Path $path ('.launcher-write-check-'+[guid]::NewGuid().ToString('N'))
  try { [IO.File]::WriteAllText($probe,'storage check') }
  catch { throw "Cannot write to $path. Move the extracted mod to a writable folder. Original error: $($_.Exception.Message)" }
  finally { if([IO.File]::Exists($probe)){[IO.File]::Delete($probe)} }
 }
}
function Test-GameFiles {
 param([string]$GameFolder,[string]$ModFolder)
 $reference=Join-Path $ModFolder 'Game-Integrity.json'
 if(!(Test-Path -LiteralPath $reference -PathType Leaf)){throw 'Missing Game-Integrity.json. Extract the full updated mod ZIP.'}
 $known=Get-Content -LiteralPath $reference -Raw | ConvertFrom-Json
 $bad=@()
 foreach($entry in $known.files){
  if($entry.name -notin @('Dementium.exe','rom.zar','dxcompiler.dll','steam_api64.dll')){throw 'Invalid game integrity reference.'}
  $p=Join-Path $GameFolder $entry.name
  if(!(Test-Path -LiteralPath $p -PathType Leaf)){Write-Host "MISSING GAME FILE: $p" -ForegroundColor Red;$bad+=$entry.name;continue}
  if((Get-LauncherFileHash -LiteralPath $p).Hash -ne $entry.sha256){Write-Host "GAME FILE DIFFERS: $p (may be damaged, modified, or a different game version)." -ForegroundColor Yellow;$bad+=$entry.name}
 }
 if($bad.Count){throw 'Game integrity differs from the supported clean release. Automatic replacement is unavailable: the mod does not contain original game files. In Steam: Dementium > Properties > Installed Files > Verify integrity. Back up optional game mods first, then retry.'}
 Write-Host 'OK: supported game executable and core data checksums.'
 $overrides=@(Get-ChildItem -LiteralPath $GameFolder -Directory | Where-Object Name -in @('sfx','music','sound','sounds'))
 foreach($dir in $overrides){Write-Host "OPTIONAL AUDIO MOD: $($dir.FullName). Not automatically removed; format checks do not prove playback compatibility."}
}
function Write-BridgeFailureAdvice {
 param([string]$Text)
 Write-GraphicsFailureAdvice -Text $Text
 if($Text -match 'directory_iterator|medicine-saves|saveSlotFiles::install') {Write-Host 'SAVE INITIALIZATION FAILED: check the folder and permission error above. Existing saves will not be deleted.' -ForegroundColor Yellow}
 elseif($Text -match 'E06D7363|e06d7363|3765269347|-529697949'){Write-Host 'BRIDGE INITIALIZATION THREW A C++ EXCEPTION. This code alone does not identify the cause. Return the initialization log path printed above; do not assume the headset or install drive is responsible.' -ForegroundColor Yellow}
 elseif($Text -match 'XR_ERROR|xrCreate|OpenXR'){Write-Host 'VR RUNTIME ERROR: use the selected launcher with its matching headset connection. Return the exact API error and initialization log above.' -ForegroundColor Yellow}
 else {Write-Host 'Launch failed. The exact error and diagnostic log above are needed; no saves or optional mods were removed.' -ForegroundColor Yellow}
}
function Backup-SaveHealth {
 param([string]$Folder)
 if(Get-Process -Name Dementium -ErrorAction SilentlyContinue){throw 'Close Dementium before backing up or repairing saves.'}
 $destination=Join-Path $Folder ('Save-Repair-Backups/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
 $null=[IO.Directory]::CreateDirectory($destination)
 foreach($name in @('save-slots','medicine-saves')) {
  $source=Join-Path $Folder $name
  if(!(Test-Path -LiteralPath $source -PathType Container)){continue}
  $items=@(Get-Item -LiteralPath $source)+@(Get-ChildItem -LiteralPath $source -Recurse -Force)
  if(@($items | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count){throw 'Linked save paths require manual backup; automatic repair stopped.'}
  Copy-Item -LiteralPath $source -Destination (Join-Path $destination $name) -Recurse -ErrorAction Stop
  foreach($file in $items | Where-Object {!$_.PSIsContainer}) {
   $copy=Join-Path (Join-Path $destination $name) $file.FullName.Substring($source.Length).TrimStart('\','/')
   if((Get-LauncherFileHash -LiteralPath $file.FullName).Hash -ne (Get-LauncherFileHash -LiteralPath $copy).Hash){throw 'Save backup verification failed. No save repair attempted.'}
  }
 }
 Write-Host "VERIFIED SAVE BACKUP: $destination"
 return $destination
}
function Repair-SavePointers {
 param([string]$Folder)
 $changed=$false
 foreach($index in 1..3){foreach($kind in @('working','current')){
  $slot=Join-Path $Folder "save-slots/slot-$index";$pattern='\Asession-[0-9-]+\z'
  if($kind -eq 'current'){$slot=Join-Path $Folder "save-slots/banks/slot-$index";$pattern='\Abank-[0-9-]+\z'}
  if(!(Test-Path -LiteralPath $slot -PathType Container)){continue}
  $pointer=Join-Path $slot $kind
  $candidates=@(Get-ChildItem -LiteralPath $slot -Directory | Where-Object {
   $_.Name -match $pattern -and !($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and (Test-Path -LiteralPath (Join-Path $_.FullName complete) -PathType Leaf)
  } | Where-Object {
   $save=$_.FullName;if($kind -eq 'working'){$save=Join-Path $save 'Dementium - The Ward/saves'}
   (Test-Path -LiteralPath $save -PathType Container) -and @((Get-ChildItem -LiteralPath $save -File -ErrorAction Stop)|Where-Object {$_.Name -match '\Aingame[0-9]{2}\.sav\z' -and $_.Length -ne 61440}).Count -eq 0
  })
  $existing='';if(Test-Path -LiteralPath $pointer -PathType Leaf){$existing=(Get-Content -LiteralPath $pointer -Raw).Trim()}
  if($existing -in $candidates.Name){continue}
  if($candidates.Count -ne 1){Write-Host "NO SAFE AUTOMATIC SAVE REPAIR: $slot has $($candidates.Count) eligible generations; no selection made.";continue}
  $progress=$candidates[0].FullName;if($kind -eq 'working'){$progress=Join-Path $progress 'Dementium - The Ward/saves'}
  if(!@(Get-ChildItem -LiteralPath $progress -File | Where-Object {$_.Name -match '\Aingame[0-9]{2}\.sav\z' -and $_.Length -eq 61440}).Count){Write-Host 'No automatic recovery to an empty save.';continue}
  $temp=$pointer+'.repair-'+[guid]::NewGuid().ToString('N')
  [IO.File]::WriteAllText($temp,$candidates[0].Name+[Environment]::NewLine,[Text.Encoding]::ASCII)
  if([IO.File]::Exists($pointer)){[IO.File]::Replace($temp,$pointer,($pointer+'.before-repair-'+[guid]::NewGuid().ToString('N')))}else{[IO.File]::Move($temp,$pointer)}
  Write-Host "REPAIRED SAVE POINTER: $pointer -> $($candidates[0].Name). Save contents unchanged."
  $changed=$true
 }}
 return $changed
}



function Test-GraphicsBinary {
 param([string]$Path)
 if(!(Test-Path -LiteralPath $Path -PathType Leaf)){throw "Missing graphics/VR dependency: $Path. Repair the selected runtime or reinstall its official package."}
 $stream=[IO.File]::OpenRead($Path);$reader=[IO.BinaryReader]::new($stream)
 try {
  if($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D){throw "Invalid graphics/VR binary header: $Path"}
  $stream.Position=60;$offset=$reader.ReadInt32()
  if($offset -lt 64 -or $offset -gt $stream.Length-6){throw "Damaged graphics/VR binary: $Path"}
  $stream.Position=$offset
  if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664){throw "Graphics/VR dependency is invalid or not x64: $Path"}
 } finally {$reader.Dispose();$stream.Dispose()}
 Write-Host "OK: x64 graphics dependency: $Path"
}
function Test-GraphicsStartup {
 param([string]$GameFolder,[string]$ModFolder,[string]$Manifest)
 try {$runtime=Get-Content -LiteralPath $Manifest -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop}
 catch {throw "Unreadable OpenXR runtime manifest: $Manifest. Original error: $($_.Exception.Message)"}
 $library=$runtime.runtime.library_path
 if(!$library -or $library -isnot [string]){throw "OpenXR runtime manifest has no library_path: $Manifest"}
 if(![IO.Path]::IsPathRooted($library)){$library=Join-Path (Split-Path -Parent $Manifest) $library}
 $library=[IO.Path]::GetFullPath($library)
 Test-GraphicsBinary $library
 foreach($name in @('d3d12.dll','dxgi.dll')){Test-GraphicsBinary (Join-Path ([Environment]::SystemDirectory) $name)}
 Write-Host "Selected graphics runtime: $($runtime.runtime.name); library: $library"
 Write-Host "Runtime file version: $((Get-Item -LiteralPath $library).VersionInfo.FileVersion)"
 try {
  $cards=@(Get-CimInstance Win32_VideoController -ErrorAction Stop)
  foreach($card in $cards){Write-Host "GPU inventory: $($card.Name); driver $($card.DriverVersion); status $($card.Status)"}
  Write-Host 'GPU inventory does not establish which adapter the headset uses; bridge initialization records its adapter LUID.'
 } catch {Write-Host "GPU inventory unavailable (non-fatal): $($_.Exception.Message)"}
 foreach($name in @('dxgi.dll','d3d11.dll','d3d12.dll','ReShade.ini')){
  $path=Join-Path $GameFolder $name
  if(Test-Path -LiteralPath $path){Write-Host "GRAPHICS OVERRIDE PRESENT: $path. Possible interaction, not proof of a conflict; left unchanged." -ForegroundColor Yellow}
 }
 Write-Host 'Graphics file checks complete. Resolution, fog, flashlight, tracking and driver settings were not changed. Visual correctness still requires a headset test.'
}
function Write-GraphicsFailureAdvice {
 param([string]$Text)
 $known=@(
  @('DXGI_ERROR_DEVICE_REMOVED|887a0005','Graphics device was removed/reset. Record the device-removed reason; this alone does not prove a bad driver. No automatic restart or setting change.'),
  @('DXGI_ERROR_DEVICE_HUNG|887a0006','GPU work failed or timed out. Return the bridge log and driver version; a renderer fault or driver issue needs investigation.'),
  @('DXGI_ERROR_DEVICE_RESET|887a0007','Graphics device reset. Return the bridge log; save progress before retrying.'),
  @('E_OUTOFMEMORY|8007000e|XR_ERROR_OUT_OF_MEMORY','Graphics/runtime allocation failed. Close unnecessary GPU applications before retrying. Resolution has not been lowered.'),
  @('XR_ERROR_GRAPHICS_DEVICE_INVALID|XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING','OpenXR rejected the graphics device or setup. Return the exact API call/result and adapter details; runtime switching is not an automatic repair.'),
  @('D3DCompile.*(fail|error)|shader.*(fail|error)','Shader setup failed. Packaged graphics files are checked against recovery copies; a remaining shader error requires the exact compiler message.')
 )
 foreach($entry in $known){if($Text -match $entry[0]){Write-Host "GRAPHICS DIAGNOSIS: $($entry[1])" -ForegroundColor Yellow}}
}



# Post-launch evidence is separate from the injection thread's success code.
function Get-VRPresentationDiagnosis {
 param([string]$Text)
 $kind='Starting';$detail='Waiting for VR rendering evidence.';$frame=-1
 $errors=[regex]::Matches($Text,'(?m)^stereo_error=(.+)$')
 $samples=[regex]::Matches($Text,'(?m)^stereo_sample_pipeline .*result=([0-9a-fA-F]{8})')
 $rows=[regex]::Matches($Text,'(?m)^xr_frame=(\d+) eye_draws=(\d+) ui_draws=(\d+) active=(\d+)')
 $menus=[regex]::Matches($Text,'(?m)^native_menu=(-?\d+)')
 if($rows.Count){
  $last=$rows[$rows.Count-1];$frame=[long]$last.Groups[1].Value
  if($last.Groups[4].Value -ne '1'){$kind='Inactive';$detail='VR session is inactive; headset stereo rendering is not confirmed. Check headset connection and the selected runtime.'}
  elseif([int]$last.Groups[2].Value -gt 0 -and $last.Groups[4].Value -eq '1'){$kind='Rendering';$detail='Stereo world draws confirmed.'}
  elseif($menus.Count -and $menus[$menus.Count-1].Groups[1].Value -eq '0' -and $last.Groups[4].Value -eq '1'){$kind='NoWorld';$detail='Gameplay is active but no stereo world draws are recorded.'}
  else{$kind='Menu';$detail='VR/menu is running; a flat menu is normal. Load gameplay to verify stereo.'}
 }
 if($Text -match 'desktop_diagnostic=1'){$kind='DesktopDiagnostic';$detail='Desktop test mode was enabled instead of headset mode.'}
 foreach($sample in $samples){if($sample.Groups[1].Value -match '^[89a-fA-F]'){$kind='PipelineFailure';$detail='Stereo sample adaptation failed: '+$sample.Value;break}}
 if($errors.Count){$kind='Error';$detail=$errors[$errors.Count-1].Value}
 [pscustomobject]@{Kind=$kind;Detail=$detail;Frame=$frame}
}
function Repair-VRLaunchEnvironment {
 # Process-local correction only: never edit the user's global runtime/registry.
 if($env:DEMENTIUM_DESKTOP_STEREO_TEST){
  Write-Host 'FIXED: cleared inherited desktop-test mode for this VR launch.'
  Remove-Item Env:DEMENTIUM_DESKTOP_STEREO_TEST -ErrorAction SilentlyContinue
 }
}
function Invoke-VRLogRepair {
 param([string]$ModFolder,[int]$GameProcessId,[string]$Diagnosis)
 Write-Host 'Checking verified repair sources after the logged failure...'
 Repair-VRLaunchEnvironment
 $running=[bool](Get-Process -Id $GameProcessId -ErrorAction SilentlyContinue)
 try {
  Repair-LauncherFiles -Folder $ModFolder -NoRepair:$running
  if(!$running){Test-ModStorage -Folder $ModFolder}
  Write-Host 'Repair checks finished. Only verified packaged files and required folders are eligible for automatic restoration.'
 } catch {
  Write-Host "REPAIR STATUS: $($_.Exception.Message)" -ForegroundColor Yellow
  if($running){Write-Host 'Close the game and run this launcher again to apply the repair automatically. The current session was not terminated.'}
 }
 if($Diagnosis -eq 'DesktopDiagnostic'){Write-Host 'Desktop-test mode has been cleared for the next launch. Close the game and launch again.'}
}
function Watch-VRPresentation {
 param([int]$GameProcessId,[string]$BridgeLog,[datetime]$StartedUtc,[int]$TimeoutSeconds=180)
 $report=Join-Path ([IO.Path]::GetTempPath()) ('Dementium-VR-presentation-'+$GameProcessId+'-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.json')
 Write-Host "Checking headset rendering after launch. Load your save; flat menus alone are normal. Diagnostic report: $report"
 $clock=[Diagnostics.Stopwatch]::StartNew();$previousFrame=-1;$noWorld=0;$lastKind='';$text=''
 $state=Get-VRPresentationDiagnosis ''
 while($clock.Elapsed.TotalSeconds -lt $TimeoutSeconds){
  if(!(Get-Process -Id $GameProcessId -ErrorAction SilentlyContinue)){$state=[pscustomobject]@{Kind='Exited';Detail='Game exited before stereo gameplay was verified.';Frame=$previousFrame};break}
  $item=Get-Item -LiteralPath $BridgeLog -ErrorAction SilentlyContinue
  if($item -and $item.LastWriteTimeUtc -ge $StartedUtc){
   # Bounded tail; shared log is flushed by the bridge. Do not hold its handle.
   $text=(Get-Content -LiteralPath $BridgeLog -Tail 2200 -ErrorAction SilentlyContinue) -join "`n"
   $state=Get-VRPresentationDiagnosis $text
   if($state.Kind -ne $lastKind){Write-Host $state.Detail;$lastKind=$state.Kind}
   if($state.Kind -eq 'Rendering'){break}
   if($state.Kind -in @('Error','PipelineFailure','DesktopDiagnostic')){break}
   if($state.Frame -ne $previousFrame){
    if($state.Kind -eq 'NoWorld'){$noWorld++}else{$noWorld=0}
    $previousFrame=$state.Frame
   }
   if($noWorld -ge 3){break}
  }
  Start-Sleep -Seconds 2
 }
 [pscustomobject]@{time_utc=[datetime]::UtcNow.ToString('o');game_pid=$GameProcessId;diagnosis=$state;bridge_log=$BridgeLog;seconds=$clock.Elapsed.TotalSeconds}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $report -Encoding UTF8
 if($state.Kind -eq 'Rendering'){Write-Host 'VR check passed: stereo gameplay rendering is active.' -ForegroundColor Green;return}
 if($state.Kind -eq 'Menu'){Write-Host 'Gameplay was not verified during this check. No rendering failure inferred from the menu.';return}
 Write-Host "VR DIAGNOSIS: $($state.Detail)" -ForegroundColor Yellow
 try{Write-LauncherFailureReport -Text ($state.Detail+"`n"+$text) -ModFolder (Split-Path -Parent $BridgeLog)}catch{Write-Host "Failure-report write failed: $_. Original VR diagnosis: $($state.Detail)"}
 Write-GraphicsFailureAdvice -Text ($state.Detail+"`n"+$text)
 Invoke-VRLogRepair -ModFolder (Split-Path -Parent $BridgeLog) -GameProcessId $GameProcessId -Diagnosis $state.Kind
 Write-Host 'Package repair and runtime selection were already applied before launch. The bridge automatically adapts supported scene sample counts. No safe automatic repair is known for this remaining failure; the running game and saves have been left intact.'
 Write-Host "Return these files: $report and $BridgeLog"
}

function Get-LauncherFailureDiagnosis {
 param([string]$Text)
 $rules=@(
  @('used by another process|sharing violation|win32=32\b|80070020','FileInUse','Close the game or application holding the named file, then rerun. The launcher will retry verified recovery; it will not kill unrelated programs.'),
  @('disk full|not enough space|80070070|win32=112\b','DiskSpace','Free space on the named drive, then rerun. No saves or other files are deleted automatically.'),
  @('access.*denied|UnauthorizedAccess|80070005','AccessDenied','The named file/folder is not accessible. Close the game and extract the mod into a writable folder. Do not disable antivirus or change global permissions.'),
  @('XR_ERROR_FORM_FACTOR_UNAVAILABLE|XR_ERROR_SESSION_NOT_READY','HeadsetNotReady','Connect and wake the headset using the selected runtime, then retry.'),
  @('XR_ERROR_RUNTIME_UNAVAILABLE|runtime.*(missing|not found)|Installation not found','RuntimeUnavailable','Check the selected SteamVR or Virtual Desktop installation. Neither launcher requires the other product.'),
  @('XR_ERROR_GRAPHICS_DEVICE_INVALID|GPU does not match|feature level','GraphicsDevice','Return the runtime and adapter information. The launcher cannot safely replace drivers or switch GPUs blindly.'),
  @('Pickups\.sav|Save component validation|Native save import|missing.*\.sav|save index','SaveComponents','Save-index checks and matching-copy recovery run before launch. A missing player-specific file cannot be downloaded as replacement progress.'),
  @('checksum|Recovery archive|Unsafe update|damaged.*ZIP','PackageIntegrity','Extract the full verified release ZIP. Local recovery and official newer-release downloads are checked before installation.'),
  @('win32=193\b|800700c1|BadImageFormat|not.*x64','Architecture','Use the supported 64-bit game, runtime, and mod binaries.'),
  @('win32=126\b|8007007e|module could not be found|dependency','Dependency','Return the resolved DLL path and original OS error. Packaged dependencies can be recovered; external runtime dependencies require their vendor installer.'),
  @('E06D7363|e06d7363|3765269347|-529697949','CppException','This exception code is not the cause. Inspect the recorded stage and std_exception text in the initialization log.'),
  @('DXGI_ERROR_DEVICE|887a000[567]','GraphicsDeviceLost','The graphics device reset or failed. Preserve the exact reason and logs; no automatic setting reset.'),
  @('stereo_error|no stereo world|sample adaptation','StereoRendering','The bridge handles supported sample-count differences. Remaining rendering errors need the exact logged stage/result.')
 )
 foreach($r in $rules){if($Text -match $r[0]){return [pscustomobject]@{Category=$r[1];Action=$r[2]}}}
 [pscustomobject]@{Category='Unknown';Action='Failure preserved for diagnosis. No speculative save, driver, runtime, or settings reset was applied.'}
}
function Write-LauncherFailureReport {
 param([string]$Text,[string]$ModFolder,[string]$InitLog='')
 if($InitLog -and (Test-Path -LiteralPath $InitLog -PathType Leaf)){
  $Text+="`nINITIALIZATION LOG TAIL:`n"+((Get-Content -LiteralPath $InitLog -Tail 100 -ErrorAction SilentlyContinue)-join "`n")
 }
 $diagnosis=Get-LauncherFailureDiagnosis $Text
 $path=Join-Path ([IO.Path]::GetTempPath()) ('Dementium-failure-'+$PID+'-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+[guid]::NewGuid().ToString('N')+'.json')
 [pscustomobject]@{time_utc=[datetime]::UtcNow.ToString('o');category=$diagnosis.Category;action=$diagnosis.Action;error=$Text;mod_folder=$ModFolder;initialization_log=$InitLog;bridge_log=(Join-Path $ModFolder 'bridge.log')}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $path -Encoding UTF8
 Write-Host "DIAGNOSIS: $($diagnosis.Category). $($diagnosis.Action)" -ForegroundColor Yellow
 Write-Host "Failure report: $path"
}
