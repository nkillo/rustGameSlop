@echo off
setlocal
pushd "%~dp0"
if not defined VCToolsInstallDir (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
)
if not defined VCToolsInstallDir (
    echo Install Visual Studio Build Tools with Desktop development with C++.
    exit /b 1
)
if not exist build mkdir build
cd build
if /i "%~1"=="test" (
    cl /nologo /std:c++20 /EHsc /W4 /O2 /MT /utf-8 "..\game.cpp" "..\game_test.cpp" /Fe:game_test.exe
    if errorlevel 1 exit /b 1
    game_test.exe
) else (
    cl /nologo /std:c++20 /EHsc /W4 /O2 /MT /utf-8 /DUNICODE /D_UNICODE "..\main.cpp" "..\game.cpp" /Fe:HollowCrypt.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib
)
set "result=%errorlevel%"
popd
exit /b %result%

