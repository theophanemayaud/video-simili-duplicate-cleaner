# End-to-End Tests for Video Simili Duplicate Cleaner

This directory contains Google Test-based end-to-end tests for the Video Simili Duplicate Cleaner application.

## Test Overview

The end-to-end tests verify the complete workflow of the application:

1. **BasicVideoScanningTest**: Tests video directory scanning and validation
2. **DuplicateDetectionTest**: Tests duplicate video detection algorithms
3. **SpaceCalculationTest**: Tests space savings calculations
4. **AutoDeleteBySizeTest**: Tests auto-deletion functionality (retains larger files)
5. **CompleteWorkflowTest**: Tests the complete end-to-end workflow

## Test Videos

The tests use two sample videos located in `samples/videos/`:
- `Nice_383p_500kbps.mp4` (smaller file, ~324KB)
- `Nice_720p_1000kbps.mp4` (larger file, ~650KB)

These are identical videos at different resolutions, perfect for testing duplicate detection and size-based deletion.

## Prerequisites

### System Requirements

- **Linux**: Ubuntu 20.04 or newer
- **macOS**: macOS 12.0 or newer (Apple Silicon recommended)
- **Windows**: Windows 10 or newer

### Dependencies

1. **CMake** (3.16 or newer)
2. **Qt5 or Qt6** with Core, Widgets, Sql, and Concurrent modules
3. **Google Test** (gtest)
4. **OpenCV** (for image processing)
5. **FFmpeg** (for video processing)

#### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake build-essential
sudo apt install qtbase5-dev qtbase5-dev-tools libqt5sql5-sqlite
sudo apt install libgtest-dev libgmock-dev
sudo apt install libopencv-dev
sudo apt install ffmpeg libavcodec-dev libavformat-dev libswscale-dev
```

**macOS (using Homebrew):**
```bash
brew install cmake
brew install qt@5
brew install googletest
brew install opencv
brew install ffmpeg
```

**Windows:**
- Install Qt from https://www.qt.io/download
- Install vcpkg for Google Test: `vcpkg install gtest`
- Install OpenCV and FFmpeg through vcpkg or manually

## Building the Tests

### Command Line Build

1. Navigate to the test directory:
```bash
cd QtProject/tests/test_end_to_end
```

2. Create a build directory:
```bash
mkdir build
cd build
```

3. Configure with CMake:
```bash
cmake ..
```

4. Build the tests:
```bash
make -j$(nproc)
```

### VSCode Setup

#### Required Extensions

Install these VSCode extensions for the best experience:

1. **C/C++** (Microsoft) - For C++ language support
2. **CMake Tools** (Microsoft) - For CMake integration
3. **Google Test Adapter** (DavidSchuldenfrei) - For running tests in VSCode
4. **Qt tools** (tonka3000) - For Qt development support

#### VSCode Configuration

1. Open the project root in VSCode
2. Install the recommended extensions when prompted
3. Configure the CMake kit:
   - Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on macOS)
   - Type "CMake: Select a Kit"
   - Choose your preferred compiler (GCC, Clang, or MSVC)

#### Building in VSCode

1. Open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`)
2. Type "CMake: Configure" and select it
3. Type "CMake: Build" and select it
4. Or use the CMake extension's build button in the status bar

#### Running Tests in VSCode

**Method 1: Using CMake Tools**
1. Open the Command Palette
2. Type "CMake: Run Tests" and select it
3. View results in the Output panel

**Method 2: Using Google Test Adapter**
1. The extension will automatically discover tests
2. View tests in the Test Explorer sidebar
3. Click the play button next to individual tests or test suites
4. View detailed results with pass/fail status

**Method 3: Using the integrated terminal**
1. Open the integrated terminal (`Ctrl+`` ` or `Cmd+`` `)
2. Navigate to the build directory
3. Run tests manually:
```bash
./video-simili-duplicate-cleaner-e2e-tests
```

## Running the Tests

### Basic Test Execution

```bash
# Run all tests
./video-simili-duplicate-cleaner-e2e-tests

# Run specific test
./video-simili-duplicate-cleaner-e2e-tests --gtest_filter="VideoEndToEndTest.BasicVideoScanningTest"

# Run tests with verbose output
./video-simili-duplicate-cleaner-e2e-tests --gtest_filter="*" --verbose
```

### Google Test Options

Common Google Test command-line options:

