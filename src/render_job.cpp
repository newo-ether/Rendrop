// render_job.cpp

#include <algorithm>
#include <chrono>
#include <limits>

#include <QThread>

#include "render_job.h"
#include "blender_file_reader.h"
#include "blender_renderer.h"

namespace {
qint64 nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

const QString localWorkerId = QStringLiteral("local");
}

RenderJob::RenderJob(
    const QUuid &id,
    const QString &name,
    const QString &filePath,
    const QString &blenderPath,
    QObject *parent
):
    RenderJob(
        ProjectInfo(
            id,
            name,
            filePath,
            QString(),
            ProjectState::Loading,
            0,
            0,
            0,
            0,
            0,
            0,
            QString(),
            0,
            0
        ),
        blenderPath,
        parent
    )
{
}

RenderJob::RenderJob(
    const ProjectInfo &project,
    const QString &blenderPath,
    QObject *parent
):
    QObject(parent),
    project(project),
    blenderPath(blenderPath),
    fileReader(new BlenderFileReader()),
    renderer(new BlenderRenderer())
{
    fileReader->setParameter(project.path, blenderPath);
    connectWorkers();
}

void RenderJob::connectWorkers()
{
    QObject::connect(fileReader, &BlenderFileReader::finishedReading, this, &RenderJob::onFinishedReading);
    QObject::connect(renderer, &BlenderRenderer::progressChanged, this, &RenderJob::onProgressChanged);
    QObject::connect(renderer, &BlenderRenderer::outputTextUpdate, this, &RenderJob::onOutputTextUpdated);
    QObject::connect(renderer, &BlenderRenderer::finishedRendering, this, &RenderJob::onFinishedRendering);
}

RenderJob::~RenderJob()
{
    shutdown();
    delete fileReader;
    delete renderer;
}

QUuid RenderJob::id() const
{
    return project.id;
}

ProjectInfo RenderJob::snapshot() const
{
    return project;
}

void RenderJob::setName(const QString &name)
{
    if (shuttingDown || name.isEmpty() || project.name == name)
    {
        return;
    }
    project.name = name;
    publishSnapshot();
}

void RenderJob::setBlenderPath(const QString &newBlenderPath)
{
    if (shuttingDown)
    {
        return;
    }
    blenderPath = newBlenderPath;
    if (!fileReader->isRunning())
    {
        fileReader->setParameter(project.path, blenderPath);
    }
}

void RenderJob::setChunkPolicy(bool enabled, const ChunkPlanner::Settings &settings)
{
    distributed = enabled;
    planner.setSettings(settings);
}

int RenderJob::nextChunk(const QString &workerId) const
{
    if (!distributed)
    {
        return std::numeric_limits<int>::max();
    }
    const int renderers = ledger.leaseHolderCount() + (ledger.holdsLease(workerId) ? 0 : 1);
    return planner.chunkFor(workerId, ledger.pendingCount(), renderers);
}

void RenderJob::recordProgress(const QUuid &leaseId, int finishedBefore)
{
    const std::optional<FrameLedger::Lease> lease = ledger.lease(leaseId);
    if (lease.has_value() && lease->finished > finishedBefore)
    {
        planner.recordProgress(lease->workerId, nowMs() - lease->claimedMs, lease->finished);
    }
}

double RenderJob::msPerFrame(const QString &workerId) const
{
    return planner.msPerFrame(workerId);
}

int RenderJob::pendingFrames() const
{
    return project.state == ProjectState::Rendering ? ledger.pendingCount() : 0;
}

bool RenderJob::setFrameRange(int frameStart, int frameEnd, int frameStep)
{
    if (shuttingDown
        || project.state == ProjectState::Loading
        || project.state == ProjectState::Rendering
        || project.state == ProjectState::Error
        || frameStep <= 0
        || frameEnd < frameStart)
    {
        return false;
    }
    project.frameStart = frameStart;
    project.frameEnd = frameEnd;
    project.frameStep = frameStep;
    project.finishedFrame = 0;
    project.completedRanges.clear();
    project.totalFrame = (frameEnd - frameStart) / frameStep + 1;
    project.state = ProjectState::Queued;
    publishSnapshot();
    return true;
}

bool RenderJob::reload()
{
    if (shuttingDown || renderer->isRunning())
    {
        return false;
    }
    if (fileReader->isRunning())
    {
        fileReader->stop();
        fileReader->wait();
    }
    ledger.clear();
    resetProjectData();
    project.state = ProjectState::Loading;
    publishSnapshot();
    fileReader->setParameter(project.path, blenderPath);
    fileReader->prepareStart();
    fileReader->start(QThread::LowestPriority);
    return true;
}

