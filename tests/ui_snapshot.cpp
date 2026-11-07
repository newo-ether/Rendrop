// Renders the main window to a PNG for visual review, without touching the
// user's configuration: QStandardPaths test mode redirects the config
// directory. A few signed fake beacons sent to the loopback discovery port
// populate the node panel.
//
// Usage: ui_snapshot <output.png> [width height]

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QHostAddress>
#include <QStandardPaths>
#include <QUdpSocket>

#include "app_paths.h"
#include "distributed_settings.h"
#include "node_discovery.h"
#include "widget.h"

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("RendropSnapshot"));
    QFontDatabase::addApplicationFont(":/font/PixelMixed.ttf");
    QFontDatabase::addApplicationFont(":/font/BitcountSingle-Bold.ttf");
    if (argc < 2)
    {
        return 1;
    }
    const QString output = QString::fromLocal8Bit(argv[1]);
    const int width = argc >= 4 ? QString::fromLocal8Bit(argv[2]).toInt() : 1200;
    const int height = argc >= 4 ? QString::fromLocal8Bit(argv[3]).toInt() : 800;

    AppPaths::ensureConfigDirectory();
    DistributedSettings settings;
    settings.enabled = true;
    settings.token = QStringLiteral("snapshot-token-0123456789abcdef");
    settings.discoveryPort = 28539;
    DistributedConfig::save(AppPaths::configFile(QStringLiteral("distributed.json")), settings);

    Widget widget(0);
    widget.resize(width, height);
    widget.show();

    QUdpSocket sender;
    const auto beacon = [&](const QString &id, const QString &name, bool busy, bool accept, bool offers, bool rendering) {
        NodeBeacon value;
        value.nodeId = id;
        value.name = name;
        value.httpPort = 28528;
        value.busy = busy;
        value.acceptRemoteWork = accept;
        value.offersWork = offers;
        value.rendering = rendering;
        value.timeMs = QDateTime::currentMSecsSinceEpoch();
        sender.writeDatagram(NodeDiscovery::encode(settings.token.toUtf8(), value), QHostAddress::LocalHost, 28539);
    };
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2500)
    {
        beacon(QStringLiteral("peer-a"), QStringLiteral("QUANTUM-WORK"), false, true, true, true);
        beacon(QStringLiteral("peer-b"), QStringLiteral("STUDIO-03"), false, true, false, false);
        beacon(QStringLiteral("peer-c"), QStringLiteral("NE-LAPTOP"), true, true, false, false);
        beacon(QStringLiteral("peer-d"), QStringLiteral("RENDER-BOX"), true, false, false, false);
        // A beacon signed with another token must not show up.
        NodeBeacon forged;
        forged.nodeId = QStringLiteral("forged");
        forged.name = QStringLiteral("FORGED");
        forged.httpPort = 28528;
        forged.timeMs = QDateTime::currentMSecsSinceEpoch();
        sender.writeDatagram(NodeDiscovery::encode("wrong-token-0000000000", forged), QHostAddress::LocalHost, 28539);
        QElapsedTimer slice;
        slice.start();
        while (slice.elapsed() < 250)
        {
            app.processEvents(QEventLoop::AllEvents, 50);
        }
    }
    const bool saved = widget.grab().save(output);
    return saved ? 0 : 2;
}
