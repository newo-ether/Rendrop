#include "rename_dialog.h"
#include "ui_rename_dialog.h"

RenameDialog::RenameDialog(QString currentName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RenameDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->nameEdit->setText(currentName);
    ui->nameEdit->setFocus();
    ui->nameEdit->selectAll();

    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

RenameDialog::~RenameDialog()
{
    delete ui;
}

QString RenameDialog::getName() const
{
    return ui->nameEdit->text();
}
