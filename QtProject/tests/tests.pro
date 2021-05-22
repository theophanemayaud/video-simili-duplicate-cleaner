TEMPLATE = app
message("QMake tests")

QT += testlib
CONFIG += qt warn_on depend_includepath testcase

include(../app/common.pri)

SOURCES +=  TestHelpers.cpp \
    tst_video.cpp


DEFINES += VID_SIMILI_IN_TESTS
