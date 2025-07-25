# Current versions

OpenCV : 4.8.0

FFmpeg : 7.1.1

Both windows and macos require QT as well of course, but that's necessary to install for development of course. Still using qmake (for tests, final deployment, debugging in QT Creator) but also starting to use cmake for development in vs code, and hopefully building libraries on windows with vcpkg.

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

# MacOS

Should now be able to use the libraries/macos/xx/x.sh scripts for simple setup (can be run with `npm run opencv-macos` or ffmpeg-macos)

## OpenCV

### TLDR script
```
mkdir opencv-build
mkdir opencv-install
git clone https://github.com/opencv/opencv.git -b 4.8.0 --depth 1
cd opencv-build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=../opencv-install \
      -DCMAKE_OSX_ARCHITECTURES='x86_64;arm64' \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_LIST=core,imgproc \
      -DOPENCV_GENERATE_PKGCONFIG=YES \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      ../opencv
make -j8
make install

#cleanup
cd ..
rm -r -f opencv-build opencv ./opencv-install/bin ./opencv-install/share
```

Flag `-DCMAKE_OSX_ARCHITECTURES='x86_64;arm64'` is for compiling from arm macs, which creates a universal library (for both intel and arm macs). Witout it, the library will work for arm only qt linking.

Then you can copy the libraries listed in the .pc file, NB to use a framework add -framwork FrameWorkName instead of -lLibraryName for libraries
like :
```
macx: LIBS += -L/Users/theophanemayaud/Dev/opencv_install/lib/opencv4/3rdparty -lzlib -lippiw -lippicv -framework OpenCL

macx: LIBS += -L/Library/Developer/CommandLineTools/SDKs/MacOSX11.0.sdk/System/Library/Frameworks -framework Accelerate -lm -ldl
```

To Test :
-DCMAKE_OSX_DEPLOYMENT_TARGET="10.13" to have builds for a little bit older versions, not only the last one !
But this needs to have the following line in the terminal .zshrc file (or enter it in the terminal) :
export MACOSX_DEPLOYMENT_TARGET=10.13

### More explanations

