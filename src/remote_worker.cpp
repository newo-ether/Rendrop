// remote_worker.cpp

#include <algorithm>
#include <atomic>
#include <chrono>

#include "httplib.h"
#include "nlohmann/json.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QPair>
#include <QThread>
#include <QUrl>
#include <QUuid>

#include "blender_renderer.h"
#include "remote_worker.h"
#include "worker_auth.h"

using nlohmann::json;

namespace {

QString fromUtf8(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string utf8(const QString &value)
{
    return value.toUtf8().toStdString();
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// Names and paths come from the coordinator; only accept values that cannot
// climb out of the worker's own directories.
bool isPlainName(const QString &name)
{
    return !name.isEmpty()
        && name != QStringLiteral(".")
        && name != QStringLiteral("..")
        && !name.contains(QLatin1Char('/'))
        && !name.contains(QLatin1Char('\\'))
        && !name.contains(QLatin1Char(':'));
}

bool isRelativeFilePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')))
    {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'));
    return std::all_of(parts.begin(), parts.end(), isPlainName);
}

bool isHexKey(const QString &key)
{
    if (key.isEmpty() || key.size() > 64)
    {
        return false;
    }
    return std::all_of(key.begin(), key.end(), [](QChar c) {
        return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
    });
}

// Percent-encodes every segment of a '/' separated path.
std::string encodePath(const QString &path)
{
    QStringList parts = path.split(QLatin1Char('/'));
    for (QString &part : parts)
    {
        part = QString::fromLatin1(QUrl::toPercentEncoding(part));
    }
    return utf8(parts.join(QLatin1Char('/')));
}

bool isFatal(int status)
{
    return status == 401 || status == 403;
}

bool isRevoked(int status)
{
    return status == 410 || isFatal(status);
}

std::unique_ptr<httplib::Client> makeClient(const RemoteWorker::Config &config)
{
    auto client = std::make_unique<httplib::Client>(config.host, config.port);
    client->set_connection_timeout(std::chrono::seconds(5));
    client->set_read_timeout(std::chrono::seconds(30));
    client->set_write_timeout(std::chrono::seconds(30));
    // Paths are percent-encoded by us; httplib's own encoding is partial.
    client->set_path_encode(false);
    return client;
}

void touch(const QString &directory)
{
    QFile marker(QDir(directory).filePath(QStringLiteral(".used")));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        marker.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
    }
}

// Keeps the current project and the most recently used other one.
void pruneCache(const QDir &blendRoot, const QString &currentKey)
{
    touch(blendRoot.filePath(currentKey));
    QFileInfoList entries = blendRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    const auto lastUsed = [](const QFileInfo &entry) {
        return QFileInfo(QDir(entry.filePath()).filePath(QStringLiteral(".used"))).lastModified();
    };
    std::sort(entries.begin(), entries.end(), [&](const QFileInfo &left, const QFileInfo &right) {
        return lastUsed(left) > lastUsed(right);
    });
    int kept = 0;
    for (const QFileInfo &entry : entries)
    {
        if (entry.fileName() != currentKey && ++kept > 1)
        {
            QDir(entry.filePath()).removeRecursively();
        }
    }
}

}

struct RemoteWorker::Assignment
{
    std::string leaseId;
    QString projectName;
    QString blendFileName;
    QString outputName;
    QString blendKey;
    qint64 blendSize = 0;
    bool packed = false;
    QString mainFile;
    int frameStart = 0;
    int frameEnd = 0;
    int frameStep = 0;
    int heartbeatSeconds = 10;

