// widget.cpp

#include "ui/window/widget.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <QWKWidgets/widgetwindowagent.h>

#include "app/app_controller.h"
#include "config/app_paths.h"
#include "config/app_preferences.h"
#include "config/language_setting.h"
#include "core/blender_file_info.h"
#include "core/blender_version_manager.h"
#include "core/render_queue.h"
#include "farm/node_agent.h"
#include "ui/motion/page_fade.h"
#include "ui/pages/nodes_page.h"
#include "ui/pages/project_page.h"
#include "ui/pages/queue_page.h"
#include "ui/pages/settings_page.h"
#include "ui/style/drop_shadow_widget.h"
#include "ui/style/look_widgets.h"
#include "ui/widgets/caption_button.h"
#include "ui/widgets/message_dialog.h"
#include "ui_widget.h"

Widget::Widget(AppController *controller, int languageIndex, QWidget *parent):
    QWidget(parent),
    ui(new Ui::widget),
    controller(controller),
    blenderVersionManager(controller->blenderVersions()),
    renderQueue(controller->queue()),
    languageIndex(languageIndex)
{
    // QWindowKit must take over the native frame before the window gets its
    // size constraints from setupUi().
    windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);
    ui->setupUi(this);
    // Before any shadow exists, so none flashes when they are off.
    DropShadowWidget::setShadowsEnabled(AppPreferencesFile::load().shadows);
    setupWindowFrame();
    setAcceptDrops(true);
    qRegisterMetaType<BlenderFileInfo>("BlenderFileInfo");
    qRegisterMetaType<ProjectInfo>("ProjectInfo");

    queuePage = new QueuePage(controller, ui->pageStack);
    ui->pageStack->addWidget(queuePage);
    QObject::connect(queuePage, &QueuePage::notificationRequested, this, &Widget::notify);
    QObject::connect(renderQueue, &RenderQueue::renderingChanged, this, [this](bool rendering) {
        if (settingsPage)
        {
            settingsPage->setBlenderEditingEnabled(!rendering);
        }
        updateStatusChip();
    });
    QObject::connect(controller, &AppController::nodeStateChanged, this, &Widget::updateStatusChip);
    QObject::connect(controller, &AppController::nodeAgentReplaced, this, [this](NodeAgent *agent) {
        if (nodesPage)
        {
            nodesPage->setAgent(agent);
        }
    });
    createPages();
    installRoundedSurfaces();
    setupTray();

    // The title bar status, node counts and the ETA follow lease activity
    // that has no signal of its own.
    QTimer *statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    QObject::connect(statusTimer, &QTimer::timeout, this, [this]() {
        updateStatusChip();
        queuePage->refreshLiveInfo();
    });
    statusTimer->start();

    updateStatusChip();
}

Widget::~Widget()
{
    // The settings page writes a pending edit when it goes; nothing may be
    // rebuilt from it while the window is torn down.
    if (settingsPage)
    {
        QObject::disconnect(settingsPage, nullptr, this, nullptr);
    }
    // The back end belongs to the controller and outlives the window.
    QObject::disconnect(controller, nullptr, this, nullptr);
    QObject::disconnect(renderQueue, nullptr, this, nullptr);
    for (DropShadowWidget *dropShadowWidget : dropShadowWidgets)
    {
        delete dropShadowWidget;
    }
    delete ui;
}

void Widget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && maximizeButton)
    {
        const bool maximized = isMaximized();
        maximizeButton->setMaximized(maximized);
        maximizeButton->setToolTip(maximized ? tr("Restore") : tr("Maximize"));
    }
    QWidget::changeEvent(event);
}

void Widget::setupWindowFrame()
{
    // Native parts (system menu, Snap Layout flyout) follow the app's scheme,
    // not the system one.
    Look::onThemeChanged(this, [this]() {
        windowAgent->setWindowAttribute(QStringLiteral("dark-mode"), !Theme::isLight());
    });

    CaptionButton *minimizeButton = new CaptionButton(CaptionButton::Kind::Minimize, ui->titleBar);
    maximizeButton = new CaptionButton(CaptionButton::Kind::Maximize, ui->titleBar);
    CaptionButton *closeButton = new CaptionButton(CaptionButton::Kind::Close, ui->titleBar);
    minimizeButton->setToolTip(tr("Minimize"));
    maximizeButton->setToolTip(tr("Maximize"));
    closeButton->setToolTip(tr("Close"));
    // Flush with the top-right corner like native caption buttons; DWM
    // rounds the window corner over the close button.
    ui->titleBarLayout->setSpacing(0);
    ui->titleBarLayout->insertSpacing(1, 14);
    ui->titleBarLayout->addWidget(minimizeButton, 0, Qt::AlignTop);
    ui->titleBarLayout->addWidget(maximizeButton, 0, Qt::AlignTop);
    ui->titleBarLayout->addWidget(closeButton, 0, Qt::AlignTop);
    QObject::connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    QObject::connect(maximizeButton, &QAbstractButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    QObject::connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    // Empty title bar space (the status chip included) drags the window.
    // Registering the system buttons gives Windows 11 its Snap Layout
    // flyout on the maximize button.
    windowAgent->setTitleBar(ui->titleBar);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, minimizeButton);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, maximizeButton);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
}

