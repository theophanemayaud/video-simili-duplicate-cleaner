cmake_minimum_required(VERSION 3.16)

project(video-simili-duplicate-cleaner VERSION 0.1 LANGUAGES CXX)
set(APP_COPYRIGHT "Copyright © 2020-2025 Théophane Mayaud, 2018-2019 Kristian Koskimäki")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# qt specific code generation
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Debug paths
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets Sql Concurrent)

# Debug found Qt paths
message(STATUS "Qt6_FOUND: ${Qt6_FOUND}")
message(STATUS "Qt6_DIR: ${Qt6_DIR}")
message(STATUS "Qt6Core_DIR: ${Qt6Core_DIR}")
message(STATUS "Qt6Widgets_DIR: ${Qt6Widgets_DIR}")
message(STATUS "Qt6Sql_DIR: ${Qt6Sql_DIR}")
message(STATUS "Qt6Concurrent_DIR: ${Qt6Concurrent_DIR}")

function(target_link_video_processing_dependencies TARGET_NAME)
    if(APPLE)
        # OpenCV
        set(OPENCV_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/opencv-arm/opencv-install")
        target_include_directories(${TARGET_NAME} PRIVATE ${OPENCV_ROOT}/include/opencv4)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${OPENCV_ROOT}/lib/libopencv_imgproc.a
            ${OPENCV_ROOT}/lib/libopencv_core.a
            ${OPENCV_ROOT}/lib/opencv4/3rdparty/libzlib.a
            ${OPENCV_ROOT}/lib/opencv4/3rdparty/libittnotify.a
            "-framework OpenCL"
            "-framework Accelerate"
        )

        # FFMPEG
        set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/ffmpeg/ffmpeg-universalized-libs")
        target_include_directories(${TARGET_NAME} SYSTEM PRIVATE
            ${FFMPEG_ROOT}/include
        )
        target_link_directories(${TARGET_NAME} PRIVATE
            ${FFMPEG_ROOT}/lib
        )
        target_link_libraries(${TARGET_NAME} PRIVATE
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
        target_include_directories(${TARGET_NAME} PRIVATE ${AOM_ROOT}/include)
        target_link_directories(${TARGET_NAME} PRIVATE
            ${AOM_ROOT}/lib
        )
        target_link_libraries(${TARGET_NAME} PRIVATE
            aom
        )

        # Apple Photos: we use PhotoKit (PHAsset/PHAssetResource) which lives in Photos.framework
        # to get the name of the video in the user's library.
        target_link_libraries(${TARGET_NAME} PRIVATE
            "-framework Photos"
        )
    elseif(WIN32)
        message(STATUS "Windows: adding libraries expecting management by vcpkg")

        find_package(OpenCV REQUIRED COMPONENTS core imgproc)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${OpenCV_LIBS}
        )

        find_package(FFMPEG REQUIRED)
        target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
        target_link_directories(${TARGET_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${FFMPEG_LIBRARIES}
        )
    else()
        message(STATUS "Unix: adding libraries expecting management by system packages")

        find_package(OpenCV REQUIRED COMPONENTS core imgproc)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${OpenCV_LIBS}
        )

        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavutil
            libavformat
            libswresample
            libavcodec
            libswscale
        )
        target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::FFMPEG)
    endif()
endfunction()

# Définir les sources du projet
set(PROJECT_SOURCES
    app/main.cpp
    app/mainwindow.cpp
    app/mainwindow.h
    app/mainwindow.ui
    app/video.cpp
    app/video.h
    app/visualfingerprint.cpp
    app/visualfingerprint.h
    app/comparison/comparison.cpp
    app/comparison/comparison.h
    app/comparison/comparison.ui
    app/comparison/internal/backgroundmatchdiscovery.cpp
    app/comparison/internal/backgroundmatchdiscovery.h
    app/comparison/internal/duplicatesetbuilder.cpp
    app/comparison/internal/duplicatesetbuilder.h
    app/comparison/internal/discoveryprogressslider.cpp
    app/comparison/internal/discoveryprogressslider.h
    app/comparison/internal/ssim.cpp
    app/comparison/internal/ssim.h
    app/comparison/internal/videopairmatcher.cpp
    app/comparison/internal/videopairmatcher.h
    app/comparison/internal/videopairspace.h
    app/thumbnail.h
    app/db.cpp
    app/db.h
    app/prefs.h
    app/prefs.cpp
    app/videometadata.h
    app/files.qrc
)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    list(APPEND PROJECT_SOURCES
        app/obj-c.mm
        app/obj-c.h
        ${APP_ICON_MACOSX}
    )
