QT += testlib
QT += core gui widgets sql
CONFIG += qt warn_on depend_includepath testcase

TEMPLATE = app

SOURCES +=  tst_video.cpp

HEADERS += \
    ../app/video.h \
    ../app/db.h

SOURCES += \
#    mainwindow.cpp \
    ../app/video.cpp \
    ../app/db.cpp
#    comparison.cpp \
#    ssim.cpp

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

