// Unit tests for the pieces of distributed rendering that need no Blender:
// lease sizing, settings migration, beacon signing and LAN discovery.

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/chunk_planner.h"
#include "config/distributed_settings.h"
#include "farm/node_discovery.h"
#include "farm/worker_auth.h"

class DistributedTest : public QObject
{
    Q_OBJECT

private slots:
    void plannerUsesInitialChunkUntilMeasured();
    void plannerSizesLeaseFromSpeed();
    void plannerCapsAtFairShare();
    void plannerFollowsSlowerRunningLease();
    void plannerFixedWhenNotAdaptive();
    void settingsWriteMissingKeys();
    void settingsKeepValues();
    void verifierRejectsReplayAndTampering();
    void beaconRoundTrip();
    void beaconRejectsWrongTokenAndStale();
    void parsePeerForms();
    void discoveryFindsPeerOverUdp();
};

void DistributedTest::plannerUsesInitialChunkUntilMeasured()
{
    ChunkPlanner planner;
    planner.setSettings({true, 10, 1000, 300});
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 10);
    QVERIFY(planner.msPerFrame(QStringLiteral("a")) < 0.0);
}

void DistributedTest::plannerSizesLeaseFromSpeed()
{
    ChunkPlanner planner;
    planner.setSettings({true, 10, 1000, 300});
    // 10 frames in 100 s: 10 s per frame, a 300 s lease is 30 frames.
    planner.recordComplete(QStringLiteral("a"), 100000, 10);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 30);
    // A second lease at 20 s per frame smooths to 15 s per frame.
    planner.recordComplete(QStringLiteral("a"), 200000, 10);
    QCOMPARE(planner.msPerFrame(QStringLiteral("a")), 15000.0);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 20);
    // Very fast frames are capped.
    planner.recordComplete(QStringLiteral("b"), 10, 10);
    QCOMPARE(planner.chunkFor(QStringLiteral("b"), 0, 1), 1000);
    planner.reset();
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 10);
}

void DistributedTest::plannerCapsAtFairShare()
{
    ChunkPlanner planner;
    planner.setSettings({true, 10, 1000, 300});
    planner.recordComplete(QStringLiteral("a"), 1000, 10);
    // 100 ms per frame would allow 1000 frames; 50 pending over 3 renderers
    // gives half an even share, at most 9.
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 50, 3), 9);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 1, 3), 1);
    // Alone, a renderer may take everything that is left.
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 50, 1), 50);
}

void DistributedTest::plannerFollowsSlowerRunningLease()
{
    ChunkPlanner planner;
    planner.setSettings({true, 10, 1000, 300});
    planner.recordComplete(QStringLiteral("a"), 10000, 10);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 300);
    // The running lease shows 30 s per frame: trust it.
    planner.recordProgress(QStringLiteral("a"), 60000, 2);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 0, 1), 10);
    // A faster running lease does not override the history.
    planner.recordComplete(QStringLiteral("a"), 10000, 10);
    planner.recordProgress(QStringLiteral("a"), 100, 2);
    QCOMPARE(planner.msPerFrame(QStringLiteral("a")), 1000.0);
}

void DistributedTest::plannerFixedWhenNotAdaptive()
{
    ChunkPlanner planner;
    planner.setSettings({false, 7, 1000, 300});
    planner.recordComplete(QStringLiteral("a"), 10, 10);
    QCOMPARE(planner.chunkFor(QStringLiteral("a"), 3, 4), 7);
}

void DistributedTest::settingsWriteMissingKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("distributed.json"));
    {
        // A file from before adaptive leases and discovery.
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"enabled": false, "token": "", "chunkSize": 25, "leaseTimeoutSeconds": 60})");
    }
    const DistributedSettings settings = DistributedConfig::load(path);
    QCOMPARE(settings.chunkSize, 25);
    QCOMPARE(settings.leaseTimeoutSeconds, 60);
    QVERIFY(settings.adaptiveChunks);
    QVERIFY(settings.acceptRemoteWork);
    QCOMPARE(settings.discoveryPort, 28529);
    QCOMPARE(settings.dependencyMode, QStringLiteral("none"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject written = QJsonDocument::fromJson(file.readAll()).object();
    for (const char *key : {"adaptiveChunks", "targetLeaseSeconds", "maxChunkSize", "acceptRemoteWork",
             "discoveryPort", "peers", "workerCacheDirectory", "dependencyMode", "batPath", "load"})
    {
        QVERIFY2(written.contains(QLatin1String(key)), key);
    }
    QCOMPARE(written.value(QStringLiteral("chunkSize")).toInt(), 25);
}

void DistributedTest::settingsKeepValues()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("distributed.json"));
    DistributedSettings settings;
    settings.enabled = true;
    settings.token = QStringLiteral("0123456789abcdef0123");
    settings.acceptRemoteWork = false;
    settings.peers = {QStringLiteral("render-box:28528"), QStringLiteral("100.64.0.7")};
    settings.dependencyMode = QStringLiteral("bat");
    settings.maxChunkSize = 50;
    QVERIFY(DistributedConfig::save(path, settings));
    const DistributedSettings loaded = DistributedConfig::load(path);
    QVERIFY(loaded.enabled);
    QCOMPARE(loaded.token, settings.token);
    QVERIFY(!loaded.acceptRemoteWork);
    QCOMPARE(loaded.peers, settings.peers);
    QCOMPARE(loaded.dependencyMode, QStringLiteral("bat"));
    QCOMPARE(loaded.maxChunkSize, 50);
    // A short token is replaced when distributed rendering is on.
    settings.token = QStringLiteral("short");
    QVERIFY(DistributedConfig::save(path, settings));
    QVERIFY(DistributedConfig::load(path).token.size() >= DistributedConfig::minimumTokenLength);
}