    bool parse(const std::string &body)
    {
        const json value = json::parse(body, nullptr, false);
        if (value.is_discarded() || !value.is_object())
        {
            return false;
        }
        try
        {
            leaseId = value.at("leaseId").get<std::string>();
            projectName = fromUtf8(value.value("projectName", std::string()));
            blendFileName = QFileInfo(QDir::fromNativeSeparators(fromUtf8(value.at("path").get<std::string>()))).fileName();
            outputName = fromUtf8(value.value("outputName", std::string()));
            blendKey = fromUtf8(value.at("blendKey").get<std::string>());
            blendSize = value.at("blendSize").get<qint64>();
            packed = value.value("packed", false);
            mainFile = fromUtf8(value.value("mainFile", std::string()));
            frameStart = value.at("frameStart").get<int>();
            frameEnd = value.at("frameEnd").get<int>();
            frameStep = value.at("frameStep").get<int>();
            heartbeatSeconds = std::max(1, value.value("heartbeatSeconds", 10));
        }
        catch (...)
        {
            return false;
        }
        // An empty output name is valid: Blender then writes "0001.png" etc.
        return !QUuid(fromUtf8(leaseId)).isNull()
            && isPlainName(blendFileName)
            && (outputName.isEmpty() || isPlainName(outputName))
            && isHexKey(blendKey)
            && blendSize >= 0
            && (!packed || isRelativeFilePath(mainFile))
            && frameStep > 0
            && frameEnd >= frameStart;
    }

    std::string path(const char *action) const
    {
        return "/leases/" + leaseId + action;
    }
};

// Uploads files Blender has finished writing. A file counts as settled once
// its size and time stamp held still for a full scan interval and it is at
// least a second old; the final pass after Blender exits takes everything.
// Anything that changes after an upload is sent again.
class RemoteWorker::Uploader
{
public:
    Uploader(RemoteWorker &worker, const Assignment &assignment, const QString &directory):
        worker(worker),
        assignment(assignment),
        directory(directory)
    {
    }

    // 200 when nothing is left to upload, 202 when some files are still
    // being written, otherwise the failing status.
    HttpStatus uploadSettled(bool final)
    {
        bool pending = false;
        const QFileInfoList files = QDir(directory).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        const QDateTime now = QDateTime::currentDateTime();
        for (const QFileInfo &info : files)
        {
            const Stamp stamp {info.size(), info.lastModified()};
            const QString name = info.fileName();
            if (uploaded.value(name) == stamp)
            {
                continue;
            }
            const bool settled = observed.value(name) == stamp && stamp.second.msecsTo(now) >= 1000;
            observed.insert(name, stamp);
            if (!final && !settled)
            {
                pending = true;
                continue;
            }
            const HttpStatus status = worker.uploadFile(assignment, info.filePath(), name);
            if (status != 200)
            {
                return status;
            }
            uploaded.insert(name, stamp);
        }
        return pending ? 202 : 200;
    }

