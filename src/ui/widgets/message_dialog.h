// message_dialog.h

#ifndef MESSAGE_DIALOG_H
#define MESSAGE_DIALOG_H

#include <QMessageBox>
#include <QString>

class QWidget;

// Modal message boxes in the app's font. Each blocks until it is answered.
namespace MessageDialog {

QMessageBox::StandardButton show(
    QWidget *parent,
    QMessageBox::Icon icon,
    const QString &title,
    const QString &text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok
);

inline QMessageBox::StandardButton error(
    QWidget *parent, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok)
{
    return show(parent, QMessageBox::Critical, title, text, buttons);
}

inline QMessageBox::StandardButton warning(
    QWidget *parent, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok)
{
    return show(parent, QMessageBox::Warning, title, text, buttons);
}

inline QMessageBox::StandardButton info(
    QWidget *parent, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok)
{
    return show(parent, QMessageBox::Information, title, text, buttons);
}

} // namespace MessageDialog

#endif // MESSAGE_DIALOG_H
