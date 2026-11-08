# Qt Widgets front end. Depends on app (AppController), farm, core and config.

SOURCES += \
    $$PWD/motion/frame_driver.cpp \
    $$PWD/motion/list_motion.cpp \
    $$PWD/motion/page_fade.cpp \
    $$PWD/motion/popup_motion.cpp \
    $$PWD/motion/smooth_scroll.cpp \
    $$PWD/motion/transition.cpp \
    $$PWD/pages/nodes_page.cpp \
    $$PWD/pages/project_page.cpp \
    $$PWD/pages/queue_page.cpp \
    $$PWD/pages/settings_page.cpp \
    $$PWD/style/drop_shadow_widget.cpp \
    $$PWD/style/look_widgets.cpp \
    $$PWD/style/rendrop_style.cpp \
    $$PWD/style/rounded_surface.cpp \
    $$PWD/widgets/add_file_button.cpp \
    $$PWD/widgets/caption_button.cpp \
    $$PWD/widgets/drop_file_tip.cpp \
    $$PWD/widgets/file_bar.cpp \
    $$PWD/widgets/frame_range_dialog.cpp \
    $$PWD/widgets/loading_bar.cpp \
    $$PWD/widgets/log_panel.cpp \
    $$PWD/widgets/message_dialog.cpp \
    $$PWD/widgets/progress_bar.cpp \
    $$PWD/widgets/rename_dialog.cpp \
    $$PWD/window/widget.cpp

HEADERS += \
    $$PWD/motion/frame_driver.h \
    $$PWD/motion/list_motion.h \
    $$PWD/motion/page_fade.h \
    $$PWD/motion/popup_motion.h \
    $$PWD/motion/smooth_scroll.h \
    $$PWD/motion/transition.h \
    $$PWD/pages/nodes_page.h \
    $$PWD/pages/project_page.h \
    $$PWD/pages/queue_page.h \
    $$PWD/pages/settings_page.h \
    $$PWD/style/color.h \
    $$PWD/style/drop_shadow_widget.h \
    $$PWD/style/look_widgets.h \
    $$PWD/style/rendrop_style.h \
    $$PWD/style/rounded_surface.h \
    $$PWD/style/style.h \
    $$PWD/widgets/add_file_button.h \
    $$PWD/widgets/caption_button.h \
    $$PWD/widgets/drop_file_tip.h \
    $$PWD/widgets/file_bar.h \
    $$PWD/widgets/frame_range_dialog.h \
    $$PWD/widgets/loading_bar.h \
    $$PWD/widgets/log_panel.h \
    $$PWD/widgets/message_dialog.h \
    $$PWD/widgets/progress_bar.h \
    $$PWD/widgets/rename_dialog.h \
    $$PWD/window/widget.h

FORMS += \
    $$PWD/forms/drop_file_tip.ui \
    $$PWD/forms/file_bar.ui \
    $$PWD/forms/frame_range_dialog.ui \
    $$PWD/forms/queue_page.ui \
    $$PWD/forms/rename_dialog.ui \
    $$PWD/forms/widget.ui
