// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QString>
#include <QPixmap>
#include <vector>
#include <QMessageBox>
#include <QCloseEvent>

#include "file_bar.h"
#include "add_file_button.h"

#include "drop_shadow_renderer.h"
#include "drop_file_tip.h"

#include "blender_version_manager.h"

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
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::widget *ui;
    AddFileButton *addFileButton;
    DropFileTip *dropFileTip;
    BlenderVersionManager *blenderVersionManager;
    std::vector<FileBar *> fileBars;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    DropShadowRenderer *dropShadowRenderer;
    QPixmap fileBarShadowPixmap;
    bool isRendering;
    int handle;

private:
    QMessageBox::StandardButton errorMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
    QMessageBox::StandardButton warningMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
    QMessageBox::StandardButton infoMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
    QMessageBox::StandardButton questionMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons = QMessageBox::Ok);

    FileBar *newFileBar(QString fileName, QString filePath);
    void createDropShadowWidget(
        QWidget *parent,
        QWidget *target,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius
    );
    void updateBlenderVersions();
    void updateButtonStatus();
    void updateStatisticInfo();
    void clearOutputText();
    void setSelectorEnabled(bool enable);
    void updateAllFileBarShadow();

private slots:
    void onFileBarUp(FileBar *fileBar);
    void onFileBarDown(FileBar *fileBar);
    void onFileBarDelete(FileBar *fileBar);
    void onFileBarReload(FileBar *fileBar);
    void onFinishedReading();
    void onAddFileButtonClicked();
    void onAddBlenderVersionButtonClicked();
    void onDeleteBlenderVersionButtonClicked();
    void onRenderButtonClicked();
    void onFinishedRendering();
    void onProgressChanged();
    void onOutputTextUpdate(QString text);
};

#endif // WIDGET_H
