// Renders the main window to a PNG for visual review, without touching the
// user's configuration: QStandardPaths test mode redirects the config
// directory. A few signed fake beacons sent to the loopback discovery port
// populate the node panel.
//
// Usage: ui_snapshot <output.png> [width height]
// RENDROP_SNAPSHOT_SCHEME=light|dark forces Qt's color scheme, so the
// light-mode rendering can be checked on a dark system and vice versa.
// RENDROP_SNAPSHOT_SCREEN=1 grabs the window from the screen instead,
// including the native shadow and corners around the custom frame.
// RENDROP_SNAPSHOT_PAGE=0|1|2 captures the queue, nodes or settings page.
// RENDROP_SNAPSHOT_BLENDER=<blender.exe> registers a Blender version first.
// RENDROP_SNAPSHOT_FILEBAR=1 adds a sample project bar to the queue.
// RENDROP_SNAPSHOT_LOG=1 opens the log card.
// RENDROP_SNAPSHOT_NO_PEERS=1 skips the fake beacons (empty node table).
// RENDROP_SNAPSHOT_DIALOG=frame|rename captures that dialog instead, parented
// inside the queue so it inherits the same style sheets as a real one.

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHostAddress>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScreen>
#include <QStyleHints>
#include <QStandardPaths>
#include <QUdpSocket>
#include <memory>

#include "app_paths.h"
#include "drop_shadow_renderer.h"
#include "file_bar.h"
#include "frame_range_dialog.h"
#include "rename_dialog.h"
#include "distributed_settings.h"
#include "node_discovery.h"
#include "look_widgets.h"
#include "widget.h"

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication app(argc, argv);
    const QByteArray scheme = qgetenv("RENDROP_SNAPSHOT_SCHEME");
    if (scheme == "light")
    {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    }
    else if (scheme == "dark")
    {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    }
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

    const QString blender = qEnvironmentVariable("RENDROP_SNAPSHOT_BLENDER");
    if (!blender.isEmpty())
    {
        QFile versions(AppPaths::configFile(QStringLiteral("blender_versions.cfg")));
        if (versions.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            // "<version>|<path>" per line, as BlenderVersionManager writes it.
            versions.write("Blender 4.5.2|");
            versions.write(QDir::toNativeSeparators(blender).toUtf8());
            versions.write("\n");
        }
    }
    else
    {
        // The test config directory persists between runs; start empty.
        QFile::remove(AppPaths::configFile(QStringLiteral("blender_versions.cfg")));
    }

    // Declared before the window so it outlives the sample bar's shadow.
    DropShadowRenderer barShadows;
    Widget widget(0);
    widget.resize(width, height);
    widget.show();
    const int page = qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_PAGE");
    const QList<NavButton *> navigation = widget.findChildren<NavButton *>();
    if (page > 0 && page < navigation.size())
    {
        navigation[page]->click();
    }

    // Widget only deletes the bars it tracks; this one goes first, while the
    // window it lives in is still intact.
    std::unique_ptr<FileBar> sampleBar;
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_FILEBAR") == 1)
    {
        // Built directly: a real job needs a readable .blend and Blender.
        QWidget *content = widget.findChild<QWidget *>(QStringLiteral("scrollAreaContent"));
        auto *list = widget.findChild<QVBoxLayout *>(QStringLiteral("scrollAreaVerticalLayout"));
        const ProjectInfo sample(QUuid::createUuid(), QStringLiteral("shot_010_lighting"), QString(), QString(),
            ProjectState::Queued, 1, 250, 1, 1920, 1080, 100, QStringLiteral("CYCLES"), 0, 250);
        if (content && list)
        {
            sampleBar = std::make_unique<FileBar>(content, &barShadows, sample);
            list->insertWidget(0, sampleBar.get());
        }
    }
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_LOG") == 1)
    {
        if (auto *toggle = widget.findChild<QPushButton *>(QStringLiteral("logToggleButton")))
        {
            toggle->click();
        }
    }

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
    const bool noPeers = qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_NO_PEERS") == 1;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2500)
    {
        if (noPeers)
        {
            app.processEvents(QEventLoop::AllEvents, 50);
            continue;
        }
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
    // RENDROP_SNAPSHOT_HOLD_MS=N keeps the window open for external probes
    // (for example WM_NCHITTEST checks) before the capture.
    const int holdMs = qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_HOLD_MS");
    if (holdMs > 0)
    {
        QElapsedTimer hold;
        hold.start();
        while (hold.elapsed() < holdMs)
        {
            app.processEvents(QEventLoop::AllEvents, 50);
        }
    }
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_SCREEN") == 1)
    {
        widget.raise();
        widget.activateWindow();
        QElapsedTimer settle;
        settle.start();
        while (settle.elapsed() < 500)
        {
            app.processEvents(QEventLoop::AllEvents, 50);
        }
        // Desktop capture around the window, margin included for the shadow.
        QScreen *screen = widget.screen();
        const QRect area = widget.frameGeometry().adjusted(-24, -24, 24, 24);
        const QRect local = area.translated(-screen->geometry().topLeft());
        const bool saved = screen->grabWindow(0, local.x(), local.y(), local.width(), local.height()).save(output);
        return saved ? 0 : 2;
    }
    const QByteArray dialogKind = qgetenv("RENDROP_SNAPSHOT_DIALOG");
    if (!dialogKind.isEmpty())
    {
        QWidget *host = widget.findChild<QWidget *>(QStringLiteral("scrollAreaContent"));
        QDialog *dialog = nullptr;
        if (dialogKind == "frame")
        {
            dialog = new FrameRangeDialog(1, 250, 1, host);
        }
        else
        {
            dialog = new RenameDialog(QStringLiteral("shot_010_lighting"), host);
        }
        dialog->show();
        QElapsedTimer settle;
        settle.start();
        while (settle.elapsed() < 300)
        {
            app.processEvents(QEventLoop::AllEvents, 50);
        }
        const bool saved = dialog->grab().save(output);
        delete dialog;
        return saved ? 0 : 2;
    }
    const bool saved = widget.grab().save(output);
    return saved ? 0 : 2;
}
