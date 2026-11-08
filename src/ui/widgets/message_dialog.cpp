// message_dialog.cpp

#include "ui/widgets/message_dialog.h"

#include "ui/style/look_widgets.h"

QMessageBox::StandardButton MessageDialog::show(
    QWidget *parent,
    QMessageBox::Icon icon,
    const QString &title,
    const QString &text,
    QMessageBox::StandardButtons buttons
)
{
    QMessageBox messageBox(parent);
    messageBox.setIcon(icon);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(Look::pixelFont(12));
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}
