// widget.cpp

#include <algorithm>
#include <cmath>
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
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <QWKWidgets/widgetwindowagent.h>

#include "app_paths.h"
#include "app_preferences.h"
#include "distributed_settings.h"
#include "rounded_surface.h"
#include "widget.h"
#include "blender_file_info.h"
#include "ui_widget.h"

Widget::Widget(int languageIndex, QWidget *parent):
    QWidget(parent),
    ui(new Ui::widget),
    addFileButton(nullptr),
    dropFileTip(nullptr),
    blenderVersionManager(nullptr),
    renderQueue(nullptr),
    dropShadowRenderer(nullptr),
    shadowBufferHandle(-1),
    httpServer(nullptr),
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

    dropShadowRenderer = new DropShadowRenderer();
    ui->scrollAreaContent->installEventFilter(this);
    QPointer<Widget> weakThis(this);
    shadowBufferHandle = dropShadowRenderer->createWidgetBuffer([weakThis]() {
        if (weakThis)
        {
            weakThis->updateAllFileBarShadow();
        }
    });

    addFileButton = new AddFileButton(ui->queuePage, dropShadowRenderer);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);
    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->queuePage, ui->selectorContainer, 12, 4, 4, 0.8f, 15);
    createDropShadowWidget(ui->selectorWidgetContent, ui->selectorComboBox, 8, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->selectorWidgetContent, ui->renderButton, 10, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->queuePage, ui->scrollAreaContainer, 12, 4, 4, 0.8f, 15);
    createDropShadowWidget(ui->queuePage, ui->outputContainer, 15, 4, 4, 0.6f, 10);
    createDropShadowWidget(ui->outputContainer, ui->outputText, 15, 4, 4, 0.6f, 10);
    createDropShadowWidget(ui->queuePage, ui->logContainer, 12, 4, 4, 0.8f, 15);

    dropFileTip = new DropFileTip(ui->scrollAreaContent);
    dropFileTip->lower();
    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    QObject::connect(ui->renderButton, &QPushButton::clicked, this, &Widget::onRenderButtonClicked);
    setLogVisible(false);
    QObject::connect(ui->logToggleButton, &QPushButton::clicked, this, [this]() {
        setLogVisible(!ui->logContainer->isVisible());
    });

    const QString blenderPath = selectedBlenderPath();
    renderQueue->setBlenderPathForAll(blenderPath);
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
    if (dropShadowRenderer)
    {
        dropShadowRenderer->deleteWidgetBuffer(shadowBufferHandle);
        delete dropShadowRenderer;
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
    if (watched == ui->scrollAreaSizeWidget && event->type() == QEvent::Resize)
    {
        constexpr float borderRadius = 16;
        constexpr float offsetX = 2;
        constexpr float offsetY = 2;
        constexpr float alphaMax = 0.6f;
        constexpr float blurRadius = 15;
        const int fileBarWidth = ui->scrollAreaSizeWidget->width();
        constexpr int fileBarHeight = 50;
        const float marginX = std::abs(offsetX) + blurRadius * 0.5f;
        const float marginY = std::abs(offsetY) + blurRadius * 0.5f;
        const float shadowWidth = fileBarWidth + marginX * 2;
        const float shadowHeight = fileBarHeight + marginY * 2;
        const float dpr = devicePixelRatio();
        const int expectedWidth = static_cast<int>(std::ceil(shadowWidth * dpr));
        const int expectedHeight = static_cast<int>(std::ceil(shadowHeight * dpr));
        fileBarShadowPixmap = dropShadowRenderer->getPixmap(shadowBufferHandle);
        if (fileBarShadowPixmap.isNull()
            || fileBarShadowPixmap.width() != expectedWidth
            || fileBarShadowPixmap.height() != expectedHeight
            || !qFuzzyCompare(static_cast<float>(fileBarShadowPixmap.devicePixelRatio()), dpr))
        {
            dropShadowRenderer->setWidgetBuffer(
                shadowBufferHandle,
                shadowWidth,
                shadowHeight,
                borderRadius,
                offsetX,
                offsetY,
                alphaMax,
                blurRadius,
                dpr
            );
            if (!fileBarShadowPixmap.isNull())
            {
                fileBarShadowPixmap = fileBarShadowPixmap.scaled(
                    expectedWidth,
                    expectedHeight,
                    Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation
                );
                fileBarShadowPixmap.setDevicePixelRatio(dpr);
            }
        }
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
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
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
    messageBox.setFont(QFont("Pixel Mixed", 12));
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
    FileBar *fileBar = new FileBar(ui->scrollAreaContent, dropShadowRenderer, project);
    fileBar->getDropShadowWidget()->setShadowUpdateEnabled(false);
    fileBar->getDropShadowWidget()->setShadowPixmap(&fileBarShadowPixmap);
    ui->scrollAreaVerticalLayout->insertWidget(static_cast<int>(fileBars.size()), fileBar);
    QObject::connect(fileBar, &FileBar::moveUpRequested, this, &Widget::onFileBarMoveUp);
    QObject::connect(fileBar, &FileBar::moveDownRequested, this, &Widget::onFileBarMoveDown);
    QObject::connect(fileBar, &FileBar::deleteRequested, this, &Widget::onFileBarDelete);
    QObject::connect(fileBar, &FileBar::reloadRequested, this, &Widget::onFileBarReload);
    QObject::connect(fileBar, &FileBar::renameRequested, this, &Widget::onFileBarRename);
    QObject::connect(fileBar, &FileBar::frameRangeRequested, this, &Widget::onFileBarFrameRange);
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
    float blurRadius
)
{
    DropShadowWidget *shadow = new DropShadowWidget(parent, target, dropShadowRenderer);
    shadow->setBorderRadius(borderRadius);
    shadow->setOffsetX(offsetX);
    shadow->setOffsetY(offsetY);
    shadow->setAlphaMax(alphaMax);
    shadow->setBlurRadius(blurRadius);
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
                createDropShadowWidget(page, child, 12, 4, 4, 0.8f, 15);
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
    settingsPage->setBlenderVersions(blenderVersionManager->getBlenderVersions());
    QObject::connect(settingsPage, &SettingsPage::languageChanged, this, &Widget::onLanguageChanged);
    QObject::connect(settingsPage, &SettingsPage::addBlenderRequested, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(settingsPage, &SettingsPage::removeBlenderRequested, this, &Widget::onDeleteBlenderVersion);
    QObject::connect(settingsPage, &SettingsPage::farmSettingsSaved, this, &Widget::applyFarmSettings);
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
    ui->pageStack->setCurrentIndex(index);
    if (index >= 0 && index < static_cast<int>(navButtons.size()))
    {
        navButtons[index]->setChecked(true);
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
    ui->logContainer->setVisible(visible);
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
    // Style sheets keep text colors, fonts and the combo boxes; these
    // surfaces replace their stepped border-radius backgrounds.
    const QColor frame(0x2C, 0x31, 0x3C);
    const QColor content(26, 28, 32);
    RoundedSurface::install(ui->selectorContainer, 12, frame);
    RoundedSurface::install(ui->selectorWidgetContent, 12, content);
    RoundedSurface::install(ui->scrollAreaContainer, 12, frame);
    RoundedSurface::install(ui->scrollArea, 12, content);
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollArea->setStyleSheet(
        QStringLiteral("QScrollArea#scrollArea { background: transparent; border: none; color: #E0E0E0; }")
        + Look::scrollBarStyleSheet());
    // Same frame-around-surface card as the queue list.
    RoundedSurface::install(ui->logContainer, 12, frame);
    RoundedSurface::install(ui->logView, 10, content);
    ui->logView->viewport()->setAutoFillBackground(false);
    ui->logView->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: transparent; border: none; padding: 4px 6px; color: rgb(180, 180, 200);"
                       " font-family: 'Pixel Mixed'; font-size: 9pt; }")
        + Look::scrollBarStyleSheet());
    RoundedSurface::install(ui->outputContainer, 15, frame);
    RoundedSurface::install(ui->outputText, 15, QColor(80, 80, 130));
    RoundedSurface::install(ui->selectorComboBox, 8, RoundedSurface::Colors {
        QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(), QColor(0x55, 0x59, 0x5F), QColor(65, 65, 85)});
    RoundedSurface::install(ui->logToggleButton, 8, RoundedSurface::Colors {
        Qt::transparent, QColor(255, 255, 255, 18), QColor(), QColor()});
    RoundedSurface::install(ui->renderButton, 10, RoundedSurface::Colors {
        QColor(90, 90, 150),
        QColor(120, 120, 180),
        QColor(70, 70, 130),
        QColor(0x3A, 0x3D, 0x45)
    });
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

void Widget::onFileBarMoveUp(QUuid id)
{
    renderQueue->moveUp(id);
}

void Widget::onFileBarMoveDown(QUuid id)
{
    renderQueue->moveDown(id);
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
}

void Widget::onJobRemoved(QUuid id)
{
    const auto iterator = std::find_if(fileBars.begin(), fileBars.end(), [id](const FileBar *fileBar) {
        return fileBar->projectId() == id;
    });
    if (iterator == fileBars.end())
    {
        return;
    }
    FileBar *fileBar = *iterator;
    ui->scrollAreaVerticalLayout->removeWidget(fileBar);
    fileBars.erase(iterator);
    fileBar->hide();
    fileBar->deleteLater();
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
    FileBar *fileBar = *iterator;
    fileBars.erase(fileBars.begin() + currentIndex);
    fileBars.insert(fileBars.begin() + toIndex, fileBar);
    fileBar->hide();
    ui->scrollAreaVerticalLayout->removeWidget(fileBar);
    ui->scrollAreaVerticalLayout->insertWidget(toIndex, fileBar);
    fileBar->show();
    Q_UNUSED(fromIndex);
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
            updateBlenderVersions();
            if (ui->selectorComboBox->count() > 0)
            {
                ui->selectorComboBox->setCurrentIndex(ui->selectorComboBox->count() - 1);
            }
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
        renderQueue->stop();
    }
    else
    {
        renderQueue->start();
    }
    clearOutputText();
}

void Widget::onOutputTextUpdated(QString text)
{
    ui->outputLabel->setText(text);
    ui->logView->appendPlainText(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + text
    );
}

void Widget::updateBlenderVersions()
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

void Widget::updateAllFileBarShadow()
{
    fileBarShadowPixmap = dropShadowRenderer->getPixmap(shadowBufferHandle);
    for (FileBar *fileBar : fileBars)
    {
        fileBar->getDropShadowWidget()->update();
    }
}
