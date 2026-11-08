# Render queue, jobs and Blender integration. Depends on config only.

SOURCES += \
    $$PWD/blender_file_info.cpp \
    $$PWD/blender_file_reader.cpp \
    $$PWD/blender_renderer.cpp \
    $$PWD/blender_version_manager.cpp \
    $$PWD/chunk_planner.cpp \
    $$PWD/dependency_packer.cpp \
    $$PWD/frame_ledger.cpp \
    $$PWD/project_info.cpp \
    $$PWD/render_job.cpp \
    $$PWD/render_queue.cpp \
    $$PWD/simple_process.cpp

HEADERS += \
    $$PWD/blender_file_info.h \
    $$PWD/blender_file_reader.h \
    $$PWD/blender_renderer.h \
    $$PWD/blender_version_manager.h \
    $$PWD/chunk_planner.h \
    $$PWD/dependency_packer.h \
    $$PWD/frame_ledger.h \
    $$PWD/frame_owner.h \
    $$PWD/project_info.h \
    $$PWD/render_job.h \
    $$PWD/render_queue.h \
    $$PWD/simple_process.h
