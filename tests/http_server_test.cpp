#include <chrono>
#include <functional>
#include <string>

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "dependency_packer.h"
#include "httplib.h"
#include "http_server.h"
#include "load_monitor.h"
#include "nlohmann/json.hpp"
#include "render_queue.h"
#include "worker_auth.h"

using nlohmann::json;

namespace {

QString createBlendFile(const QTemporaryDir &directory, const QString &name)
{
    const QString path = QDir(directory.path()).filePath(name + QStringLiteral(".blend"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return QString();
    }
    file.write("fake blend file");
    return QDir::toNativeSeparators(path);
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 8000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs)
    {
        if (condition())
        {
            return true;
        }
        QTest::qWait(10);
    }
    return condition();
}

std::string utf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

std::string projectPath(const QUuid &id)
{
    return "/projects/" + utf8(id.toString(QUuid::WithoutBraces));
}

int availablePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
    {
        return 0;
    }
    const int port = static_cast<int>(probe.serverPort());
    probe.close();
    return port;
}

}

class HttpServerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderManagementEndpoints();
    void workerEndpointsRequireToken();
    void workerProcessRendersLeases();
    void signedProtocolAndPackedFiles();
    void readManifestRejectsStaleEntries();
    void nodeStatusReportsAcceptRemoteWork();

private:
    QString fakeBlenderPath;
    QString workerPath;
};

void HttpServerTest::initTestCase()
{
    qRegisterMetaType<ProjectInfo>("ProjectInfo");
    fakeBlenderPath = qEnvironmentVariable("RENDROP_FAKE_BLENDER");
    QVERIFY2(!fakeBlenderPath.isEmpty(), "RENDROP_FAKE_BLENDER is not set");
    QVERIFY2(QFileInfo::exists(fakeBlenderPath), qPrintable(fakeBlenderPath));
    workerPath = qEnvironmentVariable("RENDROP_WORKER");
    QVERIFY2(!workerPath.isEmpty(), "RENDROP_WORKER is not set");
    QVERIFY2(QFileInfo::exists(workerPath), qPrintable(workerPath));
}

void HttpServerTest::renderManagementEndpoints()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = createBlendFile(directory, QStringLiteral("slow_first"));
    const QString secondPath = createBlendFile(directory, QStringLiteral("slow_second"));
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(!secondPath.isEmpty());

    RenderQueue queue(directory.path());
    queue.setBlenderPathForAll(fakeBlenderPath);
    const int port = availablePort();
    QVERIFY(port > 0);
    HttpServer server(&queue, port);
    server.start(QThread::LowestPriority);

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(std::chrono::milliseconds(200));
    client.set_read_timeout(std::chrono::seconds(5));
    QVERIFY(waitUntil([&]() {
        const auto response = client.Get("/health");
        return response && response->status == 200;
    }, 5000));

    auto response = client.Post("/projects", "{}", "application/json");
    QVERIFY(response);
    QCOMPARE(response->status, 400);

    response = client.Post(
        "/projects",
        json({{"path", utf8(firstPath)}, {"name", "first"}}).dump(),
        "application/json"
    );
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    const json firstAccepted = json::parse(response->body);
    QVERIFY(firstAccepted.value("accepted", false));
    const QUuid firstId(QString::fromStdString(firstAccepted.at("projectId").get<std::string>()));
    QVERIFY(!firstId.isNull());
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(firstId).state == ProjectState::Queued;
    }));

    response = client.Post(
        "/projects",
        json({{"path", utf8(secondPath)}}).dump(),
        "application/json"
    );
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    const json secondAccepted = json::parse(response->body);
    const QUuid secondId(QString::fromStdString(secondAccepted.at("projectId").get<std::string>()));
    QVERIFY(!secondId.isNull());
    QVERIFY(firstId != secondId);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(secondId).state == ProjectState::Queued;
    }));
    QCOMPARE(queue.snapshotById(secondId).name, QStringLiteral("slow_second"));

    response = client.Get("/projects");
    QVERIFY(response);
    QCOMPARE(response->status, 200);
    QCOMPARE(json::parse(response->body).size(), size_t(2));

    response = client.Get(projectPath(firstId));
    QVERIFY(response);
    QCOMPARE(response->status, 200);
    QCOMPARE(json::parse(response->body).at("id").get<std::string>(), utf8(firstId.toString(QUuid::WithoutBraces)));

    queue.setBlenderPathForAll(QDir(directory.path()).filePath(QStringLiteral("missing.exe")));
    response = client.Post(projectPath(firstId) + "/reload");
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(firstId).state == ProjectState::Error;
    }));
    queue.setBlenderPathForAll(fakeBlenderPath);
    response = client.Post(projectPath(firstId) + "/reload");
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(firstId).state == ProjectState::Queued;
    }));

    response = client.Put(
        projectPath(secondId) + "/order",
        json({{"index", 0}}).dump(),
        "application/json"
    );
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() {
        const auto snapshots = queue.snapshots();
        return snapshots.size() == 2 && snapshots[0].id == secondId;
    }));

    response = client.Put(
        projectPath(secondId) + "/order",
        json({{"index", 9}}).dump(),
        "application/json"
    );
    QVERIFY(response);
    QCOMPARE(response->status, 400);

    response = client.Post("/queue/start");
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() { return queue.isRendering(); }));
    response = client.Post("/queue/stop");
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() { return !queue.isRendering(); }));

    response = client.Delete(projectPath(secondId));
    QVERIFY(response);
    QCOMPARE(response->status, 202);
    QVERIFY(waitUntil([&]() { return queue.snapshotById(secondId).isNull; }));

    response = client.Delete(projectPath(secondId));
    QVERIFY(response);
    QCOMPARE(response->status, 404);
    response = client.Post("/projects/not-a-uuid/reload");
    QVERIFY(response);
    QCOMPARE(response->status, 400);

    response = client.Get("/health");
    QVERIFY(response);
    QCOMPARE(response->status, 200);
    QCOMPARE(json::parse(response->body).at("projectCount").get<size_t>(), size_t(1));

    server.stop();
    QVERIFY(server.wait(5000));
}

