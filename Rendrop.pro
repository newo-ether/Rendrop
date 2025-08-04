QT       += core gui openglwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

QMAKE_CXXFLAGS_RELEASE += -O2

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
    $$PWD/src/include

SOURCES += \
    src/add_file_button.cpp \
    src/drop_shadow_renderer.cpp \
    src/drop_shadow_widget.cpp \
    src/file_bar.cpp \
    src/main.cpp \
    src/widget.cpp \
    src/drop_file_tip.cpp \
    src/ring_progress_bar.cpp

HEADERS += \
    src/include/add_file_button.h \
    src/include/drop_shadow_renderer.h \
    src/include/drop_shadow_widget.h \
    src/include/file_bar.h \
    src/include/widget.h \
    src/include/drop_file_tip.h \
    src/include/ring_progress_bar.h

FORMS += \
    ui/drop_file_tip.ui \
    ui/file_bar.ui \
    ui/widget.ui


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource/resource.qrc

LIBS += opengl32.lib

RC_ICONS = resource/icon/icon.ico
