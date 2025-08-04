// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QString>
#include <QPixmap>
#include <vector>

#include "file_bar.h"

#include "add_file_button.h"
#include "drop_shadow_renderer.h"

#include "drop_file_tip.h"

#define DEBUG 0

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
    DropFileTip *dropFileTip;
    std::vector<FileBar *> fileBars;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    DropShadowRenderer *dropShadowRenderer;
    QPixmap fileBarShadowPixmap;

private:
    void newFileBar(QString fileName);
    void createDropShadowWidget(
        QWidget *parent,
        QWidget *target,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius
    );

#if DEBUG
    void printItems();
#endif

private slots:
    void onFileBarUp(FileBar *fileBar);
    void onFileBarDown(FileBar *fileBar);
    void onFileBarDelete(FileBar *fileBar);

    void onAddFileButtonClicked();

};

#endif // WIDGET_H
