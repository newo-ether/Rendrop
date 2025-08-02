// add_file_button.h

#ifndef ADDFILEBUTTON_H
#define ADDFILEBUTTON_H

#include <QPushButton>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

class AddFileButton : public QPushButton
{
    Q_OBJECT

public:
    AddFileButton(QWidget *parent, DropShadowRenderer *dropShadowRenderer);
    ~AddFileButton();

private:
    DropShadowWidget *dropShadowWidget;
    DropShadowRenderer *dropShadowRenderer;

};

#endif // ADDFILEBUTTON_H
