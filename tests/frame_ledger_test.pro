QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = frame_ledger_test

INCLUDEPATH += \
    $$PWD/../src/include

SOURCES += \
    frame_ledger_test.cpp \
    ../src/frame_ledger.cpp

HEADERS += \
    ../src/include/frame_ledger.h
