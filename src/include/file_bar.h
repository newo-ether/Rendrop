// file_bar.h

#ifndef FILE_BAR_H
#define FILE_BAR_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

#include "blender_file_info.h"
#include "blender_file_reader.h"

#include "blender_renderer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT

public:
    enum State
    {
        Loading,
        Queued,
        Error,
        Rendering,
        Finished
    };

    FileBar(
        QWidget *parent,
        DropShadowRenderer *dropShadowRenderer,
        QString fileName,
        QString filePath,
        QString blenderPath
    );
    ~FileBar();

    void setFileName(QString fileName);
    QString getFileName() const;
    QString getFilePath() const;
    void stopReading();
    DropShadowWidget *getDropShadowWidget() const;
    State getState() const;
    int getFinishedFrame() const;
    int getTotalFrame() const;
    void render();
    void stopRender();

private:
    void setFrame(int frameStart, int frameEnd, int frameStep);
    void setResolution(int resolutionX, int resolutionY, int resolutionScale);
    void setRenderEngine(int renderEngine);

private:
    Ui::fileBar *ui;
    BlenderFileReader *blenderFileReader;
    DropShadowWidget *dropShadowWidget;
    QString filePath;
    State state;
    int frameStart, frameEnd, frameStep;
    int finishedFrame, totalFrame;
    BlenderRenderer *blenderRenderer;

signals:
    void upButtonClicked(FileBar *fileBar);
    void downButtonClicked(FileBar *fileBar);
    void deleteButtonClicked(FileBar *fileBar);
    void reloadButtonClicked(FileBar *fileBar);
    void finishedReading();
    void finishedRendering();
    void progressChanged();

private slots:
    void onUpButtonClicked();
    void onDownButtonClicked();
    void onDeleteButtonClicked();
    void onReloadButtonClicked();
    void onFinishedReading(int status, BlenderFileInfo info);
    void onProgressChanged();
    void onFinishedRendering(int status);
};

#endif // FILE_BAR_H
