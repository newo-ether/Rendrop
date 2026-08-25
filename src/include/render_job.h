// render_job.h

#ifndef RENDER_JOB_H
#define RENDER_JOB_H

#include <QObject>
#include <QString>

#include "blender_file_info.h"
#include "project_info.h"

class BlenderFileReader;
class BlenderRenderer;

class RenderJob : public QObject
{
    Q_OBJECT

public:
    RenderJob(
        const QUuid &id,
        const QString &name,
        const QString &filePath,
        const QString &blenderPath,
        QObject *parent = nullptr
    );
    RenderJob(
        const ProjectInfo &project,
        const QString &blenderPath,
        QObject *parent = nullptr
    );
    ~RenderJob() override;

    QUuid id() const;
    ProjectInfo snapshot() const;
    void setName(const QString &name);
    void setBlenderPath(const QString &blenderPath);
    bool setFrameRange(int frameStart, int frameEnd, int frameStep);
    bool reload();
    bool startRender();
    void stopRender();
    void shutdown();

signals:
    void snapshotChanged(ProjectInfo snapshot);
    void renderFinished(QUuid id);
    void outputTextUpdated(QString text);

private slots:
    void onFinishedReading(int status, BlenderFileInfo info);
    void onProgressChanged();
    void onOutputTextUpdated(QString text);
    void onFinishedRendering(int status);

private:
    void connectWorkers();
    void configureRendererFromProject();
    void publishSnapshot();
    void resetProjectData();

    ProjectInfo project;
    QString blenderPath;
    BlenderFileReader *fileReader;
    BlenderRenderer *renderer;
    bool shuttingDown = false;
};

#endif // RENDER_JOB_H