namespace {
const char *testToken = "worker-test-token-0123456789";

DistributedSettings enabledSettings()
{
    DistributedSettings settings;
    settings.enabled = true;
    settings.token = QString::fromLatin1(testToken);
    settings.chunkSize = 1;
    // Fixed batches keep lease sizes predictable in tests.
    settings.adaptiveChunks = false;
    settings.leaseTimeoutSeconds = 60;
    return settings;
}

bool waitForServer(httplib::Client &client)
{
    return waitUntil([&]() {
        const auto response = client.Get("/health");
        return response && response->status == 200;
    }, 5000);
}
}

void HttpServerTest::workerEndpointsRequireToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RenderQueue queue(directory.path());
    queue.setBlenderPathForAll(fakeBlenderPath);

    const json claim = {{"workerId", "w1"}};
    {
        // No token configured: worker endpoints stay off.
        const int port = availablePort();
        HttpServer server(&queue, port);
        server.start(QThread::LowestPriority);
        httplib::Client client("127.0.0.1", port);
        client.set_connection_timeout(std::chrono::milliseconds(200));
        QVERIFY(waitForServer(client));
        auto response = client.Post("/workers/claim", claim.dump(), "application/json");
        QVERIFY(response);
        QCOMPARE(response->status, 403);
        server.stop();
        QVERIFY(server.wait(5000));
    }

    queue.setDistributedSettings(enabledSettings());
    const int port = availablePort();
    HttpServer server(&queue, port);
    server.setWorkerToken(QString::fromLatin1(testToken));
    server.start(QThread::LowestPriority);
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(std::chrono::milliseconds(200));
    client.set_read_timeout(std::chrono::seconds(5));
    QVERIFY(waitForServer(client));

    auto response = client.Post("/workers/claim", claim.dump(), "application/json");
    QVERIFY(response);
    QCOMPARE(response->status, 401);
    response = client.Post(
        "/workers/claim",
        httplib::Headers{{"Authorization", "Bearer wrong-token-0123456789"}},
        claim.dump(),
        "application/json"
    );
    QVERIFY(response);
    QCOMPARE(response->status, 401);

    const httplib::Headers auth = {{"Authorization", std::string("Bearer ") + testToken}};
    response = client.Post("/workers/claim", auth, "{}", "application/json");
    QVERIFY(response);
    QCOMPARE(response->status, 400);

    // Authorized, but nothing is rendering. The queue runs on this thread, so
    // keep its event loop alive while the request is in flight.
    httplib::Result claimResult;
    QThread *requester = QThread::create([&]() {
        claimResult = client.Post("/workers/claim", auth, claim.dump(), "application/json");
    });
    requester->start();
    QVERIFY(waitUntil([&]() { return requester->isFinished(); }, 5000));
    delete requester;
    QVERIFY(claimResult);
    QCOMPARE(claimResult->status, 204);

    httplib::Result leaseResult;
    requester = QThread::create([&]() {
        leaseResult = client.Post(
            "/leases/" + utf8(QUuid::createUuid().toString(QUuid::WithoutBraces)) + "/progress",
            auth,
            json({{"finishedCount", 1}}).dump(),
            "application/json"
        );
    });
    requester->start();
    QVERIFY(waitUntil([&]() { return requester->isFinished(); }, 5000));
    delete requester;
    QVERIFY(leaseResult);
    QCOMPARE(leaseResult->status, 410);

    // File transfer endpoints: names that leave the output directory are
    // rejected before the lease is even looked up.
    const std::string staleLease = "/leases/" + utf8(QUuid::createUuid().toString(QUuid::WithoutBraces));
    for (const char *name : {"..%5Cescape.png", "..", "C%3Aescape.png", "NUL.png", "frame.png."})
    {
        response = client.Put(staleLease + "/frames/" + name, auth, "data", "application/octet-stream");
        QVERIFY2(response, (std::string(name) + " " + httplib::to_string(response.error())).c_str());
        QVERIFY2(response->status == 400, name);
    }
    response = client.Put(staleLease + "/frames/..%2Fescape.png", auth, "data", "application/octet-stream");
    QVERIFY(response);
    QCOMPARE(response->status, 404);
    response = client.Put(staleLease + "/frames/render_0001.png", "data", "application/octet-stream");
    QVERIFY(response);
    QCOMPARE(response->status, 401);

    httplib::Result uploadResult;
    httplib::Result blendResult;
    requester = QThread::create([&]() {
        uploadResult = client.Put(staleLease + "/frames/render_0001.png", auth, "data", "application/octet-stream");
        blendResult = client.Get(staleLease + "/blend", auth);
    });
    requester->start();
    QVERIFY(waitUntil([&]() { return requester->isFinished(); }, 5000));
    delete requester;
    QVERIFY(uploadResult);
    QCOMPARE(uploadResult->status, 410);
    QVERIFY(blendResult);
    QCOMPARE(blendResult->status, 410);
    QVERIFY(QDir(directory.path()).entryList({QStringLiteral("*.png")}, QDir::Files).isEmpty());

    server.stop();
    QVERIFY(server.wait(5000));
}