    int uploadedCount() const
    {
        return static_cast<int>(uploaded.size());
    }

private:
    using Stamp = QPair<qint64, QDateTime>;
    RemoteWorker &worker;
    const Assignment &assignment;
    QString directory;
    QHash<QString, Stamp> observed;
    QHash<QString, Stamp> uploaded;
};

RemoteWorker::RemoteWorker(const Config &config):
    settings(config),
    client(makeClient(config)),
    heartbeatClient(makeClient(config))
{
}

RemoteWorker::~RemoteWorker() = default;

void RemoteWorker::setLogger(Logger newLogger)
{
    logger = std::move(newLogger);
}

void RemoteWorker::requestYield()
{
    yielding = true;
}

void RemoteWorker::clearYield()
{
    yielding = false;
}

bool RemoteWorker::yieldRequested() const
{
    return yielding;
}

RemoteWorker::Status RemoteWorker::status() const
{
    QMutexLocker locker(&statusMutex);
    return current;
}

const RemoteWorker::Config &RemoteWorker::config() const
{
    return settings;
}

void RemoteWorker::log(bool error, const QString &text) const
{
    if (logger)
    {
        logger(error, text);
    }
}

void RemoteWorker::setStatus(const Status &status)
{
    QMutexLocker locker(&statusMutex);
    current = status;
}

void RemoteWorker::setPhase(const QString &phase, int finished)
{
    QMutexLocker locker(&statusMutex);
    current.phase = phase;
    if (finished >= 0)
    {
        current.finished = finished;
    }
}

std::string RemoteWorker::authorization(
    const char *method,
    const std::string &path,
    QByteArray *nonce,
    const QByteArray &contentHash
) const
{
    const WorkerAuth::SignedRequest request = WorkerAuth::signRequest(
        settings.token,
        QByteArray(method),
        QByteArray::fromStdString(path),
        contentHash
    );
    if (nonce)
    {
        *nonce = request.nonce;
    }
    return request.header;
}

RemoteWorker::Outcome RemoteWorker::runOnce()
{
    json claimBody = {{"workerId", utf8(settings.workerId)}, {"name", utf8(settings.name)}};
    if (settings.maxFrames > 0)
    {
        claimBody["maxFrames"] = settings.maxFrames;
    }
    QByteArray nonce;
    const std::string claimPath = "/workers/claim";
    const httplib::Headers headers = {{"Authorization", authorization("POST", claimPath, &nonce)}};
    const auto result = client->Post(claimPath, headers, claimBody.dump(), "application/json");
    if (!result)
    {
        return Outcome::Unreachable;
    }
    if (isFatal(result->status))
    {
        log(true, QStringLiteral("Coordinator refused this worker (HTTP %1)").arg(result->status));
        return Outcome::Rejected;
    }
    if (result->status != 200 && result->status != 204)
    {
        log(true, QStringLiteral("Unexpected claim response HTTP %1").arg(result->status));
        return Outcome::Unreachable;
    }
    // Only a machine holding the token can sign the answer to our nonce.
    const QByteArray expected = WorkerAuth::responseSignature(settings.token, nonce, QByteArray::fromStdString(result->body));
    if (!WorkerAuth::constantTimeEquals(expected, QByteArray::fromStdString(result->get_header_value("X-Rendrop-Response"))))
    {
        log(true, QStringLiteral("Claim response is not signed with the group token, ignoring this coordinator"));
        return Outcome::Rejected;
    }
    if (result->status == 204)
    {
        return Outcome::NoWork;
    }
    Assignment assignment;
    if (!assignment.parse(result->body))
    {
        log(true, QStringLiteral("Malformed assignment from coordinator"));
        return Outcome::Unreachable;
    }
    Outcome outcome = Outcome::Worked;
    renderLease(assignment, outcome);
    setStatus(Status());
    return outcome;
}

bool RemoteWorker::renderLease(const Assignment &assignment, Outcome &outcome)
{
    log(false, QStringLiteral("Lease %1: %2 frames %3-%4 step %5")
        .arg(fromUtf8(assignment.leaseId), assignment.projectName)
        .arg(assignment.frameStart)
        .arg(assignment.frameEnd)
        .arg(assignment.frameStep));
    Status status;
    status.active = true;
    status.projectName = assignment.projectName;
    status.frameStart = assignment.frameStart;
    status.frameEnd = assignment.frameEnd;
    status.total = (assignment.frameEnd - assignment.frameStart) / assignment.frameStep + 1;
    status.phase = QStringLiteral("downloading");
    setStatus(status);

    QString blendPath;
    const HttpStatus downloaded = assignment.packed ? ensurePack(assignment, blendPath) : ensureBlend(assignment, blendPath);
    if (yielding)
    {
        post(*client, assignment, "/release", QByteArray("{\"finishedCount\":0}"));
        log(false, QStringLiteral("Machine busy, lease handed back"));
        outcome = Outcome::Yielded;
        return true;
    }
    if (downloaded != 200)
    {
        return finishFailed(assignment, downloaded, QStringLiteral("cannot download project files (HTTP %1)").arg(downloaded), outcome);
    }
    const QString outputDirectory = QDir(settings.cacheDirectory).filePath(QStringLiteral("out/") + fromUtf8(assignment.leaseId));
    QDir(outputDirectory).removeRecursively();
    if (!QDir().mkpath(outputDirectory))
    {
        return finishFailed(assignment, 0, QStringLiteral("cannot create local output directory"), outcome);
    }
    const bool result = renderInto(assignment, blendPath, outputDirectory, outcome);
    QDir(outputDirectory).removeRecursively();
    return result;
}

bool RemoteWorker::renderInto(const Assignment &assignment, const QString &blendPath, const QString &outputDirectory, Outcome &outcome)
{
    BlenderRenderer renderer;
    std::atomic_int exitStatus(1000);
    // No context object: the lambda runs directly on the render thread.
    QObject::connect(&renderer, &BlenderRenderer::finishedRendering, [&exitStatus](int value) {
        exitStatus = value;
    });
    renderer.setParameter(
        settings.blenderPath,
        blendPath,
        assignment.frameStart,
        assignment.frameEnd,
        assignment.frameStep,
        true
    );
    renderer.setOutputPath(QDir::fromNativeSeparators(outputDirectory) + QLatin1Char('/') + assignment.outputName);
    renderer.prepareStart();
    renderer.start(QThread::LowestPriority);
    setPhase(QStringLiteral("rendering"), 0);

    Uploader uploader(*this, assignment, outputDirectory);
    int reported = -1;
    QElapsedTimer heartbeat;
    heartbeat.start();
    while (!renderer.wait(QDeadlineTimer(200)))
    {
        QCoreApplication::processEvents();
        if (yielding)
        {
            // Frames whose files already reached the coordinator stay done;
            // a frame interrupted mid-write is never uploaded.
            renderer.stop();
            renderer.wait();
            const QByteArray body = QByteArray::fromStdString(json({{"finishedCount", std::max(reported, 0)}}).dump());
            const HttpStatus status = post(*client, assignment, "/release", body);
            log(false, QStringLiteral("Machine busy, handed back the lease after %1 frame(s)").arg(std::max(reported, 0)));
            outcome = isFatal(status) ? Outcome::Rejected : Outcome::Yielded;
            return true;
        }
        // Read the count before scanning: every frame counted here has
        // already been saved, so its files are part of the scan.
        const int finished = renderer.getFinishedFrame();
        HttpStatus status = uploader.uploadSettled(false);
        const bool allUploaded = status == 200;
        const bool due = heartbeat.elapsed() >= assignment.heartbeatSeconds * 1000LL;
        if (!isRevoked(status) && ((allUploaded && finished != reported) || due))
        {
            // Progress only counts frames whose files the coordinator holds.
            const int count = allUploaded ? finished : std::max(reported, 0);
            status = post(*client, assignment, "/progress", QByteArray::fromStdString(json({{"finishedCount", count}}).dump()));
            if (status == 200)
            {
                if (count != reported && count > 0)
                {
                    log(false, QStringLiteral("Uploaded %1 frame(s)").arg(count));
                }
                reported = count;
                setPhase(QStringLiteral("rendering"), count);
                heartbeat.restart();
            }
        }
        if (isRevoked(status))
        {
            log(true, QStringLiteral("Lease revoked by coordinator, stopping Blender"));
            renderer.stop();
            renderer.wait();
            outcome = isFatal(status) ? Outcome::Rejected : Outcome::Worked;
            return true;
        }
    }
    QCoreApplication::processEvents();

    if (exitStatus != 0)
    {
        return finishFailed(assignment, 0, QStringLiteral("Blender exited before finishing the range"), outcome);
    }
    setPhase(QStringLiteral("uploading"));
    HttpStatus status = uploader.uploadSettled(true);
    if (status != 200)
    {
        return finishFailed(assignment, status, QStringLiteral("frame upload failed (HTTP %1)").arg(status), outcome);
    }
    status = post(*client, assignment, "/progress", QByteArray::fromStdString(json({{"finishedCount", renderer.getTotalFrame()}}).dump()));
    if (status == 200)
    {
        status = post(*client, assignment, "/complete", QByteArray("{}"));
    }
    log(status != 200, status == 200
        ? QStringLiteral("Lease complete (%1 file(s) uploaded)").arg(uploader.uploadedCount())
        : QStringLiteral("Coordinator did not accept completion (HTTP %1)").arg(status));
    if (isFatal(status))
    {
        outcome = Outcome::Rejected;
    }
    return true;
}

RemoteWorker::HttpStatus RemoteWorker::post(httplib::Client &target, const Assignment &assignment, const char *action, const QByteArray &body)
{
    const std::string path = assignment.path(action);
    const httplib::Headers headers = {{"Authorization", authorization("POST", path)}};
    const auto result = target.Post(path, headers, body.toStdString(), "application/json");
    return result ? result->status : -1;
}

bool RemoteWorker::finishFailed(const Assignment &assignment, HttpStatus status, const QString &message, Outcome &outcome)
{
    log(true, message);
    if (isRevoked(status))
    {
        outcome = isFatal(status) ? Outcome::Rejected : Outcome::Worked;
        return false;
    }
    const HttpStatus reply = post(*client, assignment, "/fail", QByteArray::fromStdString(json({{"message", utf8(message)}}).dump()));
    outcome = isFatal(reply) ? Outcome::Rejected : Outcome::Worked;
    return false;
}

// Keeps the lease alive from a second connection while a long transfer holds
// the main one.
void RemoteWorker::heartbeatIfDue(const Assignment &assignment, qint64 &lastBeatMs)
{
    if (nowMs() - lastBeatMs >= assignment.heartbeatSeconds * 1000LL)
    {
        post(*heartbeatClient, assignment, "/progress", QByteArray("{\"finishedCount\":0}"));
        lastBeatMs = nowMs();
    }
}

// Streams path into target via a .part file. 200 on success. A file whose
// content hash does not match is fetched once more before giving up; the
// coordinator's copy may have changed between hashing and sending.
RemoteWorker::HttpStatus RemoteWorker::download(const std::string &path, const QString &target, const Assignment &assignment, QString *servedKey)
{
    HttpStatus status = 0;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        bool mismatch = false;
        status = downloadOnce(path, target, assignment, servedKey, mismatch);
        if (!mismatch || yielding)
        {
            break;
        }
        log(true, QStringLiteral("Content hash mismatch on %1, downloading again").arg(QFileInfo(target).fileName()));
    }
    return status;
}

