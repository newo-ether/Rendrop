#include "frame_range_dialog.h"
#include "ui_frame_range_dialog.h"

FrameRangeDialog::FrameRangeDialog(int start, int end, int step, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameRangeDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->startSpin->setValue(start);
    ui->endSpin->setValue(end);
    ui->stepSpin->setValue(step);

    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

FrameRangeDialog::~FrameRangeDialog()
{
    delete ui;
}

int FrameRangeDialog::getStart() const
{
    return ui->startSpin->value();
}

int FrameRangeDialog::getEnd() const
{
    return ui->endSpin->value();
}

int FrameRangeDialog::getStep() const
{
    return ui->stepSpin->value();
}