void HttpServerTest::workerProcessRendersLeases()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blendPath = createBlendFile(directory, QStringLiteral("slow_worker"));
    RenderQueue queue(directory.path());
    queue.setBlenderPathForAll(fakeBlenderPath);
    queue.setDistributedSettings(enabledSettings());
    const int port = availablePort();
    HttpServer server(&queue, port);
    server.setWorkerToken(QString::fromLatin1(testToken));
    server.start(QThread::LowestPriority);

    const QUuid id = queue.addJob(QStringLiteral("worker"), blendPath, fakeBlenderPath);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Queued;
    }));
    QVERIFY(queue.start());

    // The worker shares nothing with the coordinator but HTTP.
    QTemporaryDir workerDirectory;
    QVERIFY(workerDirectory.isValid());
    QProcess worker;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("RENDROP_WORKER_TOKEN"), QString::fromLatin1(testToken));
    worker.setProcessEnvironment(environment);
    worker.setProcessChannelMode(QProcess::MergedChannels);
    worker.start(workerPath, {
        QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
        QStringLiteral("--port"), QString::number(port),
        QStringLiteral("--blender"), fakeBlenderPath,
        QStringLiteral("--name"), QStringLiteral("test-worker"),
        QStringLiteral("--poll"), QStringLiteral("1"),
        QStringLiteral("--cache-dir"), workerDirectory.path(),
        QStringLiteral("--exit-when-idle")
    });
    QVERIFY(worker.waitForStarted(5000));

    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Finished && !queue.isRendering();
    }, 20000));
    QVERIFY(waitUntil([&]() { return worker.state() == QProcess::NotRunning; }, 10000));
    const QByteArray output = worker.readAll();
    QVERIFY2(worker.exitStatus() == QProcess::NormalExit && worker.exitCode() == 0, output.constData());
    QVERIFY2(output.contains("Lease complete"), output.constData());
    const std::vector<WorkerInfo> workers = queue.workers();
    QCOMPARE(workers.size(), size_t(1));
    QCOMPARE(workers[0].name, QStringLiteral("test-worker"));
    QVERIFY(workers[0].completedLeases >= 1);
    QCOMPARE(queue.snapshotById(id).finishedFrame, 3);

    // Every frame the worker uploaded is credited to it by name, and the
    // project listing carries the ownership to clients.
    httplib::Result listing;
    QThread *lister = QThread::create([&]() {
        httplib::Client listClient("127.0.0.1", port);
        listing = listClient.Get("/projects");
    });
    lister->start();
    QVERIFY(waitUntil([&]() { return lister->isFinished(); }, 5000));
    delete lister;
    QVERIFY(listing);
    QCOMPARE(listing->status, 200);
    const json listed = json::parse(listing->body).at(0);
    QCOMPARE(listed.at("completedRanges"), json::parse("[[0, 2]]"));
    int credited = 0;
    for (const json &owner : listed.at("frameOwners"))
    {
        QCOMPARE(owner.at("node").get<std::string>(), std::string("test-worker"));
        for (const json &range : owner.at("ranges"))
        {
            credited += range.at(1).get<int>() - range.at(0).get<int>() + 1;
        }
    }

    // The local fake Blender writes no files, so every frame on disk came
    // through the upload endpoint, named the way the project asks for.
    const QStringList frames = QDir(directory.path()).entryList({QStringLiteral("render_*.png")}, QDir::Files, QDir::Name);
    QVERIFY2(!frames.isEmpty(), output.constData());
    QCOMPARE(output.count("Uploaded render_"), qsizetype(frames.size()));
    QCOMPARE(credited, int(frames.size()));
    for (const QString &frameName : frames)
    {
        QFile frame(QDir(directory.path()).filePath(frameName));
        QVERIFY(frame.open(QIODevice::ReadOnly));
        const int number = frameName.mid(7, 4).toInt();
        QCOMPARE(frame.readAll(), QByteArray("fake frame ") + QByteArray::number(number));
    }
    // The worker kept its copy of the .blend in its own cache and cleaned
    // up its local frames.
    QDirIterator cached(workerDirectory.path(), {QStringLiteral("slow_worker.blend")}, QDir::Files, QDirIterator::Subdirectories);
    QVERIFY(cached.hasNext());
    QCOMPARE(QFileInfo(cached.next()).size(), QFileInfo(blendPath).size());
    QVERIFY(QDir(QDir(workerDirectory.path()).filePath(QStringLiteral("out"))).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());

    server.stop();
    QVERIFY(server.wait(5000));
}

