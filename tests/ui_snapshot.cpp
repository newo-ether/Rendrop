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
// RENDROP_SNAPSHOT_DETAILS=1 opens the project page with a sample split
// between three nodes, with leases in flight.
// RENDROP_SNAPSHOT_FOLD=<node>[,<node>...] folds those groups there ("local"
// is this machine); RENDROP_SNAPSHOT_FOLD_ANIMATE=1 animates the fold, so a
// RENDROP_SNAPSHOT_HOLD_MS shorter than 400 catches it mid-way.
// RENDROP_SNAPSHOT_NO_PEERS=1 skips the fake beacons (empty node table).
// RENDROP_SNAPSHOT_DIALOG=frame|rename captures that dialog instead, parented
// inside the queue so it inherits the same style sheets as a real one;
// =message captures a Yes/No message box with Qt's Chinese button labels.
// RENDROP_SNAPSHOT_MOTION=1 also saves mid-way frames of the queue motions.
// RENDROP_SNAPSHOT_FONT=<font file> swaps the UI font for that one.
// RENDROP_SNAPSHOT_POPUP=1 opens the Blender selector and saves the screen
// around it as <out>.popup.png, printing the popup window's attributes.
// RENDROP_SNAPSHOT_SCROLL=1 fills the queue and prints how many scroll steps
// per second a smooth scroll gets, with and without the frame driver.
// RENDROP_SNAPSHOT_SWITCH=light|dark switches the scheme with its fade.

#include <algorithm>

#include <QApplication>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHostAddress>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScreen>
#include <QStackedWidget>
#include <QStyleHints>
#include <QStandardPaths>
#include <QUdpSocket>
#include <memory>
#include <vector>

#include <QAbstractItemView>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QTranslator>
#include <QWindow>
#include <QScrollArea>
#include <QScrollBar>

#include <windows.h>

#include "app/app_controller.h"
#include "config/app_paths.h"
#include "core/render_queue.h"
#include "ui/widgets/file_bar.h"
#include "ui/motion/frame_driver.h"
#include "ui/motion/smooth_scroll.h"
#include "ui/widgets/frame_range_dialog.h"
#include "ui/motion/list_motion.h"
#include "ui/widgets/rename_dialog.h"
#include "config/distributed_settings.h"
#include "farm/node_discovery.h"
#include "ui/pages/project_page.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rendrop_style.h"
#include "ui/style/theme.h"
#include "ui/window/widget.h"