void DistributedTest::verifierRejectsReplayAndTampering()
{
    const QByteArray token = "0123456789abcdef0123";
    WorkerAuth::Verifier verifier;
    const WorkerAuth::SignedRequest request = WorkerAuth::signRequest(token, "POST", "/workers/claim");
    const QByteArray header = QByteArray::fromStdString(request.header);
    QByteArray nonce;
    QCOMPARE(verifier.verify(token, header, "POST", "/workers/claim", nonce), WorkerAuth::Verifier::Result::Ok);
    QCOMPARE(nonce, request.nonce);
    // The same request again is a replay.
    QCOMPARE(verifier.verify(token, header, "POST", "/workers/claim", nonce), WorkerAuth::Verifier::Result::Invalid);
    // Another path, method or token does not match the signature.
    const QByteArray other = QByteArray::fromStdString(WorkerAuth::signRequest(token, "POST", "/workers/claim").header);
    QCOMPARE(verifier.verify(token, other, "POST", "/leases/x/complete", nonce), WorkerAuth::Verifier::Result::Invalid);
    const QByteArray third = QByteArray::fromStdString(WorkerAuth::signRequest(token, "POST", "/workers/claim").header);
    QCOMPARE(verifier.verify("another-token-000000", third, "POST", "/workers/claim", nonce), WorkerAuth::Verifier::Result::Invalid);
    QCOMPARE(verifier.verify(token, "Bearer abc", "POST", "/workers/claim", nonce), WorkerAuth::Verifier::Result::Missing);
    // Old time stamps are refused.
    const qint64 old = QDateTime::currentMSecsSinceEpoch() - WorkerAuth::clockWindowMs - 1000;
    const QByteArray oldNonce = WorkerAuth::newNonce();
    const QByteArray stale = "Rendrop-HMAC ts=" + QByteArray::number(old) + ",nonce=" + oldNonce + ",sig="
        + WorkerAuth::hmacHex(token, WorkerAuth::requestMessage("POST", "/workers/claim", old, oldNonce));
    QCOMPARE(verifier.verify(token, stale, "POST", "/workers/claim", nonce), WorkerAuth::Verifier::Result::Invalid);
    // A signed content hash must arrive unchanged, and cannot be dropped.
    const QByteArray hash(64, 'a');
    const QByteArray swapped(64, 'b');
    const QByteArray put = QByteArray::fromStdString(WorkerAuth::signRequest(token, "PUT", "/leases/x/frames/f.png", hash).header);
    QCOMPARE(verifier.verify(token, put, "PUT", "/leases/x/frames/f.png", nonce, swapped), WorkerAuth::Verifier::Result::Invalid);
    const QByteArray dropped = QByteArray::fromStdString(WorkerAuth::signRequest(token, "PUT", "/leases/x/frames/f.png", hash).header);
    QCOMPARE(verifier.verify(token, dropped, "PUT", "/leases/x/frames/f.png", nonce), WorkerAuth::Verifier::Result::Invalid);
    const QByteArray intact = QByteArray::fromStdString(WorkerAuth::signRequest(token, "PUT", "/leases/x/frames/f.png", hash).header);
    QCOMPARE(verifier.verify(token, intact, "PUT", "/leases/x/frames/f.png", nonce, hash), WorkerAuth::Verifier::Result::Ok);
    QCOMPARE(WorkerAuth::hashFile(QStringLiteral("does-not-exist.bin")), QByteArray());
}

