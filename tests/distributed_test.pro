QT += core network testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = distributed_test

INCLUDEPATH += \
    $$PWD/../src/include

SOURCES += \
    distributed_test.cpp \
    ../src/chunk_planner.cpp \
    ../src/node_discovery.cpp

HEADERS += \
    ../src/include/chunk_planner.h \
    ../src/include/distributed_settings.h \
    ../src/include/load_settings.h \
    ../src/include/node_discovery.h \
    ../src/include/worker_auth.h