endif()

if(APPLE)
    add_executable(video-simili-duplicate-cleaner MACOSX_BUNDLE
        ${PROJECT_SOURCES}
    )
else()
    add_executable(video-simili-duplicate-cleaner WIN32
        ${PROJECT_SOURCES}
    )
endif()

# Link Qt libraries after target is created
target_link_libraries(video-simili-duplicate-cleaner PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Concurrent
)
target_include_directories(video-simili-duplicate-cleaner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)

if(APPLE)
    set(SHIPPED_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/release-build/macos/Info.plist")

    # The bundle takes the shipped Info.plist as is, rather than one generated from
    # MACOSX_BUNDLE_* properties, so every build carries the keys the released app needs
    # Without the privacy usage strings PhotoKit and NSAppleScript kill the
    # process when they request authorization.
    # Versions in that file are maintained by `npm run update-build-version`.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SHIPPED_INFO_PLIST}")
    # OUTPUT_NAME is not bundle metadata: it names the executable, and so the .app
    # directory. The plist's CFBundleExecutable only points at that file, and the
    # app cannot launch if the two disagree.
    set_target_properties(video-simili-duplicate-cleaner PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${SHIPPED_INFO_PLIST}"
        OUTPUT_NAME "Video simili duplicate cleaner"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Debug builds must not claim the released app's identity: macOS stores
        # privacy decisions per identifier together with the code requirement they
        # were granted to, and a local build cannot match the one recorded for the
        # released app, so reusing it gets Photos access refused without a prompt.
        # Only the identifier changes, so the usage strings cannot drift.
        set(RELEASE_BUNDLE_IDENTIFIER "video-simili-duplicate-cleaner.theophanemayaud.github.io")
        file(READ "${SHIPPED_INFO_PLIST}" SHIPPED_INFO_PLIST_CONTENT)
        string(REPLACE "<string>${RELEASE_BUNDLE_IDENTIFIER}</string>"
                       "<string>${RELEASE_BUNDLE_IDENTIFIER}.debug</string>"
                       DEBUG_INFO_PLIST_CONTENT "${SHIPPED_INFO_PLIST_CONTENT}")
        if(DEBUG_INFO_PLIST_CONTENT STREQUAL SHIPPED_INFO_PLIST_CONTENT)
            message(FATAL_ERROR "Could not find ${RELEASE_BUNDLE_IDENTIFIER} in ${SHIPPED_INFO_PLIST}")
        endif()

        # Write through configure_file so the result is only touched when it really
        # changes, which keeps the relink below from firing on every reconfigure.
        set(DEBUG_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/DebugInfo.plist")
        file(WRITE "${DEBUG_INFO_PLIST}.in" "${DEBUG_INFO_PLIST_CONTENT}")
        configure_file("${DEBUG_INFO_PLIST}.in" "${DEBUG_INFO_PLIST}" COPYONLY)

        set_target_properties(video-simili-duplicate-cleaner PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${DEBUG_INFO_PLIST}"
        )

        # The linker alone ad-hoc signs just the executable, leaving the bundle with
        # no sealed signature. macOS cannot then derive a designated requirement for
        # the app and denies privacy requests outright, without ever prompting.
        # Signing seals Info.plist, so a plist change has to relink and re-sign:
        # otherwise the bundle looks tampered with and access is refused again.
        set_property(TARGET video-simili-duplicate-cleaner APPEND PROPERTY LINK_DEPENDS "${DEBUG_INFO_PLIST}")
        add_custom_command(TARGET video-simili-duplicate-cleaner POST_BUILD
            COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:video-simili-duplicate-cleaner>"
            COMMENT "Ad-hoc signing the app bundle so macOS can prompt for Photos access"
            VERBATIM
        )
    endif()
endif()

target_compile_definitions(video-simili-duplicate-cleaner PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    APP_NAME="Video simili duplicate cleaner"
    APP_COPYRIGHT=${APP_COPYRIGHT}
)

target_link_video_processing_dependencies(video-simili-duplicate-cleaner)

# Enable testing
enable_testing()
add_subdirectory(tests)
