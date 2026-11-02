@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
 echo Open an x64 Native Tools Command Prompt for Visual Studio 2022, then run build-source.cmd.
 exit /b 1
)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
 echo Use the x64 Visual Studio developer prompt.
 exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File expand-generated.ps1
if not "%errorlevel%"=="0" exit /b 1
if not exist bin mkdir bin
cl /nologo /O2 /MD /I third_party\minhook\include /I third_party\minhook\src /c third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c /Fo:bin\
if not "%errorlevel%"=="0" exit /b 1
ml64 /nologo /c /Fo bin\native_sphere_entry.obj src\native_sphere_entry.asm
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD /LD /Zi /Fd"bin\DementiumOpenXR-compile.pdb" /I include /I third_party\minhook\include src\bridge.cpp bin\native_sphere_entry.obj bin\buffer.obj bin\hook.obj bin\trampoline.obj bin\hde64.obj /Fe:bin\DementiumOpenXR.dll /Fo:bin\bridge.obj /link d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib user32.lib /DEBUG:FULL /PDB:bin\DementiumOpenXR.pdb /OPT:REF /OPT:ICF /IMPLIB:bin\DementiumOpenXR-import.lib /INCREMENTAL:NO
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD src\attach.cpp /Fe:bin\attach.exe /Fo:bin\attach.obj /link /INCREMENTAL:NO
if not "%errorlevel%"=="0" exit /b 1
echo Built bin\DementiumOpenXR.dll and bin\attach.exe.
