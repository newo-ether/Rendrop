// add_file_button.cpp

#include <QPushButton>

#include "add_file_button.h"
#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

AddFileButton::AddFileButton(QWidget *parent, DropShadowRenderer *dropShadowRenderer):
    QPushButton(parent), dropShadowRenderer(dropShadowRenderer)
{
    resize(60, 60);
    setMinimumSize(60, 60);
    setMaximumSize(60, 60);
    setStyleSheet(R"(
        QPushButton {
            background-color: rgb(90, 90, 150);
            border-radius: 30px;
            color: white;
            padding: 6px 6px;
            border: none;
        }

        QPushButton:hover {
            background-color: rgb(120, 120, 180);
            color: white;
        }

        QPushButton:pressed {
            background-color: rgb(70, 70, 130);
            color: white;
        }

        QPushButton:disabled {
            background-color: rgb(85, 89, 95);
            color: #888888;
        }
    )");
    setIcon(QIcon(":/icon/plus.ico"));
    setIconSize(QSize(20, 20));

    dropShadowWidget = new DropShadowWidget(parent, this, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(30);
    dropShadowWidget->setOffsetX(3);
    dropShadowWidget->setOffsetY(3);
    dropShadowWidget->setAlphaMax(0.8f);
    dropShadowWidget->setBlurRadius(12);
}

AddFileButton::~AddFileButton()
{
    delete dropShadowWidget;
}
