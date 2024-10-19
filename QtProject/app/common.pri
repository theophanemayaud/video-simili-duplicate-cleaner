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
    $$PWD/comparison.h \
    $$PWD/videometadata.h

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
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 12.0
    HEADERS += \
        $$PWD/obj-c.h
    SOURCES += \
        $$PWD/obj-c.mm

    # Other things
    ICON = $$PWD/AppIcon.icns

contains(QMAKE_HOST.arch, arm64):{
    message("qmake host is macos arm64 (arm processors)")

    # OpenCV libraries
    INCLUDEPATH += $$PWD/../libraries/macos/opencv-arm/opencv-install/include/opencv4
    DEPENDPATH += $$PWD/../libraries/macos/opencv-arm/opencv-install/include/opencv4
    LIBS += -L$$PWD/../libraries/macos/opencv-arm/opencv-install/lib/ -lopencv_imgproc -lopencv_core
    ## OpenCV static libs dependencies
    LIBS += -L$$PWD/../libraries/macos/opencv-arm/opencv-install/lib/opencv4/3rdparty -lzlib -littnotify -framework OpenCL -framework Accelerate

    # ffmpeg libraries
    INCLUDEPATH += $$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/include
    ## libavformat and libavutil static libs dependencies (from pckgconfig file)
    LIBS += -L$$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/lib -lavutil -lavformat \ # wanted libraries, below are other libraries that were needed to make it work
                                         -lswresample -lavcodec \
                                         -lswscale \
                                         -lbz2 -liconv -Wl,-no_compact_unwind \
                                         -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox
    PRE_TARGETDEPS += $$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/lib/libavutil.a \
                      $$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/lib/libavformat.a
                      $$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/lib/libswresample.a
                      $$PWD/../libraries/macos/ffmpeg-arm/ffmpeg-universalized-libs/lib/libavcodec.a
}

contains(QMAKE_HOST.arch, x86_64):{
    message("qmake host is macos x86_64 (intel processors)")

    # OpenCV libraries
    INCLUDEPATH += $$PWD/../libraries/macos/opencv/include
    DEPENDPATH += $$PWD/../libraries/macos/opencv/include

    LIBS += -L$$PWD/../libraries/macos/opencv/lib/ -lopencv_imgproc -lopencv_core

    PRE_TARGETDEPS += $$PWD/../libraries/macos/opencv/lib/libopencv_core.a \
                            $$PWD/../libraries/macos/opencv/lib/libopencv_imgproc.a

    ## OpenCV static libs dependencies
    LIBS += -L$$PWD/../libraries/macos/opencv/lib/opencv4/3rdparty -lzlib -littnotify -lippiw -lippicv -framework OpenCL -framework Accelerate

    # ffmpeg libraries
    INCLUDEPATH += $$PWD/../libraries/macos/ffmpeg/include

    ## libavformat and libavutil static libs dependencies (from pckgconfig file)
    LIBS += -L$$PWD/../libraries/macos/ffmpeg/lib -lavutil -lavformat \ # wanted libraries, below are other libraries that were needed to make it work
                                         -lswresample -lavcodec \
                                         -lswscale \
                                         -lbz2 -liconv -Wl,-no_compact_unwind \
                                         -framework CoreVideo -framework Security  -framework AudioToolbox -framework CoreMedia -framework VideoToolbox

    PRE_TARGETDEPS += $$PWD/../libraries/macos/ffmpeg/lib/libavutil.a \
                      $$PWD/../libraries/macos/ffmpeg/lib/libavformat.a
                      $$PWD/../libraries/macos/ffmpeg/lib/libswresample.a
                      $$PWD/../libraries/macos/ffmpeg/lib/libavcodec.a

    # Old way that FFMPEG was included as an executable : now it's only a library !
    #APP_QML_FILES.files = \
    #    $$PWD/../deps/ffmpeg
    #APP_QML_FILES.path = Contents/MacOS
    #QMAKE_BUNDLE_DATA += APP_QML_FILES
    }
}

win32 {
    # OpenCV libraries
    INCLUDEPATH += $$PWD/../libraries/windows/opencv/include

    # On windows, opencv has two configs that must match what QT is currently also under
    # This means we have two versions of the same library, one debug, one release...
    CONFIG(debug, debug|release) {
        QMAKE_LFLAGS += /ignore:4099 # disable weird warning about pdb files... shouldn't be a problem
        LIBS += -L$$PWD/../libraries/windows/opencv/lib/ -lopencv_imgproc451d -lopencv_core451d -lzlibd

    } else {
        LIBS += -L$$PWD/../libraries/windows/opencv/lib/ -lopencv_imgproc451 -lopencv_core451 -lzlib
    }


    # ffmpeg libraries
    INCLUDEPATH += $$PWD/../libraries/windows/ffmpeg/include

    LIBS += -L$$PWD/../libraries/windows/ffmpeg/lib/ \
    # Three that we actually use directly, and their required libs (from pck config files)
                        -lavcodec ole32.lib user32.lib \
                        -lavformat ws2_32.lib \
                        -lswscale \
    # Other that seem required for the three above
                        -lavutil bcrypt.lib \
                        -lswresample

    # Windows app icon
    RC_FILE = $$PWD/app.rc
}

RESOURCES += \
    $$PWD/files.qrc
