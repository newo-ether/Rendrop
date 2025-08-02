// file_bar.h

#ifndef FILEBAR_H
#define FILEBAR_H

#include <QWidget>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT
public:
    FileBar(QWidget *parent, DropShadowRenderer *dropShadowRenderer);
    ~FileBar();

private:
    Ui::fileBar *ui;
    DropShadowWidget *dropShadowWidget;
    DropShadowRenderer *dropShadowRenderer;

};

#endif // FILEBAR_H
