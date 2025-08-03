// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QString>
#include <QImage>
#include <vector>

#include "file_bar.h"
#include "add_file_button.h"
#include "drop_shadow_renderer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::widget *ui;
    AddFileButton *addFileButton;
    std::vector<FileBar *> fileBars;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    DropShadowRenderer *dropShadowRenderer;
    QImage fileBarShadowImage;

private:
    void newFileBar(QString filename);
    void createDropShadowWidget(
        QWidget *parent,
        QWidget *target,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius
    );

};
#endif // WIDGET_H
