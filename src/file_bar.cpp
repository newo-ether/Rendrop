// file_bar.cpp

#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QObject>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>

#include "file_bar.h"
#include "ui_file_bar.h"
#include "frame_range_dialog.h"
#include "rename_dialog.h"
#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"
#include "blender_file_info.h"
#include "blender_file_reader.h"
#include "blender_renderer.h"
#include "loading_bar.h"
#include "project_info.h"

FileBar::FileBar(
    QWidget *parent,
    DropShadowRenderer *dropShadowRenderer,
    int id,
    QString fileName,
    QString filePath,
    QString blenderPath
):
    QWidget(parent),
    ui(new Ui::fileBar),
    fileName(fileName),
    filePath(filePath),
    state(ProjectState::Loading),
    id(id),
    frameStart(0),
    frameEnd(0),
    frameStep(0),
    resolutionX(0),
    resolutionY(0),
    resolutionScale(0),
    renderEngine(),
    finishedFrame(0),
    totalFrame(0),
    loadingStyle(Style(Color(80, 150, 210))),
    queueStyle(Style(Color(60, 60, 60))),
    errorStyle(Style(Color(180, 60, 80))),
    renderingStyle(Style(Color(80, 150, 210))),
    finishedStyle(Style(Color(60, 150, 80))),
    style(queueStyle),
    targetStyle(queueStyle),
    velocity(Style::zero()),
    stiffness(300),
    damping(50)
{
    ui->setupUi(this);

    ui->fileNameButton->setText(fileName);
    ui->fileNameButton->setToolTip(tr("Click to rename project"));
    ui->frameButton->setText("");
    ui->frameButton->setToolTip(tr("Click to edit frame range"));
    ui->resolutionLabel->setText("");
    ui->renderEngineLabel->setText("");
    ui->reloadButton->setEnabled(false);
    ui->progressBar->setProgressBar(0.0f);
    ui->progressBarLabel->setText("");

    frameLoadingBar = new LoadingBar(ui->frameButton);
    frameLoadingBar->setGeometry(0, 0, ui->frameButton->width(), ui->frameButton->height());
    ui->frameButton->installEventFilter(this);

    resolutionLoadingBar = new LoadingBar(ui->resolutionLabel);
    resolutionLoadingBar->setGeometry(0, 0, ui->resolutionLabel->width(), ui->resolutionLabel->height());
    ui->resolutionLabel->installEventFilter(this);

    renderEngineLoadingBar = new LoadingBar(ui->renderEngineLabel);
    renderEngineLoadingBar->setGeometry(0, 0, ui->renderEngineLabel->width(), ui->renderEngineLabel->height());
    ui->renderEngineLabel->installEventFilter(this);

    blenderFileReader = new BlenderFileReader();
    blenderFileReader->setParameter(filePath, blenderPath);
    QObject::connect(blenderFileReader, &BlenderFileReader::finishedReading, this, &FileBar::onFinishedReading);

    blenderFileReader->start(QThread::LowestPriority);

    blenderRenderer = new BlenderRenderer();
    blenderRenderer->setParameter(blenderPath, filePath, frameStart, frameEnd, frameStep);
    QObject::connect(blenderRenderer, &BlenderRenderer::progressChanged, this, &FileBar::onProgressChanged);
    QObject::connect(blenderRenderer, &BlenderRenderer::outputTextUpdate, this, &FileBar::onOutputTextUpdate);
    QObject::connect(blenderRenderer, &BlenderRenderer::finishedRendering, this, &FileBar::onFinishedRendering);

    QObject::connect(ui->upButton, &QPushButton::clicked, this, &FileBar::onUpButtonClicked);
    QObject::connect(ui->downButton, &QPushButton::clicked, this, &FileBar::onDownButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &FileBar::onDeleteButtonClicked);
    QObject::connect(ui->reloadButton, &QPushButton::clicked, this, &FileBar::onReloadButtonClicked);
    
    QObject::connect(ui->frameButton, &QPushButton::clicked, this, [this]() {
        if (state != ProjectState::Rendering && state != ProjectState::Loading)
        {
            FrameRangeDialog dialog(frameStart, frameEnd, frameStep, this);
            if (dialog.exec() == QDialog::Accepted)
            {
                int newStart = dialog.getStart();
                int newEnd = dialog.getEnd();
                int newStep = dialog.getStep();

                setFrame(newStart, newEnd, newStep);
                
                // Update Blender Renderer and Logic
                finishedFrame = 0;
                totalFrame = newStep == 0 ? 0 : (newEnd - newStart) / newStep + 1;
                
                blenderRenderer->setFrame(newStart, newEnd, newStep);
                blenderRenderer->setCurrentFrame(newStart);
                
                // Refresh Progress Bar
                float progress = totalFrame == 0 ? 0 : static_cast<float>(finishedFrame) / totalFrame * 100.0f;
                ui->progressBar->setProgressBar(0.0f);
                ui->progressBarLabel->setText(
                    QString::number(finishedFrame)
                    + "/"
                    + QString::number(totalFrame)
                    + " ("
                    + QString::number(static_cast<int>(progress))
                    + "%)"
                );
                
                emit progressChanged();
            }
        }
    });

    QObject::connect(ui->fileNameButton, &QPushButton::clicked, this, [this]() {
        RenameDialog dialog(getFileName(), this);
        if (dialog.exec() == QDialog::Accepted)
        {
            QString newName = dialog.getName();
            if (!newName.isEmpty())
            {
                setFileName(newName);
            }
        }
    });

    dropShadowWidget = new DropShadowWidget(
        parent,
        this,
        dropShadowRenderer
    );
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.4f);
    dropShadowWidget->setBlurRadius(10);

    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &FileBar::updateStyle);

    elapsedTimer = new QElapsedTimer();
    elapsedTimer->start();
    lastElapsed = 0;

    setState(ProjectState::Loading);

    show();
    dropShadowWidget->show();
    showLoadingBar();
}