RemoteWorker::HttpStatus RemoteWorker::downloadOnce(
    const std::string &path,
    const QString &target,
    const Assignment &assignment,
    QString *servedKey,
    bool &mismatch
)
{
    mismatch = false;
    const QString partPath = target + QStringLiteral(".part");
    QDir().mkpath(QFileInfo(target).absolutePath());
    QFile part(partPath);
    if (!part.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return 0;
    }
    HttpStatus status = -1;
    qint64 expected = -1;
    bool writeFailed = false;
    bool forged = false;
    QByteArray expectedHash;
    QCryptographicHash hash(WorkerAuth::contentHashAlgorithm);
    qint64 lastBeat = nowMs();
    const std::string decoded = QUrl::fromPercentEncoding(QByteArray::fromStdString(path)).toUtf8().toStdString();
    QByteArray nonce;
    const httplib::Headers headers = {{"Authorization", authorization("GET", decoded, &nonce)}};
    const auto result = client->Get(
        path,
        headers,
        [&](const httplib::Response &response) {
            status = response.status;
            if (status == 200)
            {
                if (servedKey)
                {
                    const QString served = fromUtf8(response.get_header_value("X-Rendrop-Blend-Key"));
                    if (isHexKey(served))
                    {
                        *servedKey = served;
                    }
                }
                if (response.has_header("Content-Length"))
                {
                    expected = static_cast<qint64>(response.get_header_value_u64("Content-Length"));
                }
                // Older coordinators send no hash; then only the length is checked.
                const QByteArray served = QByteArray::fromStdString(response.get_header_value(WorkerAuth::contentHashHeader));
                if (!served.isEmpty())
                {
                    const QByteArray signature = QByteArray::fromStdString(response.get_header_value("X-Rendrop-Response"));
                    if (!WorkerAuth::isContentHash(served) || !WorkerAuth::constantTimeEquals(
                            WorkerAuth::fileSignature(settings.token, nonce, served), signature))
                    {
                        forged = true;
                        return false;
                    }
                    expectedHash = served;
                }
            }
            return status == 200;
        },
        [&](const char *data, size_t length) {
            if (yielding)
            {
                return false;
            }
            if (part.write(data, static_cast<qint64>(length)) != static_cast<qint64>(length))
            {
                writeFailed = true;
                return false;
            }
            hash.addData(QByteArrayView(data, static_cast<qsizetype>(length)));
            heartbeatIfDue(assignment, lastBeat);
            return true;
        }
    );
    const qint64 received = part.size();
    part.close();
    if (!result && status == 200)
    {
        status = -1;
    }
    if (forged)
    {
        log(true, QStringLiteral("File hash is not signed with the group token, ignoring the download"));
        status = 0;
    }
    if (status == 200 && (writeFailed || expected < 0 || received != expected))
    {
        status = 0;
    }
    if (status == 200 && !expectedHash.isEmpty() && hash.result().toHex() != expectedHash)
    {
        mismatch = true;
        status = 0;
    }
    if (status != 200)
    {
        QFile::remove(partPath);
        return status;
    }
    QFile::remove(target);
    if (!QFile::rename(partPath, target))
    {
        QFile::remove(partPath);
        return 0;
    }
    return 200;
}

