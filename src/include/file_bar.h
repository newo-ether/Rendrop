// file_bar.h

#ifndef FILE_BAR_H
#define FILE_BAR_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QColor>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"
#include "blender_file_info.h"
#include "blender_file_reader.h"
#include "blender_renderer.h"
#include "loading_bar.h"
#include "style.h"
#include "project_info.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT

public:
    FileBar(
        QWidget *parent,
        DropShadowRenderer *dropShadowRenderer,
        int id,
        QString fileName,
        QString filePath,
        QString blenderPath
    );
    ~FileBar();

    int getID() const;
    void setFileName(QString fileName);
    void setFilePath(QString filePath);
    QString getFileName() const;
    QString getFilePath() const;
    void stopReading();
    DropShadowWidget *getDropShadowWidget() const;
    ProjectState getState() const;
    int getFrameStart() const;
    int getFrameEnd() const;
    int getFrameStep() const;
    int getResolutionX() const;
    int getResolutionY() const;
    int getResolutionScale() const;
    QString getRenderEngine() const;
    int getFinishedFrame() const;
    int getTotalFrame() const;
    void setOutputPath(const QString &outputPath);
    QString getOutputPath() const;
    QString getImagePathFromFrame(int frame);
    void render();
    void stopRender();

protected:
    bool eventFilter(QObject *object, QEvent *event);

private:
    void setFrame(int frameStart, int frameEnd, int frameStep);
    void setResolution(int resolutionX, int resolutionY, int resolutionScale);
    void setRenderEngine(const QString &renderEngine);
    void showLoadingBar();
    void hideLoadingBar();
    void setState(ProjectState state);

private:
    Ui::fileBar *ui;
    BlenderFileReader *blenderFileReader;
    DropShadowWidget *dropShadowWidget;
    QString filePath;
    ProjectState state;
    int id;
    int frameStart, frameEnd, frameStep;
    int resolutionX, resolutionY, resolutionScale;
    QString renderEngine;
    int finishedFrame, totalFrame;
    QString outputPath;
    BlenderRenderer *blenderRenderer;
    LoadingBar *frameLoadingBar, *resolutionLoadingBar, *renderEngineLoadingBar;

    Style loadingStyle, queueStyle, errorStyle, renderingStyle, finishedStyle;

    Style style, targetStyle, velocity;
    float stiffness, damping;
    qint64 lastElapsed;

    QTimer *timer;
    QElapsedTimer *elapsedTimer;

signals:
    void upButtonClicked(FileBar *fileBar);
    void downButtonClicked(FileBar *fileBar);
    void deleteButtonClicked(FileBar *fileBar);
    void reloadButtonClicked(FileBar *fileBar);
    void finishedReading();
    void finishedRendering();
    void progressChanged();
    void outputTextUpdate(QString text);

private slots:
    void onUpButtonClicked();
    void onDownButtonClicked();
    void onDeleteButtonClicked();
    void onReloadButtonClicked();
    void onFinishedReading(int status, BlenderFileInfo info);
    void onProgressChanged();
    void onOutputTextUpdate(QString text);
    void onFinishedRendering(int status);
    void updateStyle();
};

#endif // FILE_BAR_H
