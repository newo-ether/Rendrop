#ifndef RENAME_DIALOG_H
#define RENAME_DIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class RenameDialog;
}

class RenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RenameDialog(QString currentName, QWidget *parent = nullptr);
    ~RenameDialog();

    QString getName() const;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    Ui::RenameDialog *ui;
};

#endif // RENAME_DIALOG_H
