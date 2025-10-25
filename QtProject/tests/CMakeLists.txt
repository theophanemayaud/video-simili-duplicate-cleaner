# Qt Tests configuration
find_package(Qt6 REQUIRED COMPONENTS Test)

# Common test compile definitions
set(TEST_COMPILE_DEFINITIONS
    VID_SIMILI_IN_TESTS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    APP_NAME="Video simili duplicate cleaner"
    APP_COPYRIGHT="Copyright © 2020-2025 Théophane Mayaud, 2018-2019 Kristian Koskimäki"
)

# Common source files from main app that tests need
set(COMMON_APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/video.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/db.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/ssim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/prefs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/files.qrc
)

if(APPLE)
    list(APPEND COMMON_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../app/obj-c.mm
    )
endif()

# Function to create a test executable
function(add_qt_test TEST_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(TEST_ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create test executable
    add_executable(${TEST_NAME}
        ${TEST_ARG_SOURCES}
        ${COMMON_APP_SOURCES}
    )
    
    # Link Qt libraries
    target_link_libraries(${TEST_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Test
    )
    
    # Add compile definitions
    target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_COMPILE_DEFINITIONS})
    
    # Platform-specific libraries
    if(APPLE)
        # OpenCV
        set(OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/macos/opencv-arm/opencv-install")
        target_include_directories(${TEST_NAME} PRIVATE ${OPENCV_ROOT}/include/opencv4)
        target_link_libraries(${TEST_NAME} PRIVATE
            ${OPENCV_ROOT}/lib/libopencv_imgproc.a
            ${OPENCV_ROOT}/lib/libopencv_core.a
            ${OPENCV_ROOT}/lib/opencv4/3rdparty/libzlib.a
            ${OPENCV_ROOT}/lib/opencv4/3rdparty/libittnotify.a
            "-framework OpenCL"
            "-framework Accelerate"
        )
        
        # FFMPEG
        set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/macos/ffmpeg/ffmpeg-universalized-libs")
        target_include_directories(${TEST_NAME} PRIVATE ${FFMPEG_ROOT}/include)
        target_link_directories(${TEST_NAME} PRIVATE ${FFMPEG_ROOT}/lib)
        target_link_libraries(${TEST_NAME} PRIVATE
            avutil avformat swresample avcodec swscale
            bz2 iconv
            "-framework CoreVideo"
            "-framework Security"
            "-framework AudioToolbox"
            "-framework CoreMedia"
            "-framework VideoToolbox"
        )
        
        # AOM
        set(AOM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/macos/ffmpeg/libaom-universalized")
        target_include_directories(${TEST_NAME} PRIVATE ${AOM_ROOT}/include)
        target_link_directories(${TEST_NAME} PRIVATE ${AOM_ROOT}/lib)
        target_link_libraries(${TEST_NAME} PRIVATE aom)
    else()
        # Windows/vcpkg
        find_package(OpenCV REQUIRED COMPONENTS core imgproc)
        target_link_libraries(${TEST_NAME} PRIVATE ${OpenCV_LIBS})
        
        find_package(FFMPEG REQUIRED)
        target_include_directories(${TEST_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
        target_link_directories(${TEST_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
        target_link_libraries(${TEST_NAME} PRIVATE ${FFMPEG_LIBRARIES})
    endif()
    
    # Register test with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties for better output
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

# Test 1: test_comparison
add_qt_test(test_comparison
    SOURCES test_comparison/tst_test_comparison.cpp
)

# Test 2: test_mainwindow (currently empty but structure is there)
add_qt_test(test_mainwindow
    SOURCES test_mainwindow/tst_mainwindow.cpp
)

# Test 3: test_video
add_qt_test(test_video
    SOURCES 
        test_video/tst_video.cpp
        test_video/video_test_helpers.cpp
)

