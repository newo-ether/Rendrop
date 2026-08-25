// render_queue.h

#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <vector>

#include "project_info.h"

class RenderJob;

class RenderQueue : public QObject
{
    Q_OBJECT

public:
    explicit RenderQueue(QObject *parent = nullptr);
    RenderQueue(const QString &persistenceDirectory, QObject *parent = nullptr);
    ~RenderQueue() override;

    int restorePersistedJobs(const QString &blenderPath);
    QUuid addJob(const QString &filePath, const QString &name = QString());
    bool addJob(const QUuid &id, const QString &filePath, const QString &name = QString());
    QUuid addJob(const QString &name, const QString &filePath, const QString &blenderPath);
    bool removeJob(const QUuid &id);
    bool moveTo(const QUuid &id, int targetIndex);
    bool moveUp(const QUuid &id);
    bool moveDown(const QUuid &id);
    bool reloadJob(const QUuid &id);
    bool reloadJob(const QUuid &id, const QString &blenderPath);
    bool renameJob(const QUuid &id, const QString &name);
    bool setFrameRange(const QUuid &id, int frameStart, int frameEnd, int frameStep);
    void setBlenderPathForAll(const QString &blenderPath);
    bool start();
    void stop();
    void shutdown();

    bool isRendering() const;
    std::vector<ProjectInfo> snapshots() const;
    ProjectInfo snapshotById(const QUuid &id) const;
    QString framePath(const QUuid &id, int frame) const;

signals:
    void jobAdded(ProjectInfo project);
    void jobUpdated(ProjectInfo project);
    void jobRemoved(QUuid id);
    void jobMoved(QUuid id, int fromIndex, int toIndex);
    void queueChanged();
    void renderingChanged(bool rendering);
    void outputTextUpdated(QString text);

private slots:
    void onJobSnapshotChanged(ProjectInfo snapshot);
    void onJobRenderFinished(QUuid id);

private:
    void connectJob(RenderJob *job);
    RenderJob *findJob(const QUuid &id) const;
    int findJobIndex(const QUuid &id) const;
    bool startNextJob();
    void rebuildSnapshotCache();
    QString persistenceFilePath() const;
    void persistSnapshot(const ProjectInfo &snapshot);
    bool writePersistence() const;

    std::vector<RenderJob *> jobs;
    mutable QReadWriteLock snapshotLock;
    std::vector<ProjectInfo> snapshotCache;
    QString persistenceDirectory;
    QString activeBlenderPath;
    QHash<QUuid, ProjectInfo> persistedSnapshots;
    QUuid currentRenderingId;
    bool rendering = false;
    bool shuttingDown = false;
};

#endif // RENDER_QUEUE_H
