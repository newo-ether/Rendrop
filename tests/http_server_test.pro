QT += core gui network testlib
CONFIG += console testcase c++20
CONFIG -= app_bundle
TEMPLATE = app
TARGET = http_server_test

INCLUDEPATH += \
    $$PWD/../src \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

include(../src/config/config.pri)
include(../src/core/core.pri)

SOURCES += \
    http_server_test.cpp \
    ../src/farm/http_server.cpp \
    ../src/farm/load_monitor.cpp

HEADERS += \
    ../src/farm/http_server.h \
    ../src/farm/load_monitor.h \
    ../src/farm/worker_auth.h

win32: LIBS += -luser32 -lws2_32 -lpdh
