@echo off
setlocal
cd /d "%~dp0"
if not exist bin mkdir bin
cl /nologo /std:c++17 /EHsc /W4 /MD /I include tests\control_test.cpp /Fe:bin\control_test.exe /Fo:bin\control_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\control_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD /I include tests\bounded_sample_gate_test.cpp /Fe:bin\bounded_sample_gate_test.exe /Fo:bin\bounded_sample_gate_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\bounded_sample_gate_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD /I include tests\native_uniform_read_bounds_test.cpp /Fe:bin\native_uniform_read_bounds_test.exe /Fo:bin\native_uniform_read_bounds_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\native_uniform_read_bounds_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD /I include tests\native_copy_barrier_policy_test.cpp /Fe:bin\native_copy_barrier_policy_test.exe /Fo:bin\native_copy_barrier_policy_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\native_copy_barrier_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD /I include tests\native_copy_epoch_test.cpp /Fe:bin\native_copy_epoch_test.exe /Fo:bin\native_copy_epoch_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\native_copy_epoch_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MD tests\remote_export_test.cpp /Fe:bin\remote_export_test.exe /Fo:bin\remote_export_test.obj
if not "%errorlevel%"=="0" exit /b 1
bin\remote_export_test.exe bin\DementiumOpenXR.dll
if not "%errorlevel%"=="0" exit /b 1
echo Source checks passed.
