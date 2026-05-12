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

# If Qt is built without the offscreen platform plugin, use the native platform for CTest.
set(TEST_QPA_PLATFORM "offscreen")
if(APPLE)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    if(QT_QMAKE_EXECUTABLE)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        get_filename_component(QT_INSTALL_DIR "${QT_BIN_DIR}" DIRECTORY)
        if(NOT EXISTS "${QT_INSTALL_DIR}/plugins/platforms/libqoffscreen.dylib")
            set(TEST_QPA_PLATFORM "cocoa")
        endif()
    endif()
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

    target_link_video_processing_dependencies(${TEST_NAME})

    # Register test with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Set test properties for better output
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "QT_QPA_PLATFORM=${TEST_QPA_PLATFORM}"
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

# Test 3: test_auto_delete
add_qt_test(test_auto_delete
    SOURCES test_auto_delete/tst_auto_delete.cpp
)

# Test 4: test_video
add_qt_test(test_video
    SOURCES
        test_video/tst_video.cpp
        test_video/video_test_helpers.cpp
        test_video_simplified/video_simplified_test_helpers.cpp
)
# Set longer CTest timeout for test_video (contains 100GB tests that can take 30+ minutes)
set_tests_properties(test_video PROPERTIES TIMEOUT 3600)

# Test 5: test_video_simplified
add_qt_test(test_video_simplified
    SOURCES
        test_video_simplified/tst_video_simplified.cpp
        test_video_simplified/video_simplified_test_helpers.cpp
)