To build opencv, first follow :
- [OpenCV Installation in MacOS](https://docs.opencv.org/master/d0/db2/tutorial_macos_install.html)
- We want a custom install directory to get all the dependencies in one place so use the cmake flag ```-DCMAKE_INSTALL_PREFIX=/Users/theophanemayaud/Dev/opencv_install```
- If wanting static libraries use flag -DBUILD_SHARED_LIBS=OFF

In the end the cmake command looks like : ```cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/Users/theophanemayaud/Dev/opencv_install -DBUILD_SHARED_LIBS=OFF ../opencv```

You should now have in the ```/Users/theophanemayaud/Dev/opencv_install``` folder all the library files and includes.

Then run ```make```, then run ```make install```

Should try :
`-DWITH_LAPACK=OFF` and `WITH_ITT=OFF`
`-D OPENCV_GENERATE_PKGCONFIG=YES`

`-DBUILD_LIST=core,imgproc`


## FFmpeg

### TLDR script

```bash
# build aom libs for av1 decoding
mkdir -p libaom-{x86_64-build,x86_64-install,arm-build,arm-install,universalized}
# pkg-config needed for ffmpeg to discover dav1d lib
brew install pkg-config
git clone --depth=1 https://aomedia.googlesource.com/aom.git libaom
cd libaom-arm-build
cmake ../libaom \
    -DCMAKE_INSTALL_PREFIX=../libaom-arm-install \
    -DBUILD_SHARED_LIBS=0 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCONFIG_AV1_ENCODER=0 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
make -j
make install
cd ../libaom-x86_64-build
cmake ../libaom \
    -DCMAKE_TOOLCHAIN_FILE=../libaom/build/cmake/toolchains/x86_64-macos.cmake \
    -DCMAKE_INSTALL_PREFIX=../libaom-x86_64-install \
	-DBUILD_SHARED_LIBS=0 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCONFIG_AV1_ENCODER=0 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
make -j
make install
cd ../libaom-universalized
cp -r ../libaom-arm-install/include .
mkdir lib
lipo -create -arch arm64 ../libaom-arm-install/lib/libaom.a -arch x86_64 ../libaom-x86_64-install/lib/libaom.a -output lib/libaom.a
cp -r ../libaom-arm-install/lib/pkgconfig ./lib/pkgconfig-arm
cp -r ../libaom-x86_64-install/lib/pkgconfig ./lib/pkgconfig-x86_64-from-arm

# build ffmpeg arm
cd ..
mkdir ffmpeg-{arm,x86_64}-{build,install} ffmpeg-universalized-libs
git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg -b n4.4.5 --depth 1
cd ffmpeg-arm-build
#arm lib path
export PKG_CONFIG_PATH="../libaom-arm-install/lib/pkgconfig:$PKG_CONFIG_PATH"
../ffmpeg/configure --prefix='../ffmpeg-arm-install' --arch=arm64 --target-os=darwin --extra-cflags='-mmacosx-version-min=12.0' --extra-ldflags='-mmacosx-version-min=12.0' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --disable-encoders --disable-muxers --disable-filters --enable-avformat --enable-libaom --disable-lzma
make -j
make install

# cross build ffmpeg x86
brew install yasm || brew upgrade yasm
cd ../ffmpeg-x86_64-build
#x86 lib path
export PKG_CONFIG_PATH="../libaom-x86_64-install/lib/pkgconfig:$PKG_CONFIG_PATH"
../ffmpeg/configure --prefix='../ffmpeg-x86_64-install' --enable-cross-compile --arch=x86_64 --cc='clang -arch x86_64' --target-os=darwin --extra-cflags='-mmacosx-version-min=12.0' --extra-ldflags='-mmacosx-version-min=12.0' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --disable-encoders --disable-muxers --disable-filters --enable-avformat --enable-libaom --disable-lzma
make -j
make install

# create universalized libs
cd ../ffmpeg-universalized-libs
cp -r ../ffmpeg-arm-install/include .
mkdir lib
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libavcodec.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libavcodec.a -output lib/libavcodec.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libavdevice.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libavdevice.a -output lib/libavdevice.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libavfilter.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libavfilter.a -output lib/libavfilter.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libavformat.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libavformat.a -output lib/libavformat.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libavutil.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libavutil.a -output lib/libavutil.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libpostproc.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libpostproc.a -output lib/libpostproc.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libswresample.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libswresample.a -output lib/libswresample.a
lipo -create -arch arm64 ../ffmpeg-arm-install/lib/libswscale.a -arch x86_64 ../ffmpeg-x86_64-install/lib/libswscale.a -output lib/libswscale.a
cp -r ../ffmpeg-arm-install/lib/pkgconfig ./lib/pkgconfig-arm
cp -r ../ffmpeg-x86_64-install/lib/pkgconfig ./lib/pkgconfig-x86_64-from-arm

#cleanup
cd ..
rm -r -f ffmpeg-arm-build ffmpeg-arm-install ffmpeg-x86_64-build ffmpeg-x86_64-install ffmpeg libaom-arm-build libaom-arm-install libaom-x86_64-build libaom-x86_64-install libaom
```

NB : 
- ```--disable-lzma``` is because it has a private api, incompatible with apple app store. It's only for some tiff file compressions.
- aom is adding a lot of complexity just for supporting only one more codec: av1. It would really be nice if ffmpeg included one by default. 
    - The process could be a bit simpler using brew as the brew aom lib also includes static libs
    - But then I'd need to install the arm brew and x86_64 brew (`arch -x86_64 [usual brew install curl and script]` which installs in /usr/local/bin/brew)
    - It is then possible to install x86_64 brew packages with `arch -x86_64 /usr/local/bin/brew [brew commands]`, and packages are then installed in /usr/local/include and /usr/local/lib/, compared to arm brew installing in /opt/homebrew/lib
    - And then from those arm and x86_64 brew installed libs, create a universalized aom lib... But that's not much simpler in the end so I just build from scratch
- I disabled encoders with `--disable-encoders` (and accompanying muxers) as we don't create new videos in the app, only read them, which together with disabling filters which we also don't use in the app, reducing lib sizes (which used to be commited to github) a lot although the end app doesn't change as static linking already removed the bloat.
- I tried disabling ffmpeg protocols with `--disable-protocols` but the app then doesn't work and fails to open any video (probably at least requires file protocol). It added ~0.1MB of size to the app
- I tried disabling ffmpeg bitstream filters with `--disable-bsfs` but the app then fails to do avcodec_send_packet on specific formats (saw some MPG vids but not sure codec) so just left it on. It added ~0.8MB of size to the app.

### More explanations

Similar to guide [FFmpeg Compilation guide macOS](https://trac.ffmpeg.org/wiki/CompilationGuide/macOS)

Clone ffmpeg repository, then from this ffmpeg folder, run the following command or a similar, alternate one to configure the build options.

FFmpeg doesn't seem to support building as a univeral library (targeting both arm and intel macs), but the individual architecture libraries can be combined with apple tools : `lipo` to combine de libs, and `install_name_tool` to replace references to frameworks (listed with `otool`), as per [This python function in a github repo](https://github.com/ColorsWind/FFmpeg-macOS/blob/main/make_universal.py)

For the x86\_64 cross build from arm, ffmpeg configure scripts fails because of missing nasm/yasm (native code assembler), so simply install it with `brew install yasm`

See the [full ffmpeg build script below](#full-ffmpeg-build-script)

For configuring in QT the libs to use, I then needed to include a bunch of libraries, looking each time I got "undefined symbols for architecture x86_64" at the symbol name, looking it up on internet, and seeing what people said was the missing library. In the end I had to use :
```
## libavformat static libs dependencies (from pckgconfig file)
macx: LIBS += -L$$PWD/libraries/ffmpeg/lib -lavutil -lavformat -lswresample -lavcodec \
                                            -lbz2 -liconv -llzma -Wl,-no_compact_unwind \
                                            -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox

macx: PRE_TARGETDEPS += $$PWD/libraries/ffmpeg/lib/libavutil.a \
                        $$PWD/libraries/ffmpeg/lib/libavformat.a
                        $$PWD/libraries/ffmpeg/lib/libswresample.a
                        $$PWD/libraries/ffmpeg/lib/libavcodec.a

```
