// file_bar.cpp

#include <algorithm>

#include <QEvent>
#include <QPainter>
#include <QPen>

#include "file_bar.h"
#include "frame_range_dialog.h"
#include "rename_dialog.h"
#include "ui_file_bar.h"

FileBar::FileBar(
    QWidget *parent,
    DropShadowRenderer *dropShadowRenderer,
    const ProjectInfo &project
):
    QWidget(parent),
    ui(new Ui::fileBar),
    project(project),
    dropShadowWidget(nullptr),
    frameLoadingBar(nullptr),
    resolutionLoadingBar(nullptr),
    renderEngineLoadingBar(nullptr),
    loadingStyle(Style(Color(80, 150, 210))),
    queueStyle(Style(Color(60, 60, 60))),
    errorStyle(Style(Color(180, 60, 80))),
    renderingStyle(Style(Color(80, 150, 210))),
    finishedStyle(Style(Color(60, 150, 80))),
    style(queueStyle),
    targetStyle(queueStyle),
    velocity(Style::zero()),
    stiffness(300),
    damping(50),
    lastElapsed(0),
    timer(new QTimer(this)),
    elapsedTimer(new QElapsedTimer())
{
    ui->setupUi(this);
    ui->fileNameButton->setToolTip(tr("Click to rename project"));
    ui->frameButton->setToolTip(tr("Click to edit frame range"));

    frameLoadingBar = new LoadingBar(ui->frameButton);
    frameLoadingBar->setGeometry(0, 0, ui->frameButton->width(), ui->frameButton->height());
    ui->frameButton->installEventFilter(this);
    resolutionLoadingBar = new LoadingBar(ui->resolutionLabel);
    resolutionLoadingBar->setGeometry(0, 0, ui->resolutionLabel->width(), ui->resolutionLabel->height());
    ui->resolutionLabel->installEventFilter(this);
    renderEngineLoadingBar = new LoadingBar(ui->renderEngineLabel);
    renderEngineLoadingBar->setGeometry(0, 0, ui->renderEngineLabel->width(), ui->renderEngineLabel->height());
    ui->renderEngineLabel->installEventFilter(this);

    QObject::connect(ui->upButton, &QPushButton::clicked, this, [this]() {
        emit moveUpRequested(this->project.id);
    });
    QObject::connect(ui->downButton, &QPushButton::clicked, this, [this]() {
        emit moveDownRequested(this->project.id);
    });
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(this->project.id);
    });
    QObject::connect(ui->reloadButton, &QPushButton::clicked, this, [this]() {
        emit reloadRequested(this->project.id);
    });
    QObject::connect(ui->frameButton, &QPushButton::clicked, this, &FileBar::onFrameButtonClicked);
    QObject::connect(ui->fileNameButton, &QPushButton::clicked, this, &FileBar::onFileNameButtonClicked);

    dropShadowWidget = new DropShadowWidget(parent, this, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.4f);
    dropShadowWidget->setBlurRadius(10);

    QObject::connect(timer, &QTimer::timeout, this, &FileBar::updateStyle);
    elapsedTimer->start();
    ui->fileBarContentWidget->installEventFilter(this);
    ui->fileBarContentWidget->setStyleSheet(
        "background-color: rgb(43, 45, 48);\n"
        "border: none;\n"
        "border-radius: 16px;\n"
    );

    applySnapshot(project);
    show();
    dropShadowWidget->show();
}

FileBar::~FileBar()
{
    delete frameLoadingBar;
    delete resolutionLoadingBar;
    delete renderEngineLoadingBar;
    delete dropShadowWidget;
    delete elapsedTimer;
    delete ui;
}

QUuid FileBar::projectId() const
{
    return project.id;
}

DropShadowWidget *FileBar::getDropShadowWidget() const
{
    return dropShadowWidget;
}

void FileBar::applySnapshot(const ProjectInfo &newProject)
{
    if (newProject.isNull || newProject.id != project.id)
    {
        return;
    }
    project = newProject;
    ui->fileNameButton->setText(project.name);
    setState(project.state);

    if (project.state == ProjectState::Loading)
    {
        ui->frameButton->setText("");
        ui->resolutionLabel->setText("");
        ui->renderEngineLabel->setText("");
        ui->progressBar->setProgressBar(0.0f);
        ui->progressBarLabel->setText("");
        showLoadingBars();
        return;
    }
    updateProjectDetails();
    updateProgress();
    hideLoadingBars();
}