FileBar::~FileBar()
{
    if (blenderFileReader->isRunning())
    {
        blenderFileReader->stop();
        blenderFileReader->wait();
    }
    delete blenderFileReader;

    if (blenderRenderer->isRunning())
    {
        blenderRenderer->stop();
        blenderRenderer->wait();
    }
    delete blenderRenderer;

    delete frameLoadingBar;
    delete resolutionLoadingBar;
    delete renderEngineLoadingBar;

    delete dropShadowWidget;
    delete ui;
}

int FileBar::getID() const
{
    return id;
}

void FileBar::setFileName(QString fileName)
{
    {
        QWriteLocker locker(&dataLock);
        this->fileName = fileName;
    }
    ui->fileNameButton->setText(fileName);
}

void FileBar::setFilePath(QString filePath)
{
    QWriteLocker locker(&dataLock);
    this->filePath = filePath;
}

QString FileBar::getFileName() const
{
    QReadLocker locker(&dataLock);
    return fileName;
}

QString FileBar::getFilePath() const
{
    QReadLocker locker(&dataLock);
    return filePath;
}

void FileBar::stopReading()
{
    blenderFileReader->stop();
}

DropShadowWidget * FileBar::getDropShadowWidget() const
{
    return dropShadowWidget;
}

int FileBar::getFinishedFrame() const
{
    return finishedFrame;
}

int FileBar::getTotalFrame() const
{
    return totalFrame;
}

void FileBar::render()
{
    blenderRenderer->start(QThread::LowestPriority);
    setState(ProjectState::Rendering);

    ui->reloadButton->setEnabled(false);
    ui->deleteButton->setEnabled(false);
}

void FileBar::stopRender()
{
    blenderRenderer->stop();
    setState(ProjectState::Queued);

    ui->reloadButton->setEnabled(true);
    ui->deleteButton->setEnabled(true);
}

bool FileBar::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->frameButton && event->type() == QEvent::Resize) {
        frameLoadingBar->setGeometry(0, 0, ui->frameButton->width(), ui->frameButton->height());
    }
    else if (object == ui->resolutionLabel && event->type() == QEvent::Resize) {
        resolutionLoadingBar->setGeometry(0, 0, ui->resolutionLabel->width(), ui->resolutionLabel->height());
    }
    else if (object == ui->renderEngineLabel && event->type() == QEvent::Resize) {
        renderEngineLoadingBar->setGeometry(0, 0, ui->renderEngineLabel->width(), ui->renderEngineLabel->height());
    }

    return QWidget::eventFilter(object, event);
}

void FileBar::setFrame(int frameStart, int frameEnd, int frameStep)
{
    ui->frameButton->setText(
        QString::number(frameStart)
        + "-"
        + QString::number(frameEnd)
        + " ("
        + QString::number(frameStep)
        + ")"
    );

    this->frameStart = frameStart;
    this->frameEnd = frameEnd;
    this->frameStep = frameStep;
}

void FileBar::setResolution(int resolutionX, int resolutionY, int resolutionScale)
{
    ui->resolutionLabel->setText(
        QString::number(resolutionX)
        + "x"
        + QString::number(resolutionY)
        + " "
        + QString::number(resolutionScale)
        + "%"
    );

    this->resolutionX = resolutionX;
    this->resolutionY = resolutionY;
    this->resolutionScale = resolutionScale;
}

