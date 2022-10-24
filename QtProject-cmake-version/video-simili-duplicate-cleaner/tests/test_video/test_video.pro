TEMPLATE = app
message("QMake tests")

QT += testlib
CONFIG += qt warn_on depend_includepath testcase

include(../../app/common.pri)

SOURCES +=  \
    tst_video.cpp \
    video_test_helpers.cpp

DEFINES += VID_SIMILI_IN_TESTS