bool RenderJob::startRender()
{
    if (shuttingDown || project.state != ProjectState::Queued || renderer->isRunning())
    {
        return false;
    }
    ledger.reset(project.frameStart, project.frameEnd, project.frameStep, project.completedRanges);
    planner.reset();
    remoteFailures = 0;
    if (ledger.pendingCount() == 0)
    {
        // Nothing left to render. Report it without entering Rendering so the
        // queue can move on synchronously.
        syncProgressFromLedger();
        project.updateCompletionState();
        ledger.clear();
        publishSnapshot();
        return false;
    }
    project.state = ProjectState::Rendering;
    syncProgressFromLedger();
    publishSnapshot();
    startLocalLease();
    return true;
}

void RenderJob::stopRender()
{
    if (shuttingDown)
    {
        return;
    }
    localLeaseId = QUuid();
    if (renderer->isRunning())
    {
        renderer->stop();
        renderer->wait();
    }
    // Remote workers find out on their next heartbeat (410 Gone).
    ledger.releaseAll();
    if (project.state == ProjectState::Rendering)
    {
        project.state = ProjectState::Queued;
        syncProgressFromLedger();
        publishSnapshot();
    }
}

void RenderJob::shutdown()
{
    if (shuttingDown)
    {
        return;
    }
    shuttingDown = true;
    if (fileReader->isRunning())
    {
        fileReader->stop();
        fileReader->wait();
    }
    if (renderer->isRunning())
    {
        renderer->stop();
        renderer->wait();
    }
}

std::optional<FrameLedger::Lease> RenderJob::claimRemote(const QString &workerId, int maxFrames)
{
    if (shuttingDown || project.state != ProjectState::Rendering || !distributed)
    {
        return std::nullopt;
    }
    const int chunk = nextChunk(workerId);
    const int limit = maxFrames > 0 ? std::min(maxFrames, chunk) : chunk;
    return ledger.claim(workerId, limit, nowMs());
}

bool RenderJob::reportRemote(const QUuid &leaseId, int finishedCount)
{
    if (shuttingDown || project.state != ProjectState::Rendering || !isRemoteLease(leaseId))
    {
        return false;
    }
    const int before = ledger.doneCount();
    const int finishedBefore = ledger.lease(leaseId)->finished;
    if (!ledger.report(leaseId, finishedCount, nowMs()))
    {
        return false;
    }
    recordProgress(leaseId, finishedBefore);
    if (ledger.doneCount() != before)
    {
        syncProgressFromLedger();
        publishSnapshot();
    }
    return true;
}

bool RenderJob::completeRemote(const QUuid &leaseId)
{
    if (shuttingDown || project.state != ProjectState::Rendering || !isRemoteLease(leaseId))
    {
        return false;
    }
    const FrameLedger::Lease lease = *ledger.lease(leaseId);
    if (!ledger.complete(leaseId))
    {
        return false;
    }
    planner.recordComplete(lease.workerId, nowMs() - lease.claimedMs, lease.count);
    syncProgressFromLedger();
    publishSnapshot();
    finishIfComplete();
    return true;
}

bool RenderJob::releaseRemote(const QUuid &leaseId, int finishedCount)
{
    if (shuttingDown || project.state != ProjectState::Rendering || !isRemoteLease(leaseId))
    {
        return false;
    }
    const int finishedBefore = ledger.lease(leaseId)->finished;
    ledger.report(leaseId, finishedCount, nowMs());
    recordProgress(leaseId, finishedBefore);
    ledger.release(leaseId);
    syncProgressFromLedger();
    publishSnapshot();
    pumpLocal();
    return true;
}

bool RenderJob::failRemote(const QUuid &leaseId)
{
    if (shuttingDown || project.state != ProjectState::Rendering || !isRemoteLease(leaseId))
    {
        return false;
    }
    if (!ledger.release(leaseId))
    {
        return false;
    }
    syncProgressFromLedger();
    if (++remoteFailures >= maxRemoteFailures)
    {
        failRun();
        return true;
    }
    publishSnapshot();
    pumpLocal();
    return true;
}

std::vector<FrameLedger::Lease> RenderJob::expireRemoteLeases(qint64 timeoutMs)
{
    if (shuttingDown || project.state != ProjectState::Rendering)
    {
        return {};
    }
    const qint64 now = nowMs();
    // The local renderer reports progress only when a frame is saved; keep its
    // lease alive explicitly so long frames never expire it.
    if (!localLeaseId.isNull())
    {
        ledger.report(localLeaseId, renderer->getFinishedFrame(), now);
    }
    std::vector<FrameLedger::Lease> expired = ledger.expire(now, timeoutMs);
    if (!expired.empty())
    {
        syncProgressFromLedger();
        publishSnapshot();
        pumpLocal();
    }
    return expired;
}

int RenderJob::frameAt(int index) const
{
    return ledger.frameAt(index);
}

