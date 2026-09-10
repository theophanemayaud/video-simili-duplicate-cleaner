#!/usr/bin/env bash
# Install the same Qt version macOS builds (package.json cpp-dependencies-macos.qt),
# as official Linux gcc_64 binaries. Artifacts go under $HOME/.local so a later
# git checkout does not wipe them. Shared Qt is required for xcb plugins.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DEPS_ROOT="$HOME/.local/video-simili-deps"
mkdir -p "$DEPS_ROOT"

QT_TAG="$(npm --prefix "$PROJECT_ROOT" pkg get cpp-dependencies-macos.qt.version | tr -d '"')"
QT_VERSION="${QT_TAG#v}"
PREFIX="$DEPS_ROOT/$QT_VERSION/gcc_64"
INSTALL_LINK="$DEPS_ROOT/qt-install"
VENV="$DEPS_ROOT/qt-aqt-venv"

echo "[qt.sh] Installing Qt $QT_VERSION (tag $QT_TAG) into $DEPS_ROOT"

if [[ -x "$PREFIX/bin/qmake" && "$("$PREFIX/bin/qmake" -query QT_VERSION)" == "$QT_VERSION" ]]; then
  ln -sfn "$PREFIX" "$INSTALL_LINK"
  echo "[qt.sh] Qt $QT_VERSION already installed at $INSTALL_LINK"
  exit 0
fi

python3 -m venv "$VENV"
"$VENV/bin/pip" install -q --upgrade pip aqtinstall
"$VENV/bin/aqt" install-qt linux desktop "$QT_VERSION" linux_gcc_64 -O "$DEPS_ROOT"

if [[ ! -x "$PREFIX/bin/qmake" ]]; then
  echo "[qt.sh] Error: qmake missing at $PREFIX" >&2
  exit 1
fi
ln -sfn "$PREFIX" "$INSTALL_LINK"

echo "[qt.sh] Qt $QT_VERSION ready at $INSTALL_LINK"
