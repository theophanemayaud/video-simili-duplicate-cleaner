#!/bin/bash
set -ex

# Build universalized static libs
# FFmpeg and libaom (for AV1 support)
# Prerequisites: Homebrew (for yasm, pkg-config), git, cmake, lipo, make

FFMPEG_VERSION=n7.1.1
AOM_REPO_URL=https://aomedia.googlesource.com/aom.git
FFMPEG_REPO_URL=https://github.com/FFmpeg/FFmpeg.git

# Clean up previous builds
rm -rf libaom-* ffmpeg-*

## yasm needed to build aom and ffmpeg
brew install yasm || brew upgrade yasm
## pkg-config needed for ffmpeg to discover dav1d lib
brew install pkg-config

# build aom libs for av1 decoding
mkdir -p libaom-{arm,x86_64}-{build,install} libaom-universalized

git clone --depth=1 "$AOM_REPO_URL" libaom-source
cd libaom-arm-build
cmake ../libaom-source \
    -DCMAKE_INSTALL_PREFIX=../libaom-arm-install \
    -DBUILD_SHARED_LIBS=0 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCONFIG_AV1_ENCODER=0 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
make -j
make install
cd ../libaom-x86_64-build
cmake ../libaom-source \
    -DCMAKE_TOOLCHAIN_FILE=../libaom-source/build/cmake/toolchains/x86_64-macos.cmake \
    -DCMAKE_INSTALL_PREFIX=../libaom-x86_64-install \
	-DBUILD_SHARED_LIBS=0 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCONFIG_AV1_ENCODER=0 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
make -j
make install
## create universalized libaom
cd ../libaom-universalized
cp -r ../libaom-arm-install/include .
mkdir lib
lipo -create -arch arm64 ../libaom-arm-install/lib/libaom.a -arch x86_64 ../libaom-x86_64-install/lib/libaom.a -output lib/libaom.a
cp -r ../libaom-arm-install/lib/pkgconfig ./lib/pkgconfig-arm
cp -r ../libaom-x86_64-install/lib/pkgconfig ./lib/pkgconfig-x86_64-from-arm


# build ffmpeg arm
cd ..
mkdir ffmpeg-{arm,x86_64}-{build,install} ffmpeg-universalized-libs
git clone "$FFMPEG_REPO_URL" ffmpeg-source -b "$FFMPEG_VERSION" --depth 1
cd ffmpeg-arm-build
## arm lib path
export PKG_CONFIG_PATH="../libaom-arm-install/lib/pkgconfig:$PKG_CONFIG_PATH"
../ffmpeg-source/configure --prefix='../ffmpeg-arm-install' --arch=arm64 --target-os=darwin --extra-cflags='-mmacosx-version-min=12.0' --extra-ldflags='-mmacosx-version-min=12.0' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --disable-encoders --disable-muxers --disable-filters --enable-avformat --enable-libaom --disable-lzma
make -j
make install

# cross build ffmpeg x86
cd ../ffmpeg-x86_64-build
# x86 lib path
export PKG_CONFIG_PATH="../libaom-x86_64-install/lib/pkgconfig:$PKG_CONFIG_PATH"
../ffmpeg-source/configure --prefix='../ffmpeg-x86_64-install' --enable-cross-compile --arch=x86_64 --cc='clang -arch x86_64' --target-os=darwin --extra-cflags='-mmacosx-version-min=12.0' --extra-ldflags='-mmacosx-version-min=12.0' --enable-gpl --enable-static --disable-doc --disable-shared --disable-programs --disable-encoders --disable-muxers --disable-filters --enable-avformat --enable-libaom --disable-lzma
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
rm -r -f ffmpeg-source ffmpeg-arm-build ffmpeg-arm-install ffmpeg-x86_64-build ffmpeg-x86_64-install libaom-source libaom-arm-build libaom-arm-install libaom-x86_64-build libaom-x86_64-install
