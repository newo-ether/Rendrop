// render_queue.cpp

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QWriteLocker>

#include "app_paths.h"
#include "dependency_packer.h"
#include "render_queue.h"
#include "render_job.h"

namespace {
constexpr int persistenceSchemaVersion = 3;
// Workers that have not been heard from for this long disappear from /workers.
constexpr qint64 workerForgetMs = 10 * 60 * 1000;

qint64 nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int leaseTimerInterval(const DistributedSettings &settings)
{
    // Check several times per timeout so expiry is not delayed by a full period.
    return std::clamp(settings.leaseTimeoutSeconds * 250, 200, 5000);
}

QString frameString(int frame, int minWidth)
{
    const bool negative = frame < 0;
    QString digits = QString::number(std::abs(frame));
    if (digits.size() < minWidth)
    {
        digits.prepend(QString(minWidth - digits.size(), QChar('0')));
    }
    if (negative)
    {
        digits.prepend('-');
    }
    return digits;
}

QString resolveFramePath(const ProjectInfo &project, int frame)
{
    const QString originalPath = QDir::fromNativeSeparators(project.outputPath);
    if (originalPath.isEmpty())
    {
        return QString();
    }
    QString path = originalPath;
    const QString blendDirectory = QFileInfo(project.path).absolutePath();
    if (path.startsWith("//"))
    {
        path = QDir(blendDirectory).filePath(path.mid(2));
    }

    const bool hasHash = originalPath.contains('#');
    const bool endsWithSlash = originalPath.endsWith('/');
    const bool isDoubleSlash = originalPath == "//";
    if (endsWithSlash)
    {
        path += "####";
    }
    else if (isDoubleSlash)
    {
        path += "/####";
    }

    static const QRegularExpression hashPattern(R"(#+)");
    QRegularExpressionMatchIterator matches = hashPattern.globalMatch(path);
    QList<QPair<int, int>> spans;
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        spans.append({match.capturedStart(0), match.capturedLength(0)});
    }
    for (int index = spans.size() - 1; index >= 0; --index)
    {
        path.replace(
            spans[index].first,
            spans[index].second,
            frameString(frame, spans[index].second)
        );
    }

    if (!hasHash && !endsWithSlash && !isDoubleSlash)
    {
        const QFileInfo outputInfo(path);
        const QString numberedName = outputInfo.completeBaseName() + frameString(frame, 4);
        path = outputInfo.suffix().isEmpty()
            ? QDir(outputInfo.path()).filePath(numberedName)
            : QDir(outputInfo.path()).filePath(numberedName + "." + outputInfo.suffix());
    }
    if (QFileInfo(path).suffix().isEmpty())
    {
        path += ".png";
    }
    return QDir::toNativeSeparators(path);
}

// Directory Blender writes this project's frames into. Empty when the output
// path is unset or a directory component is numbered ('#'), which cannot be
// expressed as one directory.
QString resolveOutputDirectory(const ProjectInfo &project)
{
    QString path = QDir::fromNativeSeparators(project.outputPath);
    if (path.isEmpty())
    {
        return QString();
    }
    if (path.startsWith(QStringLiteral("//")))
    {
        path = QFileInfo(project.path).absolutePath() + QLatin1Char('/') + path.mid(2);
    }
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash < 0)
    {
        return QString();
    }
    const QString directory = path.left(slash + 1);
    if (directory.contains(QLatin1Char('#')))
    {
        return QString();
    }
    return QDir::toNativeSeparators(QDir::cleanPath(directory));
}

QString outputNameOf(const QString &outputPath)
{
    const QString path = QDir::fromNativeSeparators(outputPath);
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString blendKeyOf(const QFileInfo &info)
{
    const QByteArray identity = QStringLiteral("%1|%2|%3")
        .arg(QDir::cleanPath(info.absoluteFilePath()).toLower())
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(32)
    );
}
}

RenderQueue::RenderQueue(QObject *parent):
    RenderQueue(AppPaths::configDirectory(), parent)
{
}

