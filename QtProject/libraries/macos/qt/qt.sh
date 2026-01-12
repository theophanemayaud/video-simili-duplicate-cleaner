#!/bin/bash
set -euo pipefail

# Build Qt static libs for macOS (universal: arm64 + x86_64)
# Usage: ./qt.sh

QT_VERSION="6.9.3"
QT_REPO_URL="https://code.qt.io/qt/qt5.git"
SOURCE_DIR="qt-source"
BUILD_DIR="qt-build"
INSTALL_DIR="qt-install"
QT_SUBMODULES="qtbase" # Only pull what we need for Core/Widgets/Sql/Concurrent

# Always run in script directory even when invoked from outside so artifacts land here
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "[qt.sh] Building Qt $QT_VERSION for macOS..."

# Prerequisites (QtBase/Widgets/Sql/Concurrent do not need Python; configure
# uses init-repository under the hood when -init-submodules is set, which needs perl)
for tool in cmake git ninja; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[qt.sh] Error: $tool is required. Install it (e.g. brew install $tool)." >&2
    exit 1
  fi
done

# Clean up previous build if exists
rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$SOURCE_DIR"

# Clone Qt
git clone "$QT_REPO_URL" --branch "v$QT_VERSION" --depth 1 "$SOURCE_DIR"

# Configure via Qt's configure wrapper (documented path)
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
"../$SOURCE_DIR/configure" \
  -init-submodules \
  -submodules "$QT_SUBMODULES" \
  -prefix "$SCRIPT_DIR/$INSTALL_DIR" \
  -release -static -no-framework \
  -nomake examples -nomake tests \
  -- \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="12.0"

# Build and install
cmake --build . --parallel
cmake --install .
cd "$SCRIPT_DIR"

# Optional: clean up sources/build artifacts to save space
rm -rf "$BUILD_DIR" "$SOURCE_DIR"

echo "[qt.sh] Qt $QT_VERSION build and install complete. Libraries are in $INSTALL_DIR/lib, includes in $INSTALL_DIR/include."
