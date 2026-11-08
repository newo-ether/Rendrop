// http_server.cpp

#include "http_server.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QMetaObject>
#include <QString>
#include <QUuid>

#include "nlohmann/json.hpp"
#include "load_monitor.h"
#include "project_info.h"
#include "render_queue.h"

using nlohmann::json;

namespace {

std::string toUtf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString fromUtf8(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string uuidText(const QUuid &id)
{
    return toUtf8(id.toString(QUuid::WithoutBraces));
}

json rangesToJson(const std::vector<std::pair<int, int>> &ranges)
{
    json array = json::array();
    for (const auto &range : ranges)
    {
        array.push_back(json::array({range.first, range.second}));
    }
    return array;
}

json projectToJson(const ProjectInfo &project)
{
    // Ranges are frame indices: frame = frameStart + index * frameStep.
    // Completed frames missing from frameOwners were rendered here.
    json owners = json::array();
    for (const FrameOwnerRanges &entry : project.frameOwners)
    {
        owners.push_back({{"node", toUtf8(entry.owner)}, {"ranges", rangesToJson(entry.ranges)}});
    }
    // Frames being rendered now; an empty node is this machine.
    json leases = json::array();
    for (const FrameOwnerRanges &entry : project.frameLeases)
    {
        leases.push_back({{"node", toUtf8(entry.owner)}, {"ranges", rangesToJson(entry.ranges)}});
    }
    return {
        {"completedRanges", rangesToJson(project.completedRanges)},
        {"frameOwners", owners},
        {"frameLeases", leases},
        {"id", uuidText(project.id)},
        {"state", toUtf8(ProjectStateToString(project.state))},
        {"name", toUtf8(project.name)},
        {"path", toUtf8(project.path)},
        {"outputPath", toUtf8(project.outputPath)},
        {"frameStart", project.frameStart},
        {"frameEnd", project.frameEnd},
        {"frameStep", project.frameStep},
        {"resolutionX", project.resolutionX},
        {"resolutionY", project.resolutionY},
        {"resolutionScale", project.resolutionScale},
        {"renderEngine", toUtf8(project.renderEngine)},
        {"finishedFrame", project.finishedFrame},
        {"totalFrame", project.totalFrame}
    };
}

void setJson(httplib::Response &response, int status, const json &body)
{
    response.status = status;
    response.set_content(body.dump(4), "application/json");
}

void setJsonError(httplib::Response &response, int status, const char *message)
{
    setJson(response, status, json({{"error", message}}));
}

void setAccepted(httplib::Response &response, json body = json::object())
{
    body["accepted"] = true;
    setJson(response, 202, body);
}

bool parseJsonObject(const httplib::Request &request, httplib::Response &response, json &body)
{
    body = json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object())
    {
        setJsonError(response, 400, "invalid JSON object");
        return false;
    }
    return true;
}

bool parseProjectId(const httplib::Request &request, httplib::Response &response, QUuid &id)
{
    id = QUuid(fromUtf8(request.matches[1].str()));
    if (id.isNull())
    {
        setJsonError(response, 400, "invalid project id");
        return false;
    }
    return true;
}

constexpr size_t maxWorkerFieldLength = 128;

// Runs function(queue) on the queue's thread and waits for the result. The
// queue is single-threaded; this keeps every worker call serialized with the
// GUI. Times out instead of blocking forever so shutdown cannot deadlock.
template <typename Result, typename Function>
std::optional<Result> callInQueue(RenderQueue *queue, Function function)
{
    auto promise = std::make_shared<std::promise<Result>>();
    std::future<Result> future = promise->get_future();
    QMetaObject::invokeMethod(
        queue,
        [queue, promise, function]() { promise->set_value(function(queue)); },
        Qt::QueuedConnection
    );
    if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
    {
        return std::nullopt;
    }
    try
    {
        return future.get();
    }
    catch (...)
    {
        // The queue was destroyed before the call ran.
        return std::nullopt;
    }
}

bool constantTimeEquals(const std::string &left, const std::string &right)
{
    unsigned char difference = left.size() == right.size() ? 0 : 1;
    const size_t length = std::max(left.size(), right.size());
    for (size_t index = 0; index < length; ++index)
    {
        const unsigned char a = index < left.size() ? static_cast<unsigned char>(left[index]) : 0;
        const unsigned char b = index < right.size() ? static_cast<unsigned char>(right[index]) : 0;
        difference |= a ^ b;
    }
    return difference == 0;
}

bool parseLeaseId(const httplib::Request &request, httplib::Response &response, QUuid &id)
{
    id = QUuid(fromUtf8(request.matches[1].str()));
    if (id.isNull())
    {
        setJsonError(response, 400, "invalid lease id");
        return false;
    }
    return true;
}

bool readOptionalString(const json &body, const char *key, size_t maxLength, QString &value, httplib::Response &response)
{
    if (!body.contains(key))
    {
        return true;
    }
    if (!body[key].is_string() || body[key].get<std::string>().size() > maxLength)
    {
        setJsonError(response, 400, "invalid string field");
        return false;
    }
    value = fromUtf8(body[key].get<std::string>());
    return true;
}

bool readOptionalNonNegativeInt(const json &body, const char *key, int &value, httplib::Response &response)
{
    if (!body.contains(key))
    {
        return true;
    }
    if (!body[key].is_number_integer() && !body[key].is_number_unsigned())
    {
        setJsonError(response, 400, "invalid integer field");
        return false;
    }
    long long number = 0;
    try
    {
        number = body[key].get<long long>();
    }
    catch (...)
    {
        number = -1;
    }
    if (number < 0 || number > std::numeric_limits<int>::max())
    {
        setJsonError(response, 400, "integer field is out of range");
        return false;
    }
    value = static_cast<int>(number);
    return true;
}

json assignmentToJson(const WorkAssignment &assignment)
{
    return {
        {"leaseId", uuidText(assignment.leaseId)},
        {"projectId", uuidText(assignment.projectId)},
        {"projectName", toUtf8(assignment.projectName)},
        {"path", toUtf8(assignment.blendPath)},
        {"outputPath", toUtf8(assignment.outputPath)},
        {"outputName", toUtf8(assignment.outputName)},
        {"blendKey", toUtf8(assignment.blendKey)},
        {"blendSize", assignment.blendSize},
        {"packed", assignment.packed},
        {"mainFile", toUtf8(assignment.mainFile)},
        {"frameStart", assignment.frameStart},
        {"frameEnd", assignment.frameEnd},
        {"frameStep", assignment.frameStep},
        {"heartbeatSeconds", assignment.heartbeatSeconds}
    };
}

// Upper bound for one uploaded frame file.
constexpr qint64 maxFrameBytes = 16LL * 1024 * 1024 * 1024;

// Frame uploads may only create a plain file directly inside the output
// directory: one path segment, no drive or stream syntax, nothing Windows
// would silently rewrite (trailing dots or spaces, reserved device names).
bool isSafeFrameName(const QString &name)
{
    if (name.isEmpty() || name.size() > 200 || name == QStringLiteral(".") || name == QStringLiteral(".."))
    {
        return false;
    }
    for (const QChar character : name)
    {
        if (character.unicode() < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(character))
        {
            return false;
        }
    }
    if (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')) || name.startsWith(QLatin1Char(' ')))
    {
        return false;
    }
    static const QStringList reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")
    };
    const QString stem = name.section(QLatin1Char('.'), 0, 0).trimmed();
    return !reserved.contains(stem, Qt::CaseInsensitive);
}

void setLeaseResult(httplib::Response &response, const std::optional<bool> &result)
{
    if (!result.has_value())
    {
        setJsonError(response, 503, "render queue is busy");
    }
    else if (*result)
    {
        setJson(response, 200, json({{"ok", true}}));
    }
    else
    {
        setJsonError(response, 410, "lease is no longer valid");
    }
}

bool projectExists(const RenderQueue *renderQueue, const QUuid &id, httplib::Response &response)
{
    if (renderQueue->snapshotById(id).isNull)
    {
        setJsonError(response, 404, "not found");
        return false;
    }
    return true;
}

}

