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
        Theme::accentFill,
        Theme::accentHover,
        Theme::accentPressed,
        Theme::accentDisabled
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
