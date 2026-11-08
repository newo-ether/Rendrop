# Settings and on-disk configuration. Depends on Qt Core only, except
# distributed_settings.h, which reuses the chunking policy type from core.

SOURCES += \
    $$PWD/blender_selection.cpp \
    $$PWD/language_setting.cpp

HEADERS += \
    $$PWD/app_paths.h \
    $$PWD/app_preferences.h \
    $$PWD/blender_selection.h \
    $$PWD/distributed_settings.h \
    $$PWD/language_setting.h \
    $$PWD/load_settings.h
