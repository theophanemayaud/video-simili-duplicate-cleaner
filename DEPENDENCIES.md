# Dependencies

Versions are pinned in [`package.json`](package.json) for macOS/Linux and [`QtProject/vcpkg.json`](QtProject/vcpkg.json) for Windows.

Both Windows and macOS require Qt for development. The project is built with CMake, with static linking of dependencies FFmpeg and OpenCV, with direct build management on macOS, but vcpkg on Windows.

# Windows

Via Visual Studio Installer, install  Visual Studio Build Tools / Desktop development with C++, which should install
- windows 11 SDK,
- C++/CLI support for build tools, and MSVC VS 2022
- C++ ARM build tools
- C++ ARM/ARMEC build tools (need standard arm and arm/ArmEC too, not sure why but vcpkg errors otherwise...)
- C++ CMake tools for Windows

Also separately install vcpkg (clone git and run their setup script)

## Set path vars

The previous step installs visual studio c++ compilers, cmake, packaging tools, which must be added to the command line path by going to window's "Edit the environment variables" and adding to the env path var:
- `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` path to cmake
- `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\arm64` path to MakeAppX
- And of course the path to vcpkg, ex `C:\Dev\vcpkg`

Also the env variable `VCPKG_ROOT` pointing to the vcpkg folder.

## vcpkg Approach

To simplify dependency management and cross compilation on windows, this project is trying out vcpkg.

vcpkg is a C++ package manager that simplifies dependency management for ffmpeg and OpenCV. This project is configured to use vcpkg with cmake for both Windows and cross-compilation scenarios.

### Initial vcpkg Setup

