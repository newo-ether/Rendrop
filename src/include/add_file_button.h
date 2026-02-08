// add_file_button.h

#ifndef ADD_FILE_BUTTON_H
#define ADD_FILE_BUTTON_H

#include <QPushButton>
#include <QPointer>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

class AddFileButton : public QPushButton
{
    Q_OBJECT

public:
    AddFileButton(QWidget *parent, DropShadowRenderer *dropShadowRenderer);
    ~AddFileButton();

private:
    QPointer<DropShadowWidget> dropShadowWidget;
    DropShadowRenderer *dropShadowRenderer;

};

#endif // ADD_FILE_BUTTON_H