HttpServer::HttpServer(RenderQueue *renderQueue, int port, QObject *parent):
    QThread(parent),
    renderQueue(renderQueue),
    port(port)
{
}

HttpServer::~HttpServer()
{
    stop();
    wait();
}

void HttpServer::setWorkerToken(const QString &token)
{
    QMutexLocker locker(&configMutex);
    workerToken = toUtf8(token);
}

std::string HttpServer::currentWorkerToken() const
{
    QMutexLocker locker(&configMutex);
    return workerToken;
}

void HttpServer::setLoadMonitor(const LoadMonitor *monitor)
{
    loadMonitor = monitor;
}

void HttpServer::setBeaconProvider(std::function<QByteArray()> provider)
{
    // Waits for a running call: the old provider's owner may be deleted next.
    QMutexLocker locker(&configMutex);
    beaconProvider = std::move(provider);
}

void HttpServer::setAcceptRemoteWork(bool accept)
{
    acceptRemoteWork = accept;
}

int HttpServer::boundPort() const
{
    return listeningPort;
}

void HttpServer::stop()
{
    server.stop();
}

bool HttpServer::authorizeWorker(const httplib::Request &request, httplib::Response &response, QByteArray *nonce)
{
    const std::string workerToken = currentWorkerToken();
    if (workerToken.empty())
    {
        setJsonError(response, 403, "distributed rendering is disabled");
        return false;
    }
    const std::string header = request.get_header_value("Authorization");
    QByteArray requestNonce;
    const WorkerAuth::Verifier::Result signedResult = verifier.verify(
        QByteArray::fromStdString(workerToken),
        QByteArray::fromStdString(header),
        QByteArray::fromStdString(request.method),
        QByteArray::fromStdString(request.path),
        requestNonce,
        QByteArray::fromStdString(request.get_header_value(WorkerAuth::contentHashHeader))
    );
    if (signedResult == WorkerAuth::Verifier::Result::Ok)
    {
        if (nonce)
        {
            *nonce = requestNonce;
        }
        return true;
    }
    if (signedResult == WorkerAuth::Verifier::Result::Missing
        && constantTimeEquals(header, "Bearer " + workerToken))
    {
        return true;
    }
    setJsonError(response, 401, "invalid worker credentials");
    return false;
}

