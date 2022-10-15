TEMPLATE = app
message("QMake comparison tests")

QT += testlib
CONFIG += qt warn_on depend_includepath testcase

include(../../app/common.pri)

SOURCES +=  tst_test_comparison.cpp

DEFINES += VID_SIMILI_IN_TESTS
