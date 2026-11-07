QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = load_monitor_test

INCLUDEPATH += \
    $$PWD/../src/include

SOURCES += \
    load_monitor_test.cpp \
    ../src/load_monitor.cpp

HEADERS += \
    ../src/include/load_monitor.h \
    ../src/include/load_settings.h

win32: LIBS += -lpdh