namespace {

// Runs a blocking request off the main thread so the queue, which lives on
// the main thread, keeps processing events meanwhile.
bool runOffThread(const std::function<void()> &work)
{
    QThread *thread = QThread::create(work);
    thread->start();
    const bool finished = waitUntil([&]() { return thread->isFinished(); }, 8000);
    if (!finished)
    {
        thread->wait();
    }
    delete thread;
    return finished;
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

bool writeManifestFile(const QString &packDirectory, const QString &mainFile, const json &files)
{
    const json manifest = {{"main", utf8(mainFile)}, {"files", files}};
    return writeFile(QDir(packDirectory).filePath(QStringLiteral("manifest.json")), QByteArray::fromStdString(manifest.dump()));
}

httplib::Headers signedHeaders(const char *method, const std::string &path, QByteArray *nonce = nullptr)
{
    const WorkerAuth::SignedRequest request = WorkerAuth::signRequest(testToken, method, QByteArray::fromStdString(path));
    if (nonce)
    {
        *nonce = request.nonce;
    }
    return {{"Authorization", request.header}};
}

bool responseSigned(const httplib::Result &result, const QByteArray &nonce)
{
    const QByteArray expected = WorkerAuth::responseSignature(testToken, nonce, QByteArray::fromStdString(result->body));
    return result->get_header_value("X-Rendrop-Response") == expected.toStdString();
}

}

void HttpServerTest::signedProtocolAndPackedFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blendPath = createBlendFile(directory, QStringLiteral("slow_signed"));
    QVERIFY(!blendPath.isEmpty());

