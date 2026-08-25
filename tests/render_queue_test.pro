QT += core testlib
QT -= gui

CONFIG += console testcase c++20
CONFIG -= app_bundle

TEMPLATE = app
TARGET = render_queue_test

INCLUDEPATH += \
    $$PWD/../src/include

SOURCES += \
    render_queue_test.cpp \
    ../src/blender_file_info.cpp \
    ../src/blender_file_reader.cpp \
    ../src/blender_renderer.cpp \
    ../src/project_info.cpp \
    ../src/render_job.cpp \
    ../src/render_queue.cpp \
    ../src/simple_process.cpp

HEADERS += \
    ../src/include/blender_file_info.h \
    ../src/include/blender_file_reader.h \
    ../src/include/blender_renderer.h \
    ../src/include/project_info.h \
    ../src/include/render_job.h \
    ../src/include/render_queue.h \
    ../src/include/simple_process.h

LIBS += user32.lib
