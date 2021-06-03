TARGET = "Video simili duplicate cleaner"
message($$TARGET)

QT += core gui widgets sql

QMAKE_TARGET_PRODUCT = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_DESCRIPTION = \"\\\"$$TARGET\\\"\"
QMAKE_TARGET_COPYRIGHT = "Copyright \\251 2018-2019 Kristian Koskim\\344ki"

DEFINES += APP_NAME=\"\\\"$$TARGET\\\"\"
DEFINES += APP_COPYRIGHT=\"\\\"$$QMAKE_TARGET_COPYRIGHT\\\"\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x100000 # 0x[major][minor]00 set to higher than current version (v10.0.0) to have all deprecated functions removed

HEADERS += \
    $$PWD/ffmpeg.h \
    $$PWD/mainwindow.h \
    $$PWD/prefs.h \
    $$PWD/video.h \
    $$PWD/thumbnail.h \
    $$PWD/db.h \
    $$PWD/comparison.h

SOURCES += \
    $$PWD/mainwindow.cpp \
    $$PWD/video.cpp \
    $$PWD/db.cpp \
    $$PWD/comparison.cpp \
    $$PWD/ssim.cpp

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/comparison.ui

macx {
# OpenCV libraries
INCLUDEPATH += $$PWD/../libraries/opencv/include
DEPENDPATH += $$PWD/../libraries/opencv/include

LIBS += -L$$PWD/../libraries/opencv/lib/ -lopencv_imgproc -lopencv_core

PRE_TARGETDEPS += $$PWD/../libraries/opencv/lib/libopencv_core.a \
                        $$PWD/../libraries/opencv/lib/libopencv_imgproc.a

## OpenCV static libs dependencies
LIBS += -L$$PWD/../libraries/opencv/lib/opencv4/3rdparty -lzlib -littnotify -lippiw -lippicv -framework OpenCL -framework Accelerate

# ffmpeg libraries
INCLUDEPATH += $$PWD/../libraries/ffmpeg/include

## libavformat and libavutil static libs dependencies (from pckgconfig file)
LIBS += -L$$PWD/../libraries/ffmpeg/lib -lavutil -lavformat \ # wanted libraries, below are other libraries that were needed to make it work
                                     -lswresample -lavcodec \
                                     -lswscale \
                                     -lbz2 -liconv -Wl,-no_compact_unwind \
                                     -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox

PRE_TARGETDEPS += $$PWD/../libraries/ffmpeg/lib/libavutil.a \
                  $$PWD/../libraries/ffmpeg/lib/libavformat.a
                  $$PWD/../libraries/ffmpeg/lib/libswresample.a
                  $$PWD/../libraries/ffmpeg/lib/libavcodec.a

# Other things
ICON = $$PWD/AppIcon.icns

APP_QML_FILES.files = \
    $$PWD/../deps/ffmpeg
APP_QML_FILES.path = Contents/MacOS
QMAKE_BUNDLE_DATA += APP_QML_FILES
}

win32 {
# Other things
RC_ICONS = $$PWD/icon16.ico
}


RESOURCES += \
    $$PWD/files.qrc