void FileBar::setRenderEngine(const QString &renderEngine)
{
    ui->renderEngineLabel->setText(renderEngine);
    QWriteLocker locker(&dataLock);
    this->renderEngine = renderEngine;
}

void FileBar::showLoadingBar()
{
    frameLoadingBar->show();
    resolutionLoadingBar->show();
    renderEngineLoadingBar->show();
}

void FileBar::hideLoadingBar()
{
    frameLoadingBar->hide();
    resolutionLoadingBar->hide();
    renderEngineLoadingBar->hide();
}

void FileBar::setState(ProjectState state)
{
    this->state = state;

    ui->frameButton->setEnabled(state != ProjectState::Loading && state != ProjectState::Rendering);

    switch (state)
    {
        case ProjectState::Loading:
            targetStyle = loadingStyle;
            break;

        case ProjectState::Queued:
            targetStyle = queueStyle;
            break;

        case ProjectState::Error:
            targetStyle = errorStyle;
            break;

        case ProjectState::Rendering:
            targetStyle = renderingStyle;
            break;

        case ProjectState::Finished:
            targetStyle = finishedStyle;
            break;

        default:
            break;
    }

    if (!timer->isActive())
    {
        timer->start();
        lastElapsed = elapsedTimer->elapsed();
    }
}

ProjectState FileBar::getState() const
{
    return state;
}

int FileBar::getFrameStart() const
{
    return frameStart;
}

int FileBar::getFrameEnd() const
{
    return frameEnd;
}

int FileBar::getFrameStep() const
{
    return frameStep;
}

int FileBar::getResolutionX() const
{
    return resolutionX;
}

int FileBar::getResolutionY() const
{
    return resolutionY;
}

int FileBar::getResolutionScale() const
{
    return resolutionScale;
}

QString FileBar::getRenderEngine() const
{
    QReadLocker locker(&dataLock);
    return renderEngine;
}

void FileBar::setOutputPath(const QString &outputPath)
{
    QWriteLocker locker(&dataLock);
    this->outputPath = outputPath;
}

QString FileBar::getOutputPath() const
{
    QReadLocker locker(&dataLock);
    return outputPath;
}

// Format frame number with zero padding like Blender.
static QString frameString(int frame, int minWidth) {
    const bool isNeg = frame < 0;
    int n = std::abs(frame);

    QString digits = QString::number(n);
    if (digits.size() < minWidth)
    {
        digits.prepend(QString(minWidth - digits.size(), QChar('0')));
    }

    if (isNeg)
    {
        digits.prepend('-');
    }

    return digits;
}

QString FileBar::getImagePathFromFrame(int frame)
{
    QString currentOutputPath;
    QString currentFilePath;
    {
        QReadLocker locker(&dataLock);
        currentOutputPath = outputPath;
        currentFilePath = filePath;
    }

    // Convert to native seperators
    const QString originalPath = QDir::fromNativeSeparators(currentOutputPath);
    QString path = originalPath;

    // Get the blend file directory
    QFileInfo blendInfo(currentFilePath);
    const QString blendDir = blendInfo.absolutePath();

    // Resolve Blender-style relative path "//"
    if (path.startsWith("//"))
    {
        path = QDir(blendDir).filePath(path.mid(2));
    }

    // Get original path flags
    const bool hasHash = originalPath.contains('#');
    const bool endsWithSlash = originalPath.endsWith('/');
    const bool isDoubleSlash = (originalPath == "//");

    // If the original path indicates a directory, append default "####"
    if (endsWithSlash)
    {
        path += "####";
    }
    else if (isDoubleSlash)
    {
        path += "/####";
    }

    // Replace all '#' groups with frame numbers, zero-padded to match the group length
    static QRegularExpression re(R"(#+)");
    QRegularExpressionMatchIterator it = re.globalMatch(path);
    QList<QPair<int,int>> spans;
    while (it.hasNext())
    {
        auto m = it.next();
        spans.append({m.capturedStart(0), m.capturedLength(0)});
    }
    for (int i = spans.size() - 1; i >= 0; i--)
    {
        const int start = spans[i].first;
        const int len = spans[i].second;
        path.replace(start, len, frameString(frame, len));
    }

    // If no '#' in original path and not a directory, append 4-digit frame
    if (!hasHash && !endsWithSlash && !isDoubleSlash)
    {
        QFileInfo fileInfo(path);
        const QString dir = fileInfo.path();
        const QString base = fileInfo.completeBaseName();
        const QString ext  = fileInfo.suffix();
        const QString numbered = base + frameString(frame, 4);

        // Ensure directory separator is correct
        if (ext.isEmpty())
        {
            path = QDir(dir).filePath(numbered);
        }
        else
        {
            path = QDir(dir).filePath(numbered + "." + ext);
        }
    }

    // Ensure the file has an extension, defaults to ".png"
    QFileInfo outFileInfo(path);
    if (outFileInfo.suffix().isEmpty())
    {
        path += ".png";
    }

    // Return path with native separators for the platform
    return QDir::toNativeSeparators(path);
}

