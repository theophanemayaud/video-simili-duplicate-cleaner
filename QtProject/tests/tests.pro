TEMPLATE = app
QT += testlib
CONFIG += qt warn_on depend_includepath testcase

include(../app/common.pri)

SOURCES +=  tst_video.cpp

DEFINES += VID_SIMILI_IN_TESTS
