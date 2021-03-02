# OpenCV

To build opencv, first follow :
- https://docs.opencv.org/master/d0/db2/tutorial_macos_install.html
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

# ffmpeg

From ffmpeg folder

```./configure --prefix='/Users/theophanemayaud/Dev/ffmpeg-build' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --enable-avformat```

Then make, then make install