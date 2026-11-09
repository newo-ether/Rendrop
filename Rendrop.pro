QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

VERSION = 2.0.4

QMAKE_CXXFLAGS_RELEASE += -O2

# Sources include project headers by module path, e.g. "core/render_queue.h".
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/thirdparty/cpp-httplib \
    $$PWD/thirdparty/json/include

# QWindowKit (thirdparty/qwindowkit, Apache-2.0) is built with CMake as a
# static library; see README for the command. Override with
# qmake QWK_INSTALL=<prefix> to use another install tree.
isEmpty(QWK_INSTALL): QWK_INSTALL = $$PWD/build-dist/qwindowkit/install
!exists($$QWK_INSTALL/share/QWindowKit/qmake/QWKWidgets.pri) {
    error("QWindowKit not found at $$QWK_INSTALL; build and install thirdparty/qwindowkit first")
}
include($$QWK_INSTALL/share/QWindowKit/qmake/QWKWidgets.pri)

include(src/app/app.pri)
include(src/config/config.pri)
include(src/core/core.pri)
include(src/farm/farm.pri)
include(src/ui/ui.pri)

SOURCES += \
    src/app/main.cpp

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource/resource.qrc

TRANSLATIONS += resource/i18n/zh_CN.ts

win32: LIBS += -luser32 -lws2_32 -lpdh -luxtheme -ldwmapi -lgdi32

RC_ICONS = resource/icon/icon.ico
