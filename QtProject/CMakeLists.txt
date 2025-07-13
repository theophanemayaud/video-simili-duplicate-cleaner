cmake_minimum_required(VERSION 3.16)
set(VCPKG_TARGET_TRIPLET "arm64-windows-static-release") # vcpkg community triplet provided in vcpkg itself

project(video-simili-duplicate-cleaner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

# # Enable vcpkg integration
# if(WIN32)
#     # Find vcpkg and integrate it
#     find_package(PkgConfig REQUIRED)
    
#     # Set vcpkg toolchain if not already set
#     if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#         if(DEFINED ENV{VCPKG_ROOT})
#             set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
#         endif()
#     endif()
    
#     # Set static linking for Windows
#     set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
#     if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
#         set(VCPKG_TARGET_TRIPLET "arm64-windows-static" CACHE STRING "")
#     elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
#         set(VCPKG_TARGET_TRIPLET "x86-windows-static" CACHE STRING "")
#     endif()
# endif()

set(QT_PREFIX_PATHS "$ENV{QTDIR}")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql Concurrent PATHS ${QT_PREFIX_PATHS} NO_DEFAULT_PATH)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql Concurrent PATHS ${QT_PREFIX_PATHS} NO_DEFAULT_PATH)

# Définir les sources du projet
set(PROJECT_SOURCES
    app/main.cpp
    app/mainwindow.cpp
    app/mainwindow.h
    app/mainwindow.ui
    app/video.cpp
    app/video.h
    app/comparison.cpp
    app/comparison.h
    app/comparison.ui
    app/thumbnail.h
    app/db.cpp
    app/db.h
    app/prefs.h
    app/videometadata.h
    app/videoprocessingpool.cpp
    app/videoprocessingpool.h
    app/ssim.cpp
    app/files.qrc
)

if(APPLE)
    list(APPEND PROJECT_SOURCES
        app/obj-c.mm
        app/obj-c.h
        app/AppIcon.icns
    )
endif()

# Créer l'exécutable
add_executable(video-simili-duplicate-cleaner MACOSX_BUNDLE
    ${PROJECT_SOURCES}
)

# Définir les propriétés de l'application
set_target_properties(video-simili-duplicate-cleaner PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.theophane.video-simili-duplicate-cleaner"
    MACOSX_BUNDLE_BUNDLE_NAME "Video simili duplicate cleaner"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © 2020-2024 Théophane Mayaud, 2018-2019 Kristian Koskimäki"
)

# Définir les includes et les définitions
target_compile_definitions(video-simili-duplicate-cleaner PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x100000
    APP_NAME="Video simili duplicate cleaner"
    APP_COPYRIGHT="Copyright © 2020-2024 Théophane Mayaud, 2018-2019 Kristian Koskimäki"
)

if(APPLE)
    # Configuration spécifique pour ARM64 (Apple Silicon)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        # OpenCV
        set(OPENCV_ROOT "${CMAKE_SOURCE_DIR}/libraries/macos/opencv-arm/opencv-install")
        include_directories(${OPENCV_ROOT}/include/opencv4)
        target_link_libraries(video-simili-duplicate-cleaner PRIVATE
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
        target_link_directories(video-simili-duplicate-cleaner PRIVATE
            ${FFMPEG_ROOT}/lib
        )
        target_link_libraries(video-simili-duplicate-cleaner PRIVATE
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
        target_link_directories(video-simili-duplicate-cleaner PRIVATE
            ${AOM_ROOT}/lib
        )
        target_link_libraries(video-simili-duplicate-cleaner PRIVATE
            aom
        )
    else()
        message(FATAL_ERROR "This application only supports Apple Silicon (ARM64) processors on macOS for now")
    endif()
endif()

if(WIN32)
    # Find packages using vcpkg
    find_package(OpenCV REQUIRED COMPONENTS core imgproc)
    find_package(PkgConfig REQUIRED)
    
    # Find FFmpeg using pkg-config (vcpkg provides .pc files)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat 
        libavutil
        libswresample
        libswscale
    )
    
    # Link OpenCV
    target_link_libraries(video-simili-duplicate-cleaner PRIVATE 
        opencv_core
        opencv_imgproc
    )
    
    # Link FFmpeg
    target_link_libraries(video-simili-duplicate-cleaner PRIVATE 
        PkgConfig::FFMPEG
    )
    
    # Windows-specific libraries that FFmpeg depends on
    target_link_libraries(video-simili-duplicate-cleaner PRIVATE
        ws2_32
        ole32  
        user32
        bcrypt
        mfplat
        mfuuid
        strmiids
    )    
endif()

# Lier avec Qt
target_link_libraries(video-simili-duplicate-cleaner PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Concurrent
)
