QT += core gui widgets sql

TARGET = "Video simili duplicate cleaner"

DEFINES += APP_NAME=\"\\\"$$TARGET\\\"\"
DEFINES += APP_COPYRIGHT=\"\\\"$$QMAKE_TARGET_COPYRIGHT\\\"\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x100000 # 0x[major][minor]00 set to higher than current version (v10.0.0) to have all deprecated functions removed

HEADERS += \
    ../app/comparison.h \
    ../app/mainwindow.h \
    ../app/video.h \
    ../app/db.h

SOURCES += \
    ../app/comparison.cpp \
    ../app/mainwindow.cpp \
    ../app/video.cpp \
    ../app/db.cpp \
    ../app/ssim.cpp

FORMS += \
    ../app/mainwindow.ui \
    ../app/comparison.ui

INCLUDEPATH += ../app/libraries/opencv/include
LIBS += -L$$PWD/../app/libraries/opencv/lib/ -lopencv_imgproc -lopencv_core
PRE_TARGETDEPS += $$PWD/../app/libraries/opencv/lib/libopencv_core.a \
                        $$PWD/../app/libraries/opencv/lib/libopencv_imgproc.a
LIBS += -L$$PWD/../app/libraries/opencv/lib/opencv4/3rdparty -lzlib -littnotify -lippiw -lippicv -framework OpenCL -framework Accelerate

INCLUDEPATH += ../app/libraries/ffmpeg/include
LIBS += -L$$PWD/../app/libraries/ffmpeg/lib -lavutil -lavformat \ # wanted libraries, below are other libraries that were needed to make it work
                                     -lswresample -lavcodec \
                                     -lbz2 -liconv -Wl,-no_compact_unwind \
                                     -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox

APP_QML_FILES.files = \
    $$PWD/../app/deps/ffmpeg
APP_QML_FILES.path = Contents/MacOS
QMAKE_BUNDLE_DATA += APP_QML_FILES



RESOURCES += \
    ../app/files.qrc
