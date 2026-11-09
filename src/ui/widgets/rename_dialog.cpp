#include "ui/widgets/rename_dialog.h"
#include "ui_rename_dialog.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rounded_surface.h"

#include <QEvent>
#include <QTimer>

RenameDialog::RenameDialog(QString currentName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RenameDialog)
{
    ui->setupUi(this);

    // The boxes are painted antialiased.
    RoundedSurface::install(ui->nameEdit, 4, RoundedSurface::Colors {
        Theme::row, {}, {}, {}, Theme::outline, Theme::accent});
    for (QPushButton *button : {ui->okButton, ui->cancelButton})
    {
        RoundedSurface::install(button, 6, RoundedSurface::Colors {
            Theme::control, Theme::controlHover, Theme::controlPressed, {}, Theme::outline});
        Look::setPadding(button, QMargins(6, 6, 6, 6));
    }

    ui->nameEdit->setText(currentName);
    ui->nameEdit->setFocus();
    // QLineEdit selects everything when it gets focus through the dialog's
    // initial tab focus, so the caret is reset after each focus-in instead.
    ui->nameEdit->installEventFilter(this);

    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    // The parent is the control that opened it.
    Look::stylePopover(this, parent);
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
