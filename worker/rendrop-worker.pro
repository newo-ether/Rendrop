QT += core
QT -= gui widgets
CONFIG += console c++20
CONFIG -= windows app_bundle
TEMPLATE = app
TARGET = rendrop-worker

VERSION = 2.0.4

INCLUDEPATH += \
    $$PWD/../src \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

SOURCES += \
    main.cpp \
    ../src/core/blender_renderer.cpp \
    ../src/core/simple_process.cpp \
    ../src/farm/remote_worker.cpp

HEADERS += \
    ../src/core/blender_renderer.h \
    ../src/core/simple_process.h \
    ../src/farm/remote_worker.h \
    ../src/farm/worker_auth.h

win32: LIBS += -luser32 -lws2_32
