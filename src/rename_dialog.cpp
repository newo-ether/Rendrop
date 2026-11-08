#include "rename_dialog.h"
#include "ui_rename_dialog.h"
#include "look_widgets.h"
#include "rounded_surface.h"

#include <QEvent>
#include <QTimer>

RenameDialog::RenameDialog(QString currentName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RenameDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    Look::styleDialog(this);

    // The boxes are painted antialiased.
    RoundedSurface::install(ui->nameEdit, 4, RoundedSurface::Colors {
        QColor(43, 45, 48), QColor(), QColor(), QColor(), QColor(85, 90, 102), QColor(110, 110, 180)});
    for (QPushButton *button : {ui->okButton, ui->cancelButton})
    {
        RoundedSurface::install(button, 6, RoundedSurface::Colors {
            QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(0x2B, 0x2F, 0x38), QColor(),
            QColor(0x55, 0x5A, 0x66)});
        Look::setPadding(button, QMargins(6, 6, 6, 6));
    }

    ui->nameEdit->setText(currentName);
    ui->nameEdit->setFocus();
    // QLineEdit selects everything when it gets focus through the dialog's
    // initial tab focus, so the caret is reset after each focus-in instead.
    ui->nameEdit->installEventFilter(this);

    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

RenameDialog::~RenameDialog()
{
    delete ui;
}

bool RenameDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->nameEdit && event->type() == QEvent::FocusIn)
    {
        // Runs after QLineEdit::focusInEvent has applied its select-all.
        QTimer::singleShot(0, this, [this]() {
            ui->nameEdit->deselect();
            ui->nameEdit->end(false);
        });
    }
    return QDialog::eventFilter(object, event);
}

QString RenameDialog::getName() const
{
    return ui->nameEdit->text();
}