bool FileBar::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->fileBarContentWidget && event->type() == QEvent::Paint)
    {
        QPainter painter(ui->fileBarContentWidget);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(43, 45, 48));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(ui->fileBarContentWidget->rect(), 16, 16);
        painter.setPen(QPen(style.borderColor.toQColor(), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(ui->fileBarContentWidget->rect().adjusted(1, 1, -1, -1), 16, 16);
        return true;
    }
    if (object == ui->frameButton && event->type() == QEvent::Resize)
    {
        frameLoadingBar->setGeometry(0, 0, ui->frameButton->width(), ui->frameButton->height());
    }
    else if (object == ui->resolutionLabel && event->type() == QEvent::Resize)
    {
        resolutionLoadingBar->setGeometry(0, 0, ui->resolutionLabel->width(), ui->resolutionLabel->height());
    }
    else if (object == ui->renderEngineLabel && event->type() == QEvent::Resize)
    {
        renderEngineLoadingBar->setGeometry(0, 0, ui->renderEngineLabel->width(), ui->renderEngineLabel->height());
    }
    return QWidget::eventFilter(object, event);
}

void FileBar::onFrameButtonClicked()
{
    if (project.state == ProjectState::Loading
        || project.state == ProjectState::Rendering
        || project.state == ProjectState::Error)
    {
        return;
    }
    FrameRangeDialog dialog(project.frameStart, project.frameEnd, project.frameStep, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        emit frameRangeRequested(
            project.id,
            dialog.getStart(),
            dialog.getEnd(),
            dialog.getStep()
        );
    }
}

void FileBar::onFileNameButtonClicked()
{
    RenameDialog dialog(project.name, this);
    if (dialog.exec() == QDialog::Accepted && !dialog.getName().isEmpty())
    {
        emit renameRequested(project.id, dialog.getName());
    }
}

void FileBar::setState(ProjectState state)
{
    ui->frameButton->setEnabled(
        state != ProjectState::Loading
        && state != ProjectState::Rendering
        && state != ProjectState::Error
    );
    ui->reloadButton->setEnabled(
        state != ProjectState::Loading
        && state != ProjectState::Rendering
    );
    ui->deleteButton->setEnabled(state != ProjectState::Rendering);

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
    }
    if (!timer->isActive())
    {
        timer->start();
        lastElapsed = elapsedTimer->elapsed();
    }
}

void FileBar::showLoadingBars()
{
    frameLoadingBar->show();
    resolutionLoadingBar->show();
    renderEngineLoadingBar->show();
}

void FileBar::hideLoadingBars()
{
    frameLoadingBar->hide();
    resolutionLoadingBar->hide();
    renderEngineLoadingBar->hide();
}

void FileBar::updateProjectDetails()
{
    if (project.state == ProjectState::Error)
    {
        ui->frameButton->setText("");
        ui->resolutionLabel->setText("");
        ui->renderEngineLabel->setText("");
        return;
    }
    ui->frameButton->setText(
        QString::number(project.frameStart)
        + "-"
        + QString::number(project.frameEnd)
        + " ("
        + QString::number(project.frameStep)
        + ")"
    );
    ui->resolutionLabel->setText(
        QString::number(project.resolutionX)
        + "x"
        + QString::number(project.resolutionY)
        + " "
        + QString::number(project.resolutionScale)
        + "%"
    );
    ui->renderEngineLabel->setText(project.renderEngine);
}

void FileBar::updateProgress()
{
    const float progress = project.totalFrame <= 0
        ? 0.0f
        : static_cast<float>(project.finishedFrame) / project.totalFrame * 100.0f;
    ui->progressBar->setProgressBar(progress);
    if (project.totalFrame <= 0)
    {
        ui->progressBarLabel->setText("");
        return;
    }
    ui->progressBarLabel->setText(
        QString::number(project.finishedFrame)
        + "/"
        + QString::number(project.totalFrame)
        + " ("
        + QString::number(static_cast<int>(progress))
        + "%)"
    );
}

void FileBar::updateStyle()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = static_cast<float>(elapsed - lastElapsed) / 1000;
    lastElapsed = elapsed;
    int iteration = std::max(1, static_cast<int>(dt / 0.01f));
    iteration = std::min(iteration, 10);
    dt /= iteration;
    for (int index = 0; index < iteration; ++index)
    {
        Style delta = targetStyle - style;
        Style acceleration = stiffness * delta - damping * velocity;
        velocity += acceleration * dt;
        style += velocity * dt;
        if (delta.length() < 1.0f / 255.0f && velocity.length() < 1.0f / 255.0f)
        {
            velocity = Style::zero();
            style = targetStyle;
            timer->stop();
            break;
        }
    }
    ui->fileBarContentWidget->update();
}
