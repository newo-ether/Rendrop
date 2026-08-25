// widget.cpp

#include <algorithm>
#include <cmath>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QMimeData>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include "app_paths.h"
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
    httpServer(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(true);
    qRegisterMetaType<BlenderFileInfo>("BlenderFileInfo");
    qRegisterMetaType<ProjectInfo>("ProjectInfo");

    ui->languageComboBox->setCurrentIndex(languageIndex);
    QObject::connect(
        ui->languageComboBox,
        &QComboBox::currentIndexChanged,
        this,
        &Widget::onLanguageChanged
    );

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
        if (text.isEmpty() || text == tr("-- Add A Blender Version --"))
        {
            renderQueue->setBlenderPathForAll(QString());
            return;
        }
        renderQueue->setBlenderPathForAll(blenderVersionManager->getBlenderPath(text));
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

    addFileButton = new AddFileButton(ui->ContentWidget, dropShadowRenderer);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);
    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->ContentWidget, ui->selectorContainer, 12, 4, 4, 0.8f, 15);
    createDropShadowWidget(ui->selectorWidgetContent, ui->selectorComboBox, 8, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->selectorWidgetContent, ui->addButton, 8, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->selectorWidgetContent, ui->deleteButton, 8, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->titleBar, ui->languageComboBox, 8, 3, 3, 0.6f, 10);
    createDropShadowWidget(ui->ContentWidget, ui->infoWidgetContainer, 12, 4, 4, 0.8f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->scrollAreaContainer, 12, 4, 4, 0.8f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->renderButton, 15, 4, 4, 0.6f, 10);
    createDropShadowWidget(ui->ContentWidget, ui->outputContainer, 15, 4, 4, 0.6f, 10);
    createDropShadowWidget(ui->outputContainer, ui->outputText, 15, 4, 4, 0.6f, 10);

    dropFileTip = new DropFileTip(ui->scrollAreaContent);
    dropFileTip->lower();
    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    QObject::connect(ui->addButton, &QPushButton::clicked, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &Widget::onDeleteBlenderVersionButtonClicked);
    QObject::connect(ui->renderButton, &QPushButton::clicked, this, &Widget::onRenderButtonClicked);

    const QString blenderPath = selectedBlenderPath();
    renderQueue->setBlenderPathForAll(blenderPath);
    renderQueue->restorePersistedJobs(blenderPath);
    httpServer = new HttpServer(renderQueue);
    httpServer->start(QThread::LowestPriority);

    onRenderingChanged(false);
    updateStatisticInfo();
    updateButtonStatus();
    clearOutputText();
}

Widget::~Widget()
{
    if (httpServer)
    {
        httpServer->stop();
        httpServer->wait();
        delete httpServer;
    }
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

void Widget::closeEvent(QCloseEvent *event)
{
    const std::vector<ProjectInfo> projects = renderQueue->snapshots();
    const bool loading = std::any_of(projects.begin(), projects.end(), [](const ProjectInfo &project) {
        return project.state == ProjectState::Loading;
    });
    QMessageBox::StandardButton result = QMessageBox::Yes;
    if (renderQueue->isRendering())
    {
        result = warningMessageBox(
            tr("Warning"),
            tr("Some files are still rendering. Are you sure to exit?"),
            QMessageBox::Yes | QMessageBox::No
        );
    }
    else if (loading)
    {
        result = warningMessageBox(
            tr("Warning"),
            tr("Some files are still loading. Are you sure to exit?"),
            QMessageBox::Yes | QMessageBox::No
        );
    }
    if (result == QMessageBox::Yes)
    {
        event->accept();
    }
    else
    {
        event->ignore();
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
    setSelectorEnabled(!rendering);
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
    if (!path.isEmpty())
    {
        const int result = blenderVersionManager->addBlenderVersion(path);
        if (result == -1)
        {
            errorMessageBox(tr("Error"), tr("Cannot open specified path."));
        }
        else if (result == -2)
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
    }
    updateButtonStatus();
    clearOutputText();
}

void Widget::onDeleteBlenderVersionButtonClicked()
{
    if (blenderVersionManager->getBlenderVersionCount() > 0)
    {
        if (blenderVersionManager->deleteBlenderVersion(ui->selectorComboBox->currentText()) != 0)
        {
            errorMessageBox(tr("Error"), tr("Failed to save version configuration file."));
        }
        updateBlenderVersions();
    }
    updateButtonStatus();
    clearOutputText();
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
}

void Widget::updateBlenderVersions()
{
    ui->selectorComboBox->clear();
    const auto &versions = blenderVersionManager->getBlenderVersions();
    if (versions.empty())
    {
        ui->selectorComboBox->addItem(tr("-- Add A Blender Version --"));
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
    ui->addButton->setEnabled(enabled);
    ui->deleteButton->setEnabled(enabled);
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
    ui->framesLabel->setText(QString::number(finishedFrames) + " / " + QString::number(totalFrames));
    ui->projectsLabel->setText(QString::number(finishedProjects) + " / " + QString::number(totalProjects));
    const float frameProgress = totalFrames == 0
        ? 0.0f
        : static_cast<float>(finishedFrames) / totalFrames * 100.0f;
    const float projectProgress = totalProjects == 0
        ? 0.0f
        : static_cast<float>(finishedProjects) / totalProjects * 100.0f;
    ui->framesProgressBar->setProgressBar(frameProgress);
    ui->projectsProgressBar->setProgressBar(projectProgress);
}

void Widget::clearOutputText()
{
    if (renderQueue->isRendering())
    {
        return;
    }
    if (blenderVersionManager->getBlenderVersionCount() == 0)
    {
        ui->outputLabel->setText(tr("Please add a Blender version first."));
        dropFileTip->changeTip(tr("Please add a Blender version first."));
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
