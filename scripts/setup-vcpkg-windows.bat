@echo off
setlocal enabledelayedexpansion

echo Setting up vcpkg for Video Simili Duplicate Cleaner...

REM Check if vcpkg root is set
if "%VCPKG_ROOT%"=="" (
    echo VCPKG_ROOT environment variable not set.
    echo Please set VCPKG_ROOT to point to your vcpkg installation.
    echo Example: set VCPKG_ROOT=C:\vcpkg
    pause
    exit /b 1
)

echo Using vcpkg from: %VCPKG_ROOT%

REM Check if vcpkg exists
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo vcpkg.exe not found in %VCPKG_ROOT%
    echo Please run bootstrap-vcpkg.bat in your vcpkg directory first.
    pause
    exit /b 1
)

REM Copy custom triplets
echo Copying custom triplets...
if not exist "%VCPKG_ROOT%\triplets\community" mkdir "%VCPKG_ROOT%\triplets\community"
copy /Y "%~dp0..\triplets\*.cmake" "%VCPKG_ROOT%\triplets\community\"

REM Determine architecture
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

echo Building for architecture: %ARCH%

REM Set triplet based on architecture
if /i "%ARCH%"=="arm64" (
    set TRIPLET=arm64-windows-static
) else if /i "%ARCH%"=="x86" (
    set TRIPLET=x86-windows-static
) else (
    set TRIPLET=x64-windows-static
)

echo Using triplet: %TRIPLET%

REM Install dependencies
echo Installing ffmpeg...
"%VCPKG_ROOT%\vcpkg.exe" install ffmpeg[avcodec,avformat,avutil,swresample,swscale]:!TRIPLET!

echo Installing opencv4...
"%VCPKG_ROOT%\vcpkg.exe" install opencv4[core,imgproc]:!TRIPLET!

echo Dependencies installed successfully!
echo.
echo To build the project, run:
echo cmake -B build -S QtProject -A %ARCH%
echo cmake --build build --config Release

pause