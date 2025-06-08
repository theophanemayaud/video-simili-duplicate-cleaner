#!/bin/bash
# Build OpenCV static lib for macOS (universal: arm64 + x86_64)
# Usage: ./opencv.sh

set -xe

OPENCV_VERSION=4.8.0
SOURCE_DIR="opencv-source"
BUILD_DIR="opencv-build"
INSTALL_DIR="opencv-install"
REPO_URL="https://github.com/opencv/opencv.git"

echo "[opencv.sh] Building OpenCV $OPENCV_VERSION for macOS..."

# Prerequisites
if ! command -v cmake >/dev/null 2>&1; then
  echo "[opencv.sh] Error: cmake is not installed. Please install it (e.g. brew install cmake)." >&2
  exit 1
fi
if ! command -v git >/dev/null 2>&1; then
  echo "[opencv.sh] Error: git is not installed. Please install it (e.g. brew install git)." >&2
  exit 1
fi

# Clean up previous build if exists
rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$SOURCE_DIR"

# Clone OpenCV
git clone  "$REPO_URL" -b "$OPENCV_VERSION" --depth 1 "$SOURCE_DIR"

# Create build directory
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=../"$INSTALL_DIR" \
      -DCMAKE_OSX_ARCHITECTURES='x86_64;arm64' \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_LIST=core,imgproc \
      -DOPENCV_GENERATE_PKGCONFIG=YES \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      "../$SOURCE_DIR"

# Build and install
make -j$(sysctl -n hw.ncpu)
make install

cd ..

# Optional: Clean up build and unnecessary files
rm -rf "$BUILD_DIR" "$SOURCE_DIR" "$INSTALL_DIR"/bin "$INSTALL_DIR"/share

echo "[opencv.sh] OpenCV $OPENCV_VERSION build and install complete. Libraries are in $INSTALL_DIR/lib, includes in $INSTALL_DIR/include."
