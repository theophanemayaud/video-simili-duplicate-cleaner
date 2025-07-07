# Windows CMake Setup with vcpkg

Quick reference for setting up Windows builds with cmake and vcpkg.

## Prerequisites

- Visual Studio 2019 or later
- Qt 5 or 6
- cmake 3.16+
- Git

## Quick Setup

### 1. Install vcpkg
```cmd
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg
```

### 2. Install Dependencies
```powershell
# Run from project root
.\scripts\setup-vcpkg-windows.ps1 -Architecture x64
```

### 3. Build
```powershell
.\scripts\build-windows.ps1 -Architecture x64 -Configuration Release
```

## Cross-Compilation

### ARM64 Windows (from any platform)
```powershell
.\scripts\setup-vcpkg-windows.ps1 -Architecture arm64 -CrossCompile
.\scripts\build-windows.ps1 -Architecture arm64
```

### x86 Windows (from ARM64)
```powershell
.\scripts\setup-vcpkg-windows.ps1 -Architecture x86 -CrossCompile  
.\scripts\build-windows.ps1 -Architecture x86
```

## Manual Commands

### Install Dependencies
```cmd
vcpkg install ffmpeg[avcodec,avformat,avutil,swresample,swscale]:x64-windows-static
vcpkg install opencv4[core,imgproc]:x64-windows-static
```

### Build with cmake
```cmd
cmake -B build -S QtProject -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

## Architecture Mapping

| Target | CMake Platform | vcpkg Triplet |
|--------|----------------|---------------|
| 64-bit Intel/AMD | x64 | x64-windows-static |
| 32-bit Intel/AMD | Win32 | x86-windows-static |
| 64-bit ARM | ARM64 | arm64-windows-static |

## Environment Variables

Set these for automatic detection:

```cmd
set VCPKG_ROOT=C:\vcpkg
set VCPKG_DEFAULT_TRIPLET=x64-windows-static
```

## Troubleshooting

### "vcpkg not found"
- Ensure `VCPKG_ROOT` environment variable is set
- Run `bootstrap-vcpkg.bat` in vcpkg directory

### "Package not found"  
- Check triplet: `vcpkg list` 
- Reinstall: `vcpkg remove <package>:triplet && vcpkg install <package>:triplet`

### "Static linking errors"
- Verify using `-static` triplets
- Check MSVC runtime: should be `/MT` for Release, `/MTd` for Debug

### "Cross-compilation fails"
- Ensure custom triplets are copied to vcpkg
- Verify Visual Studio has the target architecture installed