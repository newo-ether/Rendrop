QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = frame_ledger_test

INCLUDEPATH += \
    $$PWD/../src

SOURCES += \
    frame_ledger_test.cpp \
    ../src/core/frame_ledger.cpp

HEADERS += \
    ../src/core/frame_ledger.h \
    ../src/core/frame_owner.h