    // Seed the packer cache the way a finished BAT run leaves it, so the
    // queue serves the pack without running Blender Asset Tracer.
    const QString key = RenderQueue::blendKey(blendPath);
    const QString packRoot = QDir(QDir::temp().filePath(QStringLiteral("rendrop-packs"))).filePath(key);
    QDir(packRoot).removeRecursively();
    const QString filesRoot = QDir(packRoot).filePath(QStringLiteral("files"));
    const QByteArray texture = "texture bytes";
    QVERIFY(writeFile(QDir(filesRoot).filePath(QStringLiteral("slow_signed.blend")), "fake blend file"));
    QVERIFY(writeFile(QDir(filesRoot).filePath(QStringLiteral("tex/a.png")), texture));
    QVERIFY(writeFile(QDir(filesRoot).filePath(QStringLiteral("secret.txt")), "unlisted"));
    QVERIFY(writeManifestFile(packRoot, QStringLiteral("slow_signed.blend"), json::array({
        {{"path", "slow_signed.blend"}, {"size", 15}},
        {{"path", "tex/a.png"}, {"size", texture.size()}}
    })));
    const auto cleanup = qScopeGuard([&]() { QDir(packRoot).removeRecursively(); });

    RenderQueue queue(directory.path());
    queue.setBlenderPathForAll(fakeBlenderPath);
    DistributedSettings settings = enabledSettings();
    settings.dependencyMode = QStringLiteral("bat");
    queue.setDistributedSettings(settings);
    const int port = availablePort();
    HttpServer server(&queue, port);
    server.setWorkerToken(QString::fromLatin1(testToken));
    server.start(QThread::LowestPriority);
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(std::chrono::milliseconds(200));
    client.set_read_timeout(std::chrono::seconds(5));
    QVERIFY(waitForServer(client));

    const QUuid id = queue.addJob(QStringLiteral("signed"), blendPath, fakeBlenderPath);
    QVERIFY(waitUntil([&]() { return queue.snapshotById(id).state == ProjectState::Queued; }));
    QVERIFY(queue.start());

    // Signed claim: accepted, and the assignment carries a response MAC.
    const std::string claimBody = json({{"workerId", "signed"}, {"name", "signed"}}).dump();
    QByteArray claimNonce;
    const httplib::Headers claimHeaders = signedHeaders("POST", "/workers/claim", &claimNonce);
    httplib::Result claim;
    QVERIFY(runOffThread([&]() { claim = client.Post("/workers/claim", claimHeaders, claimBody, "application/json"); }));
    QVERIFY(claim);
    QCOMPARE(claim->status, 200);
    QVERIFY(responseSigned(claim, claimNonce));
    const json assignment = json::parse(claim->body);
    QVERIFY(assignment.at("packed").get<bool>());
    QCOMPARE(assignment.at("mainFile").get<std::string>(), std::string("slow_signed.blend"));
    const std::string lease = "/leases/" + assignment.at("leaseId").get<std::string>();

    // The same signed header cannot be replayed, and a signature for one
    // path does not open another.
    httplib::Result replay;
    httplib::Result wrongPath;
    QVERIFY(runOffThread([&]() {
        replay = client.Post("/workers/claim", claimHeaders, claimBody, "application/json");
        wrongPath = client.Get(lease + "/manifest", signedHeaders("GET", "/workers"));
    }));
    QVERIFY(replay);
    QCOMPARE(replay->status, 401);
    QVERIFY(wrongPath);
    QCOMPARE(wrongPath->status, 401);

    // Manifest lists exactly the packed files and is signed.
    QByteArray manifestNonce;
    httplib::Result manifest;
    const httplib::Headers manifestHeaders = signedHeaders("GET", lease + "/manifest", &manifestNonce);
    QVERIFY(runOffThread([&]() { manifest = client.Get(lease + "/manifest", manifestHeaders); }));
    QVERIFY(manifest);
    QCOMPARE(manifest->status, 200);
    QVERIFY(responseSigned(manifest, manifestNonce));
    const json listing = json::parse(manifest->body);
    QCOMPARE(listing.at("main").get<std::string>(), std::string("slow_signed.blend"));
    QCOMPARE(listing.at("files").size(), size_t(2));

