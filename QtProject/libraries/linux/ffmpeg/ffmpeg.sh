#!/usr/bin/env bash
# Build FFmpeg + libaom from the same tags macOS uses
# (package.json cpp-dependencies-macos.ffmpeg / .aom). Shared FFmpeg keeps the
# Unix CMake pkg-config path unchanged; aom stays static and is linked in.
# Installs under $HOME/.local so a later git checkout does not wipe the prefix.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DEPS_ROOT="${VIDEO_SIMILI_LINUX_DEPS:-$HOME/.local/video-simili-deps}"
mkdir -p "$DEPS_ROOT"

dependency_value() {
  npm --prefix "$PROJECT_ROOT" pkg get "cpp-dependencies-macos.$1.$2" | tr -d '"'
}

FFMPEG_REPO_URL="$(dependency_value ffmpeg repo)"
FFMPEG_VERSION="$(dependency_value ffmpeg version)"
AOM_REPO_URL="$(dependency_value aom repo)"
AOM_VERSION="$(dependency_value aom version)"
JOBS="$(nproc)"
INSTALL_DIR="$DEPS_ROOT/ffmpeg-install"
AOM_INSTALL="$DEPS_ROOT/libaom-install"
VERSION_FILE="$INSTALL_DIR/.built-version"

echo "[ffmpeg.sh] Building FFmpeg $FFMPEG_VERSION from $FFMPEG_REPO_URL"
echo "[ffmpeg.sh] Building libaom $AOM_VERSION from $AOM_REPO_URL"

if [[ -f "$VERSION_FILE" && "$(cat "$VERSION_FILE")" == "$FFMPEG_VERSION+$AOM_VERSION" && -f "$INSTALL_DIR/lib/pkgconfig/libavcodec.pc" ]]; then
  echo "[ffmpeg.sh] FFmpeg $FFMPEG_VERSION already installed"
  exit 0
fi

rm -rf "$DEPS_ROOT/libaom-source" "$DEPS_ROOT/libaom-build" "$AOM_INSTALL" \
  "$DEPS_ROOT/ffmpeg-source" "$DEPS_ROOT/ffmpeg-build" "$INSTALL_DIR"

git clone -b "$AOM_VERSION" --depth=1 "$AOM_REPO_URL" "$DEPS_ROOT/libaom-source"
# Same nasm 3+ configure fix as the macOS script (harmless on older nasm).
git -C "$DEPS_ROOT/libaom-source" fetch origin 6d2b7f71b98bfa28e372b1f2d85f137280bdb3de
git -C "$DEPS_ROOT/libaom-source" cherry-pick --no-commit 6d2b7f71b98bfa28e372b1f2d85f137280bdb3de

cmake -S "$DEPS_ROOT/libaom-source" -B "$DEPS_ROOT/libaom-build" -G Ninja \
  -DCMAKE_C_COMPILER="${CC:-gcc}" \
  -DCMAKE_CXX_COMPILER="${CXX:-g++}" \
  -DCMAKE_INSTALL_PREFIX="$AOM_INSTALL" \
  -DBUILD_SHARED_LIBS=0 \
  -DENABLE_DOCS=0 \
  -DENABLE_EXAMPLES=0 \
  -DENABLE_TESTDATA=0 \
  -DENABLE_TESTS=0 \
  -DENABLE_TOOLS=0 \
  -DCONFIG_AV1_ENCODER=0
cmake --build "$DEPS_ROOT/libaom-build" --parallel "$JOBS"
cmake --install "$DEPS_ROOT/libaom-build"

git clone "$FFMPEG_REPO_URL" "$DEPS_ROOT/ffmpeg-source" -b "$FFMPEG_VERSION" --depth 1
mkdir "$DEPS_ROOT/ffmpeg-build"
cd "$DEPS_ROOT/ffmpeg-build"
export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"
export PKG_CONFIG_PATH="$AOM_INSTALL/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
../ffmpeg-source/configure \
  --cc="$CC" \
  --cxx="$CXX" \
  --prefix="$INSTALL_DIR" \
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
cd "$DEPS_ROOT"

# Shared FFmpeg libs need $ORIGIN so libavcodec can find libswresample without
# LD_LIBRARY_PATH. Executable RUNPATH is not searched for those transitive deps.
if ! command -v patchelf >/dev/null 2>&1; then
  echo "[ffmpeg.sh] Error: patchelf is required to set \$ORIGIN on FFmpeg libraries." >&2
  exit 1
fi
for lib in "$INSTALL_DIR"/lib/lib*.so*; do
  if [[ -f "$lib" && ! -L "$lib" ]]; then
    patchelf --set-rpath '$ORIGIN' "$lib"
  fi
done

rm -rf "$DEPS_ROOT/libaom-source" "$DEPS_ROOT/libaom-build" "$DEPS_ROOT/ffmpeg-source" "$DEPS_ROOT/ffmpeg-build"
echo "$FFMPEG_VERSION+$AOM_VERSION" > "$VERSION_FILE"

echo "[ffmpeg.sh] FFmpeg $FFMPEG_VERSION ready at $INSTALL_DIR"
