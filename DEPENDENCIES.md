# OpenCV

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

# ffmpeg

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