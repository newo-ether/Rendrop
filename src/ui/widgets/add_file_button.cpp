// add_file_button.cpp

#include <QPushButton>

#include "ui/widgets/add_file_button.h"
#include "ui/style/drop_shadow_widget.h"
#include "ui/style/rounded_surface.h"

AddFileButton::AddFileButton(QWidget *parent):
    QPushButton(parent)
{
    resize(60, 60);
    setMinimumSize(60, 60);
    setMaximumSize(60, 60);
    // RoundedSurface paints the antialiased circle and fades its states.
    RoundedSurface::install(this, 30, RoundedSurface::Colors {
        QColor(90, 90, 150),
        QColor(120, 120, 180),
        QColor(70, 70, 130),
        QColor(85, 89, 95)
    });
    setIcon(QIcon(":/icon/plus.ico"));
    setIconSize(QSize(20, 20));

    dropShadowWidget = new DropShadowWidget(parent, this);
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