```bash
# List all available tests
./video-simili-duplicate-cleaner-e2e-tests --gtest_list_tests

# Run tests matching a pattern
./video-simili-duplicate-cleaner-e2e-tests --gtest_filter="*Duplicate*"

# Run tests with detailed output
./video-simili-duplicate-cleaner-e2e-tests --gtest_print_time=1

# Generate XML test report
./video-simili-duplicate-cleaner-e2e-tests --gtest_output=xml:test_results.xml
```

### Using CTest

If you prefer CTest (CMake's test runner):

```bash
# Run all tests
ctest

# Run tests with verbose output
ctest --verbose

# Run specific test
ctest -R "VideoEndToEndTest.BasicVideoScanningTest"
```

## Test Results and Debugging

### Expected Output

When all tests pass, you should see output similar to:
```
[==========] Running 5 tests from 1 test case.
[----------] Global test environment set-up.
[----------] 5 tests from VideoEndToEndTest
[ RUN      ] VideoEndToEndTest.BasicVideoScanningTest
[       OK ] VideoEndToEndTest.BasicVideoScanningTest (120 ms)
[ RUN      ] VideoEndToEndTest.DuplicateDetectionTest
[       OK ] VideoEndToEndTest.DuplicateDetectionTest (85 ms)
[ RUN      ] VideoEndToEndTest.SpaceCalculationTest
[       OK ] VideoEndToEndTest.SpaceCalculationTest (90 ms)
[ RUN      ] VideoEndToEndTest.AutoDeleteBySizeTest
[       OK ] VideoEndToEndTest.AutoDeleteBySizeTest (110 ms)
[ RUN      ] VideoEndToEndTest.CompleteWorkflowTest
[       OK ] VideoEndToEndTest.CompleteWorkflowTest (180 ms)
[----------] 5 tests from VideoEndToEndTest (585 ms total)

[----------] Global test environment tear-down
[==========] 5 tests from 1 test case ran. (585 ms total)
[  PASSED  ] 5 tests.
```

### Debugging Test Failures

1. **Check sample videos**: Ensure the sample videos exist in `samples/videos/`
2. **Verify dependencies**: Make sure all required libraries are installed
3. **Check permissions**: Ensure the test can create temporary directories
4. **Enable verbose output**: Add debug output to understand what's happening

### VSCode Debugging

1. Set breakpoints in the test code
2. Use the Debug view (`Ctrl+Shift+D` / `Cmd+Shift+D`)
3. Create a launch configuration for the test executable
4. Add this to `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug E2E Tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/QtProject/tests/test_end_to_end/build/video-simili-duplicate-cleaner-e2e-tests",
            "args": ["--gtest_filter=VideoEndToEndTest.BasicVideoScanningTest"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
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

## Test Structure

### Files Overview

- `CMakeLists.txt`: Build configuration
- `test_end_to_end.cpp`: Main test file with all test cases
- `test_helpers.h`: Helper function declarations
- `test_helpers.cpp`: Helper function implementations
- `README.md`: This documentation file

### Adding New Tests

To add a new test:

1. Add a new `TEST_F(VideoEndToEndTest, YourTestName)` function to `test_end_to_end.cpp`
2. Use the `TestHelpers` class for common operations
3. Follow the existing test patterns for setup and teardown
4. Rebuild and run the tests

### Test Data Management

- Tests create temporary directories automatically
- Sample videos are copied to test directories for each test
- Test directories are cleaned up after each test
- No permanent files are created or modified

## Troubleshooting

### Common Issues

1. **"Samples directory does not exist"**: Ensure the samples/videos directory exists with the test videos
2. **"Failed to copy sample videos"**: Check file permissions and disk space
3. **"Google Test not found"**: Install googletest development package
4. **"Qt not found"**: Install Qt development libraries and set Qt5_DIR or Qt6_DIR
5. **"OpenCV not found"**: Install OpenCV development package

### Platform-Specific Issues

**Linux:**
- May need to install additional Qt modules: `sudo apt install qt5-default`
- Ensure proper paths in CMake configuration

**macOS:**
- May need to set Qt path: `export Qt5_DIR=/usr/local/opt/qt5/lib/cmake/Qt5`
- Ensure Xcode command line tools are installed

**Windows:**
- Set proper paths for Qt, OpenCV, and FFmpeg in CMake
- May need to adjust library paths in CMakeLists.txt

## Contributing

When adding new tests:
1. Follow the existing naming convention
2. Include proper setup and teardown
3. Use descriptive test names and assertions
4. Add documentation for complex test scenarios
5. Ensure tests are independent and can run in any order

## License

These tests are part of the Video Simili Duplicate Cleaner project and follow the same license terms.