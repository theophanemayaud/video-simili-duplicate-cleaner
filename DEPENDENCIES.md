# Current versions

OpenCV : 4.5.1

FFmpeg : n4.4 on windows, not sure for mac, couldn't find it => next time make them the same !

# Windows

First off, to build the app and dependencies, we need Microsoft Visual Studio (2019). Once installed, we'll have the "Cross Tools Command Prompt" with git and cmake already installed.

## FFmpeg

From guide https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT : must install MSYS2 and other things...

git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg
cd ffmpeg
git checkout tags/nx.x [<= need to insert version, which in ffmpeg standards looks like n4.4]
cd ..
mkdir ffmpeg_build
mkdir ffmpeg_install
cd ffmpeg_build

Qt seems to be unsing link and cl tools from : C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30037\bin\Hostx64\x64\

../ffmpeg/configure \
--target-os=win64 --arch=x86_64 --toolchain=msvc \
--enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --enable-avformat \
--extra-cflags="-MD" --extra-cxxflags="-MD" --extra-ldflags="/nodefaultlib:libcmt.lib" \
--disable-lzma \
--prefix=../ffmpeg_install

 NB : nodefaultlib:libcmt.lib for fixing weird thing about msvc confict :-1: warning: LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library


make -j80 #number after j is the number of threads or something, more goes faster !
make install

Rename the lib\*\*\*.a library files, into standard windows \*\*\*.lib files. They are just named wrong by build tools, but are the same.
With something like :
```
rename lib '' ../ffmpeg_install/lib/*.a
rename .a '.lib' ../ffmpeg_install/lib/*.a
```
Then copy the include and lib folders. 

## OpenCV

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

## OpenCV

To build opencv, first follow :
- [https://docs.opencv.org/master/d0/db2/tutorial_macos_install.html](https://docs.opencv.org/master/d0/db2/tutorial_macos_install.html)
- We want a custom install directory to get all the dependencies in one place so use the cmake flag ```-DCMAKE_INSTALL_PREFIX=/Users/theophanemayaud/Dev/opencv_install```
- If wanting static libraries use flag -DBUILD_SHARED_LIBS=OFF

In the end the cmake command looks like : ```cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/Users/theophanemayaud/Dev/opencv_install -DBUILD_SHARED_LIBS=OFF ../opencv```

You should now have in the ```/Users/theophanemayaud/Dev/opencv_install``` folder all the library files and includes.

Then run ```make```, then run ```make install```

Should try :
```-DWITH_LAPACK=OFF``` and ```WITH_ITT=OFF```
```-D OPENCV_GENERATE_PKGCONFIG=YES```

```-DBUILD_LIST=core,imgproc```

In the end :
```
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/Users/theophanemayaud/Dev/opencv_install -DBUILD_SHARED_LIBS=OFF -DBUILD_LIST=core,imgproc -D OPENCV_GENERATE_PKGCONFIG=YES ../opencv
```

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

## FFmpeg

Clone ffmpeg repository, then from this ffmpeg folder, run the following command or a similar, alternate one to configure the build options.

```
./configure --prefix='/Users/theophanemayaud/Dev/ffmpeg-install' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --enable-avformat --disable-lzma
```

NB : 
- ```--disable-lzma``` is because it has a private api, incompatible with apple app store. It's only for some tiff file compressions.

Then make (NB flag -j means parallel threads, so -j8 will be much faster because of 8 threads !!!), then make install

I then needed to include a bunch of libraries, looking each time I got "undefined symbols for architecture x86_64" at the symbol name, looking it up on internet, and seeing what people said was the missing library. In the end I had to use :
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