// Downloads the .blend unless an identical copy is cached. The cache key is
// the coordinator's path, size and modification time.
RemoteWorker::HttpStatus RemoteWorker::ensureBlend(const Assignment &assignment, QString &blendPath)
{
    const QDir blendRoot(QDir(settings.cacheDirectory).filePath(QStringLiteral("blend")));
    blendPath = QDir(blendRoot.filePath(assignment.blendKey)).filePath(assignment.blendFileName);
    if (QFileInfo(blendPath).isFile() && QFileInfo(blendPath).size() == assignment.blendSize)
    {
        log(false, QStringLiteral("Using cached %1").arg(assignment.blendFileName));
        touch(blendRoot.filePath(assignment.blendKey));
        return 200;
    }
    log(false, QStringLiteral("Downloading %1 (%2 MiB)")
        .arg(assignment.blendFileName)
        .arg(assignment.blendSize / (1024.0 * 1024.0), 0, 'f', 1));
    const QString staging = QDir(settings.cacheDirectory).filePath(QStringLiteral("download-") + fromUtf8(assignment.leaseId));
    QString key = assignment.blendKey;
    const HttpStatus status = download(assignment.path("/blend"), staging, assignment, &key);
    if (status != 200)
    {
        return status;
    }
    const QString keyDirectory = blendRoot.filePath(key);
    blendPath = QDir(keyDirectory).filePath(assignment.blendFileName);
    QDir(keyDirectory).removeRecursively();
    if (!QDir().mkpath(keyDirectory) || !QFile::rename(staging, blendPath))
    {
        QFile::remove(staging);
        return 0;
    }
    pruneCache(blendRoot, key);
    return 200;
}

