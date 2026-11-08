// render_job.h

#ifndef RENDER_JOB_H
#define RENDER_JOB_H

#include <optional>

#include <QObject>
#include <QString>

#include "blender_file_info.h"
#include "chunk_planner.h"
#include "frame_ledger.h"
#include "project_info.h"

class BlenderFileReader;
class BlenderRenderer;

class RenderJob : public QObject
{
    Q_OBJECT

public:
    // A remote job failing this many leases in one run marks the job as Error.
    static constexpr int maxRemoteFailures = 3;

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
    // distributed false: the local renderer takes the whole remaining range
    // in one Blender process and remote claims are refused.
    void setChunkPolicy(bool distributed, const ChunkPlanner::Settings &settings);
    bool setFrameRange(int frameStart, int frameEnd, int frameStep);
    bool reload();
    bool startRender();
    void stopRender();
    void shutdown();

    // Remote lease operations. They only succeed while the job is rendering
    // and the lease belongs to a remote worker.
    // owner is the name credited with the frames; empty falls back to workerId.
    std::optional<FrameLedger::Lease> claimRemote(const QString &workerId, int maxFrames, const QString &owner = QString());
    bool reportRemote(const QUuid &leaseId, int finishedCount);
    bool completeRemote(const QUuid &leaseId);
    bool failRemote(const QUuid &leaseId);
    // The worker gives its lease back on purpose (its machine became busy).
    // Frames it reported stay done; this never counts as a failure.
    bool releaseRemote(const QUuid &leaseId, int finishedCount);
    double msPerFrame(const QString &workerId) const;
    // Frames nobody holds yet; 0 when not rendering.
    int pendingFrames() const;
    std::vector<FrameLedger::Lease> expireRemoteLeases(qint64 timeoutMs);
    int frameAt(int index) const;

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
    void publishSnapshot();
    void resetProjectData();
    void syncProgressFromLedger();
    bool isRemoteLease(const QUuid &leaseId) const;
    bool startLocalLease();
    void pumpLocal();
    void finishIfComplete();
    void failRun();
    int nextChunk(const QString &workerId) const;
    void recordProgress(const QUuid &leaseId, int doneBefore);

    ProjectInfo project;
    QString blenderPath;
    BlenderFileReader *fileReader;
    BlenderRenderer *renderer;
    FrameLedger ledger;
    QUuid localLeaseId;
    // Every renderer run emits exactly one finishedRendering signal. Only the
    // signal of the newest run is acted upon; older ones are stale.
    int pendingFinishSignals = 0;
    bool distributed = false;
    ChunkPlanner planner;
    int remoteFailures = 0;
    bool shuttingDown = false;
};

#endif // RENDER_JOB_H
