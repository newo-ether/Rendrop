// add_file_button.cpp

#include <QPushButton>

#include "add_file_button.h"
#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"
#include "rounded_surface.h"

AddFileButton::AddFileButton(QWidget *parent, DropShadowRenderer *dropShadowRenderer):
    QPushButton(parent), dropShadowRenderer(dropShadowRenderer)
{
    resize(60, 60);
    setMinimumSize(60, 60);
    setMaximumSize(60, 60);
    // The background is painted antialiased by RoundedSurface.
    setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: white;
            padding: 6px 6px;
            border: none;
        }

        QPushButton:disabled {
            color: #888888;
        }
    )");
    RoundedSurface::install(this, 30, RoundedSurface::Colors {
        QColor(90, 90, 150),
        QColor(120, 120, 180),
        QColor(70, 70, 130),
        QColor(85, 89, 95)
    });
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