// Downloads the packed file set into <cache>/blend/<key>/, reusing files that
// are already present with the right size.
RemoteWorker::HttpStatus RemoteWorker::ensurePack(const Assignment &assignment, QString &blendPath)
{
    QByteArray nonce;
    const std::string path = assignment.path("/manifest");
    const httplib::Headers headers = {{"Authorization", authorization("GET", path, &nonce)}};
    const auto result = client->Get(path, headers);
    if (!result)
    {
        return -1;
    }
    if (result->status != 200)
    {
        return result->status;
    }
    const QByteArray expected = WorkerAuth::responseSignature(settings.token, nonce, QByteArray::fromStdString(result->body));
    if (!WorkerAuth::constantTimeEquals(expected, QByteArray::fromStdString(result->get_header_value("X-Rendrop-Response"))))
    {
        log(true, QStringLiteral("Manifest is not signed with the group token"));
        return 0;
    }
    const json manifest = json::parse(result->body, nullptr, false);
    if (manifest.is_discarded() || !manifest.is_object() || !manifest.contains("files") || !manifest["files"].is_array())
    {
        return 0;
    }
    const QString key = fromUtf8(manifest.value("key", std::string()));
    const QString main = fromUtf8(manifest.value("main", std::string()));
    if (!isHexKey(key) || !isRelativeFilePath(main))
    {
        return 0;
    }
    const QDir blendRoot(QDir(settings.cacheDirectory).filePath(QStringLiteral("blend")));
    const QDir root(blendRoot.filePath(key));
    qint64 totalBytes = 0;
    QList<QPair<QString, qint64>> files;
    for (const json &entry : manifest["files"])
    {
        if (!entry.is_object() || !entry.contains("path") || !entry["path"].is_string() || !entry.contains("size"))
        {
            return 0;
        }
        const QString relative = fromUtf8(entry["path"].get<std::string>());
        const qint64 size = entry["size"].get<qint64>();
        if (!isRelativeFilePath(relative) || size < 0)
        {
            return 0;
        }
        files.append({relative, size});
        totalBytes += size;
    }
    log(false, QStringLiteral("Project uses %1 file(s), %2 MiB").arg(files.size()).arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 1));
    for (const auto &[relative, size] : files)
    {
        const QString target = root.filePath(relative);
        if (QFileInfo(target).isFile() && QFileInfo(target).size() == size)
        {
            continue;
        }
        const HttpStatus status = download(assignment.path("/files/") + encodePath(relative), target, assignment, nullptr);
        if (status != 200 || yielding)
        {
            return status;
        }
        if (QFileInfo(target).size() != size)
        {
            return 0;
        }
    }
    pruneCache(blendRoot, key);
    blendPath = root.filePath(main);
    return 200;
}