    // Only listed files are served; Bearer still works for older workers.
    const httplib::Headers bearer = {{"Authorization", std::string("Bearer ") + testToken}};
    httplib::Result listed;
    httplib::Result signedListed;
    httplib::Result unlisted;
    httplib::Result traversal;
    QVERIFY(runOffThread([&]() {
        listed = client.Get(lease + "/files/tex/a.png", bearer);
        signedListed = client.Get(lease + "/files/tex/a.png", signedHeaders("GET", lease + "/files/tex/a.png"));
        unlisted = client.Get(lease + "/files/secret.txt", bearer);
        traversal = client.Get(lease + "/files/tex/..%2Fsecret.txt", bearer);
    }));
    QVERIFY(listed);
    QCOMPARE(listed->status, 200);
    QCOMPARE(QByteArray::fromStdString(listed->body), texture);
    QVERIFY(signedListed);
    QCOMPARE(signedListed->status, 200);
    QVERIFY(unlisted);
    QCOMPARE(unlisted->status, 404);
    QVERIFY(traversal);
    QCOMPARE(traversal->status, 404);

    // Signed downloads carry the content hash, bound to the request nonce.
    // Bearer clients cannot verify it and do not get it.
    const QByteArray textureHash = QCryptographicHash::hash(texture, WorkerAuth::contentHashAlgorithm).toHex();
    QCOMPARE(QByteArray::fromStdString(listed->get_header_value(WorkerAuth::contentHashHeader)), QByteArray());
    QByteArray fileNonce;
    const httplib::Headers fileHeaders = signedHeaders("GET", lease + "/files/tex/a.png", &fileNonce);
    httplib::Result hashed;
    QVERIFY(runOffThread([&]() { hashed = client.Get(lease + "/files/tex/a.png", fileHeaders); }));
    QVERIFY(hashed);
    QCOMPARE(hashed->status, 200);
    QCOMPARE(QByteArray::fromStdString(hashed->get_header_value(WorkerAuth::contentHashHeader)), textureHash);
    QCOMPARE(QByteArray::fromStdString(hashed->get_header_value("X-Rendrop-Response")),
        WorkerAuth::fileSignature(testToken, fileNonce, textureHash));
    QVERIFY(WorkerAuth::fileSignature(testToken, fileNonce, QByteArray(64, 'a'))
        != WorkerAuth::fileSignature(testToken, fileNonce, textureHash));

    // Uploads: the hash is signed into the request and checked on arrival.
    const QByteArray frameBytes = "rendered frame";
    const QByteArray frameHash = QCryptographicHash::hash(frameBytes, WorkerAuth::contentHashAlgorithm).toHex();
    const std::string framePath = lease + "/frames/hashcheck_0001.png";
    const auto hashedPut = [&](const QByteArray &hash, const QByteArray &signedHash) {
        const WorkerAuth::SignedRequest request = WorkerAuth::signRequest(testToken, "PUT", QByteArray::fromStdString(framePath), signedHash);
        httplib::Headers headers = {{"Authorization", request.header}};
        if (!hash.isEmpty())
        {
            headers.emplace(WorkerAuth::contentHashHeader, hash.toStdString());
        }
        return client.Put(framePath, headers, frameBytes.toStdString(), "application/octet-stream");
    };
    const QByteArray wrongHash(64, '0');
    httplib::Result mismatch;
    httplib::Result unsignedHash;
    httplib::Result malformed;
    httplib::Result matched;
    QVERIFY(runOffThread([&]() {
        mismatch = hashedPut(wrongHash, wrongHash);
        unsignedHash = hashedPut(frameHash, QByteArray());
        malformed = hashedPut("xyz", "xyz");
    }));
    QVERIFY(mismatch);
    QCOMPARE(mismatch->status, 422);
    QVERIFY(unsignedHash);
    QCOMPARE(unsignedHash->status, 401);
    QVERIFY(malformed);
    QCOMPARE(malformed->status, 400);
    QVERIFY(!QFileInfo::exists(QDir(directory.path()).filePath(QStringLiteral("hashcheck_0001.png"))));
    QVERIFY(runOffThread([&]() { matched = hashedPut(frameHash, frameHash); }));
    QVERIFY(matched);
    QCOMPARE(matched->status, 200);
    QFile uploaded(QDir(directory.path()).filePath(QStringLiteral("hashcheck_0001.png")));
    QVERIFY(uploaded.open(QIODevice::ReadOnly));
    QCOMPARE(uploaded.readAll(), frameBytes);
    uploaded.close();

