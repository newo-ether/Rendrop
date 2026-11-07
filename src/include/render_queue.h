// render_queue.h

#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#include <optional>
#include <vector>

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>

#include "dependency_packer.h"
#include "distributed_settings.h"
#include "project_info.h"

class QTimer;
class RenderJob;

// A contiguous frame range handed to a remote worker.
struct WorkAssignment
{
    QUuid leaseId;
    QUuid projectId;
    QString projectName;
    QString blendPath;
    QString outputPath;
    // Last path component of the output path (may hold '#' and an
    // extension). Workers render to <local dir>/<outputName> and upload the
    // resulting files, which land in the coordinator's output directory.
    QString outputName;
    // Changes whenever the .blend's path, size or modification time does, so
    // workers can cache downloads.
    QString blendKey;
    qint64 blendSize = 0;
    // True when the project and its assets come as a file set (see
    // packManifest); the worker then renders mainFile inside that set.
    bool packed = false;
    QString mainFile;
    int frameStart = 0;
    int frameEnd = 0;
    int frameStep = 0;
    int heartbeatSeconds = 0;
};

// What a lease holder may read and write on the coordinator.
struct LeaseFiles
{
    QString blendPath;
    QString outputDirectory;
    // The packed file set, absent when the .blend goes alone.
    std::optional<DependencyPacker::Pack> pack;
};

struct WorkerInfo
{
    QString id;
    QString name;
    qint64 lastSeenMs = 0;
    QUuid leaseId;
    int completedLeases = 0;
    // Measured on the current job, negative when unknown.
    double msPerFrame = -1.0;
};


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
    void setDistributedSettings(const DistributedSettings &settings);
    DistributedSettings distributedSettings() const;
    bool start();
    void stop();
    void shutdown();

    bool isRendering() const;
    std::vector<ProjectInfo> snapshots() const;
    ProjectInfo snapshotById(const QUuid &id) const;
    QString framePath(const QUuid &id, int frame) const;

    // Remote worker protocol. Leases only exist on the job currently being
    // rendered; anything else is reported as gone.
    std::optional<WorkAssignment> claimWork(const QString &workerId, const QString &workerName, int maxFrames);
    bool reportWork(const QUuid &leaseId, int finishedCount);
    bool completeWork(const QUuid &leaseId);
    bool failWork(const QUuid &leaseId, const QString &message);
    // A worker hands its lease back because its machine became busy.
    bool releaseWork(const QUuid &leaseId, int finishedCount);
    // True while the current job has frames a remote worker could take.
    bool offersRemoteWork() const;
    // Renews the lease. Empty when the lease is gone.
    std::optional<LeaseFiles> leaseFiles(const QUuid &leaseId);
    // Same value as WorkAssignment::blendKey for the file as it is now.
    // Only touches the file system, so any thread may call it.
    static QString blendKey(const QString &path);
    std::vector<WorkerInfo> workers() const;

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
    void onLeaseTimer();

private:
    void connectJob(RenderJob *job);
    RenderJob *findJob(const QUuid &id) const;
    int findJobIndex(const QUuid &id) const;
    bool startNextJob();
    void rebuildSnapshotCache();
    QString persistenceFilePath() const;
    void persistSnapshot(const ProjectInfo &snapshot);
    bool writePersistence() const;
    void applyDistributedSettings(RenderJob *job) const;
    RenderJob *currentJob() const;
    WorkerInfo *workerForLease(const QUuid &leaseId);

    std::vector<RenderJob *> jobs;
    mutable QReadWriteLock snapshotLock;
    std::vector<ProjectInfo> snapshotCache;
    QString persistenceDirectory;
    QString activeBlenderPath;
    QHash<QUuid, ProjectInfo> persistedSnapshots;
    QUuid currentRenderingId;
    bool rendering = false;
    bool shuttingDown = false;
    DistributedSettings distributed;
    QHash<QString, WorkerInfo> workerRegistry;
    QTimer *leaseTimer = nullptr;
    DependencyPacker *packer = nullptr;
};

#endif // RENDER_QUEUE_H
