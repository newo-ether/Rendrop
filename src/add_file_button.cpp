// add_file_button.cpp

#include <QPushButton>

#include "add_file_button.h"
#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

AddFileButton::AddFileButton(QWidget *parent, DropShadowRenderer *dropShadowRenderer)
    : QPushButton(parent), dropShadowRenderer(dropShadowRenderer)
{
    resize(60, 60);
    setMinimumSize(60, 60);
    setMaximumSize(60, 60);
    setStyleSheet(R"(
        QPushButton {
            background-color: #5C6BC0;
            border-radius: 30px;
            color: white;
            padding: 6px 6px;
            border: none;
        }

        QPushButton:hover {
            background-color: #7986CB;
            color: white;
        }

        QPushButton:pressed {
            background-color: #3F51B5;
            color: white;
        }
    )");
    setIcon(QIcon(":/icon/plus.ico"));
    setIconSize(QSize(20, 20));

    dropShadowWidget = new DropShadowWidget(parent, this, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(30);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.3f);
    dropShadowWidget->setBlurRadius(12);
}

AddFileButton::~AddFileButton()
{
    delete dropShadowWidget;
}
