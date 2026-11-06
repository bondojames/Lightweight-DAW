@echo off
rem Usage: tools\build.cmd [release^|debug]
rem Configures, builds and tests from any shell; finds MSVC itself.
rem Optional: set LDAW_CMAKE_ARGS for extra configure arguments.
setlocal
set PRESET=%~1
if "%PRESET%"=="" set PRESET=release
set "VSINSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "PATH=%VSINSTALLER%;%PATH%"
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo MSVC x64 build tools not found.& exit /b 1)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
pushd "%~dp0.."
cmake --preset %PRESET% %LDAW_CMAKE_ARGS% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
ctest --preset %PRESET% || exit /b 1
popd
