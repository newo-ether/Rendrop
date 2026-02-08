QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

QMAKE_CXXFLAGS_RELEASE += -O2

INCLUDEPATH += \
    $$PWD/src/include \
    $$PWD/thirdparty/cpp-httplib \
    $$PWD/thirdparty/json/include

SOURCES += \
    src/add_file_button.cpp \
    src/blender_file_info.cpp \
    src/blender_file_reader.cpp \
    src/blender_renderer.cpp \
    src/drop_shadow_renderer.cpp \
    src/drop_shadow_widget.cpp \
    src/file_bar.cpp \
    src/blender_version_manager.cpp \
    src/http_server.cpp \
    src/loading_bar.cpp \
    src/main.cpp \
    src/progress_bar.cpp \
    src/simple_process.cpp \
    src/widget.cpp \
    src/drop_file_tip.cpp \
    src/ring_progress_bar.cpp \
    src/frame_range_dialog.cpp \
    src/rename_dialog.cpp

HEADERS += \
    src/include/blender_renderer.h \
    src/include/blender_file_info.h \
    src/include/blender_file_reader.h \
    src/include/add_file_button.h \
    src/include/blender_version_manager.h \
    src/include/color.h \
    src/include/drop_shadow_renderer.h \
    src/include/drop_shadow_widget.h \
    src/include/file_bar.h \
    src/include/http_server.h \
    src/include/project_info.h \
    src/include/simple_process.h \
    src/include/style.h \
    src/include/widget.h \
    src/include/drop_file_tip.h \
    src/include/ring_progress_bar.h \
    src/include/progress_bar.h \
    src/include/loading_bar.h \
    src/include/frame_range_dialog.h \
    src/include/rename_dialog.h

FORMS += \
    ui/drop_file_tip.ui \
    ui/file_bar.ui \
    ui/widget.ui \
    ui/frame_range_dialog.ui \
    ui/rename_dialog.ui


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource/resource.qrc

TRANSLATIONS += resource/i18n/zh_CN.ts

LIBS += user32.lib

RC_ICONS = resource/icon/icon.ico
