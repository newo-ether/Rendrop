// widget.cpp

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QProcess>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTextDocument>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <QWKWidgets/widgetwindowagent.h>

#include "config/app_paths.h"
#include "config/app_preferences.h"
#include "config/distributed_settings.h"
#include "ui/motion/list_motion.h"
#include "ui/style/look_widgets.h"
#include "ui/motion/page_fade.h"
#include "ui/motion/smooth_scroll.h"
#include "ui/style/rounded_surface.h"
#include "ui/window/widget.h"
#include "core/blender_file_info.h"
#include "ui_widget.h"

Widget::Widget(int languageIndex, QWidget *parent):
    QWidget(parent),
    ui(new Ui::widget),
    addFileButton(nullptr),
    dropFileTip(nullptr),
    blenderVersionManager(nullptr),
    renderQueue(nullptr),
    httpServer(nullptr),
    languageIndex(languageIndex)
{
    // QWindowKit must take over the native frame before the window gets its
    // size constraints from setupUi().
    windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);
    ui->setupUi(this);
    ui->selectorComboBox->setFont(Look::pixelFont(11, QFont::Medium));
    ui->summaryLabel->setFont(Look::monoFont(9));
    ui->renderButton->setFont(Look::pixelFont(11, QFont::Medium));
    ui->logToggleButton->setFont(Look::pixelFont(9, QFont::Medium));
    // Before any shadow exists, so none flashes when they are off.
    DropShadowWidget::setShadowsEnabled(AppPreferencesFile::load().shadows);
    setupWindowFrame();
    setAcceptDrops(true);
    qRegisterMetaType<BlenderFileInfo>("BlenderFileInfo");
    qRegisterMetaType<ProjectInfo>("ProjectInfo");

    renderQueue = new RenderQueue();
    QObject::connect(renderQueue, &RenderQueue::jobAdded, this, &Widget::onJobAdded);
    QObject::connect(renderQueue, &RenderQueue::jobUpdated, this, &Widget::onJobUpdated);
    QObject::connect(renderQueue, &RenderQueue::jobRemoved, this, &Widget::onJobRemoved);
    QObject::connect(renderQueue, &RenderQueue::jobMoved, this, &Widget::onJobMoved);
    QObject::connect(renderQueue, &RenderQueue::queueChanged, this, &Widget::onQueueChanged);
    QObject::connect(renderQueue, &RenderQueue::renderingChanged, this, &Widget::onRenderingChanged);
    QObject::connect(renderQueue, &RenderQueue::outputTextUpdated, this, &Widget::onOutputTextUpdated);

    blenderVersionManager = new BlenderVersionManager(
        AppPaths::configFile(QStringLiteral("blender_versions.cfg"))
    );
    updateBlenderVersions();
    QFile selectionFile(AppPaths::configFile(QStringLiteral("blender_selection.cfg")));
    if (selectionFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QString savedVersion = selectionFile.readLine().trimmed();
        const int index = ui->selectorComboBox->findText(savedVersion);
        if (index != -1)
        {
            ui->selectorComboBox->setCurrentIndex(index);
        }
    }
    QObject::connect(ui->selectorComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.isEmpty() || text == tr("Add Blender..."))
        {
            renderQueue->setBlenderPathForAll(QString());
            if (nodeAgent)
            {
                nodeAgent->setBlenderPath(QString());
            }
            return;
        }
        renderQueue->setBlenderPathForAll(blenderVersionManager->getBlenderPath(text));
        if (nodeAgent)
        {
            nodeAgent->setBlenderPath(blenderVersionManager->getBlenderPath(text));
        }
        const bool configDirectoryReady = AppPaths::ensureConfigDirectory();
        if (!configDirectoryReady)
        {
            return;
        }
        QFile file(AppPaths::configFile(QStringLiteral("blender_selection.cfg")));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(text.toUtf8());
            file.write("\n");
        }
    });

    ui->scrollAreaContent->installEventFilter(this);

    addFileButton = new AddFileButton(ui->queuePage);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);
    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Cards lift their shadow under the pointer; controls inside them do not.
    createDropShadowWidget(ui->queuePage, ui->selectorContainer, 12, 4, 4, 0.8f, 15, true);
    createDropShadowWidget(ui->selectorWidgetContent, ui->selectorComboBox, 8, 3, 3, 0.6f, 10, false);
    createDropShadowWidget(ui->selectorWidgetContent, ui->renderButton, 10, 3, 3, 0.6f, 10, false);
    createDropShadowWidget(ui->queuePage, ui->scrollAreaContainer, 12, 4, 4, 0.8f, 15, true);
    createDropShadowWidget(ui->queuePage, ui->outputContainer, 15, 4, 4, 0.6f, 10, true);
    createDropShadowWidget(ui->queuePage, ui->logContainer, 12, 4, 4, 0.8f, 15, true);
    logShadow = dropShadowWidgets.back();

    dropFileTip = new DropFileTip(ui->scrollAreaContent);
    dropFileTip->lower();
    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    QObject::connect(ui->renderButton, &QPushButton::clicked, this, &Widget::onRenderButtonClicked);
    setLogVisible(false);
    QObject::connect(ui->logToggleButton, &QPushButton::clicked, this, [this]() {
        setLogVisible(!logShown);
    });

    const QString blenderPath = selectedBlenderPath();
    renderQueue->setBlenderPathForAll(blenderPath);
    renderQueue->setMaxConcurrentLoads(AppPreferencesFile::load().maxConcurrentLoads);
    renderQueue->restorePersistedJobs(blenderPath);
    const DistributedSettings distributed = DistributedConfig::load(
        AppPaths::configFile(QStringLiteral("distributed.json"))
    );
    renderQueue->setDistributedSettings(distributed);
    loadMonitor = new LoadMonitor(distributed.load);
    // Normal priority: a starved sampler under full render load would leave
    // gaps that reset the window.
    loadMonitor->start();
    // The server outlives settings changes; only the token it checks moves.
    httpServer = new HttpServer(renderQueue);
    httpServer->setLoadMonitor(loadMonitor);
    httpServer->setWorkerToken(distributed.enabled ? distributed.token : QString());
    farmSettings = distributed;
    startNodeAgent(distributed);
    httpServer->start(QThread::LowestPriority);
    createPages();
    installRoundedSurfaces();
    setupTray();

    // The title bar status, node counts and the ETA follow lease activity
    // that has no signal of its own.
    QTimer *statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    QObject::connect(statusTimer, &QTimer::timeout, this, [this]() {
        updateStatusChip();
        updateNodeCounts();
        updateStatisticInfo();
    });
    statusTimer->start();

    onRenderingChanged(false);
    updateStatisticInfo();
    updateButtonStatus();
    clearOutputText();
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
    // Hands a lease rendered for another node back before anything stops.
    if (nodeAgent)
    {
        nodeAgent->shutdown();
    }
    if (httpServer)
    {
        httpServer->stop();
        httpServer->wait();
        delete httpServer;
    }
    // After the server: its beacon handler reads the agent's discovery.
    delete nodeAgent;
    nodeAgent = nullptr;
    // After the server and the agent: both read the monitor.
    delete loadMonitor;
    if (renderQueue)
    {
        renderQueue->shutdown();
        delete renderQueue;
    }
    for (FileBar *fileBar : fileBars)
    {
        delete fileBar;
    }
    fileBars.clear();
    delete addFileButton;
    delete dropFileTip;
    for (DropShadowWidget *dropShadowWidget : dropShadowWidgets)
    {
        delete dropShadowWidget;
    }
    delete blenderVersionManager;
    delete ui;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollAreaContainer && event->type() == QEvent::Resize)
    {
        QWidget *container = ui->scrollAreaContainer;
        addFileButton->move(
            container->x() + container->width() - 110,
            container->y() + container->height() - 110
        );
        addFileButton->raise();
    }
    if (watched == ui->scrollAreaContent && event->type() == QEvent::Resize)
    {
        QWidget *content = ui->scrollAreaContent;
        dropFileTip->setGeometry(0, 0, content->width(), content->height());
        dropFileTip->lower();
    }
    return QWidget::eventFilter(watched, event);
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
    // The app has a single dark theme; keep native parts (system menu, Snap
    // Layout flyout) dark too, whatever the system color scheme is.
    windowAgent->setWindowAttribute(QStringLiteral("dark-mode"), true);

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
    menu->addSeparator();
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
    return warningMessageBox(tr("Warning"), text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
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

void Widget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!addFileButton->isEnabled() || !event->mimeData()->hasUrls())
    {
        event->ignore();
        return;
    }
    const bool hasBlendFile = std::any_of(
        event->mimeData()->urls().begin(),
        event->mimeData()->urls().end(),
        [](const QUrl &url) {
            return url.isLocalFile() && url.toLocalFile().endsWith(".blend", Qt::CaseInsensitive);
        }
    );
    if (hasBlendFile)
    {
        event->acceptProposedAction();
    }
}

