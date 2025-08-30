// drop_file_tip.cpp

#include <QString>

#include "drop_file_tip.h"
#include "ui_drop_file_tip.h"

DropFileTip::DropFileTip(QWidget *parent):
    QWidget(parent),
    ui(new Ui::DropFileTip)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->label->setText(tr("Drop your Blender Projects here."));
}

DropFileTip::~DropFileTip()
{
    delete ui;
}

void DropFileTip::changeTip(QString text)
{
    ui->label->setText(text);
}
