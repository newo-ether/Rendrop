// drop_file_tip.cpp

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

#include "drop_file_tip.h"
#include "ui_drop_file_tip.h"

DropFileTip::DropFileTip(QWidget *parent):
    QWidget(parent),
    ui(new Ui::DropFileTip)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

DropFileTip::~DropFileTip()
{
    delete ui;
}
