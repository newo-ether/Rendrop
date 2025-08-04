// file_bar.cpp

#include "file_bar.h"
#include "ui_file_bar.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

FileBar::FileBar(
    QWidget *parent,
    DropShadowRenderer *dropShadowRenderer,
    unsigned int id
):
    QWidget(parent),
    ui(new Ui::fileBar),
    id(id)
{
    ui->setupUi(this);

    QObject::connect(ui->upButton, &QPushButton::clicked, this, &FileBar::onUpButtonClicked);
    QObject::connect(ui->downButton, &QPushButton::clicked, this, &FileBar::onDownButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &FileBar::onDeleteButtonClicked);

    dropShadowWidget = new DropShadowWidget(
        parent,
        this,
        dropShadowRenderer
    );
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.3f);
    dropShadowWidget->setBlurRadius(10);
    dropShadowWidget->show();
}

FileBar::~FileBar()
{
    delete dropShadowWidget;
    delete ui;
}

void FileBar::setFileName(QString fileName)
{
    ui->fileNameLabel->setText(fileName);
}

QString FileBar::getFileName() const
{
    return ui->fileNameLabel->text();
}

DropShadowWidget * FileBar::getDropShadowWidget() const
{
    return dropShadowWidget;
}

void FileBar::setID(unsigned int id)
{
    this->id = id;
}

unsigned int FileBar::getID() const
{
    return id;
}

void FileBar::onUpButtonClicked()
{
    emit upButtonClicked(this);
}

void FileBar::onDownButtonClicked()
{
    emit downButtonClicked(this);
}

void FileBar::onDeleteButtonClicked()
{
    emit deleteButtonClicked(this);
}
