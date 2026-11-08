// add_file_button.h

#ifndef ADD_FILE_BUTTON_H
#define ADD_FILE_BUTTON_H

#include <QPushButton>
#include <QPointer>

#include "ui/style/drop_shadow_widget.h"

class AddFileButton : public QPushButton
{
    Q_OBJECT

public:
    explicit AddFileButton(QWidget *parent);
    ~AddFileButton();

private:
    QPointer<DropShadowWidget> dropShadowWidget;

};

#endif // ADD_FILE_BUTTON_H
