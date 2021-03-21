TARGET = "Video simili duplicate cleaner"
TEMPLATE = app

QT += core gui widgets sql

HEADERS += \
    ffmpeg.h \
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

# OpenCV libraries
INCLUDEPATH += $$PWD/libraries/opencv/include
DEPENDPATH += $$PWD/libraries/opencv/include

macx: LIBS += -L$$PWD/libraries/opencv/lib/ -lopencv_imgproc -lopencv_core

macx: PRE_TARGETDEPS += $$PWD/libraries/opencv/lib/libopencv_core.a \
                        $$PWD/libraries/opencv/lib/libopencv_imgproc.a

## OpenCV static libs dependencies
macx: LIBS += -L$$PWD/libraries/opencv/lib/opencv4/3rdparty -lzlib -littnotify -lippiw -lippicv -framework OpenCL -framework Accelerate

# ffmpeg libraries
INCLUDEPATH += $$PWD/libraries/ffmpeg/include

## libavformat and libavutil static libs dependencies (from pckgconfig file)
macx: LIBS += -L$$PWD/libraries/ffmpeg/lib -lavutil -lavformat \ # wanted libraries, below are other libraries that were needed to make it work
                                            -lswresample -lavcodec \
                                            -lbz2 -liconv -llzma -Wl,-no_compact_unwind \
                                            -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox

macx: PRE_TARGETDEPS += $$PWD/libraries/ffmpeg/lib/libavutil.a \
                        $$PWD/libraries/ffmpeg/lib/libavformat.a
                        $$PWD/libraries/ffmpeg/lib/libswresample.a
                        $$PWD/libraries/ffmpeg/lib/libavcodec.a

# Other things
RC_ICONS = icon16.ico
ICON = AppIcon.icns

APP_QML_FILES.files = \
    $$PWD/deps/ffmpeg
APP_QML_FILES.path = Contents/Frameworks
QMAKE_BUNDLE_DATA += APP_QML_FILES

QMAKE_TARGET_PRODUCT = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_DESCRIPTION = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_COPYRIGHT = "Copyright \\251 2018-2019 Kristian Koskim\\344ki"

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
