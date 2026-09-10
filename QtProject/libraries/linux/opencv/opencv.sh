#!/usr/bin/env bash
# Build OpenCV from the same tag macOS uses (package.json cpp-dependencies-macos.opencv).
# Installs under $HOME/.local so a later git checkout does not wipe the prefix.
#
# Image codecs are off because some Linux desktop images ship libtiff/libwebp/
# OpenEXR. OpenCV then exports those 3rdparty targets, but with BUILD_LIST=
# core,imgproc cmake --install does not copy the archives, so find_package fails.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DEPS_ROOT="${VIDEO_SIMILI_LINUX_DEPS:-$HOME/.local/video-simili-deps}"
mkdir -p "$DEPS_ROOT"

REPO_URL="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.opencv.repo | tr -d '"')"
OPENCV_VERSION="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.opencv.version | tr -d '"')"
SOURCE_DIR="$DEPS_ROOT/opencv-source"
BUILD_DIR="$DEPS_ROOT/opencv-build"
INSTALL_DIR="$DEPS_ROOT/opencv-install"
VERSION_FILE="$INSTALL_DIR/.built-version"
MODULES_RELEASE="$INSTALL_DIR/lib/cmake/opencv4/OpenCVModules-release.cmake"

imported_archives_exist() {
  python3 - "$INSTALL_DIR" "$MODULES_RELEASE" <<'PY'
import re, sys
from pathlib import Path
prefix, cmake = Path(sys.argv[1]), Path(sys.argv[2])
if not cmake.is_file():
    sys.exit(1)
missing = []
for match in re.finditer(r'IMPORTED_LOCATION_RELEASE "\$\{_IMPORT_PREFIX\}([^"]+)"', cmake.read_text()):
    path = prefix / match.group(1).lstrip("/")
    if not path.is_file():
        missing.append(str(path))
if missing:
    print("[opencv.sh] Missing imported archives:", file=sys.stderr)
    print("\n".join(missing), file=sys.stderr)
    sys.exit(1)
PY
}

echo "[opencv.sh] Building OpenCV $OPENCV_VERSION from $REPO_URL into $INSTALL_DIR"

if [[ -f "$VERSION_FILE" && "$(cat "$VERSION_FILE")" == "$OPENCV_VERSION" && -f "$INSTALL_DIR/lib/cmake/opencv4/OpenCVConfig.cmake" ]] \
    && imported_archives_exist; then
  echo "[opencv.sh] OpenCV $OPENCV_VERSION already installed at $INSTALL_DIR"
  exit 0
fi

rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$SOURCE_DIR"
git clone "$REPO_URL" -b "$OPENCV_VERSION" --depth 1 "$SOURCE_DIR"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_C_COMPILER="${CC:-gcc}" \
  -DCMAKE_CXX_COMPILER="${CXX:-g++}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_LIST=core,imgproc \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_opencv_apps=OFF \
  -DWITH_PROTOBUF=OFF \
  -DBUILD_PROTOBUF=OFF \
  -DWITH_ADE=OFF \
  -DWITH_TIFF=OFF \
  -DWITH_WEBP=OFF \
  -DWITH_OPENEXR=OFF \
  -DWITH_JPEG=OFF \
  -DWITH_PNG=OFF \
  -DWITH_OPENJPEG=OFF \
  -DWITH_JASPER=OFF \
  -DBUILD_TIFF=OFF \
  -DBUILD_WEBP=OFF \
  -DBUILD_OPENEXR=OFF \
  -DBUILD_JPEG=OFF \
  -DBUILD_PNG=OFF \
  -DBUILD_OPENJPEG=OFF \
  -DOPENCV_GENERATE_PKGCONFIG=YES

cmake --build "$BUILD_DIR" --parallel "$(nproc)"
cmake --install "$BUILD_DIR"

rm -rf "$BUILD_DIR" "$SOURCE_DIR" "$INSTALL_DIR/bin" "$INSTALL_DIR/share"
imported_archives_exist
echo "$OPENCV_VERSION" > "$VERSION_FILE"

echo "[opencv.sh] OpenCV $OPENCV_VERSION ready at $INSTALL_DIR"
