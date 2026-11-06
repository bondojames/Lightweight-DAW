@echo off
rem Usage: tools\package.cmd NN   Packages the release build as Phase NN.
setlocal
if "%~1"=="" (echo Usage: tools\package.cmd PHASE_NUMBER& exit /b 1)
set "VSINSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "PATH=%VSINSTALLER%;%PATH%"
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo MSVC x64 build tools not found.& exit /b 1)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\packaging\portable\package.ps1" -Phase %~1
