#!/usr/bin/env bash
# Build FFmpeg + libaom from the same tags macOS uses
# (package.json cpp-dependencies-macos.ffmpeg / .aom). Shared FFmpeg keeps the
# Unix CMake pkg-config path unchanged; aom stays static and is linked in.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

dependency_value() {
  npm --prefix "$PROJECT_ROOT" pkg get "cpp-dependencies-macos.$1.$2" | tr -d '"'
}

FFMPEG_REPO_URL="$(dependency_value ffmpeg repo)"
FFMPEG_VERSION="$(dependency_value ffmpeg version)"
AOM_REPO_URL="$(dependency_value aom repo)"
AOM_VERSION="$(dependency_value aom version)"
JOBS="$(nproc)"
VERSION_FILE="ffmpeg-install/.built-version"

echo "[ffmpeg.sh] Building FFmpeg $FFMPEG_VERSION from $FFMPEG_REPO_URL"
echo "[ffmpeg.sh] Building libaom $AOM_VERSION from $AOM_REPO_URL"

if [[ -f "$VERSION_FILE" && "$(cat "$VERSION_FILE")" == "$FFMPEG_VERSION+$AOM_VERSION" && -f ffmpeg-install/lib/pkgconfig/libavcodec.pc ]]; then
  echo "[ffmpeg.sh] FFmpeg $FFMPEG_VERSION already installed"
  exit 0
fi

rm -rf libaom-source libaom-build libaom-install ffmpeg-source ffmpeg-build ffmpeg-install

git clone -b "$AOM_VERSION" --depth=1 "$AOM_REPO_URL" libaom-source
# Same nasm 3+ configure fix as the macOS script (harmless on older nasm).
git -C libaom-source fetch origin 6d2b7f71b98bfa28e372b1f2d85f137280bdb3de
git -C libaom-source cherry-pick --no-commit 6d2b7f71b98bfa28e372b1f2d85f137280bdb3de

cmake -S libaom-source -B libaom-build -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR/libaom-install" \
  -DBUILD_SHARED_LIBS=0 \
  -DENABLE_DOCS=0 \
  -DENABLE_EXAMPLES=0 \
  -DENABLE_TESTDATA=0 \
  -DENABLE_TESTS=0 \
  -DENABLE_TOOLS=0 \
  -DCONFIG_AV1_ENCODER=0
cmake --build libaom-build --parallel "$JOBS"
cmake --install libaom-build

git clone "$FFMPEG_REPO_URL" ffmpeg-source -b "$FFMPEG_VERSION" --depth 1
mkdir ffmpeg-build
cd ffmpeg-build
export PKG_CONFIG_PATH="$SCRIPT_DIR/libaom-install/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
../ffmpeg-source/configure \
  --prefix="$SCRIPT_DIR/ffmpeg-install" \
  --enable-gpl \
  --enable-shared \
  --disable-static \
  --disable-doc \
  --disable-programs \
  --disable-encoders \
  --disable-muxers \
  --disable-filters \
  --enable-avformat \
  --enable-libaom \
  --disable-lzma
make -j"$JOBS"
make install
cd "$SCRIPT_DIR"

rm -rf libaom-source libaom-build ffmpeg-source ffmpeg-build
echo "$FFMPEG_VERSION+$AOM_VERSION" > "$VERSION_FILE"

echo "[ffmpeg.sh] FFmpeg $FFMPEG_VERSION ready at ffmpeg-install"