void Widget::closeEvent(QCloseEvent *event)
{
    // The close button only hides the window: rendering, the farm server
    // and remote work keep running in the tray.
    event->ignore();
    if (trayIcon && trayIcon->isVisible())
    {
        hide();
        // Once per user, not per run: the file remembers it was shown.
        AppPreferences preferences = AppPreferencesFile::load();
        if (!preferences.trayHintShown)
        {
            preferences.trayHintShown = true;
            AppPreferencesFile::save(preferences);
            trayIcon->showMessage(
                QStringLiteral("Rendrop"),
                tr("Rendrop keeps running in the tray. Right-click the icon to quit."),
                QSystemTrayIcon::Information,
                4000
            );
        }
        return;
    }
    // No tray on this desktop: closing quits.
    requestQuit();
}

void Widget::setupTray()
{
    if (trayIcon)
    {
        return;
    }
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        // Started before the taskbar: try again once a second for a minute.
        if (++trayAttempts < maxTrayAttempts)
        {
            QTimer::singleShot(1000, this, &Widget::setupTray);
        }
        else if (backgroundStart && isHidden())
        {
            // No tray at all: a hidden window would be unreachable.
            showMinimized();
        }
        return;
    }
    trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon/icon.ico")), this);
    trayIcon->setToolTip(QStringLiteral("Rendrop"));
    auto *menu = new QMenu(this);
    menu->setFont(Look::pixelFont(10));
    QAction *showAction = menu->addAction(tr("Show Rendrop"));
    QAction *quitAction = menu->addAction(tr("Quit"));
    QObject::connect(showAction, &QAction::triggered, this, &Widget::showFromTray);
    QObject::connect(quitAction, &QAction::triggered, this, &Widget::requestQuit);
    trayIcon->setContextMenu(menu);
    QObject::connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        {
            showFromTray();
        }
    });
    QObject::connect(trayIcon, &QSystemTrayIcon::messageClicked, this, &Widget::showFromTray);
    trayIcon->show();
}

void Widget::startInBackground()
{
    // Read by setupTray's last retry; the window itself just stays hidden.
    backgroundStart = true;
}

void Widget::showFromTray()
{
    if (isMinimized())
    {
        showNormal();
    }
    else
    {
        show();
    }
    raise();
    activateWindow();
}

bool Widget::confirmQuit()
{
    const std::vector<ProjectInfo> projects = renderQueue->snapshots();
    const bool loading = std::any_of(projects.begin(), projects.end(), [](const ProjectInfo &project) {
        return project.state == ProjectState::Loading;
    });
    if (!renderQueue->isRendering() && !loading)
    {
        return true;
    }
    // The question needs a visible parent.
    showFromTray();
    const QString text = renderQueue->isRendering()
        ? tr("Some files are still rendering. Are you sure to exit?")
        : tr("Some files are still loading. Are you sure to exit?");
    return MessageDialog::warning(this, tr("Warning"), text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void Widget::requestQuit()
{
    if (confirmQuit())
    {
        if (trayIcon)
        {
            trayIcon->hide();
        }
        QCoreApplication::exit(0);
    }
}

void Widget::notify(const QString &title, const QString &text, bool error)
{
    if (trayIcon && trayIcon->isVisible() && QSystemTrayIcon::supportsMessages())
    {
        trayIcon->showMessage(title, text, error ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information, 6000);
    }
}

// Dropped .blend files anywhere on the window go to the queue.
static QStringList droppedBlendFiles(const QMimeData *mimeData)
{
    QStringList paths;
    for (const QUrl &url : mimeData->urls())
    {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".blend", Qt::CaseInsensitive))
        {
            paths.append(url.toLocalFile());
        }
    }
    return paths;
}

