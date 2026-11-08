QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = render_queue_test

INCLUDEPATH += \
    $$PWD/../src

include(../src/config/config.pri)
include(../src/core/core.pri)

SOURCES += \
    render_queue_test.cpp

win32: LIBS += -luser32 -lws2_32
