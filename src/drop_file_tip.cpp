// drop_file_tip.cpp

#include "drop_file_tip.h"
#include "ui_drop_file_tip.h"

DropFileTip::DropFileTip(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DropFileTip)
{
    ui->setupUi(this);
}

DropFileTip::~DropFileTip()
{
    delete ui;
}
