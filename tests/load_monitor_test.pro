QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = load_monitor_test

INCLUDEPATH += \
    $$PWD/../src

SOURCES += \
    load_monitor_test.cpp \
    ../src/farm/load_monitor.cpp

HEADERS += \
    ../src/farm/load_monitor.h \
    ../src/config/load_settings.h

win32: LIBS += -lpdh
