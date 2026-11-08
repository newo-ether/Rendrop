QT += core gui widgets network
CONFIG += c++20 console
TEMPLATE = app
TARGET = ui_snapshot

INCLUDEPATH += \
    $$PWD/../src \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

isEmpty(QWK_INSTALL): QWK_INSTALL = $$PWD/../build-dist/qwindowkit/install
include($$QWK_INSTALL/share/QWindowKit/qmake/QWKWidgets.pri)

include(../src/app/app.pri)
include(../src/config/config.pri)
include(../src/core/core.pri)
include(../src/farm/farm.pri)
include(../src/ui/ui.pri)

SOURCES += \
    ui_snapshot.cpp

RESOURCES += ../resource/resource.qrc

win32: LIBS += -luser32 -lws2_32 -lpdh -luxtheme -ldwmapi -lgdi32