void FileBar::onUpButtonClicked()
{
    emit upButtonClicked(this);
}

void FileBar::onDownButtonClicked()
{
    emit downButtonClicked(this);
}

void FileBar::onDeleteButtonClicked()
{
    emit deleteButtonClicked(this);
}

void FileBar::onReloadButtonClicked()
{
    ui->reloadButton->setEnabled(false);

    if (blenderFileReader->isRunning())
    {
        blenderFileReader->stop();
        blenderFileReader->wait();
    }

    setState(ProjectState::Loading);

    finishedFrame = 0;
    totalFrame = 0;

    blenderFileReader->start(QThread::LowestPriority);

    ui->frameButton->setText("");
    ui->resolutionLabel->setText("");
    ui->renderEngineLabel->setText("");
    ui->progressBar->setProgressBar(0.0f);
    ui->progressBarLabel->setText("");

    showLoadingBar();

    emit reloadButtonClicked(this);
}

void FileBar::onFinishedReading(int status, BlenderFileInfo info)
{
    if (status == 0)
    {
        setFrame(info.frameBegin, info.frameEnd, info.frameStep);
        setResolution(info.resolutionX, info.resolutionY, info.resolutionScale);
        setRenderEngine(info.renderEngine);

        setState(ProjectState::Queued);

        finishedFrame = 0;
        totalFrame = info.frameStep == 0 ? 0 : (info.frameEnd - info.frameBegin) / info.frameStep + 1;
        {
            QWriteLocker locker(&dataLock);
            outputPath = info.outputPath;
        }

        blenderRenderer->setFrame(info.frameBegin, info.frameEnd, info.frameStep);
        blenderRenderer->setCurrentFrame(info.frameBegin);

        float progress = static_cast<float>(finishedFrame) / totalFrame * 100.0f;
        ui->progressBar->setProgressBar(0.0f);
        ui->progressBarLabel->setText(
            QString::number(finishedFrame)
            + "/"
            + QString::number(totalFrame)
            + " ("
            + QString::number(static_cast<int>(progress))
            + "%)"
        );
    }
    else
    {
        setState(ProjectState::Error);
        frameStart = 0;
        frameEnd = 0;
        frameStep = 0;
        finishedFrame = 0;
        totalFrame = 0;
    }

    ui->reloadButton->setEnabled(true);
    hideLoadingBar();
    emit finishedReading();
}

void FileBar::onProgressChanged()
{
    finishedFrame = blenderRenderer->getFinishedFrame();
    totalFrame = blenderRenderer->getTotalFrame();
    float progress = static_cast<float>(finishedFrame) / totalFrame * 100.0f;
    ui->progressBar->setProgressBar(progress);
    ui->progressBarLabel->setText(
        QString::number(finishedFrame)
        + "/"
        + QString::number(totalFrame)
        + " ("
        + QString::number(static_cast<int>(progress))
        + "%)"
    );

    emit progressChanged();
}

void FileBar::onOutputTextUpdate(QString text)
{
    emit outputTextUpdate(text);
}

void FileBar::onFinishedRendering(int status)
{
    if (status != 0)
    {
        setState(ProjectState::Error);
    }
    else
    {
        setState(ProjectState::Finished);
    }

    ui->reloadButton->setEnabled(true);
    ui->deleteButton->setEnabled(true);

    emit finishedRendering();
}

void FileBar::updateStyle()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = static_cast<float>(elapsed - lastElapsed) / 1000;
    lastElapsed = elapsed;

    int iteration = std::max(1, (int)(dt / 0.01f));
    iteration = std::min(iteration, 10);
    dt /= iteration;

    for (int i = 0; i < iteration; i++)
    {
        Style delta = targetStyle - style;
        Style accel = stiffness * delta - damping * velocity;

        velocity += accel * dt;
        style += velocity * dt;

        if (delta.length() < 1.0f / 255.0f && velocity.length() < 1.0f / 255.0f)
        {
            velocity = Style::zero();
            style = targetStyle;
            timer->stop();
            break;
        }
    }

    ui->fileBarContentWidget->setStyleSheet(
        QString("background-color: rgb(43, 45, 48);\n")
        + "border: 1px solid "
        + style.borderColor.toText()
        + ";\n"
        + "border-radius: 16px;\n"
    );
    ui->fileBarContentWidget->update();
}
