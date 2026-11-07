QT += core
QT -= gui widgets
CONFIG += console c++20
CONFIG -= windows app_bundle
TEMPLATE = app
TARGET = rendropctl

VERSION = 2.0.0

INCLUDEPATH += \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

SOURCES += main.cpp

win32: LIBS += -lws2_32