void DistributedTest::beaconRoundTrip()
{
    const QByteArray token = "0123456789abcdef0123";
    NodeBeacon beacon;
    beacon.nodeId = QStringLiteral("node-1");
    beacon.name = QStringLiteral("渲染机");
    beacon.httpPort = 28528;
    beacon.busy = false;
    beacon.acceptRemoteWork = true;
    beacon.offersWork = true;
    beacon.rendering = true;
    beacon.timeMs = QDateTime::currentMSecsSinceEpoch();
    const std::optional<NodeBeacon> decoded = NodeDiscovery::decode(token, NodeDiscovery::encode(token, beacon), beacon.timeMs);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->nodeId, beacon.nodeId);
    QCOMPARE(decoded->name, beacon.name);
    QCOMPARE(decoded->httpPort, 28528);
    QVERIFY(!decoded->busy);
    QVERIFY(decoded->acceptRemoteWork);
    QVERIFY(decoded->offersWork);
    QVERIFY(decoded->rendering);
}

void DistributedTest::beaconRejectsWrongTokenAndStale()
{
    const QByteArray token = "0123456789abcdef0123";
    NodeBeacon beacon;
    beacon.nodeId = QStringLiteral("node-1");
    beacon.httpPort = 28528;
    beacon.timeMs = QDateTime::currentMSecsSinceEpoch();
    const QByteArray data = NodeDiscovery::encode(token, beacon);
    QVERIFY(!NodeDiscovery::decode("another-token-000000", data, beacon.timeMs));
    QVERIFY(!NodeDiscovery::decode(token, data, beacon.timeMs + WorkerAuth::clockWindowMs + 1));
    QVERIFY(!NodeDiscovery::decode(QByteArray(), data, beacon.timeMs));
    // Changing the payload breaks the signature.
    QByteArray tampered = data;
    tampered.replace("28528", "28529");
    QVERIFY(!NodeDiscovery::decode(token, tampered, beacon.timeMs));
    QVERIFY(!NodeDiscovery::decode(token, "not json", beacon.timeMs));
}

void DistributedTest::parsePeerForms()
{
    using Peer = QPair<QString, int>;
    QCOMPARE(NodeDiscovery::parsePeer(QStringLiteral("render-box")), Peer(QStringLiteral("render-box"), 28528));
    QCOMPARE(NodeDiscovery::parsePeer(QStringLiteral(" 100.64.0.7:30000 ")), Peer(QStringLiteral("100.64.0.7"), 30000));
    QCOMPARE(NodeDiscovery::parsePeer(QStringLiteral("[fd7a::1]:28600")), Peer(QStringLiteral("fd7a::1"), 28600));
    QCOMPARE(NodeDiscovery::parsePeer(QStringLiteral("fd7a::1")), Peer(QStringLiteral("fd7a::1"), 28528));
    QVERIFY(NodeDiscovery::parsePeer(QStringLiteral("host:0")).first.isEmpty());
    QVERIFY(NodeDiscovery::parsePeer(QStringLiteral("host:abc")).first.isEmpty());
    QVERIFY(NodeDiscovery::parsePeer(QStringLiteral("http://host")).first.isEmpty());
    QVERIFY(NodeDiscovery::parsePeer(QString()).first.isEmpty());
}

void DistributedTest::discoveryFindsPeerOverUdp()
{
    // Two nodes sharing one discovery port on this machine.
    const QByteArray token = "0123456789abcdef0123";
    constexpr int port = 28549;
    NodeDiscovery first(token, port, {});
    NodeDiscovery second(token, port, {});
    NodeDiscovery stranger("another-token-000000", port, {});
    if (!first.start() || !second.start() || !stranger.start())
    {
        QSKIP("discovery port is not available");
    }
    NodeBeacon a;
    a.nodeId = QStringLiteral("first");
    a.name = QStringLiteral("First");
    a.httpPort = 28528;
    a.offersWork = true;
    first.setLocalBeacon(a);
    NodeBeacon b = a;
    b.nodeId = QStringLiteral("second");
    b.name = QStringLiteral("Second");
    b.offersWork = false;
    second.setLocalBeacon(b);
    NodeBeacon c = a;
    c.nodeId = QStringLiteral("stranger");
    stranger.setLocalBeacon(c);

    QTRY_VERIFY_WITH_TIMEOUT(!second.peers().isEmpty(), 8000);
    QTRY_VERIFY_WITH_TIMEOUT(!first.peers().isEmpty(), 8000);
    // Give the stranger's beacons time to arrive too.
    QTest::qWait(NodeDiscovery::announceIntervalMs + 500);
    const QList<PeerNode> seen = second.peers();
    QCOMPARE(seen.size(), 1);
    QCOMPARE(seen.first().beacon.nodeId, QStringLiteral("first"));
    QVERIFY(seen.first().beacon.offersWork);
    QVERIFY(!seen.first().host.isEmpty());
    QCOMPARE(first.peers().size(), 1);
    QCOMPARE(first.peers().first().beacon.nodeId, QStringLiteral("second"));
    QVERIFY(!first.currentBeacon().isEmpty());
}

QTEST_MAIN(DistributedTest)
#include "distributed_test.moc"