RenderQueue::RenderQueue(const QString &persistenceDirectory, QObject *parent):
    QObject(parent),
    persistenceDirectory(persistenceDirectory),
    leaseTimer(new QTimer(this)),
    packer(new DependencyPacker(QDir::temp().filePath(QStringLiteral("rendrop-packs")), this))
{
    QObject::connect(leaseTimer, &QTimer::timeout, this, &RenderQueue::onLeaseTimer);
    QObject::connect(packer, &DependencyPacker::finished, this, [this](QString, bool, QString message) {
        emit outputTextUpdated(message);
    });
    leaseTimer->start(leaseTimerInterval(distributed));
}

RenderQueue::~RenderQueue()
{
    shutdown();
}

int RenderQueue::restorePersistedJobs(const QString &blenderPath)
{
    if (shuttingDown || !jobs.empty())
    {
        return 0;
    }
    activeBlenderPath = blenderPath;
    QFile file(persistenceFilePath());
    if (!file.exists())
    {
        return 0;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot open render queue persistence file:" << file.fileName();
        return 0;
    }
    const QByteArray persistedData = file.readAll();
    file.close();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(persistedData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        qWarning() << "Cannot parse render queue persistence file:" << parseError.errorString();
        return 0;
    }
    const QJsonObject root = document.object();
    const int rootSchemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (rootSchemaVersion < 1
        || rootSchemaVersion > persistenceSchemaVersion
        || !root.value(QStringLiteral("projects")).isArray())
    {
        qWarning() << "Unsupported render queue persistence schema";
        return 0;
    }
    QSet<QUuid> restoredIds;
    std::vector<ProjectInfo> restoredProjects;
    for (const QJsonValue &value : root.value(QStringLiteral("projects")).toArray())
    {
        if (!value.isObject())
        {
            continue;
        }
        const std::optional<ProjectInfo> project = ProjectInfo::fromJson(value.toObject());
        if (!project.has_value() || restoredIds.contains(project->id))
        {
            continue;
        }
        restoredIds.insert(project->id);
        restoredProjects.push_back(*project);
    }
    for (const ProjectInfo &project : restoredProjects)
    {
        RenderJob *job = new RenderJob(project, blenderPath, this);
        connectJob(job);
        jobs.push_back(job);
        persistedSnapshots.insert(project.id, project);
    }
    if (restoredProjects.empty())
    {
        writePersistence();
        return 0;
    }
    rebuildSnapshotCache();
    for (const ProjectInfo &project : restoredProjects)
    {
        emit jobAdded(project);
    }
    emit queueChanged();
    writePersistence();
    return static_cast<int>(restoredProjects.size());
}

void RenderQueue::connectJob(RenderJob *job)
{
    applyDistributedSettings(job);
    QObject::connect(job, &RenderJob::snapshotChanged, this, &RenderQueue::onJobSnapshotChanged);
    QObject::connect(job, &RenderJob::renderFinished, this, &RenderQueue::onJobRenderFinished);
    QObject::connect(job, &RenderJob::outputTextUpdated, this, &RenderQueue::outputTextUpdated);
}

QUuid RenderQueue::addJob(const QString &filePath, const QString &name)
{
    const QUuid id = QUuid::createUuid();
    return addJob(id, filePath, name) ? id : QUuid();
}

bool RenderQueue::addJob(const QUuid &id, const QString &filePath, const QString &name)
{
    if (shuttingDown || id.isNull() || findJob(id))
    {
        return false;
    }
    QString projectName = name.trimmed();
    if (projectName.isEmpty())
    {
        projectName = QFileInfo(filePath).completeBaseName();
    }
    RenderJob *job = new RenderJob(id, projectName, filePath, activeBlenderPath, this);
    connectJob(job);
    jobs.push_back(job);
    rebuildSnapshotCache();
    emit jobAdded(job->snapshot());
    emit queueChanged();
    job->reload();
    return true;
}

QUuid RenderQueue::addJob(const QString &name, const QString &filePath, const QString &blenderPath)
{
    if (shuttingDown)
    {
        return QUuid();
    }
    const QUuid id = QUuid::createUuid();
    RenderJob *job = new RenderJob(id, name, filePath, blenderPath, this);
    connectJob(job);
    jobs.push_back(job);
    rebuildSnapshotCache();
    emit jobAdded(job->snapshot());
    emit queueChanged();
    job->reload();
    return id;
}

