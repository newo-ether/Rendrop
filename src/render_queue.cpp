// render_queue.cpp

#include <algorithm>
#include <cmath>
#include <utility>

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
#include <QWriteLocker>

#include "app_paths.h"
#include "render_queue.h"
#include "render_job.h"

namespace {
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
}

RenderQueue::RenderQueue(QObject *parent):
    RenderQueue(AppPaths::configDirectory(), parent)
{
}

RenderQueue::RenderQueue(const QString &persistenceDirectory, QObject *parent):
    QObject(parent),
    persistenceDirectory(persistenceDirectory)
{
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
    if ((rootSchemaVersion != 1 && rootSchemaVersion != 2)
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
        {QStringLiteral("schemaVersion"), 2},
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
