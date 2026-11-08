QT += core network testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = distributed_test

INCLUDEPATH += \
    $$PWD/../src

include(../src/config/config.pri)

SOURCES += \
    distributed_test.cpp \
    ../src/core/chunk_planner.cpp \
    ../src/farm/node_discovery.cpp

HEADERS += \
    ../src/core/chunk_planner.h \
    ../src/farm/node_discovery.h \
    ../src/farm/worker_auth.h
