#include <chrono>
#include <functional>
#include <string>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "httplib.h"
#include "http_server.h"
#include "nlohmann/json.hpp"
#include "render_queue.h"

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

private:
    QString fakeBlenderPath;
};

void HttpServerTest::initTestCase()
{
    qRegisterMetaType<ProjectInfo>("ProjectInfo");
    fakeBlenderPath = qEnvironmentVariable("RENDROP_FAKE_BLENDER");
    QVERIFY2(!fakeBlenderPath.isEmpty(), "RENDROP_FAKE_BLENDER is not set");
    QVERIFY2(QFileInfo::exists(fakeBlenderPath), qPrintable(fakeBlenderPath));
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

QTEST_GUILESS_MAIN(HttpServerTest)

#include "http_server_test.moc"
