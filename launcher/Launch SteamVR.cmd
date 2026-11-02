@echo off
setlocal EnableExtensions DisableDelayedExpansion
if not exist "%~dp0Mod Files\Launcher-Health.ps1" goto missing_files
if exist "%~dp0Mod Files\Launcher-Health.ps1\" goto missing_files
set "launcher_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "launcher_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%launcher_powershell%" goto missing_powershell
"%launcher_powershell%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Mod Files\Launcher-Health.ps1" -Runtime SteamVR %*
set "launch_status=%errorlevel%"
if not "%launch_status%"=="0" (
  echo Launcher stopped with exit code %launch_status%.
  echo Keep the error above. Diagnostic logs are in your TEMP folder or LocalAppData\DementiumVR\Diagnostics.
  echo No automatic restart will be attempted.
  if /i not "%~1"=="-ValidateOnly" pause
)
exit /b %launch_status%
:missing_files
echo Missing or invalid Mod Files\Launcher-Health.ps1. Extract the entire ZIP beside this launcher.
if /i not "%~1"=="-ValidateOnly" pause
exit /b 1
:missing_powershell
echo Windows PowerShell could not be found at "%launcher_powershell%".
echo Repair Windows PowerShell before launching. No game or save files were changed.
if /i not "%~1"=="-ValidateOnly" pause
exit /b 1