void HttpServer::signResponse(httplib::Response &response, const QByteArray &nonce) const
{
    if (nonce.isEmpty())
    {
        return;
    }
    const QByteArray signature = WorkerAuth::responseSignature(
        QByteArray::fromStdString(currentWorkerToken()),
        nonce,
        QByteArray::fromStdString(response.body)
    );
    response.set_header("X-Rendrop-Response", signature.toStdString());
}

QByteArray HttpServer::contentHash(const QString &path)
{
    const auto identity = [&path]() {
        const QFileInfo info(path);
        return QByteArray::number(info.size()) + '|' + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    };
    const QByteArray before = identity();
    {
        QMutexLocker locker(&hashMutex);
        const auto cached = hashCache.constFind(path);
        if (cached != hashCache.cend() && cached->first == before)
        {
            return cached->second;
        }
    }
    // Hashed outside the lock; two workers fetching the same new file both
    // pay for it once, which is cheaper than serialising every transfer.
    const QByteArray hash = WorkerAuth::hashFile(path);
    if (hash.isEmpty() || identity() != before)
    {
        return QByteArray();
    }
    QMutexLocker locker(&hashMutex);
    if (hashCache.size() > 4096)
    {
        hashCache.clear();
    }
    hashCache.insert(path, {before, hash});
    return hash;
}

