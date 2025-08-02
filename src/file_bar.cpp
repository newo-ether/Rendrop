// file_bar.cpp

#include "file_bar.h"
#include "ui_file_bar.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

FileBar::FileBar(QWidget *parent, DropShadowRenderer *dropShadowRenderer):
    QWidget(parent),
    ui(new Ui::fileBar),
    dropShadowRenderer(dropShadowRenderer)
{
    ui->setupUi(this);

    dropShadowWidget = new DropShadowWidget(parent, this, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.3f);
    dropShadowWidget->setBlurRadius(15);
}

FileBar::~FileBar()
{
    delete dropShadowWidget;
    delete ui;
}
