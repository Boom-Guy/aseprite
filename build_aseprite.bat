@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo [Wild Conquest] Building Aseprite with MSVC + Ninja
echo ===================================================

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do (
            echo Loading MSVC environment from: %%i
            call "%%i"
        )
    )
)

if not exist build mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ^
      -DLAF_BACKEND=skia ^
      -DSKIA_DIR=D:\TOOL\SKIA\Skia-Windows-Release-x64 ^
      -DSKIA_LIBRARY_DIR=D:\TOOL\SKIA\Skia-Windows-Release-x64\out\Release-x64 ^
      -DSKIA_LIBRARY=D:\TOOL\SKIA\Skia-Windows-Release-x64\out\Release-x64\skia.lib ^
      -G Ninja ..

if %errorlevel% neq 0 (
    echo CMake configuration failed.
    exit /b %errorlevel%
)

ninja aseprite
if %errorlevel% neq 0 (
    echo Ninja build failed.
    exit /b %errorlevel%
)

echo.
echo ===================================================
echo [Wild Conquest] Aseprite Build Complete!
echo Binary located at: build\bin\aseprite.exe
echo ===================================================
cd ..