    // Release hands the frames back; the lease is then gone.
    httplib::Result released;
    httplib::Result releasedAgain;
    QVERIFY(runOffThread([&]() {
        released = client.Post(lease + "/release", bearer, json({{"finishedCount", 0}}).dump(), "application/json");
        releasedAgain = client.Post(lease + "/release", bearer, "", "application/json");
    }));
    QVERIFY(released);
    QCOMPARE(released->status, 200);
    QVERIFY(releasedAgain);
    QCOMPARE(releasedAgain->status, 410);
    // The released frames are rendered locally after all.
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Finished && !queue.isRendering();
    }, 20000));

    server.stop();
    QVERIFY(server.wait(5000));
}

void HttpServerTest::readManifestRejectsStaleEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString pack = directory.path();
    const QString files = QDir(pack).filePath(QStringLiteral("files"));
    QVERIFY(writeFile(QDir(files).filePath(QStringLiteral("scene.blend")), "12345"));
    QVERIFY(writeFile(QDir(files).filePath(QStringLiteral("tex/b.png")), "abc"));

    QVERIFY(!DependencyPacker::readManifest(pack, QStringLiteral("k")).has_value());

    QVERIFY(writeManifestFile(pack, QStringLiteral("scene.blend"), json::array({
        {{"path", "scene.blend"}, {"size", 5}},
        {{"path", "tex/b.png"}, {"size", 3}}
    })));
    const auto loaded = DependencyPacker::readManifest(pack, QStringLiteral("k"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->key, QStringLiteral("k"));
    QCOMPARE(loaded->mainFile, QStringLiteral("scene.blend"));
    QCOMPARE(loaded->files.size(), size_t(2));
    QCOMPARE(QDir(loaded->directory).absolutePath(), QDir(files).absolutePath());

    // Size no longer matches the disk.
    QVERIFY(writeManifestFile(pack, QStringLiteral("scene.blend"), json::array({
        {{"path", "scene.blend"}, {"size", 6}}
    })));
    QVERIFY(!DependencyPacker::readManifest(pack, QStringLiteral("k")).has_value());

    // Main file not listed.
    QVERIFY(writeManifestFile(pack, QStringLiteral("scene.blend"), json::array({
        {{"path", "tex/b.png"}, {"size", 3}}
    })));
    QVERIFY(!DependencyPacker::readManifest(pack, QStringLiteral("k")).has_value());

    // Paths that are not clean relative paths.
    QVERIFY(writeFile(QDir(pack).filePath(QStringLiteral("outside.txt")), "x"));
    for (const char *path : {"../outside.txt", "tex\\b.png", "/scene.blend", "C:/scene.blend", "tex//b.png", "./scene.blend"})
    {
        QVERIFY(writeManifestFile(pack, QStringLiteral("scene.blend"), json::array({
            {{"path", "scene.blend"}, {"size", 5}},
            {{"path", path}, {"size", 1}}
        })));
        QVERIFY2(!DependencyPacker::readManifest(pack, QStringLiteral("k")).has_value(), path);
    }
}

void HttpServerTest::nodeStatusReportsAcceptRemoteWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RenderQueue queue(directory.path());
    LoadMonitor monitor;
    const int port = availablePort();
    HttpServer server(&queue, port);
    server.setLoadMonitor(&monitor);
    server.start(QThread::LowestPriority);
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(std::chrono::milliseconds(200));
    QVERIFY(waitForServer(client));

    auto response = client.Get("/node/status");
    QVERIFY(response);
    QCOMPARE(response->status, 200);
    json status = json::parse(response->body);
    QVERIFY(status.at("acceptRemoteWork").get<bool>());
    QCOMPARE(status.at("busy").get<bool>(), status.at("loadBusy").get<bool>());

    server.setAcceptRemoteWork(false);
    response = client.Get("/node/status");
    QVERIFY(response);
    status = json::parse(response->body);
    QVERIFY(!status.at("acceptRemoteWork").get<bool>());
    // Refusing remote work never reads as idle.
    QVERIFY(status.at("busy").get<bool>());

    server.stop();
    QVERIFY(server.wait(5000));
}

QTEST_GUILESS_MAIN(HttpServerTest)

#include "http_server_test.moc"