bool RenderQueue::removeJob(const QUuid &id)
{
    if (shuttingDown)
    {
        return false;
    }
    const int index = findJobIndex(id);
    if (index < 0)
    {
        return false;
    }
    RenderJob *job = jobs[static_cast<size_t>(index)];
    const bool wasRendering = rendering && currentRenderingId == id;
    if (wasRendering)
    {
        currentRenderingId = QUuid();
    }
    jobs.erase(jobs.begin() + index);
    job->shutdown();
    delete job;
    persistedSnapshots.remove(id);
    rebuildSnapshotCache();
    writePersistence();
    emit jobRemoved(id);
    emit queueChanged();
    if (wasRendering)
    {
        startNextJob();
    }
    return true;
}

bool RenderQueue::moveUp(const QUuid &id)
{
    const int index = findJobIndex(id);
    return index > 0 && moveTo(id, index - 1);
}

bool RenderQueue::moveDown(const QUuid &id)
{
    const int index = findJobIndex(id);
    return index >= 0 && moveTo(id, index + 1);
}

bool RenderQueue::moveTo(const QUuid &id, int targetIndex)
{
    if (shuttingDown || targetIndex < 0 || targetIndex >= static_cast<int>(jobs.size()))
    {
        return false;
    }
    const int index = findJobIndex(id);
    if (index < 0)
    {
        return false;
    }
    if (index == targetIndex)
    {
        return true;
    }
    RenderJob *job = jobs[static_cast<size_t>(index)];
    jobs.erase(jobs.begin() + index);
    jobs.insert(jobs.begin() + targetIndex, job);
    rebuildSnapshotCache();
    writePersistence();
    emit jobMoved(id, index, targetIndex);
    emit queueChanged();
    return true;
}

bool RenderQueue::reloadJob(const QUuid &id)
{
    return reloadJob(id, activeBlenderPath);
}

bool RenderQueue::reloadJob(const QUuid &id, const QString &newBlenderPath)
{
    if (shuttingDown)
    {
        return false;
    }
    RenderJob *job = findJob(id);
    if (!job)
    {
        return false;
    }
    job->setBlenderPath(newBlenderPath);
    return job->reload();
}

bool RenderQueue::renameJob(const QUuid &id, const QString &name)
{
    if (shuttingDown || name.isEmpty())
    {
        return false;
    }
    RenderJob *job = findJob(id);
    if (!job)
    {
        return false;
    }
    job->setName(name);
    return true;
}

bool RenderQueue::setFrameRange(const QUuid &id, int frameStart, int frameEnd, int frameStep)
{
    if (shuttingDown)
    {
        return false;
    }
    RenderJob *job = findJob(id);
    return job && job->setFrameRange(frameStart, frameEnd, frameStep);
}

void RenderQueue::setBlenderPathForAll(const QString &blenderPath)
{
    if (shuttingDown)
    {
        return;
    }
    activeBlenderPath = blenderPath;
    for (RenderJob *job : jobs)
    {
        job->setBlenderPath(blenderPath);
    }
}

void RenderQueue::setDistributedSettings(const DistributedSettings &settings)
{
    if (shuttingDown)
    {
        return;
    }
    distributed = settings;
    for (RenderJob *job : jobs)
    {
        applyDistributedSettings(job);
    }
    leaseTimer->start(leaseTimerInterval(distributed));
}

DistributedSettings RenderQueue::distributedSettings() const
{
    return distributed;
}

void RenderQueue::applyDistributedSettings(RenderJob *job) const
{
    job->setChunkPolicy(distributed.enabled, distributed.chunkPlan());
}

bool RenderQueue::start()
{
    if (shuttingDown)
    {
        return false;
    }
    if (rendering)
    {
        return true;
    }
    const bool hasQueuedJob = std::any_of(jobs.begin(), jobs.end(), [](const RenderJob *job) {
        return job->snapshot().state == ProjectState::Queued;
    });
    if (!hasQueuedJob)
    {
        return false;
    }
    rendering = true;
    emit renderingChanged(true);
    return startNextJob();
}

