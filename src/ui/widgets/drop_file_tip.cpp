// drop_file_tip.cpp

#include <QString>

#include "ui/widgets/drop_file_tip.h"
#include "ui/style/look_widgets.h"
#include "ui_drop_file_tip.h"

DropFileTip::DropFileTip(QWidget *parent):
    QWidget(parent),
    ui(new Ui::DropFileTip)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    Look::setTextColor(ui->label, Theme::textStrong);
    Look::setGlyphPixmap(ui->icon, QStringLiteral(":/icon/3d_file.ico"), Theme::iconFaint);
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