RemoteWorker::HttpStatus RemoteWorker::uploadFile(const Assignment &assignment, const QString &filePath, const QString &name)
{
    const std::string signedPath = assignment.path("/frames/") + utf8(name);
    const std::string path = assignment.path("/frames/") + encodePath(name);
    HttpStatus status = -1;
    // Transient failures (network, busy queue, a corrupted transfer) get a
    // few retries.
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        // Hashed per attempt: the coordinator compares against the bytes
        // read below, and the file is settled, so both see the same content.
        const QByteArray contentHash = WorkerAuth::hashFile(filePath);
        auto file = std::make_shared<QFile>(filePath);
        if (contentHash.isEmpty() || !file->open(QIODevice::ReadOnly))
        {
            return 0;
        }
        const qint64 size = file->size();
        const httplib::Headers headers = {
            {"Authorization", authorization("PUT", signedPath, nullptr, contentHash)},
            {WorkerAuth::contentHashHeader, contentHash.toStdString()}
        };
        const auto result = client->Put(
            path,
            headers,
            static_cast<size_t>(size),
            [file](size_t offset, size_t length, httplib::DataSink &sink) {
                constexpr size_t chunk = 1024 * 1024;
                if (!file->seek(static_cast<qint64>(offset)))
                {
                    return false;
                }
                const QByteArray bytes = file->read(static_cast<qint64>(std::min(length, chunk)));
                return !bytes.isEmpty() && sink.write(bytes.constData(), static_cast<size_t>(bytes.size()));
            },
            "application/octet-stream"
        );
        status = result ? result->status : -1;
        if (status == 422)
        {
            log(true, QStringLiteral("Content hash mismatch on %1, uploading again").arg(name));
        }
        else if (status == 200 || (status >= 400 && status < 500))
        {
            break;
        }
        QThread::sleep(1);
    }
    if (status == 200)
    {
        log(false, QStringLiteral("Uploaded %1").arg(name));
    }
    return status;
}
