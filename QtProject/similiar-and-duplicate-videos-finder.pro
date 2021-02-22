TARGET = "Similiar and duplicate videos finder"
TEMPLATE = app
# VERSION = 0.3.0

QT += core gui widgets sql

#QMAKE_LFLAGS += -Wl#,--large-address-aware
#QMAKE_CXXFLAGS_RELEASE -= -O
#QMAKE_CXXFLAGS_RELEASE -= -O1
#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE *= -O3

HEADERS += \
    mainwindow.h \
    prefs.h \
    video.h \
    thumbnail.h \
    db.h \
    comparison.h

SOURCES += \
    mainwindow.cpp \
    video.cpp \
    db.cpp \
    comparison.cpp \
    ssim.cpp

FORMS += \
    mainwindow.ui \
    comparison.ui

LIBS += \
    $$PWD/deps/libopencv_core.3.4.dylib \
    $$PWD/deps/libopencv_imgproc.3.4.dylib

#macx: LIBS += -L$$PWD/bin/libopencv_core.3.4.11/ -lbin/libopencv_core.3.4.11
#INCLUDEPATH += $$PWD/bin/libopencv_core.3.4.11
#DEPENDPATH += $$PWD/libopencv_core.3.4.11

#MediaFiles.files += \
#    bin/libopencv_core.3.4.11.dylib \
#    bin/libopencv_imgproc.3.4.11.dylib
#MediaFiles.path = Contents/MacOS
#QMAKE_BUNDLE_DATA += MediaFiles

RC_ICONS = icon16.ico
ICON = AppIcon.icns

APP_QML_FILES.files = \
    $$PWD/deps/libopencv_core.3.4.dylib \
    $$PWD/deps/libopencv_imgproc.3.4.dylib \
    $$PWD/deps/ffmpeg #\
    #$$PWD/extensions.ini
APP_QML_FILES.path = Contents/Frameworks
QMAKE_BUNDLE_DATA += APP_QML_FILES

QMAKE_TARGET_PRODUCT = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_DESCRIPTION = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_COPYRIGHT = "Copyright \\251 2018-2019 Kristian Koskim\\344ki"

# DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_NAME=\"\\\"$$TARGET\\\"\"
DEFINES += APP_COPYRIGHT=\"\\\"$$QMAKE_TARGET_COPYRIGHT\\\"\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x100000 # 0x[major][minor]00 set to higher than current version (v10.0.0) to have all deprecated functions removed

#How to compile this program:
    #Qt5.xx (https://www.qt.io/) MingW-32 is the default compiler and was used for the development of the program
    #If compilation fails, click on the computer icon in lower left corner of Qt Creator and select a kit

    #OpenCV 3.xx (32 bit) (https://www.opencv.org/)
    #Compiling OpenCV with MingW can be hard, so download binaries from https://github.com/huihut/OpenCV-MinGW-Build
    #put OpenCV \bin folder in source folder (only libopencv_core and libopencv_imgproc dll files are needed)
    #put OpenCV \opencv2 folder in source folder (contains the header files)
    #add path to \bin folder: Projects -> Build Environment -> Details -> Path -> C:\the_full_Qt_path\program\bin
    #The program will crash on start if the path to \bin was not set or the OpenCV DLL files are not in \bin

    #FFmpeg 4.xx (https://ffmpeg.org/)
    #ffmpeg.exe must be in same folder where the program executable is generated (or any folder in %PATH%)

    #extensions.ini must be in folder where the program executable is generated

RESOURCES += \
    files.qrc

DISTFILES +=
