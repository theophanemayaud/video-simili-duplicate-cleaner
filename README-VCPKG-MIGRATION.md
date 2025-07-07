# Migration Guide: Manual Dependencies → vcpkg

This guide helps you migrate from the manual dependency management system to the new vcpkg-based system.

## Why Migrate?

The new vcpkg system offers several advantages:

✅ **Simplified setup** - No more manual ffmpeg/OpenCV compilation  
✅ **Cross-compilation support** - Build ARM64 and x86 from any platform  
✅ **Consistent versions** - Reproducible builds across machines  
✅ **Automatic dependency resolution** - vcpkg handles all transitive dependencies  
✅ **Static linking** - Self-contained executables  

## Migration Steps

### 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat  # Windows
./bootstrap-vcpkg.sh   # Linux/macOS

# Set environment variable
set VCPKG_ROOT=C:\vcpkg  # Windows
export VCPKG_ROOT=/path/to/vcpkg  # Linux/macOS
```

### 2. Install Dependencies

**Option A: Use provided scripts (Recommended)**
```powershell
# For x64 Windows
.\scripts\setup-vcpkg-windows.ps1 -Architecture x64

# For ARM64 Windows  
.\scripts\setup-vcpkg-windows.ps1 -Architecture arm64

# For x86 Windows
.\scripts\setup-vcpkg-windows.ps1 -Architecture x86
```

**Option B: Manual installation**
```bash
# Install for x64 Windows
vcpkg install ffmpeg[avcodec,avformat,avutil,swresample,swscale]:x64-windows-static
vcpkg install opencv4[core,imgproc]:x64-windows-static

# Install for ARM64 Windows
vcpkg install ffmpeg[avcodec,avformat,avutil,swresample,swscale]:arm64-windows-static  
vcpkg install opencv4[core,imgproc]:arm64-windows-static
```

### 3. Build the Project

**Option A: Use build script**
```powershell
.\scripts\build-windows.ps1 -Architecture x64 -Configuration Release
```

**Option B: Manual cmake**
```bash
cmake -B build -S QtProject -A x64
cmake --build build --config Release
```

## What Changes

### Old System (Manual)
- Manual ffmpeg compilation with MSYS2
- Manual OpenCV compilation with Visual Studio  
- Complex library management in `common.pri`
- Architecture-specific binary copies
- Debug/Release library variants

### New System (vcpkg)
- Automated dependency management
- Single cmake configuration
- Cross-compilation support
- Consistent static linking
- Automatic transitive dependencies

## Comparison

| Aspect | Manual | vcpkg |
|--------|--------|-------|
| Setup time | ~2-3 hours | ~15-30 minutes |
| Cross-compilation | Complex/manual | Built-in |
| Updates | Manual recompilation | `vcpkg upgrade` |
| Reproducibility | Machine-dependent | Consistent |
| Maintenance | High | Low |

## Troubleshooting Migration

### "Cannot find OpenCV"
- Ensure you're using `opencv4` not `opencv` in vcpkg
- Check the triplet matches your target architecture

### "FFmpeg linking errors"  
- Verify you're using static triplets (`*-windows-static`)
- Check that all required ffmpeg features are installed

### "Qt integration issues"
- Ensure Qt is found before vcpkg packages
- Use the cmake configuration, not qmake

### "Cross-compilation not working"
- Verify the custom triplets are copied to vcpkg
- Check the cmake generator platform matches the triplet

## Rollback (if needed)

If you need to return to the manual system:

1. Switch to qmake instead of cmake
2. Use the existing `common.pri` configuration  
3. Remove vcpkg-related files:
   - `vcpkg.json`
   - `vcpkg-configuration.json`  
   - `triplets/` directory
   - `scripts/` directory

## Getting Help

- Check the main [DEPENDENCIES.md](DEPENDENCIES.md) file
- Review cmake configuration in `QtProject/CMakeLists.txt`
- Verify vcpkg package information: `vcpkg search ffmpeg`