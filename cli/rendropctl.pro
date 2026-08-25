QT += core
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TEMPLATE = app
TARGET = rendropctl

VERSION = 1.0.34

INCLUDEPATH += \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

SOURCES += main.cpp

LIBS += user32.lib
