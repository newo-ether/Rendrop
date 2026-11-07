QT += core
QT -= gui widgets
CONFIG += console c++20
CONFIG -= windows app_bundle
TEMPLATE = app
TARGET = rendrop-worker

VERSION = 1.0.35

INCLUDEPATH += \
    $$PWD/../src/include \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

SOURCES += \
    main.cpp \
    ../src/blender_renderer.cpp \
    ../src/remote_worker.cpp \
    ../src/simple_process.cpp

HEADERS += \
    ../src/include/blender_renderer.h \
    ../src/include/remote_worker.h \
    ../src/include/simple_process.h \
    ../src/include/worker_auth.h

win32: LIBS += -luser32 -lws2_32