bool HttpServer::serveFile(httplib::Response &response, const QString &path, const QByteArray &nonce)
{
    auto file = std::make_shared<QFile>(path);
    if (!file->open(QIODevice::ReadOnly))
    {
        return false;
    }
    // Without a nonce (bearer clients) there is nothing to sign the hash to.
    const QByteArray hash = nonce.isEmpty() ? QByteArray() : contentHash(path);
    if (!hash.isEmpty())
    {
        response.set_header(WorkerAuth::contentHashHeader, hash.toStdString());
        response.set_header("X-Rendrop-Response",
            WorkerAuth::fileSignature(QByteArray::fromStdString(currentWorkerToken()), nonce, hash).toStdString());
    }
    response.set_content_provider(
        static_cast<size_t>(file->size()),
        "application/octet-stream",
        [file](size_t offset, size_t length, httplib::DataSink &sink) {
            constexpr size_t chunk = 1024 * 1024;
            if (!file->seek(static_cast<qint64>(offset)))
            {
                return false;
            }
            const QByteArray bytes = file->read(static_cast<qint64>(std::min(length, chunk)));
            return !bytes.isEmpty() && sink.write(bytes.constData(), static_cast<size_t>(bytes.size()));
        },
        [file](bool) { file->close(); }
    );
    return true;
}

