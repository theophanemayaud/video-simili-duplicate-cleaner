#!/usr/bin/env bash
# Build OpenCV from the same tag macOS uses (package.json cpp-dependencies-macos.opencv).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

REPO_URL="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.opencv.repo | tr -d '"')"
OPENCV_VERSION="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.opencv.version | tr -d '"')"
SOURCE_DIR="opencv-source"
BUILD_DIR="opencv-build"
INSTALL_DIR="opencv-install"
VERSION_FILE="$INSTALL_DIR/.built-version"

echo "[opencv.sh] Building OpenCV $OPENCV_VERSION from $REPO_URL"

if [[ -f "$VERSION_FILE" && "$(cat "$VERSION_FILE")" == "$OPENCV_VERSION" && -f "$INSTALL_DIR/lib/cmake/opencv4/OpenCVConfig.cmake" ]]; then
  echo "[opencv.sh] OpenCV $OPENCV_VERSION already installed at $INSTALL_DIR"
  exit 0
fi

rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$SOURCE_DIR"
git clone "$REPO_URL" -b "$OPENCV_VERSION" --depth 1 "$SOURCE_DIR"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR/$INSTALL_DIR" \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_LIST=core,imgproc \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_opencv_apps=OFF \
  -DOPENCV_GENERATE_PKGCONFIG=YES

cmake --build "$BUILD_DIR" --parallel "$(nproc)"
cmake --install "$BUILD_DIR"

rm -rf "$BUILD_DIR" "$SOURCE_DIR" "$INSTALL_DIR/bin" "$INSTALL_DIR/share"
echo "$OPENCV_VERSION" > "$VERSION_FILE"

echo "[opencv.sh] OpenCV $OPENCV_VERSION ready at $INSTALL_DIR"
