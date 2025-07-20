# End-to-End Tests for Video Simili Duplicate Cleaner

This directory contains simplified end-to-end tests that test the actual app functionality using Google Test.

## What These Tests Do

These tests use the **actual app classes and methods** rather than custom test helpers:

1. **VideoScanningTest**: Tests video scanning using real `MainWindow` and `Video` classes
2. **AutoDeleteBySizeTest**: Tests auto-deletion by size using the real `Comparison::on_autoDelOnlySizeDiffersButton_clicked()` method
3. **DuplicateDetectionTest**: Tests duplicate detection using real `Comparison::bothVideosMatch()` and `reportMatchingVideos()` methods  
4. **CompleteWorkflowTest**: Tests the complete workflow from scanning to auto-deletion

## Test Setup

The tests:
- Copy the sample videos from `samples/videos/` to a temporary test directory
- Use the actual app classes (`MainWindow`, `Comparison`, `Video`, etc.)
- Test real app functionality, not custom helper methods
- Clean up automatically after each test

## Prerequisites

- CMake 3.16+
- Qt 6 or Qt 5 with Core, Widgets, Sql, Concurrent components
- Google Test
- VS Code with CMake Tools extension

### Installing Dependencies

**macOS (using Homebrew):**
```bash
# Install Qt and other dependencies
brew install qt6 cmake googletest

# Set Qt path for CMake (add to your ~/.zshrc or ~/.bash_profile)
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6/lib/cmake:$CMAKE_PREFIX_PATH"
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake build-essential
sudo apt install qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite3
sudo apt install libgtest-dev libgmock-dev
```

**Windows:**
- Install Qt from https://www.qt.io/download
- Install Visual Studio with C++ support
- Install vcpkg: `vcpkg install gtest`

## Running Tests

### Option 1: Using VS Code with CMake Tools

1. **Open the project** in VS Code (open the root `video-simili-duplicate-cleaner` folder)
2. **Install CMake Tools extension** if not already installed
3. **Configure with tests enabled**:
   - Press `Ctrl+Shift+P` → "CMake: Configure"
   - Choose your kit (e.g., GCC, Clang, or MSVC)
   - **Important**: Add `-DBUILD_TESTS=ON` to CMake configure args in VS Code settings
4. **Build everything**:
   - Press `Ctrl+Shift+P` → "CMake: Build"
   - This builds both the main app and tests
5. **Run the tests**:
   - Press `Ctrl+Shift+P` → "CMake: Run Tests"
   - Tests will appear in the Test Explorer panel

### Option 2: Using Terminal (From Root Project)

1. **Navigate to project root**:
   ```bash
   cd video-simili-duplicate-cleaner/QtProject
   ```
2. **Configure with tests enabled**:
   ```bash
   mkdir -p build && cd build
   cmake .. -DBUILD_TESTS=ON
   ```
3. **Build everything**:
   ```bash
   make
   ```
4. **Run tests**:
   ```bash
   ctest --verbose
   # or directly:
   ./tests/test_end_to_end/video-simili-duplicate-cleaner-e2e-tests
   ```

### Option 3: Standalone Test Build

If you prefer to build tests separately:
```bash
cd QtProject/tests/test_end_to_end
mkdir -p build && cd build
cmake ..
make
./video-simili-duplicate-cleaner-e2e-tests
```

## VS Code Configuration

For better VS Code integration, you can create these configuration files:

### `.vscode/settings.json` (if not exists):
```json
{
    "cmake.configureSettings": {
        "CMAKE_BUILD_TYPE": "Debug"
    },
    "cmake.buildDirectory": "${workspaceFolder}/QtProject/build"
}
```

### `.vscode/launch.json` (for debugging):
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug E2E Tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/QtProject/build/tests/test_end_to_end/video-simili-duplicate-cleaner-e2e-tests",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/QtProject/build",
            "environment": [],
            "externalConsole": false,
            "MIMode": "lldb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
```

## Expected Output

When tests run successfully, you should see output like:
```
[==========] Running 4 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 4 tests from VideoEndToEndTest
[ RUN      ] VideoEndToEndTest.VideoScanningTest
=== Running VideoScanningTest ===
Test directory: /tmp/tmp.XXXXXXXX
Copied Nice_720p_1000kbps.mp4 to test directory
Copied Nice_383p_500kbps.mp4 to test directory
...
[       OK ] VideoEndToEndTest.VideoScanningTest (XXXX ms)
...
[==========] 4 tests from 1 test suite ran. (XXXX ms total)
[  PASSED  ] 4 tests.
```

## Troubleshooting

### Common Build Issues

**"Could not find a package configuration file provided by QT":**
```bash
# On macOS with Homebrew:
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6/lib/cmake:$CMAKE_PREFIX_PATH"

# On Linux, try:
sudo apt install qt6-base-dev

# On Windows, set in CMake GUI or command line:
cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/msvc2019_64" ..
```

**"Google Test not found":**
```bash
# On macOS:
brew install googletest

# On Ubuntu:
sudo apt install libgtest-dev libgmock-dev

# On Windows:
vcpkg install gtest
```

**Missing video processing libraries:**
- The tests reuse the same OpenCV and FFmpeg configuration as the main app
- If you get FFmpeg/OpenCV errors, make sure the main app builds first

### Runtime Issues

- **Tests failing**: Check that sample videos exist in `samples/videos/` directory
- **Permission errors**: Ensure write access to temporary directories
- **Qt application errors**: Make sure you have a display available (or run in headless mode)

## Key Improvements Over Previous Version

✅ **Uses actual app classes**: `MainWindow`, `Comparison`, `Video`  
✅ **Tests real functionality**: Calls actual methods like `on_autoDelOnlySizeDiffersButton_clicked()`  
✅ **Much simpler**: Removed 100+ lines of custom test helper code  
✅ **More reliable**: Tests what the app actually does, not test-specific implementations  
✅ **Easier to maintain**: Changes to app code automatically reflected in tests  
✅ **Google Test only**: No Qt Test dependencies, just Google Test as requested