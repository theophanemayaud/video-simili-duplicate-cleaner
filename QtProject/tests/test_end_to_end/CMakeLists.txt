cmake_minimum_required(VERSION 3.16)

project(video-simili-duplicate-cleaner-e2e-tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required Qt components (same as main app)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql Concurrent)

# Find Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Set up paths
set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../app")
set(SAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../samples")

# Include directories
include_directories(${APP_DIR})

# Common source files from the main app (excluding main.cpp)
set(APP_SOURCES
    ${APP_DIR}/video.cpp
    ${APP_DIR}/video.h
    ${APP_DIR}/comparison.cpp
    ${APP_DIR}/comparison.h
    ${APP_DIR}/comparison.ui
    ${APP_DIR}/mainwindow.cpp
    ${APP_DIR}/mainwindow.h
    ${APP_DIR}/mainwindow.ui
    ${APP_DIR}/db.cpp
    ${APP_DIR}/db.h
    ${APP_DIR}/prefs.h
    ${APP_DIR}/videometadata.h
    ${APP_DIR}/videoprocessingpool.cpp
    ${APP_DIR}/videoprocessingpool.h
    ${APP_DIR}/ssim.cpp
    ${APP_DIR}/thumbnail.h
    ${APP_DIR}/ffmpeg.h
    ${APP_DIR}/files.qrc
)

if(APPLE)
    list(APPEND APP_SOURCES
        ${APP_DIR}/obj-c.mm
        ${APP_DIR}/obj-c.h
    )
endif()

# Test sources
set(TEST_SOURCES
    test_end_to_end.cpp
)

# Create the test executable
add_executable(video-simili-duplicate-cleaner-e2e-tests
    ${APP_SOURCES}
    ${TEST_SOURCES}
)

# Compile definitions (reuse from main app)
target_compile_definitions(video-simili-duplicate-cleaner-e2e-tests PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x100000
    APP_NAME="Video simili duplicate cleaner"
    APP_COPYRIGHT="Copyright © 2020-2024 Théophane Mayaud, 2018-2019 Kristian Koskimäki"
    VID_SIMILI_IN_TESTS
    SAMPLES_DIR="${SAMPLES_DIR}"
)

# Platform-specific configurations (same as main app)
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    # OpenCV
    set(OPENCV_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/opencv-arm/opencv-install")
    include_directories(${OPENCV_ROOT}/include/opencv4)
    target_link_libraries(video-simili-duplicate-cleaner-e2e-tests PRIVATE
        ${OPENCV_ROOT}/lib/libopencv_imgproc.a
        ${OPENCV_ROOT}/lib/libopencv_core.a
        ${OPENCV_ROOT}/lib/opencv4/3rdparty/libzlib.a
        ${OPENCV_ROOT}/lib/opencv4/3rdparty/libittnotify.a
        "-framework OpenCL"
        "-framework Accelerate"
    )
    
    # FFMPEG
    set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/ffmpeg/ffmpeg-universalized-libs")
    include_directories(${FFMPEG_ROOT}/include)
    target_link_directories(video-simili-duplicate-cleaner-e2e-tests PRIVATE
        ${FFMPEG_ROOT}/lib
    )
    target_link_libraries(video-simili-duplicate-cleaner-e2e-tests PRIVATE
        avutil avformat swresample avcodec swscale
        bz2 iconv
        "-framework CoreVideo"
        "-framework Security"
        "-framework AudioToolbox"
        "-framework CoreMedia"
        "-framework VideoToolbox"
    )
    
    # AOM
    set(AOM_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/ffmpeg/libaom-universalized")
    include_directories(${AOM_ROOT}/include)
    target_link_directories(video-simili-duplicate-cleaner-e2e-tests PRIVATE
        ${AOM_ROOT}/lib
    )
    target_link_libraries(video-simili-duplicate-cleaner-e2e-tests PRIVATE
        aom
    )
endif()

# Link with Qt and Google Test (NO Qt Test, only Google Test)
target_link_libraries(video-simili-duplicate-cleaner-e2e-tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Concurrent
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
gtest_discover_tests(video-simili-duplicate-cleaner-e2e-tests)