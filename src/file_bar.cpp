// file_bar.cpp

#include <QTimer>
#include <QElapsedTimer>
#include <QThread>

#include "file_bar.h"
#include "ui_file_bar.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

#include "blender_file_info.h"
#include "blender_file_reader.h"

#include "blender_renderer.h"

#include "loading_bar.h"

FileBar::FileBar(
    QWidget *parent,
    DropShadowRenderer *dropShadowRenderer,
    QString fileName,
    QString filePath,
    QString blenderPath
):
    QWidget(parent),
    ui(new Ui::fileBar),
    state(State::Loading),
    frameStart(0),
    frameEnd(0),
    frameStep(0),
    finishedFrame(0),
    totalFrame(0)
{
    ui->setupUi(this);

    ui->fileNameLabel->setText(fileName);
    ui->frameLabel->setText("");
    ui->resolutionLabel->setText("");
    ui->renderEngineLabel->setText("");
    ui->reloadButton->setEnabled(false);
    ui->progressBar->setProgressBar(0.0f);
    ui->progressBarLabel->setText("");

    frameLoadingBar = new LoadingBar(ui->frameLabel);
    frameLoadingBar->setGeometry(0, 0, ui->frameLabel->width(), ui->frameLabel->height());
    ui->frameLabel->installEventFilter(this);

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
    QObject::connect(blenderRenderer, &BlenderRenderer::finishedRendering, this, &FileBar::onFinishedRendering);

    QObject::connect(ui->upButton, &QPushButton::clicked, this, &FileBar::onUpButtonClicked);
    QObject::connect(ui->downButton, &QPushButton::clicked, this, &FileBar::onDownButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &FileBar::onDeleteButtonClicked);
    QObject::connect(ui->reloadButton, &QPushButton::clicked, this, &FileBar::onReloadButtonClicked);

    dropShadowWidget = new DropShadowWidget(
        parent,
        this,
        dropShadowRenderer
    );
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.3f);
    dropShadowWidget->setBlurRadius(10);

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

void FileBar::setFileName(QString fileName)
{
    ui->fileNameLabel->setText(fileName);
}

QString FileBar::getFileName() const
{
    return ui->fileNameLabel->text();
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
    state = State::Rendering;

    ui->reloadButton->setEnabled(false);
    ui->deleteButton->setEnabled(false);
}

void FileBar::stopRender()
{
    blenderRenderer->stop();
    state = State::Queued;

    ui->reloadButton->setEnabled(true);
    ui->deleteButton->setEnabled(true);
}

bool FileBar::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->frameLabel && event->type() == QEvent::Resize) {
        frameLoadingBar->setGeometry(0, 0, ui->frameLabel->width(), ui->frameLabel->height());
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
    ui->frameLabel->setText(
        QString::number(frameStart)
        + "-"
        + QString::number(frameEnd)
        + " ("
        + QString::number(frameStep)
        + ")"
    );
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
}

void FileBar::setRenderEngine(int renderEngine)
{
    QString renderEngineText;
    if (renderEngine == 0)
    {
        renderEngineText = "Cycles";
    }
    else if (renderEngine == 1)
    {
        renderEngineText = "EEVEE";
    }
    else if (renderEngine == 2)
    {
        renderEngineText = "Workbench";
    }
    else
    {
        renderEngineText = "Unknown";
    }

    ui->renderEngineLabel->setText(renderEngineText);
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

FileBar::State FileBar::getState() const
{
    return state;
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

    state = State::Loading;
    finishedFrame = 0;
    totalFrame = 0;

    blenderFileReader->start(QThread::LowestPriority);

    ui->frameLabel->setText("");
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

        state = State::Queued;
        frameStart = info.frameBegin;
        frameEnd = info.frameEnd;
        frameStep = info.frameStep;
        finishedFrame = 0;
        totalFrame = info.frameStep == 0 ? 0 : (info.frameEnd - info.frameBegin) / info.frameStep + 1;

        blenderRenderer->setFrame(frameStart, frameEnd, frameStep);
        blenderRenderer->setCurrentFrame(frameStart);

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
        state = State::Error;
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

void FileBar::onFinishedRendering(int status)
{
    if (status != 0)
    {
        state = State::Error;
    }
    else
    {
        state = State::Finished;
    }

    ui->reloadButton->setEnabled(true);
    ui->deleteButton->setEnabled(true);

    emit finishedRendering();
}
