$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$allowed = @('src/inventory_reference_art.hpp', 'src/mod_menu_art.hpp', 'src/inventory_art.hpp', 'src/wristband_texture.hpp')
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $PSScriptRoot 'generated-ui-headers.zip'))
try {
 foreach ($entry in $archive.Entries) {
  if ($entry.FullName -cnotin $allowed) { throw "Unexpected generated header: $($entry.FullName)" }
  $target = Join-Path $PSScriptRoot $entry.FullName
  [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
 }
} finally { $archive.Dispose() }
