#include "ui/widgets/frame_range_dialog.h"
#include "ui_frame_range_dialog.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rounded_surface.h"

FrameRangeDialog::FrameRangeDialog(int start, int end, int step, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameRangeDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    Look::styleDialog(this);

    // Keeps the fixed-width dialog from squeezing the form labels.
    for (QSpinBox *spin : {ui->startSpin, ui->endSpin, ui->stepSpin})
    {
        spin->setMinimumWidth(96);
        // RendropStyle draws the step buttons; the box is painted antialiased.
        RoundedSurface::install(spin, 6, RoundedSurface::Colors {
            QColor(43, 45, 48), QColor(), QColor(), QColor(), QColor(85, 90, 102), QColor(110, 110, 180)});
    }
    for (QPushButton *button : {ui->okButton, ui->cancelButton})
    {
        RoundedSurface::install(button, 6, RoundedSurface::Colors {
            QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(0x2B, 0x2F, 0x38), QColor(),
            QColor(0x55, 0x5A, 0x66)});
        Look::setPadding(button, QMargins(6, 6, 6, 6));
    }

    ui->startSpin->setValue(start);
    ui->endSpin->setValue(end);
    ui->stepSpin->setValue(step);

    connect(ui->okButton, &QPushButton::clicked, this, [this]() {
        if (ui->startSpin->value() > ui->endSpin->value()) {
            ui->endSpin->setValue(ui->startSpin->value());
        }
        if (ui->stepSpin->value() < 1) {
            ui->stepSpin->setValue(1);
        }
        accept();
    });
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
