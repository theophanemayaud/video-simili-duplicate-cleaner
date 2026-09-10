#!/usr/bin/env bash
# Install the same Qt version macOS builds (package.json cpp-dependencies-macos.qt),
# as official Linux gcc_64 binaries. Artifacts go under $HOME/.local so a later
# git checkout does not wipe them. Shared Qt is required for xcb plugins.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DEPS_ROOT="${VIDEO_SIMILI_LINUX_DEPS:-$HOME/.local/video-simili-deps}"
mkdir -p "$DEPS_ROOT"

QT_TAG="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.qt.version | tr -d '"')"
QT_VERSION="${QT_TAG#v}"
INSTALL_LINK="$DEPS_ROOT/qt-install"
VENV="$DEPS_ROOT/qt-aqt-venv"

echo "[qt.sh] Installing Qt $QT_VERSION (tag $QT_TAG) into $DEPS_ROOT"

resolve_prefix() {
  local candidate installed
  for candidate in "$DEPS_ROOT/$QT_VERSION/gcc_64" "$DEPS_ROOT/$QT_VERSION/linux_gcc_64"; do
    if [[ -x "$candidate/bin/qmake" ]]; then
      installed="$("$candidate/bin/qmake" -query QT_VERSION || true)"
      if [[ "$installed" == "$QT_VERSION" ]]; then
        echo "$candidate"
        return 0
      fi
    fi
  done
  return 1
}

if prefix="$(resolve_prefix)"; then
  ln -sfn "$prefix" "$INSTALL_LINK"
  echo "[qt.sh] Qt $QT_VERSION already installed at $INSTALL_LINK"
  exit 0
fi

python3 -m venv "$VENV"
"$VENV/bin/pip" install -q --upgrade pip aqtinstall

arch="$("$VENV/bin/aqt" list-qt linux desktop --arch "$QT_VERSION")"
echo "[qt.sh] Available architectures: $arch"
if echo "$arch" | grep -qw gcc_64; then
  aqt_arch=gcc_64
elif echo "$arch" | grep -qw linux_gcc_64; then
  aqt_arch=linux_gcc_64
else
  echo "[qt.sh] Error: no gcc_64 architecture for Qt $QT_VERSION" >&2
  exit 1
fi

"$VENV/bin/aqt" install-qt linux desktop "$QT_VERSION" "$aqt_arch" -O "$DEPS_ROOT"

prefix=""
for candidate in "$DEPS_ROOT/$QT_VERSION/gcc_64" "$DEPS_ROOT/$QT_VERSION/linux_gcc_64" "$DEPS_ROOT/$QT_VERSION/$aqt_arch"; do
  if [[ -x "$candidate/bin/qmake" ]]; then
    prefix="$candidate"
    break
  fi
done
if [[ -z "$prefix" ]]; then
  echo "[qt.sh] Error: qmake missing under $DEPS_ROOT/$QT_VERSION" >&2
  exit 1
fi
ln -sfn "$prefix" "$INSTALL_LINK"

echo "[qt.sh] Qt $QT_VERSION ready at $INSTALL_LINK"
