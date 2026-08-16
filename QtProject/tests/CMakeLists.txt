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
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/visualfingerprint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison/internal/videopairmatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison/internal/backgroundmatchdiscovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/db.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison/comparison.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison/internal/discoveryprogressslider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/comparison/internal/ssim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/prefs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/files.qrc
)

if(APPLE)
    list(APPEND COMMON_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../app/obj-c.mm
    )
endif()

# Prefer Qt's offscreen platform so widget tests do not depend on a window server.
set(TEST_QPA_PLATFORM "offscreen")
if(APPLE)
    if(TARGET Qt6::QOffscreenIntegrationPlugin)
        set(TEST_PLATFORM_PLUGIN Qt6::QOffscreenIntegrationPlugin)
    else()
        set(TEST_QPA_PLATFORM "cocoa")
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
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../app
    )

    # Add compile definitions
    target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_COMPILE_DEFINITIONS})

    target_link_video_processing_dependencies(${TEST_NAME})

    if(TEST_PLATFORM_PLUGIN)
        # Static Qt plugins must be imported into each test executable. Checking
        # the CMake target also handles libqoffscreen.a, not only a .dylib file.
        qt_import_plugins(${TEST_NAME} INCLUDE_BY_TYPE platforms ${TEST_PLATFORM_PLUGIN})
    endif()

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

# Test 3: checked-in fixture cleanup flow
add_qt_test(test_repo_auto_delete
    SOURCES test_repo_auto_delete/tst_auto_delete.cpp
)
set_tests_properties(test_repo_auto_delete PROPERTIES LABELS "repo-fixtures")

# Test 4: optional ~/Dev extraction and reference regression
add_qt_test(test_local_video_extraction_regression
    SOURCES
        local/video_extraction_regression/tst_video_extraction_regression.cpp
        local/video_corpus_test_helpers.cpp
        video_extraction_test_helpers.cpp
)

# Test 5: optional ~/Dev end-to-end whole-app scans
add_qt_test(test_local_whole_app_scan
    SOURCES
        local/whole_app_scan/tst_whole_app_scan.cpp
        local/video_corpus_test_helpers.cpp
        video_extraction_test_helpers.cpp
)
set_tests_properties(test_local_video_extraction_regression test_local_whole_app_scan
    PROPERTIES LABELS "local-fixtures" RUN_SERIAL TRUE
)

# Test 6: separately mounted large corpus. Its functions can take 30+ minutes.
add_qt_test(test_local_large_video_corpus
    SOURCES
        local/large_video_corpus/tst_large_video_corpus.cpp
        local/video_corpus_test_helpers.cpp
        video_extraction_test_helpers.cpp
)
set_tests_properties(test_local_large_video_corpus PROPERTIES
    TIMEOUT 7200
    LABELS "external-corpus;requires-mounted-100gb"
    RUN_SERIAL TRUE
)

# Test 7: checked-in metadata and thumbnail extraction snapshots
add_qt_test(test_repo_video_extraction_regression
    SOURCES
        test_repo_video_extraction_regression/tst_video_extraction_regression.cpp
        video_extraction_test_helpers.cpp
)
set_tests_properties(test_repo_video_extraction_regression PROPERTIES LABELS "repo-fixtures")

# Test 8: checked-in matching contract.
add_qt_test(test_repo_video_matching
    SOURCES
        test_repo_video_matching/tst_repo_video_matching.cpp
        video_matching_fixture_manifest.cpp
        video_matching_test_helpers.cpp
)
set_tests_properties(test_repo_video_matching PROPERTIES LABELS "repo-fixtures")

# Test 9: optional ~/Dev matching contract.
add_qt_test(test_local_video_matching
    SOURCES
        local/video_matching/tst_video_matching.cpp
        video_matching_fixture_manifest.cpp
        video_matching_test_helpers.cpp
)
set_tests_properties(test_local_video_matching PROPERTIES LABELS "local-fixtures")