void RenderJob::onFinishedReading(int status, BlenderFileInfo info)
{
    if (shuttingDown)
    {
        return;
    }
    if (status == 0)
    {
        project.frameStart = info.frameBegin;
        project.frameEnd = info.frameEnd;
        project.frameStep = info.frameStep;
        project.resolutionX = info.resolutionX;
        project.resolutionY = info.resolutionY;
        project.resolutionScale = info.resolutionScale;
        project.renderEngine = info.renderEngine;
        project.outputPath = info.outputPath;
        project.finishedFrame = 0;
        project.completedRanges.clear();
        project.totalFrame = info.frameStep <= 0
            ? 0
            : (info.frameEnd - info.frameBegin) / info.frameStep + 1;
        project.state = project.outputPath.isEmpty() || project.totalFrame <= 0
            ? ProjectState::Error
            : ProjectState::Queued;
    }
    else
    {
        resetProjectData();
        project.state = ProjectState::Error;
    }
    publishSnapshot();
}

void RenderJob::onProgressChanged()
{
    if (shuttingDown || project.state != ProjectState::Rendering || localLeaseId.isNull())
    {
        return;
    }
    const int before = ledger.doneCount();
    const std::optional<FrameLedger::Lease> localLease = ledger.lease(localLeaseId);
    const int finishedBefore = localLease.has_value() ? localLease->finished : 0;
    ledger.report(localLeaseId, renderer->getFinishedFrame(), nowMs());
    recordProgress(localLeaseId, finishedBefore);
    if (ledger.doneCount() != before)
    {
        syncProgressFromLedger();
        publishSnapshot();
    }
}

void RenderJob::onOutputTextUpdated(QString text)
{
    if (!shuttingDown)
    {
        emit outputTextUpdated(text);
    }
}

void RenderJob::onFinishedRendering(int status)
{
    if (shuttingDown)
    {
        return;
    }
    pendingFinishSignals = std::max(pendingFinishSignals - 1, 0);
    if (pendingFinishSignals > 0 || status > 0)
    {
        // Stale signal from an older run, or a run stopped on purpose.
        return;
    }
    if (project.state != ProjectState::Rendering || localLeaseId.isNull())
    {
        return;
    }
    const QUuid finishedLease = localLeaseId;
    localLeaseId = QUuid();
    if (status == 0)
    {
        if (const std::optional<FrameLedger::Lease> lease = ledger.lease(finishedLease))
        {
            planner.recordComplete(lease->workerId, nowMs() - lease->claimedMs, lease->count);
        }
        ledger.complete(finishedLease);
        syncProgressFromLedger();
        publishSnapshot();
        pumpLocal();
        return;
    }
    ledger.release(finishedLease);
    failRun();
}

void RenderJob::publishSnapshot()
{
    emit snapshotChanged(project);
}

void RenderJob::resetProjectData()
{
    project.outputPath.clear();
    project.frameStart = 0;
    project.frameEnd = 0;
    project.frameStep = 0;
    project.resolutionX = 0;
    project.resolutionY = 0;
    project.resolutionScale = 0;
    project.renderEngine.clear();
    project.finishedFrame = 0;
    project.completedRanges.clear();
    project.totalFrame = 0;
}

void RenderJob::syncProgressFromLedger()
{
    project.finishedFrame = ledger.doneCount();
    project.completedRanges = ledger.completedRanges();
}

bool RenderJob::isRemoteLease(const QUuid &leaseId) const
{
    return !leaseId.isNull() && leaseId != localLeaseId && ledger.lease(leaseId).has_value();
}

bool RenderJob::startLocalLease()
{
    const std::optional<FrameLedger::Lease> lease = ledger.claim(localWorkerId, nextChunk(localWorkerId), nowMs());
    if (!lease.has_value())
    {
        return false;
    }
    // The previous run has already emitted its finish signal but its thread
    // may still be unwinding; QThread::start() is a no-op while running.
    renderer->wait();
    localLeaseId = lease->id;
    renderer->setParameter(
        blenderPath,
        project.path,
        ledger.frameAt(lease->firstIndex),
        ledger.frameAt(lease->firstIndex + lease->count - 1),
        project.frameStep,
        true
    );
    renderer->prepareStart();
    ++pendingFinishSignals;
    renderer->start(QThread::LowestPriority);
    return true;
}

void RenderJob::pumpLocal()
{
    if (project.state != ProjectState::Rendering)
    {
        return;
    }
    if (localLeaseId.isNull() && startLocalLease())
    {
        return;
    }
    finishIfComplete();
}

void RenderJob::finishIfComplete()
{
    if (project.state != ProjectState::Rendering
        || !localLeaseId.isNull()
        || !ledger.isComplete()
        || ledger.hasLeases())
    {
        return;
    }
    project.finishedFrame = project.totalFrame;
    project.completedRanges = ProjectInfo::prefixRanges(project.totalFrame);
    project.state = ProjectState::Finished;
    ledger.clear();
    publishSnapshot();
    emit renderFinished(project.id);
}

void RenderJob::failRun()
{
    localLeaseId = QUuid();
    if (renderer->isRunning())
    {
        renderer->stop();
        renderer->wait();
    }
    ledger.releaseAll();
    syncProgressFromLedger();
    ledger.clear();
    project.state = ProjectState::Error;
    publishSnapshot();
    emit renderFinished(project.id);
}