void RenderQueue::stop()
{
    if (shuttingDown || !rendering)
    {
        return;
    }
    rendering = false;
    RenderJob *currentJob = findJob(currentRenderingId);
    currentRenderingId = QUuid();
    if (currentJob)
    {
        currentJob->stopRender();
    }
    emit renderingChanged(false);
    emit queueChanged();
}

void RenderQueue::shutdown()
{
    if (shuttingDown)
    {
        return;
    }
    shuttingDown = true;
    rendering = false;
    currentRenderingId = QUuid();
    packer->stop();
    for (RenderJob *job : jobs)
    {
        job->shutdown();
        delete job;
    }
    jobs.clear();
    QWriteLocker locker(&snapshotLock);
    snapshotCache.clear();
}

bool RenderQueue::isRendering() const
{
    return rendering;
}

std::vector<ProjectInfo> RenderQueue::snapshots() const
{
    QReadLocker locker(&snapshotLock);
    return snapshotCache;
}

ProjectInfo RenderQueue::snapshotById(const QUuid &id) const
{
    QReadLocker locker(&snapshotLock);
    const auto iterator = std::find_if(
        snapshotCache.begin(),
        snapshotCache.end(),
        [&id](const ProjectInfo &project) { return project.id == id; }
    );
    return iterator == snapshotCache.end() ? ProjectInfo() : *iterator;
}

QString RenderQueue::framePath(const QUuid &id, int frame) const
{
    const ProjectInfo project = snapshotById(id);
    return project.isNull ? QString() : resolveFramePath(project, frame);
}

std::optional<WorkAssignment> RenderQueue::claimWork(
    const QString &workerId,
    const QString &workerName,
    int maxFrames
)
{
    if (shuttingDown || !distributed.enabled || workerId.isEmpty())
    {
        return std::nullopt;
    }
    WorkerInfo &worker = workerRegistry[workerId];
    worker.id = workerId;
    worker.name = workerName.isEmpty() ? workerId : workerName;
    worker.lastSeenMs = nowMs();
    worker.leaseId = QUuid();
    RenderJob *job = currentJob();
    if (!job)
    {
        return std::nullopt;
    }
    const ProjectInfo current = job->snapshot();
    const QFileInfo blendInfo(current.path);
    // Workers need the file and somewhere for their frames to land.
    if (!blendInfo.isFile() || resolveOutputDirectory(current).isEmpty())
    {
        return std::nullopt;
    }
    const QString key = blendKeyOf(blendInfo);
    std::optional<DependencyPacker::Pack> pack;
    if (distributed.dependencyMode == QStringLiteral("bat"))
    {
        packer->setTool(activeBlenderPath, distributed.batPath);
        switch (packer->request(current.path, key))
        {
        case DependencyPacker::State::Ready:
            pack = packer->pack(key);
            break;
        case DependencyPacker::State::Failed:
            // Already reported; workers get the bare .blend.
            break;
        default:
            // Remote work starts once the assets are collected.
            return std::nullopt;
        }
    }
    const std::optional<FrameLedger::Lease> lease = job->claimRemote(workerId, maxFrames, worker.name);
    if (!lease.has_value())
    {
        return std::nullopt;
    }
    const ProjectInfo project = job->snapshot();
    WorkAssignment assignment;
    assignment.leaseId = lease->id;
    assignment.projectId = project.id;
    assignment.projectName = project.name;
    assignment.blendPath = project.path;
    assignment.outputPath = project.outputPath;
    assignment.outputName = outputNameOf(project.outputPath);
    assignment.blendKey = key;
    assignment.blendSize = blendInfo.size();
    if (pack.has_value())
    {
        assignment.packed = true;
        assignment.mainFile = pack->mainFile;
    }
    assignment.frameStart = job->frameAt(lease->firstIndex);
    assignment.frameEnd = job->frameAt(lease->firstIndex + lease->count - 1);
    assignment.frameStep = project.frameStep;
    assignment.heartbeatSeconds = std::max(1, distributed.leaseTimeoutSeconds / 4);
    worker.leaseId = lease->id;
    worker.leaseFrameStart = assignment.frameStart;
    worker.leaseFrameEnd = assignment.frameEnd;
    emit outputTextUpdated(
        tr("Worker %1 took frames %2-%3")
            .arg(worker.name)
            .arg(assignment.frameStart)
            .arg(assignment.frameEnd)
    );
    return assignment;
}