void Widget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!queuePage->acceptsFiles() || !event->mimeData()->hasUrls())
    {
        event->ignore();
        return;
    }
    if (!droppedBlendFiles(event->mimeData()).isEmpty())
    {
        event->acceptProposedAction();
    }
}

void Widget::dropEvent(QDropEvent *event)
{
    if (!queuePage->acceptsFiles() || !event->mimeData()->hasUrls())
    {
        event->ignore();
        return;
    }
    const QStringList paths = droppedBlendFiles(event->mimeData());
    if (!paths.isEmpty())
    {
        queuePage->addFiles(paths);
        event->acceptProposedAction();
    }
}

void Widget::createPages()
{
    // Same margins as the queue page so the cards line up when switching.
    const QMargins margins = queuePage->layout()->contentsMargins();
    const auto wrap = [this, margins](QWidget *page) {
        QWidget *holder = new QWidget(ui->pageStack);
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        // The margins go inside the page: card shadows are siblings of the
        // cards and need that room to be drawn.
        page->layout()->setContentsMargins(margins);
        page->setParent(holder);
        layout->addWidget(page);
        ui->pageStack->addWidget(holder);
        for (QWidget *child : page->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        {
            if (child->property(Look::cardProperty).toBool())
            {
                dropShadowWidgets.push_back(DropShadowWidget::attach(page, child, {12, 4, 4, 0.8f, 15}, true));
            }
        }
    };
    nodesPage = new NodesPage(controller->nodeAgent(), renderQueue, controller->loadMonitor());
    wrap(nodesPage);
    settingsPage = new SettingsPage(AppPaths::distributedFile(), languageIndex);
    wrap(settingsPage);
    // The NodeAgent's name for this machine, so local frames match the legend
    // the phone shows.
    projectPage = new ProjectPage(renderQueue, QSysInfo::machineHostName());
    wrap(projectPage);
    QObject::connect(projectPage, &ProjectPage::backRequested, this, [this]() {
        showPage(0);
    });
    QObject::connect(queuePage, &QueuePage::projectDetailsRequested, this, [this](QUuid id) {
        projectPage->showProject(id);
        showPage(projectPageIndex);
    });
    settingsPage->setBlenderVersions(blenderVersionManager->getBlenderVersions());
    settingsPage->setBlenderEditingEnabled(!renderQueue->isRendering());
    QObject::connect(settingsPage, &SettingsPage::languageChanged, this, &Widget::onLanguageChanged);
    QObject::connect(settingsPage, &SettingsPage::addBlenderRequested, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(settingsPage, &SettingsPage::removeBlenderRequested, this, &Widget::onDeleteBlenderVersion);
    QObject::connect(settingsPage, &SettingsPage::farmSettingsSaved, this, &Widget::applyFarmSettings);
    QObject::connect(settingsPage, &SettingsPage::maxConcurrentLoadsChanged, controller, &AppController::setMaxConcurrentLoads);
    QObject::connect(nodesPage, &NodesPage::manageNodesRequested, this, [this]() {
        showPage(settingsPageIndex);
        settingsPage->showSection(SettingsPage::Section::Farm);
    });
    QObject::connect(queuePage, &QueuePage::addBlenderRequested, this, [this]() {
        showPage(settingsPageIndex);
        settingsPage->showSection(SettingsPage::Section::Blender);
    });

    const struct
    {
        NavButton::Icon icon;
        QString tip;
    } entries[] = {
        {NavButton::Icon::Queue, tr("Queue")},
        {NavButton::Icon::Nodes, tr("Nodes")},
        {NavButton::Icon::Settings, tr("Settings")}
    };
    for (int index = 0; index < 3; ++index)
    {
        NavButton *button = new NavButton(entries[index].icon, ui->navRail);
        button->setToolTip(entries[index].tip);
        button->setAccessibleName(entries[index].tip);
        if (index == settingsPageIndex)
        {
            ui->navRailLayout->addStretch(1);
        }
        ui->navRailLayout->addWidget(button, 0, Qt::AlignHCenter);
        QObject::connect(button, &QAbstractButton::clicked, this, [this, index]() {
            showPage(index);
        });
        navButtons.push_back(button);
    }
    showPage(0);
}

void Widget::showPage(int index)
{
    PageFade::switchTo(ui->pageStack, index);
    // A project's page belongs to the queue entry of the rail.
    const int nav = index == projectPageIndex ? 0 : index;
    if (nav >= 0 && nav < static_cast<int>(navButtons.size()))
    {
        navButtons[nav]->setChecked(true);
    }
}

void Widget::updateStatusChip()
{
    const NodeAgent *agent = controller->nodeAgent();
    if (!agent)
    {
        return;
    }
    const NodeAgent::Snapshot snapshot = agent->snapshot();
    if (renderQueue->isRendering())
    {
        int nodes = 1;
        for (const WorkerInfo &worker : renderQueue->workers())
        {
            nodes += worker.leaseId.isNull() ? 0 : 1;
        }
        ui->statusChip->setStatus(
            Theme::accent,
            nodes > 1 ? tr("Rendering · %n node(s)", nullptr, nodes) : tr("Rendering")
        );
        return;
    }
    if (blenderVersionManager->getBlenderVersionCount() == 0)
    {
        ui->statusChip->setStatus(Theme::warning, tr("No Blender"));
        return;
    }
    Theme::Tone dot;
    const QString self = NodesPage::describeSelf(snapshot, &dot);
    if (!snapshot.enabled)
    {
        ui->statusChip->setStatus(Theme::ok, tr("Ready"));
        return;
    }
    const int peers = static_cast<int>(snapshot.peers.size());
    ui->statusChip->setStatus(dot, peers > 0 ? tr("%1 · %n peer(s)", nullptr, peers).arg(self) : self);
}

void Widget::installRoundedSurfaces()
{
    // The pages paint their own cards; only the window background is left.
    ui->ContentWidget->setAutoFillBackground(true);
    Look::setTextColor(ui->rendropLabel, Theme::textStrong);
}

void Widget::onLanguageChanged(int index)
{
    if (!LanguageSetting::save(LanguageSetting::localeForIndex(index)))
    {
        return;
    }
    const auto result = MessageDialog::warning(
        this,
        tr("Warning"),
        tr("Language settings will take effect after restarting the program. Restart now?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result == QMessageBox::Yes)
    {
        QCoreApplication::exit(773);
    }
}

void Widget::onBlenderVersionsChanged()
{
    settingsPage->setBlenderVersions(blenderVersionManager->getBlenderVersions());
    queuePage->refreshBlenderVersions();
    updateStatusChip();
}

void Widget::onAddBlenderVersionButtonClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Blender Version"),
        "",
        tr("Blender Executable (blender.exe)")
    );
    if (path.isEmpty())
    {
        return;
    }
    if (!QFileInfo(path).isFile())
    {
        MessageDialog::error(this, tr("Error"), tr("Cannot open specified path."));
        return;
    }

    // "blender --version" can take seconds on a cold start; run it without
    // blocking the event loop and keep the Add button off until it ends.
    settingsPage->setAddBlenderBusy(true);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, process, &QProcess::kill);

    const auto finish = [this, process, path](bool started) {
        QString version;
        if (started && process->exitStatus() == QProcess::NormalExit)
        {
            version = BlenderVersionManager::parseVersionOutput(
                QString::fromLocal8Bit(process->readAllStandardOutput()));
        }
        process->deleteLater();
        settingsPage->setAddBlenderBusy(false);

        const int result = blenderVersionManager->addBlenderVersion(path, version);
        if (result == -2)
        {
            MessageDialog::error(this, tr("Error"), tr("Failed to start Blender."));
        }
        else if (result == -3)
        {
            MessageDialog::info(this, tr("Note"), tr("Selected Blender version already exists."));
        }
        else if (result != 0)
        {
            MessageDialog::error(this, tr("Error"), tr("Failed to save version configuration file."));
        }
        // The new version only becomes the selection when there was none;
        // the selector keeps the current one otherwise.
        onBlenderVersionsChanged();
    };
    QObject::connect(process, &QProcess::finished, this, [finish]() { finish(true); });
    QObject::connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        // A crash or kill also emits finished(); only a failed start does not.
        if (error == QProcess::FailedToStart)
        {
            finish(false);
        }
    });
    timeout->start(3000);
    process->start(path, {QStringLiteral("--version")});
}

void Widget::onDeleteBlenderVersion(QString version)
{
    if (renderQueue->isRendering())
    {
        return;
    }
    const auto result = MessageDialog::warning(
        this,
        tr("Warning"),
        tr("Remove Blender %1 from Rendrop? The program itself is not deleted.").arg(version),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result != QMessageBox::Yes)
    {
        return;
    }
    if (blenderVersionManager->deleteBlenderVersion(version) != 0)
    {
        MessageDialog::error(this, tr("Error"), tr("Failed to save version configuration file."));
    }
    onBlenderVersionsChanged();
}

void Widget::applyFarmSettings(DistributedSettings settings)
{
    controller->applyFarmSettings(settings);
    updateStatusChip();
}
