# AGENTS.md

## Cursor Cloud specific instructions

### Overview

This is a C++ Qt6 desktop application ("Video simili duplicate cleaner") that finds duplicate/similar video files via digital video fingerprinting. It uses Qt6 (Widgets, Sql, Concurrent, Test), FFmpeg, and OpenCV. The primary target platforms are macOS and Windows; Linux builds are supported for development/testing.

### Building on Linux

The project's CMakeLists.txt expects FFmpeg via `find_package(FFMPEG)` which relies on an `FFMPEGConfig.cmake` (provided by vcpkg on Windows). On Linux with system FFmpeg packages, a custom `FindFFMPEG.cmake` module is needed at `QtProject/cmake/FindFFMPEG.cmake`. Pass it via:

```
cmake -DCMAKE_MODULE_PATH=/workspace/QtProject/cmake ...
```

Qt 6.8+ is installed via `aqtinstall` at `/opt/qt6/6.8.3/gcc_64`. Point CMake to it:

```
cmake -DCMAKE_PREFIX_PATH=/opt/qt6/6.8.3/gcc_64 ...
```

Full configure + build command:

```bash
cd /workspace/QtProject
cmake -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++-13 \
  -DCMAKE_MODULE_PATH=/workspace/QtProject/cmake \
  -DCMAKE_PREFIX_PATH=/opt/qt6/6.8.3/gcc_64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-linux
```

### Running the application

The application needs the Qt runtime libraries on `LD_LIBRARY_PATH` and the xcb plugin:

```bash
export LD_LIBRARY_PATH=/opt/qt6/6.8.3/gcc_64/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH=/opt/qt6/6.8.3/gcc_64/plugins
export DISPLAY=:1
./QtProject/build-linux/video-simili-duplicate-cleaner
```

For headless/offscreen testing: `QT_QPA_PLATFORM=offscreen ./QtProject/build-linux/video-simili-duplicate-cleaner`

### Running tests

```bash
cd /workspace/QtProject/build-linux && ctest --output-on-failure
```

- `test_comparison` and `test_mainwindow` pass on Linux.
- `test_video_simplified` runs but has expected hash mismatches because reference data was generated on macOS with FFmpeg 7.x, while Linux uses FFmpeg 6.1.x. The SSIM values (0.995+) confirm the output is nearly identical.
- `test_video` requires a large external video dataset not available on the cloud VM; tests will abort with "Expected 218 videos but found 0".

### Lint / commit hooks

Conventional commits are enforced via `commitlint` + `husky`. Run `npm install` to set up hooks. Validate messages with: `echo "feat: message" | npx commitlint`

### Gotchas

- `libxcb-cursor0` must be installed for the Qt xcb platform plugin to load (required when running the GUI on a real or virtual X display).
- The `APP_COPYRIGHT` definition in `CMakeLists.txt` contains UTF-8 characters (©) that require quoting for g++ on Linux. The fix wraps it in escaped quotes.
- `int64_t` is `long` on Linux x64 vs `long long` on macOS/Windows, causing ambiguous `QVariant` conversion. The fix in `db.cpp` adds explicit `static_cast<qlonglong>()`.