bool RenderQueue::reportWork(const QUuid &leaseId, int finishedCount)
{
    RenderJob *job = shuttingDown ? nullptr : currentJob();
    if (!job || !job->reportRemote(leaseId, finishedCount))
    {
        return false;
    }
    if (WorkerInfo *worker = workerForLease(leaseId))
    {
        worker->lastSeenMs = nowMs();
    }
    return true;
}

bool RenderQueue::completeWork(const QUuid &leaseId)
{
    RenderJob *job = shuttingDown ? nullptr : currentJob();
    WorkerInfo *worker = workerForLease(leaseId);
    // completeRemote can finish the job and advance the queue, so capture
    // everything needed before calling it.
    if (!job || !job->completeRemote(leaseId))
    {
        return false;
    }
    if (worker)
    {
        worker->lastSeenMs = nowMs();
        worker->leaseId = QUuid();
        ++worker->completedLeases;
    }
    return true;
}

bool RenderQueue::failWork(const QUuid &leaseId, const QString &message)
{
    RenderJob *job = shuttingDown ? nullptr : currentJob();
    WorkerInfo *worker = workerForLease(leaseId);
    const QString workerName = worker ? worker->name : tr("unknown");
    if (!job || !job->failRemote(leaseId))
    {
        return false;
    }
    if (worker)
    {
        worker->lastSeenMs = nowMs();
        worker->leaseId = QUuid();
    }
    emit outputTextUpdated(tr("Worker %1 failed: %2").arg(workerName, message));
    return true;
}

bool RenderQueue::releaseWork(const QUuid &leaseId, int finishedCount)
{
    RenderJob *job = shuttingDown ? nullptr : currentJob();
    WorkerInfo *worker = workerForLease(leaseId);
    const QString workerName = worker ? worker->name : tr("unknown");
    if (!job || !job->releaseRemote(leaseId, finishedCount))
    {
        return false;
    }
    if (worker)
    {
        worker->lastSeenMs = nowMs();
        worker->leaseId = QUuid();
    }
    emit outputTextUpdated(tr("Worker %1 is busy and returned its frames").arg(workerName));
    return true;
}

bool RenderQueue::offersRemoteWork() const
{
    const RenderJob *job = shuttingDown || !distributed.enabled ? nullptr : currentJob();
    if (!job || job->pendingFrames() <= 0)
    {
        return false;
    }
    const ProjectInfo project = job->snapshot();
    return QFileInfo(project.path).isFile() && !resolveOutputDirectory(project).isEmpty();
}

std::optional<LeaseFiles> RenderQueue::leaseFiles(const QUuid &leaseId)
{
    RenderJob *job = shuttingDown ? nullptr : currentJob();
    // Reporting zero frames renews the lease without changing progress.
    if (!job || !job->reportRemote(leaseId, 0))
    {
        return std::nullopt;
    }
    if (WorkerInfo *worker = workerForLease(leaseId))
    {
        worker->lastSeenMs = nowMs();
    }
    const ProjectInfo project = job->snapshot();
    LeaseFiles files {project.path, resolveOutputDirectory(project), std::nullopt};
    if (distributed.dependencyMode == QStringLiteral("bat"))
    {
        files.pack = packer->pack(blendKeyOf(QFileInfo(project.path)));
    }
    return files;
}

QString RenderQueue::blendKey(const QString &path)
{
    return blendKeyOf(QFileInfo(path));
}

std::vector<WorkerInfo> RenderQueue::workers() const
{
    std::vector<WorkerInfo> result;
    const RenderJob *job = currentJob();
    for (WorkerInfo worker : workerRegistry)
    {
        worker.msPerFrame = job ? job->msPerFrame(worker.id) : -1.0;
        result.push_back(worker);
    }
    std::sort(result.begin(), result.end(), [](const WorkerInfo &left, const WorkerInfo &right) {
        return left.name < right.name;
    });
    return result;
}