void Widget::dropEvent(QDropEvent *event)
{
    if (!addFileButton->isEnabled() || !event->mimeData()->hasUrls())
    {
        event->ignore();
        return;
    }
    bool added = false;
    for (const QUrl &url : event->mimeData()->urls())
    {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".blend", Qt::CaseInsensitive))
        {
            const QString filePath = QDir::toNativeSeparators(url.toLocalFile());
            addProject(QFileInfo(filePath).completeBaseName(), filePath);
            added = true;
        }
    }
    if (added)
    {
        event->acceptProposedAction();
    }
}

QMessageBox::StandardButton Widget::iconMessageBox(
    QMessageBox::Icon icon,
    QString title,
    QString text,
    QMessageBox::StandardButtons buttons
)
{
    QMessageBox messageBox(this);
    messageBox.setIcon(icon);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(Look::pixelFont(12));
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

QMessageBox::StandardButton Widget::errorMessageBox(
    QString title,
    QString text,
    QMessageBox::StandardButtons buttons
)
{
    return iconMessageBox(QMessageBox::Critical, title, text, buttons);
}

QMessageBox::StandardButton Widget::warningMessageBox(
    QString title,
    QString text,
    QMessageBox::StandardButtons buttons
    )
{
    return iconMessageBox(QMessageBox::Warning, title, text, buttons);
}

QMessageBox::StandardButton Widget::infoMessageBox(
    QString title,
    QString text,
    QMessageBox::StandardButtons buttons
    )
{
    return iconMessageBox(QMessageBox::Information, title, text, buttons);
}

void Widget::addProject(const QString &fileName, const QString &filePath)
{
    renderQueue->addJob(filePath, fileName);
}

void Widget::createFileBar(const ProjectInfo &project)
{
    FileBar *fileBar = new FileBar(ui->scrollAreaContent, project);
    ui->scrollAreaVerticalLayout->insertWidget(static_cast<int>(fileBars.size()), fileBar);
    // Restored jobs at startup just appear; ones added later fade in.
    if (isVisible() && ui->scrollAreaContent->isVisible())
    {
        ui->scrollAreaVerticalLayout->activate();
        ListMotion::appear(fileBar, fileBar->getDropShadowWidget());
    }
    QObject::connect(fileBar, &FileBar::dragStarted, this, &Widget::onFileBarDragStarted);
    QObject::connect(fileBar, &FileBar::dragMoved, this, &Widget::onFileBarDragMoved);
    QObject::connect(fileBar, &FileBar::dragFinished, this, &Widget::onFileBarDragFinished);
    QObject::connect(fileBar, &FileBar::deleteRequested, this, &Widget::onFileBarDelete);
    QObject::connect(fileBar, &FileBar::reloadRequested, this, &Widget::onFileBarReload);
    QObject::connect(fileBar, &FileBar::renameRequested, this, &Widget::onFileBarRename);
    QObject::connect(fileBar, &FileBar::frameRangeRequested, this, &Widget::onFileBarFrameRange);
    QObject::connect(fileBar, &FileBar::detailsRequested, this, [this](QUuid id) {
        projectPage->showProject(id);
        showPage(projectPageIndex);
    });
    fileBars.push_back(fileBar);
    dropFileTip->hide();
}

FileBar *Widget::findFileBar(const QUuid &id) const
{
    const auto iterator = std::find_if(fileBars.begin(), fileBars.end(), [id](const FileBar *fileBar) {
        return fileBar->projectId() == id;
    });
    return iterator == fileBars.end() ? nullptr : *iterator;
}

void Widget::createDropShadowWidget(
    QWidget *parent,
    QWidget *target,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius,
    bool hoverLift
)
{
    DropShadowWidget *shadow = new DropShadowWidget(parent, target);
    shadow->setBorderRadius(borderRadius);
    shadow->setOffsetX(offsetX);
    shadow->setOffsetY(offsetY);
    shadow->setAlphaMax(alphaMax);
    shadow->setBlurRadius(blurRadius);
    shadow->setHoverLift(hoverLift);
    dropShadowWidgets.push_back(shadow);
}

void Widget::createPages()
{
    // Same margins as the queue page so the cards line up when switching.
    const auto wrap = [this](QWidget *page) {
        QWidget *holder = new QWidget(ui->pageStack);
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        // The margins go inside the page: card shadows are siblings of the
        // cards and need that room to be drawn.
        page->layout()->setContentsMargins(ui->queuePageLayout->contentsMargins());
        page->setParent(holder);
        layout->addWidget(page);
        ui->pageStack->addWidget(holder);
        for (QWidget *child : page->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        {
            if (child->property(Look::cardProperty).toBool())
            {
                createDropShadowWidget(page, child, 12, 4, 4, 0.8f, 15, true);
            }
        }
    };
    nodesPage = new NodesPage(nodeAgent, renderQueue, loadMonitor);
    wrap(nodesPage);
    settingsPage = new SettingsPage(
        AppPaths::configFile(QStringLiteral("distributed.json")),
        languageIndex
    );
    wrap(settingsPage);
    // The NodeAgent's name for this machine, so local frames match the legend
    // the phone shows.
    projectPage = new ProjectPage(renderQueue, QSysInfo::machineHostName());
    wrap(projectPage);
    QObject::connect(projectPage, &ProjectPage::backRequested, this, [this]() {
        showPage(0);
    });
    settingsPage->setBlenderVersions(blenderVersionManager->getBlenderVersions());
    QObject::connect(settingsPage, &SettingsPage::languageChanged, this, &Widget::onLanguageChanged);
    QObject::connect(settingsPage, &SettingsPage::addBlenderRequested, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(settingsPage, &SettingsPage::removeBlenderRequested, this, &Widget::onDeleteBlenderVersion);
    QObject::connect(settingsPage, &SettingsPage::farmSettingsSaved, this, &Widget::applyFarmSettings);
    QObject::connect(settingsPage, &SettingsPage::maxConcurrentLoadsChanged, this, [this](int limit) {
        renderQueue->setMaxConcurrentLoads(limit);
    });
    QObject::connect(nodesPage, &NodesPage::manageNodesRequested, this, [this]() {
        showPage(2);
        settingsPage->showSection(SettingsPage::Section::Farm);
    });
    // The placeholder entry of an empty selector leads to where versions
    // are added.
    QObject::connect(ui->selectorComboBox, &QComboBox::activated, this, [this]() {
        if (blenderVersionManager->getBlenderVersionCount() == 0)
        {
            showPage(2);
            settingsPage->showSection(SettingsPage::Section::Blender);
        }
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
        if (index == 2)
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
    if (!nodeAgent)
    {
        return;
    }
    const NodeAgent::Snapshot snapshot = nodeAgent->snapshot();
    if (renderQueue->isRendering())
    {
        int nodes = 1;
        for (const WorkerInfo &worker : renderQueue->workers())
        {
            nodes += worker.leaseId.isNull() ? 0 : 1;
        }
        ui->statusChip->setStatus(
            QColor(110, 110, 180),
            nodes > 1 ? tr("Rendering · %n node(s)", nullptr, nodes) : tr("Rendering")
        );
        return;
    }
    if (blenderVersionManager->getBlenderVersionCount() == 0)
    {
        ui->statusChip->setStatus(QColor(215, 165, 70), tr("No Blender"));
        return;
    }
    QColor dot;
    const QString self = NodesPage::describeSelf(snapshot, &dot);
    if (!snapshot.enabled)
    {
        ui->statusChip->setStatus(QColor(90, 185, 120), tr("Ready"));
        return;
    }
    const int peers = static_cast<int>(snapshot.peers.size());
    ui->statusChip->setStatus(dot, peers > 0 ? tr("%1 · %n peer(s)", nullptr, peers).arg(self) : self);
}

void Widget::updateNodeCounts()
{
    if (!renderQueue->isRendering())
    {
        for (FileBar *fileBar : fileBars)
        {
            fileBar->setNodeCount(0);
        }
        return;
    }
    // Leases only exist on the job being rendered.
    int nodes = 1;
    for (const WorkerInfo &worker : renderQueue->workers())
    {
        nodes += worker.leaseId.isNull() ? 0 : 1;
    }
    for (FileBar *fileBar : fileBars)
    {
        const ProjectInfo project = renderQueue->snapshotById(fileBar->projectId());
        fileBar->setNodeCount(project.state == ProjectState::Rendering ? nodes : 0);
    }
}

void Widget::setLogVisible(bool visible)
{
    constexpr int logHeight = 160;
    constexpr int logMs = 260;
    logShown = visible;
    QWidget *log = ui->logContainer;
    if (logAnimation)
    {
        logAnimation->stop();
    }
    if (!isVisible() || !ui->queuePage->isVisible())
    {
        // A fade cut short by the stop above must not stay half-way.
        log->setGraphicsEffect(nullptr);
        if (logShadow)
        {
            logShadow->setFade(1.0);
        }
        log->setFixedHeight(logHeight);
        log->setVisible(visible);
        followLogTail();
    }
    else
    {
        // The card grows from or shrinks to nothing; the list above gives
        // way and the output bar below rides along.
        const int from = log->isVisible() ? log->height() : 0;
        const int to = visible ? logHeight : 0;
        // Below twice the corner radius the rounded card cannot be drawn as
        // itself, so it stays invisible there and fades in over the next
        // stretch of height.
        constexpr int logRadius = 12;
        constexpr int hiddenBelow = 2 * logRadius;
        constexpr int fadeSpan = 48;
        auto *fade = qobject_cast<QGraphicsOpacityEffect *>(log->graphicsEffect());
        if (!fade)
        {
            fade = new QGraphicsOpacityEffect(log);
            log->setGraphicsEffect(fade);
        }
        const auto applyHeight = [this, log, fade](int height) {
            const qreal alpha = std::clamp(static_cast<qreal>(height - hiddenBelow) / fadeSpan, 0.0, 1.0);
            fade->setOpacity(alpha);
            if (logShadow)
            {
                logShadow->setFade(alpha);
            }
            log->setFixedHeight(height);
            followLogTail();
        };
        applyHeight(from);
        log->setVisible(true);
        auto *animation = new QVariantAnimation(this);
        animation->setStartValue(from);
        animation->setEndValue(to);
        // A reversal part way only travels what is left.
        animation->setDuration(std::max(120, logMs * std::abs(to - from) / logHeight));
        animation->setEasingCurve(Look::bezierInOut(Look::cubicInOut));
        QObject::connect(animation, &QVariantAnimation::valueChanged, log, [applyHeight](const QVariant &value) {
            applyHeight(value.toInt());
        });
        QObject::connect(animation, &QVariantAnimation::finished, log, [this, log, visible]() {
            // The effect re-renders the whole card offscreen; drop it at rest.
            log->setGraphicsEffect(nullptr);
            if (logShadow)
            {
                logShadow->setFade(1.0);
            }
            log->setVisible(visible);
            log->setFixedHeight(logHeight);
            followLogTail();
        });
        logAnimation = animation;
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
    // Icon only; the accent tint marks the open state and the tooltip names it.
    ui->logToggleButton->setText(QString());
    ui->logToggleButton->setFixedSize(32, 24);
    ui->logToggleButton->setIconSize(QSize(16, 16));
    ui->logToggleButton->setIcon(Look::terminalIcon(visible ? Look::accentColor.lighter(130) : QColor(150, 150, 190)));
    ui->logToggleButton->setToolTip(visible ? tr("Hide log") : tr("Show log"));
    ui->logToggleButton->setAccessibleName(ui->logToggleButton->toolTip());
}

void Widget::installRoundedSurfaces()
{
    // Everything here is painted: RoundedSurface draws the backgrounds and
    // fades them, RendropStyle the controls; palettes carry the text colors.
    const QColor frame(0x2C, 0x31, 0x3C);
    const QColor content(26, 28, 32);
    ui->ContentWidget->setAutoFillBackground(true);
    Look::setTextColor(ui->rendropLabel, Qt::white);
    RoundedSurface::install(ui->selectorContainer, 12, frame);
    RoundedSurface::install(ui->selectorWidgetContent, 12, content);
    RoundedSurface::install(ui->scrollAreaContainer, 12, frame);
    RoundedSurface::install(ui->scrollArea, 12, content);
    Look::clearScrollArea(ui->scrollArea);
    SmoothScroll::install(ui->scrollArea);
    // Same frame-around-surface card as the queue list.
    RoundedSurface::install(ui->logContainer, 12, frame);
    RoundedSurface::install(ui->logView, 10, content);
    Look::clearScrollArea(ui->logView);
    ui->logView->setFont(Look::monoFont(8));
    ui->logView->document()->setDocumentMargin(6);
    Look::setTextColor(ui->logView, QColor(180, 180, 200));
    // The log follows its newest line, like a terminal: whenever its range
    // changes (a line arrives, the card is shown or resized) it stays at the
    // bottom, unless the user has scrolled up to read; scrolling back to the
    // bottom resumes following. While the card was hidden the range used to
    // be stale, so it opened at the top.
    QScrollBar *logBar = ui->logView->verticalScrollBar();
    QObject::connect(logBar, &QScrollBar::rangeChanged, this, [this, logBar](int, int maximum) {
        if (logFollow)
        {
            logBar->setValue(maximum);
        }
    });
    // User scrolling only (wheel, drag, arrows); the editor's own relayouts
    // also move the value and must not stop the following.
    QObject::connect(logBar, &QScrollBar::actionTriggered, this, [this, logBar](int) {
        // sliderPosition already holds where the action goes.
        logFollow = logBar->sliderPosition() >= logBar->maximum();
    });
    RoundedSurface::install(ui->outputContainer, 15, frame);
    RoundedSurface::install(ui->outputText, 15, QColor(80, 80, 130));
    Look::setTextColor(ui->outputText, QColor(200, 200, 240));
    ui->outputLabel->setContentsMargins(10, 0, 10, 0);
    // Blender's last line, same face as the log it comes from.
    ui->outputLabel->setFont(Look::monoFont(8));
    Look::setTextColor(ui->outputLabel, QColor(180, 180, 200));
    RoundedSurface::install(ui->selectorComboBox, 8, RoundedSurface::Colors {
        QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(), QColor(0x55, 0x59, 0x5F), QColor(65, 65, 85)});
    Look::setTextColor(ui->selectorComboBox, Qt::white, QColor(0xAA, 0xAA, 0xAA));
    RoundedSurface::Colors logToggle {Qt::transparent, QColor(255, 255, 255, 18), QColor(), QColor()};
    logToggle.text = QColor(150, 150, 190);
    logToggle.hoverText = Qt::white;
    RoundedSurface::install(ui->logToggleButton, 8, logToggle);
    Look::setPadding(ui->logToggleButton, QMargins(8, 2, 8, 2));
    RoundedSurface::install(ui->renderButton, 10, RoundedSurface::Colors {
        QColor(90, 90, 150),
        QColor(120, 120, 180),
        QColor(70, 70, 130),
        QColor(0x3A, 0x3D, 0x45)
    });
    Look::setTextColor(ui->renderButton, Qt::white, QColor(0xAA, 0xAA, 0xAA));
    Look::setPadding(ui->renderButton, QMargins(6, 6, 6, 6));
}

QString Widget::selectedBlenderPath() const
{
    return blenderVersionManager->getBlenderPath(ui->selectorComboBox->currentText());
}

void Widget::onLanguageChanged(int index)
{
    const bool configDirectoryReady = AppPaths::ensureConfigDirectory();
    if (!configDirectoryReady)
    {
        return;
    }
    QFile configFile(AppPaths::configFile(QStringLiteral("language.cfg")));
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        configFile.write(index == 0 ? "en_US\n" : "zh_CN\n");
    }
    const auto result = warningMessageBox(
        tr("Warning"),
        tr("Language settings will take effect after restarting the program. Restart now?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result == QMessageBox::Yes)
    {
        QCoreApplication::exit(773);
    }
}

void Widget::onFileBarDragStarted(QUuid id, QPointF globalPosition)
{
    FileBar *fileBar = findFileBar(id);
    if (!fileBar || draggedBar)
    {
        return;
    }
    // Settle every bar first so the slots are where the layout puts them.
    for (FileBar *bar : fileBars)
    {
        ListMotion::stopSlide(bar);
    }
    ui->scrollAreaVerticalLayout->activate();
    dragSlots.clear();
    for (FileBar *bar : fileBars)
    {
        dragSlots.push_back(bar->y());
    }
    draggedBar = fileBar;
    dragStartIndex = static_cast<int>(std::find(fileBars.begin(), fileBars.end(), fileBar) - fileBars.begin());
    dragPointer = globalPosition;
    dragGrabOffset = ui->scrollAreaContent->mapFromGlobal(globalPosition).y() - fileBar->y();
    // The bar leaves the layout and follows the pointer freely; a blank of
    // its size keeps its slot and moves through the list instead.
    auto *placeholder = new QWidget(ui->scrollAreaContent);
    placeholder->setFixedHeight(fileBar->height());
    placeholder->setSizePolicy(fileBar->sizePolicy());
    placeholder->setAttribute(Qt::WA_TransparentForMouseEvents);
    delete ui->scrollAreaVerticalLayout->replaceWidget(fileBar, placeholder);
    placeholder->show();
    dragPlaceholder = placeholder;
    // Above its neighbours while it passes over them, shadow included.
    if (DropShadowWidget *shadow = fileBar->getDropShadowWidget())
    {
        shadow->raise();
    }
    fileBar->raise();
    if (!dragScrollTimer)
    {
        dragScrollTimer = new QTimer(this);
        dragScrollTimer->setInterval(16);
        QObject::connect(dragScrollTimer, &QTimer::timeout, this, &Widget::autoScrollDrag);
    }
    dragScrollTimer->start();
}

void Widget::onFileBarDragMoved(QUuid id, QPointF globalPosition)
{
    if (!draggedBar || draggedBar->projectId() != id)
    {
        return;
    }
    dragPointer = globalPosition;
    placeDraggedBar();
}

void Widget::onFileBarDragFinished(QUuid id)
{
    if (!draggedBar || draggedBar->projectId() != id)
    {
        return;
    }
    const int finalIndex = static_cast<int>(std::find(fileBars.begin(), fileBars.end(), draggedBar.data()) - fileBars.begin());
    const int startIndex = dragStartIndex;
    // Only now does the bar settle into its slot.
    endDrag();
    // The queue reports the move through onJobMoved, which finds the bar
    // already in place and leaves its slide alone.
    if (finalIndex != startIndex && !renderQueue->moveTo(id, finalIndex))
    {
        // Refused: put the list back the way the queue has it.
        animateQueueLayout([this, id, startIndex]() {
            FileBar *bar = findFileBar(id);
            fileBars.erase(std::find(fileBars.begin(), fileBars.end(), bar));
            fileBars.insert(fileBars.begin() + startIndex, bar);
            ui->scrollAreaVerticalLayout->removeWidget(bar);
            ui->scrollAreaVerticalLayout->insertWidget(startIndex, bar);
        });
    }
}

void Widget::endDrag()
{
    if (dragPlaceholder)
    {
        if (draggedBar)
        {
            const QPoint from = draggedBar->pos();
            delete ui->scrollAreaVerticalLayout->replaceWidget(dragPlaceholder, draggedBar);
            ui->scrollAreaVerticalLayout->activate();
            ListMotion::slide(draggedBar, from);
        }
        delete dragPlaceholder.data();
    }
    draggedBar = nullptr;
    dragSlots.clear();
    dragStartIndex = -1;
    if (dragScrollTimer)
    {
        dragScrollTimer->stop();
    }
}

void Widget::placeDraggedBar()
{
    FileBar *fileBar = draggedBar.data();
    if (!fileBar || dragSlots.size() != fileBars.size() || dragSlots.empty())
    {
        return;
    }
    const qreal pointerY = ui->scrollAreaContent->mapFromGlobal(dragPointer).y();
    const int top = std::clamp(static_cast<int>(std::lround(pointerY - dragGrabOffset)), dragSlots.front(), dragSlots.back());
    // The slot nearest to the bar's top is where it would land now.
    int target = 0;
    for (int index = 1; index < static_cast<int>(dragSlots.size()); ++index)
    {
        if (std::abs(dragSlots[static_cast<size_t>(index)] - top) < std::abs(dragSlots[static_cast<size_t>(target)] - top))
        {
            target = index;
        }
    }
    const int current = static_cast<int>(std::find(fileBars.begin(), fileBars.end(), fileBar) - fileBars.begin());
    if (target != current && dragPlaceholder)
    {
        // The others slide aside; the dragged one stays under the pointer.
        QWidget *placeholder = dragPlaceholder.data();
        animateQueueLayout([this, fileBar, placeholder, current, target]() {
            fileBars.erase(fileBars.begin() + current);
            fileBars.insert(fileBars.begin() + target, fileBar);
            ui->scrollAreaVerticalLayout->removeWidget(placeholder);
            ui->scrollAreaVerticalLayout->insertWidget(target, placeholder);
        });
    }
    // Width follows the slot in case the window was resized mid-drag.
    const QWidget *slot = dragPlaceholder ? dragPlaceholder.data() : fileBar;
    fileBar->setGeometry(slot->x(), top, slot->width(), fileBar->height());
}

void Widget::autoScrollDrag()
{
    if (!draggedBar)
    {
        endDrag();
        return;
    }
    QWidget *viewport = ui->scrollArea->viewport();
    const qreal y = viewport->mapFromGlobal(dragPointer).y();
    constexpr qreal zone = 40.0;
    constexpr qreal maxStep = 14.0;
    qreal step = 0.0;
    if (y < zone)
    {
        step = -maxStep * std::min(1.0, (zone - y) / zone);
    }
    else if (y > viewport->height() - zone)
    {
        step = maxStep * std::min(1.0, (y - (viewport->height() - zone)) / zone);
    }
    QScrollBar *bar = ui->scrollArea->verticalScrollBar();
    const int next = std::clamp(bar->value() + static_cast<int>(std::lround(step)), bar->minimum(), bar->maximum());
    if (next != bar->value())
    {
        bar->setValue(next);
        placeDraggedBar();
    }
}

void Widget::onFileBarDelete(QUuid id)
{
    const ProjectInfo project = renderQueue->snapshotById(id);
    if (project.isNull)
    {
        return;
    }
    if (project.state == ProjectState::Loading)
    {
        const auto result = warningMessageBox(
            tr("Warning"),
            project.name + tr(" is still loading. Are you sure to delete?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result == QMessageBox::No)
        {
            return;
        }
    }
    if (project.state == ProjectState::Rendering)
    {
        const auto result = warningMessageBox(
            tr("Warning"),
            project.name + tr(" is still rendering. Are you sure to delete?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result == QMessageBox::No)
        {
            return;
        }
    }
    renderQueue->removeJob(id);
}

void Widget::onFileBarReload(QUuid id)
{
    renderQueue->reloadJob(id);
}

void Widget::onFileBarRename(QUuid id, QString name)
{
    renderQueue->renameJob(id, name);
}

void Widget::onFileBarFrameRange(QUuid id, int frameStart, int frameEnd, int frameStep)
{
    renderQueue->setFrameRange(id, frameStart, frameEnd, frameStep);
}

void Widget::onJobAdded(ProjectInfo project)
{
    createFileBar(project);
}

void Widget::onJobUpdated(ProjectInfo project)
{
    FileBar *fileBar = findFileBar(project.id);
    if (fileBar)
    {
        fileBar->applySnapshot(project);
    }

    const ProjectState previous = lastJobStates.value(project.id, project.state);
    lastJobStates.insert(project.id, project.state);
    // A job publishes Finished / Error before the queue moves on, so a run
    // that is still going is the one it belongs to. A stopped run is over
    // before the killed job reports back.
    if (previous != ProjectState::Rendering || project.state == ProjectState::Rendering
        || !renderQueue->isRendering())
    {
        return;
    }
    if (project.state == ProjectState::Finished)
    {
        ++finishedThisRun;
    }
    else if (project.state == ProjectState::Error)
    {
        ++failedThisRun;
        notify(
            tr("Render failed"),
            tr("%1 stopped at frame %2 / %3. See the log for details.")
                .arg(project.name).arg(project.finishedFrame).arg(project.totalFrame),
            true
        );
    }
}

void Widget::onJobRemoved(QUuid id)
{
    lastJobStates.remove(id);
    const auto iterator = std::find_if(fileBars.begin(), fileBars.end(), [id](const FileBar *fileBar) {
        return fileBar->projectId() == id;
    });
    if (iterator == fileBars.end())
    {
        return;
    }
    FileBar *fileBar = *iterator;
    if (draggedBar)
    {
        // The slots no longer match the list. The dragged bar, if it stays,
        // slides back into its place below with the rest.
        endDrag();
    }
    if (isVisible())
    {
        ListMotion::vanish(fileBar, fileBar->getDropShadowWidget());
    }
    // The bars below close the gap instead of jumping into it.
    animateQueueLayout([this, fileBar]() {
        ui->scrollAreaVerticalLayout->removeWidget(fileBar);
        fileBars.erase(std::find(fileBars.begin(), fileBars.end(), fileBar));
        fileBar->hide();
        fileBar->deleteLater();
    });
    if (fileBars.empty())
    {
        dropFileTip->show();
    }
}

void Widget::onJobMoved(QUuid id, int fromIndex, int toIndex)
{
    const auto iterator = std::find_if(fileBars.begin(), fileBars.end(), [id](const FileBar *fileBar) {
        return fileBar->projectId() == id;
    });
    if (iterator == fileBars.end() || toIndex < 0 || toIndex >= static_cast<int>(fileBars.size()))
    {
        return;
    }
    const int currentIndex = static_cast<int>(std::distance(fileBars.begin(), iterator));
    if (currentIndex == toIndex)
    {
        // A drop already put the bar there.
        return;
    }
    if (draggedBar)
    {
        // Moved from elsewhere mid-drag: settle the drag first.
        endDrag();
    }
    FileBar *fileBar = *iterator;
    // Both bars of a swap slide past each other to their new places.
    animateQueueLayout([this, fileBar, currentIndex, toIndex]() {
        fileBars.erase(fileBars.begin() + currentIndex);
        fileBars.insert(fileBars.begin() + toIndex, fileBar);
        ui->scrollAreaVerticalLayout->removeWidget(fileBar);
        ui->scrollAreaVerticalLayout->insertWidget(toIndex, fileBar);
    });
    Q_UNUSED(fromIndex);
}

void Widget::animateQueueLayout(const std::function<void ()> &change)
{
    const bool animate = isVisible() && ui->scrollAreaContent->isVisible();
    std::unordered_map<FileBar *, std::pair<QPoint, QPointF>> before;
    if (animate)
    {
        for (FileBar *fileBar : fileBars)
        {
            if (fileBar == draggedBar)
            {
                // Out of the layout, under the pointer.
                continue;
            }
            // Where it is seen now, mid-slide included, and how fast it goes.
            const QPoint position = fileBar->pos();
            before.emplace(fileBar, std::make_pair(position, ListMotion::stopSlide(fileBar)));
        }
    }
    change();
    if (!animate)
    {
        return;
    }
    ui->scrollAreaVerticalLayout->activate();
    for (FileBar *fileBar : fileBars)
    {
        const auto found = before.find(fileBar);
        if (found != before.end() && (found->second.first != fileBar->pos() || !found->second.second.isNull()))
        {
            ListMotion::slide(fileBar, found->second.first, found->second.second);
        }
    }
}

void Widget::onQueueChanged()
{
    updateButtonStatus();
    updateStatisticInfo();
    clearOutputText();
}

void Widget::onRenderingChanged(bool rendering)
{
    ui->renderButton->setText(rendering ? tr("Stop Render") : tr("Start Render"));
    if (rendering)
    {
        stopRequested = false;
        failedThisRun = 0;
        finishedThisRun = 0;
        renderClock.start();
        framesAtRenderStart = 0;
        for (const ProjectInfo &project : renderQueue->snapshots())
        {
            framesAtRenderStart += project.finishedFrame;
        }
    }
    else
    {
        renderClock.invalidate();
        if (!stopRequested && finishedThisRun + failedThisRun > 0)
        {
            // The queue ran dry by itself: every job finished or failed.
            const QString summary = failedThisRun == 0
                ? tr("Render complete: %1 finished.").arg(finishedThisRun)
                : tr("Render ended: %1 finished, %2 failed.").arg(finishedThisRun).arg(failedThisRun);
            onOutputTextUpdated(summary);
            // A lone failure already raised its own toast.
            if (finishedThisRun > 0 || failedThisRun == 0)
            {
                notify(failedThisRun == 0 ? tr("Render complete") : tr("Render ended with errors"),
                       summary, failedThisRun > 0);
            }
        }
        stopRequested = false;
    }
    setSelectorEnabled(!rendering);
    updateStatisticInfo();
    updateStatusChip();
    updateButtonStatus();
    if (!rendering)
    {
        clearOutputText();
    }
}

void Widget::onAddFileButtonClicked()
{
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Open File"),
        "",
        tr("Blender Projects (*.blend)")
    );
    for (const QString &filePath : filePaths)
    {
        if (!filePath.isEmpty())
        {
            const QString nativePath = QDir::toNativeSeparators(filePath);
            addProject(QFileInfo(nativePath).completeBaseName(), nativePath);
        }
    }
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
        errorMessageBox(tr("Error"), tr("Cannot open specified path."));
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
            errorMessageBox(tr("Error"), tr("Failed to start Blender."));
        }
        else if (result == -3)
        {
            infoMessageBox(tr("Note"), tr("Selected Blender version already exists."));
        }
        else if (result != 0)
        {
            errorMessageBox(tr("Error"), tr("Failed to save version configuration file."));
        }
        else
        {
            // The new version only becomes the selection when there was none;
            // updateBlenderVersions keeps the current one otherwise.
            updateBlenderVersions();
        }
        updateButtonStatus();
        clearOutputText();
        updateStatusChip();
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
    const auto result = warningMessageBox(
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
        errorMessageBox(tr("Error"), tr("Failed to save version configuration file."));
    }
    updateBlenderVersions();
    updateButtonStatus();
    clearOutputText();
    updateStatusChip();
}

void Widget::startNodeAgent(const DistributedSettings &settings)
{
    nodeAgent = new NodeAgent(
        renderQueue,
        loadMonitor,
        settings,
        AppPaths::configFile(QStringLiteral("distributed.json")),
        this
    );
    nodeAgent->setBlenderPath(selectedBlenderPath());
    nodeAgent->attachServer(httpServer);
    QObject::connect(nodeAgent, &NodeAgent::outputText, this, &Widget::onOutputTextUpdated);
    QObject::connect(nodeAgent, &NodeAgent::changed, this, &Widget::updateStatusChip);
    nodeAgent->start();
    if (nodesPage)
    {
        nodesPage->setAgent(nodeAgent);
    }
}

void Widget::applyFarmSettings(DistributedSettings settings)
{
    // Thresholds, chunking and the key apply in place; nothing restarts.
    loadMonitor->setSettings(settings.load);
    renderQueue->setDistributedSettings(settings);
    httpServer->setWorkerToken(settings.enabled ? settings.token : QString());

    // Discovery binds its port and reads the peers once, so a change there
    // swaps the agent. acceptRemoteWork is the Nodes page's switch, which the
    // agent already follows; the load thresholds were applied above.
    const auto agentPart = [](DistributedSettings value) {
        value.acceptRemoteWork = true;
        value.load = LoadSettings();
        return DistributedConfig::toJson(value);
    };
    const bool rebuild = agentPart(settings) != agentPart(farmSettings);
    farmSettings = settings;
    if (!rebuild)
    {
        return;
    }
    if (nodesPage)
    {
        nodesPage->setAgent(nullptr);
    }
    // Hands a lease taken for another node back before the agent goes.
    nodeAgent->shutdown();
    httpServer->setBeaconProvider({});
    delete nodeAgent;
    nodeAgent = nullptr;
    startNodeAgent(settings);
    updateStatusChip();
}

void Widget::onRenderButtonClicked()
{
    if (renderQueue->isRendering())
    {
        QString where;
        for (const ProjectInfo &project : renderQueue->snapshots())
        {
            if (project.state == ProjectState::Rendering)
            {
                where = tr(" (%1 at frame %2 / %3)")
                    .arg(project.name).arg(project.finishedFrame).arg(project.totalFrame);
                break;
            }
        }
        stopRequested = true;
        onOutputTextUpdated(tr("Render stopped manually") + where);
        renderQueue->stop();
    }
    else
    {
        renderQueue->start();
    }
    clearOutputText();
}

void Widget::notify(const QString &title, const QString &text, bool error)
{
    if (trayIcon && trayIcon->isVisible() && QSystemTrayIcon::supportsMessages())
    {
        trayIcon->showMessage(title, text, error ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information, 6000);
    }
}

void Widget::onOutputTextUpdated(QString text)
{
    ui->outputLabel->setText(text);
    ui->logView->appendPlainText(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + text
    );
    followLogTail();
}

void Widget::followLogTail()
{
    if (logFollow)
    {
        QScrollBar *bar = ui->logView->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

void Widget::updateBlenderVersions()
{
    // Rebuilding the list must not pass through other selections, or the
    // queue would briefly switch Blender and the saved choice would change.
    const QString previous = ui->selectorComboBox->currentText();
    {
        const QSignalBlocker blocker(ui->selectorComboBox);
        rebuildBlenderSelector();
        const int index = ui->selectorComboBox->findText(previous);
        if (index != -1)
        {
            ui->selectorComboBox->setCurrentIndex(index);
        }
    }
    const QString current = ui->selectorComboBox->currentText();
    if (current != previous)
    {
        emit ui->selectorComboBox->currentTextChanged(current);
    }
}

void Widget::rebuildBlenderSelector()
{
    ui->selectorComboBox->clear();
    const auto &versions = blenderVersionManager->getBlenderVersions();
    if (settingsPage)
    {
        settingsPage->setBlenderVersions(versions);
    }
    if (versions.empty())
    {
        ui->selectorComboBox->addItem(tr("Add Blender..."));
        return;
    }
    for (const auto &version : versions)
    {
        ui->selectorComboBox->addItem(version.version);
    }
}

void Widget::updateButtonStatus()
{
    const bool blenderExists = blenderVersionManager->getBlenderVersionCount() > 0;
    addFileButton->setEnabled(blenderExists);
    const std::vector<ProjectInfo> projects = renderQueue->snapshots();
    const bool loading = std::any_of(projects.begin(), projects.end(), [](const ProjectInfo &project) {
        return project.state == ProjectState::Loading;
    });
    const bool queued = std::any_of(projects.begin(), projects.end(), [](const ProjectInfo &project) {
        return project.state == ProjectState::Queued;
    });
    ui->renderButton->setEnabled(
        renderQueue->isRendering()
        || (!projects.empty() && blenderExists && !loading && queued)
    );
}

void Widget::setSelectorEnabled(bool enabled)
{
    ui->selectorComboBox->setEnabled(enabled);
    if (settingsPage)
    {
        settingsPage->setBlenderEditingEnabled(enabled);
    }
}

void Widget::updateStatisticInfo()
{
    int finishedFrames = 0;
    int totalFrames = 0;
    int finishedProjects = 0;
    const std::vector<ProjectInfo> projects = renderQueue->snapshots();
    for (const ProjectInfo &project : projects)
    {
        if (project.state == ProjectState::Finished)
        {
            ++finishedProjects;
        }
        finishedFrames += project.finishedFrame;
        totalFrames += project.totalFrame;
    }
    const int totalProjects = static_cast<int>(projects.size());
    QString detail = tr("Projects %1 / %2").arg(finishedProjects).arg(totalProjects);
    // ETA from the throughput since this render started, so frames finished
    // in earlier sessions do not make it look faster than it is.
    const int renderedNow = finishedFrames - framesAtRenderStart;
    if (renderQueue->isRendering() && renderClock.isValid() && renderedNow > 0 && totalFrames > finishedFrames)
    {
        const double msPerFrame = static_cast<double>(renderClock.elapsed()) / renderedNow;
        const qint64 minutes = static_cast<qint64>(std::ceil(msPerFrame * (totalFrames - finishedFrames) / 60000.0));
        const QString eta = minutes >= 60
            ? tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60)
            : tr("%1 min").arg(std::max<qint64>(1, minutes));
        detail += QStringLiteral(" · ") + tr("ETA %1").arg(eta);
    }
    ui->summaryLabel->setText(
        QStringLiteral("<span style=\"color:#FFFFFF\">%1</span>&nbsp;&nbsp;<span style=\"color:#6E7382\">%2</span>")
            .arg(tr("Frames %1 / %2").arg(finishedFrames).arg(totalFrames).toHtmlEscaped(), detail.toHtmlEscaped())
    );
    ui->summaryProgress->setValue(totalFrames == 0 ? 0.0 : 100.0 * finishedFrames / totalFrames);
}

void Widget::clearOutputText()
{
    if (renderQueue->isRendering())
    {
        return;
    }
    if (blenderVersionManager->getBlenderVersionCount() == 0)
    {
        ui->outputLabel->setText(tr("Add a Blender version in Settings first."));
        dropFileTip->changeTip(tr("Add a Blender version in Settings first."));
    }
    else if (renderQueue->snapshots().empty())
    {
        ui->outputLabel->setText(tr("Add a Blender project to get started."));
        dropFileTip->changeTip(tr("Drop your Blender Projects here."));
    }
    else
    {
        ui->outputLabel->setText(tr("Ready to render."));
    }
}