1. **Clone vcpkg** (if not already installed):
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ```

2. **Set environment variable** (necessary depending on how cmake is then run):
   ```bash
   # Windows (PowerShell)
   $env:VCPKG_ROOT = "C:\path\to\vcpkg"
   ```

### CMake Integration

The project automatically detects and integrates with vcpkg when the proper vcpkg path is set in QtProject\CMakeUserPresets.json for VCPKG_ROOT

Install Visual Studio, can simply use the Visual Studio Installer to install the specific elements needed: for now Visual Studio Build Tools 2022 "Desktop development with C++" i.e. Windows SDK, and MSVC build tools (compilers)

You will need to use the project's CMakePresets either directly in VS Code by selecting in the CMake extension the correct one for you, or by passing the preset to cmake when invoked from command line.

## Non vcpkg way (hopefully not needed anymore)

### FFmpeg

From guide https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT (WinRT also means UWP or in the guide, its simply windows !): must install MSYS2 and other things, then compile ffmpeg with "hacky" linux commands but using Visual Studio tools. We'll need to run the msys2 command line utility ("shell"), but launching it from the correct windows/visual studio cmd (use the Native x64 one, as we are on x64 platform with x64 windows os version) !

Download and run the msys2 installer : https://www.msys2.org/

- Have it install for example in C:\Dev\msys64 (or another folder, but then adapt the next commands for the correct path)

Download YASM which is needed by ffmpeg to be build, from http://yasm.tortall.net/Download.html (Win64 .exe for general use on 64-bit Windows)

- Rename it yasm.exe
- Move it into C:\Dev\msys64\usr\bin so that msys2 can find it later

To avoid conflicts of utilities between native msys2 and windows later, delete link.exe from C:\Dev\msys64\usr\bin

Edit msys2 launch bat file : C:\Dev\msys64\msys2_shell.cmd (right click, edit) replace `rem set MSYS2_PATH_TYPE=inherit` with `set MSYS2_PATH_TYPE=inherit` this will allow the environment variables for Visual Studio to be transferred to the MSYS2 environment and back

Qt seems to be using _link_ and _cl_ tools from : C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30037\bin\Hostx64\x64\ (you can check this in QT by going in the projects config part, and add a custom build step where link or where cl, disabling the first build step and build, then looking at the compile output), so we'll have to build ffmpeg with these same build tools. From x64 Native VS cmd (after installing Visual studio, it will make available these alternative cmd apps, just search in the apps "native", or "x64 native" and there should be one cmd app named like "x64 Native Tools Command Prompt for VS 2019" ) : launch msys2 from the cmd, by entering its bat file location and pressing enter : C:\Dev\msys64\msys2_shell.cmd

- This will open another "terminal" which is actually msys2, an "environment" that has tools more like unix terminal tools, but understands its on windows and kind of makes the translation between ffmpeg build tools which assumes its on linux, and the actual windows system.
- Once it has launched, it should have inherited automatically the environment variables from the x64 Native VS cmd that we opened it from. Check with "which link" and "which cl" (link and cl are key tools used when building) and make sure the paths are something like "/c/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30037/bin/HostX64/x64/link"
- If it isn't this path, go back and check if you did delete the link.exe (or cl.exe if it exists ?) in the msys64\usr\bin folder, or wherever the "which" command tells you the path is.

In this msys2 shell, we need to install a few things needed by ffmpeg build tools, by running :

- pacman -S make
- pacman -S diffutils

From this msys2 shell, run (NB to copy and paste on this msys2 terminal, you have to right click, copy/paste. Standard keyboard ctrl+c/v don't work !) :

git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg
cd ffmpeg
git checkout tags/nx.x [<= need to insert version, which in ffmpeg standards looks like n4.4]
cd ..
mkdir ffmpeg_build
mkdir ffmpeg_install
cd ffmpeg_build

NB : Be advised, this next command is pretty sloooowww !
../ffmpeg/configure \
--target-os=win64 --arch=x86_64 --toolchain=msvc \
--enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --enable-avformat \
--extra-cflags="-MD" --extra-cxxflags="-MD" --extra-ldflags="/nodefaultlib:libcmt.lib" \
--disable-encoders --disable-muxers --disable-outdevs --disable-bsfs --disable-protocols --enable-protocol=file --disable-filters --enable-filter=scale --disable-lzma \
--prefix=../ffmpeg_install

Problems with mpg files :

remove --disable-protocols --enable-protocol=file : no change

remove --disable-encoders --disable-muxers : no change

remove  --disable-bsfs : now it works !

NB : Here we disable encoders, muxers, output devices (outdevs) and filters except scale which we use. lzma specifically is not needed, its a compression thing for some very very very few files which we shouldn't need. We need to keep bit stream filters (bsfs) for some raw frame decoding apparently (I tried deactivating them also, but it would break for some mpg and mov files...). Also, the argument with nodefaultlib:libcmt.lib is for fixing weird thing about msvc confict warning when building in QT : -1: warning: LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library

make -j80 #number after j is the number of threads used while building or something, more goes faster !
make install

You now have the ffmpeg static lib files in the ffmpeg_install folder (C:\Dev\msys64\home\theop\ffmpeg_install). Just rename the lib\*\*\*.a library files, into standard windows \*\*\*.lib files. They are just named wrong by ffmpeg build tools, but are the same. Do this for example by running, from msys2 shell as before, the command :
```
rename lib '' ../ffmpeg_install/lib/*.a
rename .a '.lib' ../ffmpeg_install/lib/*.a
```

Then copy the include and lib folders to the Qt project windows ffmpeg libraries folder.

### OpenCV

git clone https://github.com/opencv/opencv
cd opencv
git checkout tags/x.x.x [<= need to put correct version there, like 4.5.1]
cd ..
mkdir opencv_build
mkdir opencv_install
cd opencv_build

cmake -G"Visual Studio 16 2019" ^
-DCMAKE_INSTALL_PREFIX:PATH="C:/Dev/opencv_install" ^
-DBUILD_opencv_core:BOOL="1" ^
-DBUILD_opencv_imgproc:BOOL="1" ^
-DCMAKE_CONFIGURATION_TYPES:STRING="Debug;Release" ^
-DBUILD_SHARED_LIBS:BOOL="0" ^
-DBUILD_opencv_ml:BOOL="0" -DBUILD_opencv_dnn:BOOL="0" -DWITH_TIFF:BOOL="0" -Dthunder:BOOL="0" -DWITH_WIN32UI:BOOL="0" -Dnext:BOOL="0" -DWITH_JPEG:BOOL="0" -DWITH_EIGEN:BOOL="0" -DWITH_IMGCODEC_SUNRASTER:BOOL="0" -DBUILD_opencv_java_bindings_generator:BOOL="0" -DBUILD_OPENEXR:BOOL="0" -Dccitt:BOOL="0" -DWITH_LAPACK:BOOL="0" -DWITH_IMGCODEC_PFM:BOOL="0" -DCPACK_SOURCE_7Z:BOOL="0" -DWITH_WEBP:BOOL="0" -DUSE_WIN32_FILEIO:BOOL="0" -DCPACK_SOURCE_ZIP:BOOL="0" -DBUILD_TIFF:BOOL="0" -DBUILD_opencv_features2d:BOOL="0" -DCMAKE_STATIC_LINKER_FLAGS_RELEASE:STRING="" -DWITH_QUIRC:BOOL="0" -DWITH_OPENCL:BOOL="0" -DBUILD_PROTOBUF:BOOL="0" -DVIDEOIO_ENABLE_PLUGINS:BOOL="0" -DBUILD_opencv_objdetect:BOOL="0" -DBUILD_opencv_ts:BOOL="0" -DBUILD_opencv_video:BOOL="0" -Dlzw:BOOL="0" -DWITH_OPENCL_D3D11_NV:BOOL="0" -DWITH_IPP:BOOL="0" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE:STRING="/INCREMENTAL:NO" -Dlogluv:BOOL="0" -DWITH_ITT:BOOL="0" -DBUILD_JAVA:BOOL="0" -Dmdi:BOOL="0" -DVIDEOIO_ENABLE_STRICT_PLUGIN_CHECK:BOOL="0" -DWITH_VTK:BOOL="0" -DENABLE_SOLUTION_FOLDERS:BOOL="0" -DBUILD_opencv_gapi:BOOL="0" -DBUILD_ZLIB:BOOL="0" -DBUILD_PACKAGE:BOOL="0" -DBUILD_WEBP:BOOL="0" -DWITH_OPENJPEG:BOOL="0" -DCPACK_BINARY_NSIS:BOOL="0" -DBUILD_opencv_python_tests:BOOL="0" -DWITH_FFMPEG:BOOL="0" -DWITH_MSMF_DXVA:BOOL="0" -DWITH_MSMF:BOOL="0" -DWITH_PROTOBUF:BOOL="0" -DCMAKE_RC_FLAGS_RELEASE:STRING="" -DBUILD_JPEG:BOOL="0" -DWITH_1394:BOOL="0" -DBUILD_opencv_flann:BOOL="0" -DBUILD_opencv_python_bindings_generator:BOOL="0" -DWITH_JASPER:BOOL="0" -DWITH_OPENCLAMDFFT:BOOL="0" -DWITH_GSTREAMER:BOOL="0" -DWITH_IMGCODEC_PXM:BOOL="0" -DBUILD_opencv_imgcodecs:BOOL="0" -Dpackbits:BOOL="0" -DBUILD_opencv_highgui:BOOL="0" -DCMAKE_RC_FLAGS:STRING="-DWIN32" -DBUILD_OPENJPEG:BOOL="0" -DBUILD_opencv_videoio:BOOL="0" -DBUILD_WITH_STATIC_CRT:BOOL="0" -DBUILD_PERF_TESTS:BOOL="0" -DOPENCV_ENABLE_ALLOCATOR_STATS:BOOL="1" -DWITH_DIRECTX:BOOL="0" -DWITH_IMGCODEC_HDR:BOOL="0" -DBUILD_opencv_objc_bindings_generator:BOOL="0" -DBUILD_TESTS:BOOL="0" -DWITH_ADE:BOOL="0" -DBUILD_IPP_IW:BOOL="0" -DBUILD_JASPER:BOOL="0" -DWITH_OPENCLAMDBLAS:BOOL="0" -DBUILD_PNG:BOOL="0" -DBUILD_opencv_stitching:BOOL="0" -DWITH_PNG:BOOL="0" -DOPENCV_DNN_OPENCL:BOOL="0" -DWITH_OPENEXR:BOOL="0" -DWITH_DSHOW:BOOL="0" -DBUILD_opencv_calib3d:BOOL="0" -DBUILD_opencv_js_bindings_generator:BOOL="0" -DWITH_ARITH_ENC:BOOL="0" -DBUILD_opencv_photo:BOOL="0" -DBUILD_ITT:BOOL="0" -DWITH_ARITH_DEC:BOOL="0" -DBUILD_opencv_apps:BOOL="0"  ^
"C:\Dev\opencv"

NB : for some reason on windows, we need to have two opencv libraries, one for debug and one for release... ! debug libs end with d.lib whereas release just end with .lib

cmake --build . --target install --config release
cmake --build . --target install --config debug

Then copy from install directory the includes and the lib files to the correct location in the project ! NB : zlib is required from opencvcore, although we don't really use it, we need it linked. It will be alongside the built opencv libs

# macOS

Build the static universal dependencies (Intel and Apple Silicon) from the repository root:

```sh
npm run qt-macos
npm run ffmpeg-macos
npm run opencv-macos
```

Build options and output paths are defined in the [Qt](QtProject/libraries/macos/qt/qt.sh), [FFmpeg/libaom](QtProject/libraries/macos/ffmpeg/ffmpeg.sh), and [OpenCV](QtProject/libraries/macos/opencv-arm/opencv.sh) scripts. Linking is handled by [CMake](QtProject/CMakeLists.txt). See [Qt for macOS](https://doc.qt.io/qt-6/macos.html) for platform requirements.