void RenderQueue::onLeaseTimer()
{
    if (shuttingDown)
    {
        return;
    }
    const qint64 now = nowMs();
    for (auto iterator = workerRegistry.begin(); iterator != workerRegistry.end();)
    {
        if (now - iterator->lastSeenMs > workerForgetMs)
        {
            iterator = workerRegistry.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
    RenderJob *job = currentJob();
    if (!job)
    {
        return;
    }
    const std::vector<FrameLedger::Lease> expired =
        job->expireRemoteLeases(static_cast<qint64>(distributed.leaseTimeoutSeconds) * 1000);
    for (const FrameLedger::Lease &lease : expired)
    {
        if (WorkerInfo *worker = workerForLease(lease.id))
        {
            worker->leaseId = QUuid();
        }
        emit outputTextUpdated(tr("Lease for worker %1 expired, frames returned to the queue").arg(lease.workerId));
    }
}

RenderJob *RenderQueue::currentJob() const
{
    return rendering ? findJob(currentRenderingId) : nullptr;
}

WorkerInfo *RenderQueue::workerForLease(const QUuid &leaseId)
{
    for (WorkerInfo &worker : workerRegistry)
    {
        if (worker.leaseId == leaseId)
        {
            return &worker;
        }
    }
    return nullptr;
}

void RenderQueue::onJobSnapshotChanged(ProjectInfo snapshot)
{
    if (shuttingDown)
    {
        return;
    }
    rebuildSnapshotCache();
    if (snapshot.state != ProjectState::Loading)
    {
        persistSnapshot(snapshot);
    }
    emit jobUpdated(snapshot);
    emit queueChanged();
}

void RenderQueue::onJobRenderFinished(QUuid id)
{
    if (shuttingDown || !rendering || id != currentRenderingId)
    {
        return;
    }
    currentRenderingId = QUuid();
    startNextJob();
}

RenderJob *RenderQueue::findJob(const QUuid &id) const
{
    const auto iterator = std::find_if(jobs.begin(), jobs.end(), [&id](const RenderJob *job) {
        return job->id() == id;
    });
    return iterator == jobs.end() ? nullptr : *iterator;
}

int RenderQueue::findJobIndex(const QUuid &id) const
{
    const auto iterator = std::find_if(jobs.begin(), jobs.end(), [&id](const RenderJob *job) {
        return job->id() == id;
    });
    return iterator == jobs.end() ? -1 : static_cast<int>(std::distance(jobs.begin(), iterator));
}

bool RenderQueue::startNextJob()
{
    for (RenderJob *job : jobs)
    {
        if (job->snapshot().state == ProjectState::Queued && job->startRender())
        {
            currentRenderingId = job->id();
            return true;
        }
    }
    currentRenderingId = QUuid();
    if (rendering)
    {
        rendering = false;
        emit renderingChanged(false);
    }
    emit queueChanged();
    return false;
}

void RenderQueue::rebuildSnapshotCache()
{
    std::vector<ProjectInfo> updatedSnapshots;
    updatedSnapshots.reserve(jobs.size());
    for (const RenderJob *job : jobs)
    {
        updatedSnapshots.push_back(job->snapshot());
    }
    QWriteLocker locker(&snapshotLock);
    snapshotCache = std::move(updatedSnapshots);
}

QString RenderQueue::persistenceFilePath() const
{
    return QDir(persistenceDirectory).filePath(QStringLiteral("projects.json"));
}

void RenderQueue::persistSnapshot(const ProjectInfo &snapshot)
{
    persistedSnapshots.insert(snapshot.id, snapshot);
    writePersistence();
}

bool RenderQueue::writePersistence() const
{
    QDir directory(persistenceDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        qWarning() << "Cannot create render queue persistence directory:" << persistenceDirectory;
        return false;
    }
    QJsonArray projects;
    for (const RenderJob *job : jobs)
    {
        const auto iterator = persistedSnapshots.constFind(job->id());
        if (iterator != persistedSnapshots.cend())
        {
            projects.append(iterator.value().toJson());
        }
    }
    const QJsonObject root = {
        {QStringLiteral("schemaVersion"), persistenceSchemaVersion},
        {QStringLiteral("projects"), projects}
    };
    QSaveFile file(persistenceFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "Cannot write render queue persistence file:" << file.fileName();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        qWarning() << "Cannot commit render queue persistence file:" << file.fileName();
        return false;
    }
    return true;
}
