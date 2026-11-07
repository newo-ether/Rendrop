QT += core gui widgets network
CONFIG += c++20 console
TEMPLATE = app
TARGET = ui_snapshot

INCLUDEPATH += \
    $$PWD/../src/include \
    $$PWD/../thirdparty/cpp-httplib \
    $$PWD/../thirdparty/json/include

SOURCES += \
    ui_snapshot.cpp \
    ../src/add_file_button.cpp \
    ../src/blender_file_info.cpp \
    ../src/blender_file_reader.cpp \
    ../src/blender_renderer.cpp \
    ../src/blender_version_manager.cpp \
    ../src/chunk_planner.cpp \
    ../src/dependency_packer.cpp \
    ../src/drop_file_tip.cpp \
    ../src/drop_shadow_renderer.cpp \
    ../src/drop_shadow_widget.cpp \
    ../src/file_bar.cpp \
    ../src/frame_ledger.cpp \
    ../src/frame_range_dialog.cpp \
    ../src/http_server.cpp \
    ../src/load_monitor.cpp \
    ../src/loading_bar.cpp \
    ../src/node_agent.cpp \
    ../src/node_discovery.cpp \
    ../src/node_panel.cpp \
    ../src/progress_bar.cpp \
    ../src/project_info.cpp \
    ../src/remote_worker.cpp \
    ../src/rename_dialog.cpp \
    ../src/render_job.cpp \
    ../src/render_queue.cpp \
    ../src/ring_progress_bar.cpp \
    ../src/rounded_surface.cpp \
    ../src/simple_process.cpp \
    ../src/widget.cpp

HEADERS += $$files($$PWD/../src/include/*.h)

FORMS += \
    ../ui/drop_file_tip.ui \
    ../ui/file_bar.ui \
    ../ui/widget.ui \
    ../ui/frame_range_dialog.ui \
    ../ui/rename_dialog.ui

RESOURCES += ../resource/resource.qrc

win32: LIBS += -luser32 -lws2_32 -lpdh
