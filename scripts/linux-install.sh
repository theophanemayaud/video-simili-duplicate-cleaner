#!/usr/bin/env bash
# Idempotent Linux install: OS packages needed to run Qt, then the same
# Qt/OpenCV/FFmpeg versions macOS builds (package.json cpp-dependencies-macos).
# Does not configure or compile the app; use the debug-linux CMake preset after.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

export DEBIAN_FRONTEND=noninteractive
# Some Linux images point /usr/bin/c++ at Clang, which fails to link libstdc++.
# The debug-linux preset already forces g++ for the app; the dep builds must too.
export CC=gcc
export CXX=g++

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  g++ \
  ninja-build \
  pkg-config \
  nasm \
  perl \
  python3 \
  python3-venv \
  python3-pip \
  p7zip-full \
  patchelf \
  zlib1g-dev \
  libgl1-mesa-dev \
  libegl1-mesa-dev \
  libfontconfig1-dev \
  libfreetype-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libx11-xcb-dev \
  libxcb-cursor0 \
  libxcb-cursor-dev \
  libxcb-icccm4-dev \
  libxcb-image0-dev \
  libxcb-keysyms1-dev \
  libxcb-randr0-dev \
  libxcb-render-util0-dev \
  libxcb-shape0-dev \
  libxcb-xinerama0-dev \
  libxcb-xkb-dev \
  libdbus-1-dev \
  cmake

"$ROOT/QtProject/libraries/linux/qt/qt.sh"
"$ROOT/QtProject/libraries/linux/opencv/opencv.sh"
"$ROOT/QtProject/libraries/linux/ffmpeg/ffmpeg.sh"