// Rewrites one family to another on every widget as it is polished, which
// covers fonts set in .ui files and in code before show().
class FontSwap : public QObject
{
public:
    FontSwap(const QString &original, const QString &family, QObject *parent):
        QObject(parent), original(original), family(family) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish && watched->isWidgetType())
        {
            auto *target = static_cast<QWidget *>(watched);
            QFont font = target->font();
            if (font.family() == original)
            {
                font.setFamily(family);
                target->setFont(font);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QString original;
    QString family;
};

// Counts the window's repaints (UpdateRequest: one per frame that reaches
// the screen) and the time each takes, painting and flushing included.
class FrameMeter : public QObject
{
public:
    using QObject::QObject;
    int frames = 0;
    qint64 busyNs = 0;
    qint64 worstNs = 0;

    void reset()
    {
        frames = 0;
        busyNs = 0;
        worstNs = 0;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::UpdateRequest || reentered)
        {
            return QObject::eventFilter(watched, event);
        }
        QElapsedTimer clock;
        clock.start();
        reentered = true;
        QCoreApplication::sendEvent(watched, event);
        reentered = false;
        const qint64 spent = clock.nsecsElapsed();
        ++frames;
        busyNs += spent;
        worstNs = std::max(worstNs, spent);
        return true;
    }

private:
    bool reentered = false;
};

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    Look::selectFontEngine();
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
    RendropStyle::install();
    // Follows the scheme forced above, as the app does with "Follow system".
    Theme::init(Theme::Mode::System);
    Look::installTextRendering();
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_DRIVER") == 1)
    {
        FrameDriver::installForScreen();
    }
    QCoreApplication::setApplicationName(QStringLiteral("RendropSnapshot"));
    QFontDatabase::addApplicationFont(":/font/MiOutfit.ttf");
    QFontDatabase::addApplicationFont(":/font/JetBrainsMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/font/JetBrainsMono-Bold.ttf");
    // RENDROP_SNAPSHOT_FONT=<font file> draws everything that asks for the UI
    // font with that font instead, to compare typefaces. Substitution only
    // applies to missing families, so the family is swapped on each widget.
    const QString fontOverride = qEnvironmentVariable("RENDROP_SNAPSHOT_FONT");
    if (!fontOverride.isEmpty())
    {
        const int fontId = QFontDatabase::addApplicationFont(fontOverride);
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
        {
            app.installEventFilter(new FontSwap(Look::pixelFamily(), families.first(), &app));
            Look::setPixelFamily(families.first());
        }
    }
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

    AppController controller;
    Widget widget(&controller, 0);
    controller.start();
    widget.resize(width, height);
    widget.show();
    const int page = qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_PAGE");
    const QList<NavButton *> navigation = widget.findChildren<NavButton *>();
    const auto settle = [&app](int ms) {
        QElapsedTimer slice;
        slice.start();
        while (slice.elapsed() < ms)
        {
            app.processEvents(QEventLoop::AllEvents, 5);
        }
    };
    // RENDROP_SNAPSHOT_FADE=1 saves the page crossfade half way, queue to
    // settings, as <out>.fade.png, and reports whether the overlay went away.
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_FADE") == 1 && navigation.size() > 2)
    {
        settle(300);
        navigation[2]->click();
        settle(100);
        QString path = output;
        path.insert(path.lastIndexOf(QLatin1Char('.')), QStringLiteral(".fade"));
        widget.grab().save(path);
        settle(300);
        const bool left = widget.findChild<QWidget *>(QStringLiteral("rendropPageFade")) != nullptr;
        std::printf("fade overlay after 400ms: %s\n", left ? "still present" : "gone");
        std::fflush(stdout);
        navigation[0]->click();
    }
    if (page > 0 && page < navigation.size())
    {
        navigation[page]->click();
    }
    // Lets a page crossfade finish before anything is captured.
    settle(300);
    // RENDROP_SNAPSHOT_CJK=1 puts Chinese text on the visible labels, to check
    // the font engine's fallback for the translation.
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_CJK") == 1)
    {
        for (QLabel *label : widget.findChildren<QLabel *>())
        {
            if (label->isVisible() && !label->text().isEmpty() && label->objectName() != QLatin1String("rendropLabel"))
            {
                label->setText(QStringLiteral("渲染队列 设置 节点 Launch at login 开机启动"));
            }
        }
        settle(100);
    }
    // RENDROP_SNAPSHOT_FONTS=1 lists the label fonts' rendering settings.
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_FONTS") == 1)
    {
        int shown = 0;
        for (QLabel *label : widget.findChildren<QLabel *>())
        {
            if (label->isVisible() && !label->text().isEmpty() && shown++ < 8)
            {
                const QFont font = label->font();
                std::printf("%-30s family=%s strategy=0x%x hinting=%d\n",
                    qPrintable(label->text().left(30)), qPrintable(font.family()),
                    static_cast<unsigned>(font.styleStrategy()), static_cast<int>(font.hintingPreference()));
            }
        }
        std::fflush(stdout);
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
            sampleBar = std::make_unique<FileBar>(content, sample);
            list->insertWidget(0, sampleBar.get());
        }
    }
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_DETAILS") == 1)
    {
        ProjectInfo sample(QUuid::createUuid(), QStringLiteral("shot_010_lighting"), QStringLiteral("D:/shots/shot_010.blend"),
            QString(), ProjectState::Rendering, 1, 120, 1, 1920, 1080, 100, QStringLiteral("CYCLES"), 0, 120);
        // 0-29 local, 30-49 QUANTUM-WORK, 50-59 STUDIO-03, 60-69 local again.
        sample.completedRanges = {{0, 69}};
        sample.finishedFrame = 70;
        sample.frameOwners = {
            FrameOwnerRanges {QStringLiteral("QUANTUM-WORK"), {{30, 49}}},
            FrameOwnerRanges {QStringLiteral("STUDIO-03"), {{50, 59}}}
        };
        sample.frameLeases = {
            FrameOwnerRanges {QString(), {{70, 74}}},
            FrameOwnerRanges {QStringLiteral("QUANTUM-WORK"), {{75, 84}}},
            FrameOwnerRanges {QStringLiteral("STUDIO-03"), {{85, 89}}}
        };
        auto *details = widget.findChild<ProjectPage *>();
        auto *stack = widget.findChild<QStackedWidget *>(QStringLiteral("pageStack"));
        if (details && stack)
        {
            details->showPreview(sample);
            stack->setCurrentIndex(3);
            auto *grid = details->findChild<FrameGrid *>();
            const QString folded = qEnvironmentVariable("RENDROP_SNAPSHOT_FOLD");
            const bool animate = qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_FOLD_ANIMATE") == 1;
            if (grid && !folded.isEmpty())
            {
                for (const QString &node : folded.split(QLatin1Char(','), Qt::SkipEmptyParts))
                {
                    grid->setGroupExpanded(node == QStringLiteral("local") ? QString() : node, false, animate);
                }
            }
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
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_POPUP") == 1)
    {
        if (auto *combo = widget.findChild<QComboBox *>(QStringLiteral("selectorComboBox")))
        {
            combo->addItem(QStringLiteral("Blender 5.2.0 LTS"));
            combo->showPopup();
            settle(800);
            QWidget *popup = combo->view()->window();
            const HWND hwnd = reinterpret_cast<HWND>(popup->winId());
            const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            fprintf(stderr, "popup class=%s translucent=%d alphaFormat=%d layered=%d noshadow=%d opacity=%.2f\n",
                popup->metaObject()->className(),
                popup->testAttribute(Qt::WA_TranslucentBackground),
                popup->windowHandle() ? popup->windowHandle()->format().hasAlpha() : -1,
                (exStyle & WS_EX_LAYERED) ? 1 : 0,
                popup->windowFlags().testFlag(Qt::NoDropShadowWindowHint),
                popup->windowOpacity());
            const QRect area = popup->frameGeometry().adjusted(-12, -12, 12, 12);
            QScreen *screen = popup->screen();
            const QPoint origin = screen->geometry().topLeft();
            screen->grabWindow(0, area.x() - origin.x(), area.y() - origin.y(), area.width(), area.height())
                .save(QFileInfo(output).path() + QStringLiteral("/") + QFileInfo(output).completeBaseName()
                    + QStringLiteral(".popup.png"));
            combo->hidePopup();
            settle(300);
        }
    }
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_SCROLL") == 1)
    {
        QWidget *content = widget.findChild<QWidget *>(QStringLiteral("scrollAreaContent"));
        auto *list = widget.findChild<QVBoxLayout *>(QStringLiteral("scrollAreaVerticalLayout"));
        auto *area = widget.findChild<QScrollArea *>(QStringLiteral("scrollArea"));
        std::vector<std::unique_ptr<FileBar>> bars;
        if (content && list && area)
        {
            for (int index = 0; index < 40; ++index)
            {
                const ProjectInfo sample(QUuid::createUuid(), QStringLiteral("shot_%1").arg(index, 3, 10, QLatin1Char('0')),
                    QString(), QString(), ProjectState::Queued, 1, 250, 1, 1920, 1080, 100, QStringLiteral("CYCLES"), 0, 250);
                bars.push_back(std::make_unique<FileBar>(content, sample));
                list->insertWidget(0, bars.back().get());
            }
            settle(500);
            QScrollBar *bar = area->verticalScrollBar();
            int steps = 0;
            QElapsedTimer clock;
            qint64 last = 0;
            qint64 worst = 0;
            const auto counter = QObject::connect(bar, &QScrollBar::valueChanged, [&]() {
                ++steps;
                const qint64 now = clock.nsecsElapsed();
                if (last > 0)
                {
                    worst = std::max(worst, now - last);
                }
                last = now;
            });
            auto *meter = new FrameMeter(&widget);
            widget.installEventFilter(meter);
            const auto report = [&](const char *name, double seconds) {
                fprintf(stderr, "%s frames=%d rate=%.1f/s mean=%.2fms worst=%.2fms\n", name, meter->frames,
                    meter->frames / seconds, meter->frames ? meter->busyNs / 1e6 / meter->frames : 0.0,
                    meter->worstNs / 1e6);
                meter->reset();
            };
            // Animation ticks with nothing to paint: the driver's own rate.
            int ticks = 0;
            const auto countTicks = [&widget, &ticks](int ms) {
                auto *control = new QVariantAnimation(&widget);
                control->setDuration(ms);
                control->setStartValue(0.0);
                control->setEndValue(1.0);
                QObject::connect(control, &QVariantAnimation::valueChanged, [&ticks]() { ++ticks; });
                control->start(QAbstractAnimation::DeleteWhenStopped);
            };
            countTicks(1000);
            settle(1000);
            fprintf(stderr, "idle ticks=%d/s\n", ticks);
            ticks = 0;
            countTicks(3000);
            clock.start();
            // Long jumps back and forth so the motion never rests.
            for (int round = 0; round < 6; ++round)
            {
                SmoothScroll::scrollTo(area, round % 2 == 0 ? bar->maximum() : 0);
                settle(500);
            }
            const double seconds = clock.nsecsElapsed() / 1e9;
            QObject::disconnect(counter);
            fprintf(stderr, "scroll steps=%d seconds=%.2f rate=%.1f/s worst-gap=%.1fms screen=%.1fHz\n",
                steps, seconds, steps / seconds, worst / 1e6, widget.screen()->refreshRate());
            report("scroll", seconds);
            // Hover lift: pointer moving down the list, a new card every 120ms.
            settle(400);
            meter->reset();
            clock.restart();
            for (int index = 0; index < 24; ++index)
            {
                FileBar *target = bars[static_cast<size_t>(bars.size() - 1 - index)].get();
                QEvent enter(QEvent::Enter);
                QCoreApplication::sendEvent(target, &enter);
                settle(120);
                QEvent leave(QEvent::Leave);
                QCoreApplication::sendEvent(target, &leave);
            }
            report("hover", clock.nsecsElapsed() / 1e9);
            // Page crossfade, queue <-> settings.
            settle(400);
            if (navigation.size() > 2)
            {
                meter->reset();
                clock.restart();
                for (int round = 0; round < 8; ++round)
                {
                    navigation[round % 2 == 0 ? 2 : 0]->click();
                    settle(300);
                }
                report("fade", clock.nsecsElapsed() / 1e9);
                // Section jumps on the settings page.
                navigation[2]->click();
                settle(400);
                QScrollArea *settingsArea = nullptr;
                for (QScrollArea *candidate : widget.findChildren<QScrollArea *>())
                {
                    if (candidate->isVisible() && candidate != area)
                    {
                        settingsArea = candidate;
                    }
                }
                if (settingsArea)
                {
                    QScrollBar *settingsBar = settingsArea->verticalScrollBar();
                    meter->reset();
                    ticks = 0;
                    countTicks(3000);
                    clock.restart();
                    for (int round = 0; round < 6; ++round)
                    {
                        SmoothScroll::scrollTo(settingsArea, round % 2 == 0 ? settingsBar->maximum() : 0);
                        settle(500);
                    }
                    fprintf(stderr, "settings max=%d ticks=%d\n", settingsBar->maximum(), ticks);
                    report("settings", clock.nsecsElapsed() / 1e9);
                }
            }
            widget.removeEventFilter(meter);
            for (auto &sample : bars)
            {
                sample.reset();
            }
        }
    }
    // RENDROP_SNAPSHOT_MOTION=1 runs the queue list motions on sample bars and
    // saves a mid-way frame of each next to the output: <out>.appear.png,
    // .slide.png, .vanish.png and .log.png (the log card growing).
    if (qEnvironmentVariableIntValue("RENDROP_SNAPSHOT_MOTION") == 1)
    {
        QWidget *content = widget.findChild<QWidget *>(QStringLiteral("scrollAreaContent"));
        auto *list = widget.findChild<QVBoxLayout *>(QStringLiteral("scrollAreaVerticalLayout"));
        const auto pump = [&app](int ms) {
            QElapsedTimer slice;
            slice.start();
            while (slice.elapsed() < ms)
            {
                app.processEvents(QEventLoop::AllEvents, 5);
            }
        };
        const auto frame = [&widget, &output](const char *name) {
            QString path = output;
            path.insert(path.lastIndexOf(QLatin1Char('.')), QLatin1Char('.') + QLatin1String(name));
            widget.grab().save(path);
        };
        std::vector<std::unique_ptr<FileBar>> bars;
        for (int index = 0; index < 3 && content && list; ++index)
        {
            const ProjectInfo sample(QUuid::createUuid(), QStringLiteral("shot_0%10").arg(index + 1), QString(), QString(),
                ProjectState::Queued, 1, 250, 1, 1920, 1080, 100, QStringLiteral("CYCLES"), 0, 250);
            bars.push_back(std::make_unique<FileBar>(content, sample));
            list->insertWidget(index, bars.back().get());
            list->activate();
            if (index == 2)
            {
                ListMotion::appear(bars.back().get(), bars.back()->getDropShadowWidget());
            }
        }
        if (bars.size() == 3)
        {
            pump(90);
            frame("appear");
            pump(400);
            // Swap the first two, as a move-down does.
            const QPoint first = bars[0]->pos();
            const QPoint second = bars[1]->pos();
            list->removeWidget(bars[0].get());
            list->insertWidget(1, bars[0].get());
            list->activate();
            ListMotion::slide(bars[0].get(), first);
            ListMotion::slide(bars[1].get(), second);
            pump(130);
            frame("slide");
            pump(400);
            // Remove the middle bar; the last one closes the gap.
            const QPoint last = bars[2]->pos();
            ListMotion::vanish(bars[0].get(), bars[0]->getDropShadowWidget());
            list->removeWidget(bars[0].get());
            bars[0].reset();
            list->activate();
            ListMotion::slide(bars[2].get(), last);
            pump(90);
            frame("vanish");
            pump(400);
        }
        // RENDROP_SNAPSHOT_DRAG=<fake blender> adds real jobs and drags the
        // first bar's grip down past two others: <out>.drag.png mid-way,
        // .dragend.png after the drop settled.
        const QString dragBlender = qEnvironmentVariable("RENDROP_SNAPSHOT_DRAG");
        RenderQueue *queue = controller.queue();
        QTemporaryDir dragDirectory;
        if (!dragBlender.isEmpty() && queue && dragDirectory.isValid())
        {
            bars.clear();
            list->activate();
            std::vector<QUuid> dragJobs;
            for (int index = 0; index < 4; ++index)
            {
                const QString path = QDir(dragDirectory.path()).filePath(QStringLiteral("drag_%1.blend").arg(index + 1));
                QFile blend(path);
                if (blend.open(QIODevice::WriteOnly))
                {
                    blend.write("fake");
                }
                blend.close();
                dragJobs.push_back(queue->addJob(QStringLiteral("drag_%1").arg(index + 1), path, dragBlender));
            }
            pump(2500);
            std::vector<FileBar *> live;
            for (FileBar *bar : content->findChildren<FileBar *>(Qt::FindDirectChildrenOnly))
            {
                if (bar->isVisible())
                {
                    live.push_back(bar);
                }
            }
            std::sort(live.begin(), live.end(), [](FileBar *a, FileBar *b) { return a->y() < b->y(); });
            QWidget *handle = live.empty() ? nullptr : live.front()->findChild<QWidget *>(QStringLiteral("dragHandle"));
            if (handle && live.size() >= 3)
            {
                // Global positions: the handle itself travels with the bar.
                const auto send = [handle](QEvent::Type type, QPointF global, Qt::MouseButtons buttons) {
                    QMouseEvent event(type, handle->mapFromGlobal(global), handle->window()->mapFromGlobal(global), global,
                        type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
                    QCoreApplication::sendEvent(handle, &event);
                };
                const QPointF start = handle->mapToGlobal(QRectF(handle->rect()).center());
                const qreal pitch = live[1]->y() - live[0]->y();
                send(QEvent::MouseButtonPress, start, Qt::LeftButton);
                QPointF pointer = start;
                for (int step = 1; step <= 12; ++step)
                {
                    pointer = start + QPointF(0, pitch * 1.6 * step / 12.0);
                    send(QEvent::MouseMove, pointer, Qt::LeftButton);
                    pump(16);
                }
                pump(120);
                frame("drag");
                send(QEvent::MouseButtonRelease, pointer, Qt::NoButton);
                pump(500);
                frame("dragend");
            }
            // The snapshot config persists between runs; leave no jobs behind.
            for (const QUuid &id : dragJobs)
            {
                queue->removeJob(id);
            }
            pump(300);
        }
        if (auto *logView = widget.findChild<QPlainTextEdit *>(QStringLiteral("logView")))
        {
            for (int i = 1; i <= 40; ++i)
                logView->appendPlainText(QStringLiteral("[17:58:%1] sample log line %2")
                                             .arg(i % 60, 2, 10, QLatin1Char('0'))
                                             .arg(i));
        }
        if (auto *toggle = widget.findChild<QPushButton *>(QStringLiteral("logToggleButton")))
        {
            toggle->click();
            pump(110);
            frame("log");
            pump(400);
            frame("logend");
        }
    }
    // RENDROP_SNAPSHOT_HOVER=<objectName> lifts that card's shadow as if the
    // pointer rested on it.
    const QString hoverName = qEnvironmentVariable("RENDROP_SNAPSHOT_HOVER");
    if (!hoverName.isEmpty())
    {
        if (QWidget *target = widget.findChild<QWidget *>(hoverName))
        {
            QEvent enter(QEvent::Enter);
            QCoreApplication::sendEvent(target, &enter);
            QElapsedTimer settle;
            settle.start();
            while (settle.elapsed() < 500)
            {
                app.processEvents(QEventLoop::AllEvents, 20);
            }
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
    // RENDROP_SNAPSHOT_SWITCH=light|dark switches the scheme on the shown
    // window, saves <out>.mid.png half-way through the cross-fade and then
    // the settled result as <out>.
    const QByteArray switchTo = qgetenv("RENDROP_SNAPSHOT_SWITCH");
    if (!switchTo.isEmpty())
    {
        Theme::setMode(switchTo == "light" ? Theme::Mode::Light : Theme::Mode::Dark);
        const auto pumpFor = [&app](int ms) {
            QElapsedTimer settle;
            settle.start();
            while (settle.elapsed() < ms)
            {
                app.processEvents(QEventLoop::AllEvents, 5);
            }
        };
        pumpFor(160);
        widget.grab().save(output + QStringLiteral(".mid.png"));
        pumpFor(400);
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
        else if (dialogKind == "message")
        {
            // Same setup as MessageDialog::show, with Qt's Chinese strings.
            auto *qtTranslator = new QTranslator(&app);
            if (qtTranslator->load(QStringLiteral(":/i18n/qtbase_zh_CN.qm")))
            {
                app.installTranslator(qtTranslator);
            }
            auto *box = new QMessageBox(&widget);
            box->setIcon(QMessageBox::Warning);
            box->setText(QStringLiteral("确定要删除这个任务吗？"));
            box->setWindowTitle(QStringLiteral("Warning"));
            box->setFont(Look::pixelFont(12));
            box->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            dialog = box;
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
