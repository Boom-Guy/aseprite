@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo [Wild Conquest] Fast Run: Aseprite
echo ===================================================

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do (
            call "%%i" >nul 2>nul
        )
    )
)

if exist build\build.ninja (
    cd build
    ninja aseprite
    if %errorlevel% equ 0 (
        echo Starting Aseprite...
        start "" bin\aseprite.exe
        cd ..
        exit /b 0
    )
    cd ..
)

call build_aseprite.bat
if %errorlevel% equ 0 (
    start "" build\bin\aseprite.exe
)
