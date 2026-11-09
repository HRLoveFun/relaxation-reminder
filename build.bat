@echo off
setlocal EnableExtensions

rem Build with Visual Studio 2019 (v142) and the Windows SDK.
rem Usage: build.bat           -> release build
rem        build.bat test      -> test-mode build plus state machine unit tests
rem        build.bat installer -> release build plus Inno Setup installer

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
    "%VSWHERE%" -latest -version "[16.0,17.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\rr_vswhere.txt"
    set /p VSPATH=<"%TEMP%\rr_vswhere.txt"
    del "%TEMP%\rr_vswhere.txt" >nul 2>&1
)

if "%VSPATH%"=="" (
    echo [error] Visual Studio 2019 with the C++ toolset was not found.
    echo         Install "Desktop development with C++" or adjust this script.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [error] Failed to initialize the MSVC environment.
    exit /b 1
)

set "EXTRA_DEFS="
if /I "%~1"=="test" set "EXTRA_DEFS=/DRR_TEST_MODE"

if not exist build mkdir build

rc /nologo /fo build\app.res src\resources\app.rc
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /permissive- /utf-8 /O2 /MT /W4 /EHsc /Isrc /experimental:deterministic "/pathmap:%~dp0=." ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX %EXTRA_DEFS% ^
    /Fo:build\ /Fe:build\RelaxationReminder.exe ^
    src\win32\main.cpp src\win32\reminder_window.cpp src\win32\character_image.cpp ^
    src\win32\rest_sound.cpp src\win32\settings_window.cpp src\win32\splash_window.cpp ^
    src\core\config.cpp src\core\state_machine.cpp src\core\growth.cpp build\app.res ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X64 /DELAYLOAD:winmm.dll ^
    user32.lib gdi32.lib gdiplus.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib advapi32.lib shcore.lib winmm.lib delayimp.lib
if errorlevel 1 exit /b 1

echo.
echo Built: build\RelaxationReminder.exe
for %%f in (build\RelaxationReminder.exe) do echo Size:  %%~zf bytes

if /I "%~1"=="test" (
    call :run_unit_tests
    if errorlevel 1 exit /b 1
)
if /I "%~1"=="installer" (
    call :build_installer
    if errorlevel 1 exit /b 1
)
exit /b 0

:build_installer
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo [error] Inno Setup 6 was not found.
    echo         Install it from https://jrsoftware.org/isdl.php
    exit /b 1
)
echo.
"%ISCC%" installer\RelaxationReminder.iss
if errorlevel 1 exit /b 1
for %%f in (dist\RelaxationReminder-Setup-*.exe) do echo Installer: %%~ff (%%~zf bytes)
exit /b 0

:run_unit_tests
echo.
if not exist build\test mkdir build\test
cl /nologo /std:c++17 /permissive- /utf-8 /O2 /MT /W4 /EHsc /Isrc /experimental:deterministic "/pathmap:%~dp0=." ^
    /Fo:build\test\ /Fe:build\state_machine_test.exe ^
    tests\state_machine_test.cpp src\core\config.cpp src\core\state_machine.cpp src\core\growth.cpp
if errorlevel 1 exit /b 1
build\state_machine_test.exe
exit /b %errorlevel%