void HttpServer::run()
{
    server.Get("/health", [this](const httplib::Request &, httplib::Response &response) {
        const auto projects = renderQueue->snapshots();
        setJson(response, 200, {
            {"status", "ok"},
            {"projectCount", projects.size()}
        });
    });

    server.Get("/node/status", [this](const httplib::Request &, httplib::Response &response) {
        if (!loadMonitor)
        {
            setJsonError(response, 503, "load monitor is not running");
            return;
        }
        const LoadStatus status = loadMonitor->status();
        const auto means = [](const LoadWindow::Means &value) {
            return json {
                {"cpuPercent", value.cpuPercent},
                {"gpuPercent", value.gpuPercent},
                {"covered", value.covered}
            };
        };
        const bool accepting = acceptRemoteWork;
        setJson(response, 200, {
            // A node that refuses remote work never reports itself idle.
            {"busy", status.busy || !accepting},
            {"loadBusy", status.busy},
            {"acceptRemoteWork", accepting},
            {"gpuAvailable", status.gpuAvailable},
            {"latest", {
                {"cpuPercent", status.latest.cpuPercent},
                {"gpuPercent", status.latest.gpuPercent},
                {"ownCpuPercent", status.latest.ownCpuPercent},
                {"ownGpuPercent", status.latest.ownGpuPercent}
            }},
            {"busyWindow", means(status.busyWindow)},
            {"idleWindow", means(status.idleWindow)},
            {"settings", {
                {"sampleIntervalMs", status.settings.sampleIntervalMs},
                {"busyThresholdPercent", status.settings.busyThresholdPercent},
                {"busyWindowSeconds", status.settings.busyWindowSeconds},
                {"idleThresholdPercent", status.settings.idleThresholdPercent},
                {"idleWindowSeconds", status.settings.idleWindowSeconds}
            }}
        });
    });

    // Signed node announcement for peers that cannot hear broadcasts.
    server.Get("/node/beacon", [this](const httplib::Request &, httplib::Response &response) {
        QByteArray beacon;
        {
            QMutexLocker locker(&configMutex);
            if (beaconProvider)
            {
                beacon = beaconProvider();
            }
        }
        if (beacon.isEmpty())
        {
            setJsonError(response, 404, "node is not announcing");
            return;
        }
        response.set_content(beacon.constData(), static_cast<size_t>(beacon.size()), "application/json");
    });

    server.Get("/projects", [this](const httplib::Request &, httplib::Response &response) {
        json projects = json::array();
        for (const ProjectInfo &project : renderQueue->snapshots())
        {
            projects.push_back(projectToJson(project));
        }
        setJson(response, 200, projects);
    });

    server.Get("/projects/", [](const httplib::Request &, httplib::Response &response) {
        response.set_redirect("/projects");
    });

    server.Get(R"(/projects/([^/]+))", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id))
        {
            return;
        }
        const ProjectInfo project = renderQueue->snapshotById(id);
        if (project.isNull)
        {
            setJsonError(response, 404, "not found");
            return;
        }
        setJson(response, 200, projectToJson(project));
    });

    server.Post("/projects", [this](const httplib::Request &request, httplib::Response &response) {
        json body;
        if (!parseJsonObject(request, response, body)
            || !body.contains("path")
            || !body["path"].is_string())
        {
            if (response.status == -1)
            {
                setJsonError(response, 400, "path must be a string");
            }
            return;
        }
        const QString path = fromUtf8(body["path"].get<std::string>());
        if (path.trimmed().isEmpty())
        {
            setJsonError(response, 400, "path must not be empty");
            return;
        }
        QString name;
        if (body.contains("name"))
        {
            if (!body["name"].is_string())
            {
                setJsonError(response, 400, "name must be a string");
                return;
            }
            name = fromUtf8(body["name"].get<std::string>());
        }
        const QUuid id = QUuid::createUuid();
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id, path, name]() {
                queue->addJob(id, path, name);
            },
            Qt::QueuedConnection
        );
        setAccepted(response, {{"projectId", uuidText(id)}});
    });

    server.Delete(R"(/projects/([^/]+))", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id]() { queue->removeJob(id); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Put(R"(/projects/([^/]+)/order)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        json body;
        if (!parseJsonObject(request, response, body)
            || !body.contains("index")
            || (!body["index"].is_number_integer() && !body["index"].is_number_unsigned()))
        {
            if (response.status == -1)
            {
                setJsonError(response, 400, "index must be an integer");
            }
            return;
        }
        long long indexValue = 0;
        try
        {
            indexValue = body["index"].get<long long>();
        }
        catch (...)
        {
            setJsonError(response, 400, "index is out of range");
            return;
        }
        const auto projects = renderQueue->snapshots();
        if (indexValue < 0
            || indexValue > std::numeric_limits<int>::max()
            || indexValue >= static_cast<long long>(projects.size()))
        {
            setJsonError(response, 400, "index is out of range");
            return;
        }
        const int targetIndex = static_cast<int>(indexValue);
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id, targetIndex]() { queue->moveTo(id, targetIndex); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post(R"(/projects/([^/]+)/reload)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id]() { queue->reloadJob(id); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post("/queue/start", [this](const httplib::Request &, httplib::Response &response) {
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue]() { queue->start(); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post("/queue/stop", [this](const httplib::Request &, httplib::Response &response) {
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue]() { queue->stop(); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Get("/frame", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        int frame = 0;
        int thumbnail = 0;
        try
        {
            if (!request.has_param("id") || !request.has_param("frame"))
            {
                throw std::invalid_argument("missing parameters");
            }
            id = QUuid(fromUtf8(request.get_param_value("id")));
            if (id.isNull())
            {
                throw std::invalid_argument("invalid project id");
            }
            frame = std::stoi(request.get_param_value("frame"));
            if (request.has_param("thumb"))
            {
                thumbnail = std::stoi(request.get_param_value("thumb"));
            }
        }
        catch (...)
        {
            setJsonError(response, 400, "invalid parameters");
            return;
        }
        const QString framePath = renderQueue->framePath(id, frame);
        if (framePath.isEmpty())
        {
            setJsonError(response, 404, "not found");
            return;
        }
        QImage image(framePath);
        if (image.isNull())
        {
            setJsonError(response, 404, "not found");
            return;
        }
        if (thumbnail != 0)
        {
            image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        response.set_content(bytes.constData(), static_cast<size_t>(bytes.size()), "image/png");
    });

    server.Post("/workers/claim", [this](const httplib::Request &request, httplib::Response &response) {
        QByteArray nonce;
        if (!authorizeWorker(request, response, &nonce))
        {
            return;
        }
        json body;
        if (!parseJsonObject(request, response, body))
        {
            return;
        }
        QString workerId;
        QString workerName;
        int maxFrames = 0;
        if (!readOptionalString(body, "workerId", maxWorkerFieldLength, workerId, response)
            || !readOptionalString(body, "name", maxWorkerFieldLength, workerName, response)
            || !readOptionalNonNegativeInt(body, "maxFrames", maxFrames, response))
        {
            return;
        }
        if (workerId.trimmed().isEmpty())
        {
            setJsonError(response, 400, "workerId must be a non-empty string");
            return;
        }
        const auto result = callInQueue<std::optional<WorkAssignment>>(
            renderQueue,
            [workerId, workerName, maxFrames](RenderQueue *queue) {
                return queue->claimWork(workerId, workerName, maxFrames);
            }
        );
        if (!result.has_value())
        {
            setJsonError(response, 503, "render queue is busy");
            return;
        }
        if (!result->has_value())
        {
            response.status = 204;
            signResponse(response, nonce);
            return;
        }
        setJson(response, 200, assignmentToJson(**result));
        signResponse(response, nonce);
    });

    server.Get("/workers", [this](const httplib::Request &request, httplib::Response &response) {
        if (!authorizeWorker(request, response))
        {
            return;
        }
        const auto result = callInQueue<std::vector<WorkerInfo>>(
            renderQueue,
            [](RenderQueue *queue) { return queue->workers(); }
        );
        if (!result.has_value())
        {
            setJsonError(response, 503, "render queue is busy");
            return;
        }
        json workers = json::array();
        for (const WorkerInfo &worker : *result)
        {
            workers.push_back({
                {"id", toUtf8(worker.id)},
                {"name", toUtf8(worker.name)},
                {"leaseId", worker.leaseId.isNull() ? json(nullptr) : json(uuidText(worker.leaseId))},
                {"completedLeases", worker.completedLeases},
                {"msPerFrame", worker.msPerFrame}
            });
        }
        setJson(response, 200, workers);
    });

    server.Post(R"(/leases/([^/]+)/progress)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid leaseId;
        json body;
        int finishedCount = -1;
        if (!authorizeWorker(request, response)
            || !parseLeaseId(request, response, leaseId)
            || !parseJsonObject(request, response, body)
            || !readOptionalNonNegativeInt(body, "finishedCount", finishedCount, response))
        {
            return;
        }
        if (finishedCount < 0)
        {
            setJsonError(response, 400, "finishedCount must be a non-negative integer");
            return;
        }
        setLeaseResult(response, callInQueue<bool>(
            renderQueue,
            [leaseId, finishedCount](RenderQueue *queue) { return queue->reportWork(leaseId, finishedCount); }
        ));
    });

    server.Post(R"(/leases/([^/]+)/complete)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid leaseId;
        if (!authorizeWorker(request, response) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        setLeaseResult(response, callInQueue<bool>(
            renderQueue,
            [leaseId](RenderQueue *queue) { return queue->completeWork(leaseId); }
        ));
    });

    server.Post(R"(/leases/([^/]+)/fail)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid leaseId;
        QString message;
        if (!authorizeWorker(request, response) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        if (!request.body.empty())
        {
            json body;
            if (!parseJsonObject(request, response, body)
                || !readOptionalString(body, "message", 1024, message, response))
            {
                return;
            }
        }
        setLeaseResult(response, callInQueue<bool>(
            renderQueue,
            [leaseId, message](RenderQueue *queue) { return queue->failWork(leaseId, message); }
        ));
    });

    // Gives the lease back without counting as a failure: the worker's
    // machine became busy. Reported frames stay done.
    server.Post(R"(/leases/([^/]+)/release)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid leaseId;
        int finishedCount = 0;
        if (!authorizeWorker(request, response) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        if (!request.body.empty())
        {
            json body;
            if (!parseJsonObject(request, response, body)
                || !readOptionalNonNegativeInt(body, "finishedCount", finishedCount, response))
            {
                return;
            }
        }
        setLeaseResult(response, callInQueue<bool>(
            renderQueue,
            [leaseId, finishedCount](RenderQueue *queue) { return queue->releaseWork(leaseId, finishedCount); }
        ));
    });

    // Lists the packed file set of the lease's project.
    server.Get(R"(/leases/([^/]+)/manifest)", [this](const httplib::Request &request, httplib::Response &response) {
        QByteArray nonce;
        QUuid leaseId;
        if (!authorizeWorker(request, response, &nonce) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        const auto files = callInQueue<std::optional<LeaseFiles>>(
            renderQueue,
            [leaseId](RenderQueue *queue) { return queue->leaseFiles(leaseId); }
        );
        if (!files.has_value())
        {
            setJsonError(response, 503, "render queue is busy");
            return;
        }
        if (!files->has_value() || !(*files)->pack.has_value())
        {
            setJsonError(response, 410, "lease is no longer valid");
            return;
        }
        const DependencyPacker::Pack &pack = *(*files)->pack;
        json entries = json::array();
        for (const DependencyPacker::File &file : pack.files)
        {
            entries.push_back({{"path", toUtf8(file.path)}, {"size", file.size}});
        }
        setJson(response, 200, {
            {"key", toUtf8(pack.key)},
            {"main", toUtf8(pack.mainFile)},
            {"files", entries}
        });
        signResponse(response, nonce);
    });

    // Streams one file of the packed set. Only paths listed in the manifest
    // are served, which rules out traversal by construction.
    server.Get(R"(/leases/([^/]+)/files/(.+))", [this](const httplib::Request &request, httplib::Response &response) {
        QByteArray nonce;
        QUuid leaseId;
        if (!authorizeWorker(request, response, &nonce) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        const QString relative = fromUtf8(request.matches[2].str());
        const auto files = callInQueue<std::optional<LeaseFiles>>(
            renderQueue,
            [leaseId](RenderQueue *queue) { return queue->leaseFiles(leaseId); }
        );
        if (!files.has_value())
        {
            setJsonError(response, 503, "render queue is busy");
            return;
        }
        if (!files->has_value() || !(*files)->pack.has_value())
        {
            setJsonError(response, 410, "lease is no longer valid");
            return;
        }
        const DependencyPacker::Pack &pack = *(*files)->pack;
        const bool listed = std::any_of(pack.files.begin(), pack.files.end(), [&relative](const DependencyPacker::File &file) {
            return file.path == relative;
        });
        if (!listed)
        {
            setJsonError(response, 404, "not part of the pack");
            return;
        }
        if (!serveFile(response, QDir(pack.directory).filePath(relative), nonce))
        {
            setJsonError(response, 404, "file is not readable");
        }
    });

    // Streams the lease's .blend. The key header lets the worker cache by the
    // file's identity at download time, even if it changed after the claim.
    server.Get(R"(/leases/([^/]+)/blend)", [this](const httplib::Request &request, httplib::Response &response) {
        QByteArray nonce;
        QUuid leaseId;
        if (!authorizeWorker(request, response, &nonce) || !parseLeaseId(request, response, leaseId))
        {
            return;
        }
        const auto files = callInQueue<std::optional<LeaseFiles>>(
            renderQueue,
            [leaseId](RenderQueue *queue) { return queue->leaseFiles(leaseId); }
        );
        if (!files.has_value())
        {
            setJsonError(response, 503, "render queue is busy");
            return;
        }
        if (!files->has_value())
        {
            setJsonError(response, 410, "lease is no longer valid");
            return;
        }
        if (!serveFile(response, (*files)->blendPath, nonce))
        {
            setJsonError(response, 404, "blend file is not readable");
            return;
        }
        response.set_header("X-Rendrop-Blend-Key", toUtf8(RenderQueue::blendKey((*files)->blendPath)));
    });

    // Receives one rendered file into the coordinator's output directory. The
    // file only replaces an existing one once it arrived in full.
    server.Put(R"(/leases/([^/]+)/frames/([^/]+))", [this](
        const httplib::Request &request,
        httplib::Response &response,
        const httplib::ContentReader &contentReader
    ) {
        // A rejected upload still has its body on the socket. Windows resets
        // a connection closed with unread data, which can destroy the error
        // response before the client reads it, so read and discard the body
        // when it is reasonably small and only drop the connection otherwise.
        // Unauthenticated callers get a much smaller allowance.
        const uint64_t declaredLength = request.has_header("Content-Length")
            ? request.get_header_value_u64("Content-Length")
            : std::numeric_limits<uint64_t>::max();
        const auto discardBody = [&](uint64_t limit) {
            if (declaredLength <= limit)
            {
                contentReader([](const char *, size_t) { return true; });
            }
            else
            {
                response.set_header("Connection", "close");
            }
        };
        constexpr uint64_t anonymousDiscardLimit = 1024 * 1024;
        QUuid leaseId;
        if (!authorizeWorker(request, response))
        {
            discardBody(anonymousDiscardLimit);
            return;
        }
        const auto reject = [&](int status, const char *message) {
            setJsonError(response, status, message);
            discardBody(static_cast<uint64_t>(maxFrameBytes));
        };
        if (!parseLeaseId(request, response, leaseId))
        {
            discardBody(static_cast<uint64_t>(maxFrameBytes));
            return;
        }
        const QString name = fromUtf8(request.matches[2].str());
        if (!isSafeFrameName(name))
        {
            reject(400, "invalid frame file name");
            return;
        }
        // Signed into the request when present; older workers omit it.
        const QByteArray declaredHash = QByteArray::fromStdString(request.get_header_value(WorkerAuth::contentHashHeader));
        if (!declaredHash.isEmpty() && !WorkerAuth::isContentHash(declaredHash))
        {
            reject(400, "invalid content hash");
            return;
        }
        if (declaredLength != std::numeric_limits<uint64_t>::max()
            && declaredLength > static_cast<uint64_t>(maxFrameBytes))
        {
            setJsonError(response, 413, "frame file is too large");
            response.set_header("Connection", "close");
            return;
        }
        const auto files = callInQueue<std::optional<LeaseFiles>>(
            renderQueue,
            [leaseId](RenderQueue *queue) { return queue->leaseFiles(leaseId); }
        );
        if (!files.has_value())
        {
            reject(503, "render queue is busy");
            return;
        }
        if (!files->has_value() || (*files)->outputDirectory.isEmpty())
        {
            reject(410, "lease is no longer valid");
            return;
        }
        const QDir directory((*files)->outputDirectory);
        if (!directory.mkpath(QStringLiteral(".")))
        {
            reject(500, "cannot create output directory");
            return;
        }
        QSaveFile file(directory.filePath(name));
        if (!file.open(QIODevice::WriteOnly))
        {
            reject(500, "cannot open output file");
            return;
        }
        qint64 received = 0;
        bool tooLarge = false;
        QCryptographicHash hash(WorkerAuth::contentHashAlgorithm);
        const bool read = contentReader([&](const char *data, size_t length) {
            received += static_cast<qint64>(length);
            if (received > maxFrameBytes)
            {
                tooLarge = true;
                return false;
            }
            hash.addData(QByteArrayView(data, static_cast<qsizetype>(length)));
            return file.write(data, static_cast<qint64>(length)) == static_cast<qint64>(length);
        });
        if (tooLarge)
        {
            file.cancelWriting();
            setJsonError(response, 413, "frame file is too large");
            response.set_header("Connection", "close");
            return;
        }
        if (!read)
        {
            file.cancelWriting();
            setJsonError(response, 400, "upload was interrupted");
            response.set_header("Connection", "close");
            return;
        }
        if (!declaredHash.isEmpty() && hash.result().toHex() != declaredHash)
        {
            // The existing file stays; the worker sends this one again.
            file.cancelWriting();
            setJsonError(response, 422, "content hash mismatch");
            return;
        }
        if (!file.commit())
        {
            setJsonError(response, 500, "cannot write output file");
            return;
        }
        setJson(response, 200, json({{"ok", true}, {"bytes", received}}));
    });

    if (port == 0)
    {
        const int bound = server.bind_to_any_port("0.0.0.0");
        if (bound <= 0)
        {
            return;
        }
        listeningPort = bound;
    }
    else
    {
        if (!server.bind_to_port("0.0.0.0", port))
        {
            return;
        }
        listeningPort = port;
    }
    server.listen_after_bind();
    listeningPort = 0;
}
