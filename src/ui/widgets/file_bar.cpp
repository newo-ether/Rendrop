// file_bar.cpp

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include "ui/widgets/file_bar.h"
#include "ui/style/look_widgets.h"
#include "ui/motion/transition.h"
#include "ui/style/rounded_surface.h"
#include "ui/widgets/frame_range_dialog.h"
#include "ui/widgets/rename_dialog.h"
#include "ui_file_bar.h"

namespace
{

Style rowStyle(const Theme::Tone &tone)
{
    const QColor color = tone.color();
    return Style(Color(color.red(), color.green(), color.blue()));
}

}

FileBar::FileBar(QWidget *parent, const ProjectInfo &project):
    QWidget(parent),
    ui(new Ui::fileBar),
    project(project),
    dropShadowWidget(nullptr),
    frameLoadingBar(nullptr),
    resolutionLoadingBar(nullptr),
    renderEngineLoadingBar(nullptr),
    loadingStyle(rowStyle(Theme::rowActive)),
    queueStyle(rowStyle(Theme::rowQueued)),
    errorStyle(rowStyle(Theme::rowError)),
    renderingStyle(rowStyle(Theme::rowActive)),
    finishedStyle(rowStyle(Theme::rowFinished)),
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
    // The name stands out a little; numbers that line up use the mono face.
    ui->fileNameButton->setFont(Look::pixelFont(10, QFont::Medium));
    ui->frameButton->setFont(Look::monoFont(9));
    ui->resolutionLabel->setFont(Look::monoFont(9));
    ui->renderEngineLabel->setFont(Look::pixelFont(10, QFont::Medium));
    ui->progressBarLabel->setFont(Look::monoFont(8));
    ui->fileNameButton->setToolTip(tr("Click to rename project"));
    ui->frameButton->setToolTip(tr("Click to edit frame range"));

    // RoundedSurface paints the pills and fades them between states;
    // RendropStyle draws the labels with the surface's text color.
    const Theme::Tone &outline = Theme::outline;
    const Theme::Tone &pill = Theme::pill;
    RoundedSurface::install(ui->fileNameButton, 8, RoundedSurface::Colors {
        Theme::control, Theme::controlHover, Theme::controlPressed, {}, outline});
    Look::setPadding(ui->fileNameButton, QMargins(12, 0, 12, 0), Qt::AlignLeft | Qt::AlignVCenter);
    RoundedSurface::install(ui->frameButton, 8, RoundedSurface::Colors {
        pill, Theme::pillHover, Theme::pillPressed, {}, outline});
    Look::setPadding(ui->frameButton, QMargins());
    for (QLabel *label : {ui->resolutionLabel, ui->renderEngineLabel})
    {
        Look::setTextColor(label, Theme::textStrong);
        RoundedSurface::install(label, 8, RoundedSurface::Colors {pill, {}, {}, {}, outline});
    }
    for (QPushButton *button : {ui->reloadButton, ui->deleteButton})
    {
        Look::setTextColor(button, Theme::textStrong, Theme::textDisabledLight);
        RoundedSurface::install(button, 6, RoundedSurface::Colors {
            Theme::control, Theme::controlHover, Theme::controlPressed, Theme::disabledFill, outline});
        Look::setPadding(button, QMargins());
    }
    const Theme::Tone &glyphTone = Theme::iconGlyph;
    Look::setGlyphIcon(ui->reloadButton, QStringLiteral(":/icon/retry.ico"), glyphTone);
    Look::setGlyphIcon(ui->deleteButton, QStringLiteral(":/icon/close.ico"), glyphTone);
    ui->dragHandle->setAttribute(Qt::WA_Hover);
    ui->dragHandle->setCursor(Qt::OpenHandCursor);
    ui->dragHandle->setToolTip(tr("Drag to reorder"));
    ui->dragHandle->installEventFilter(this);
    Look::setTextColor(ui->progressBarLabel, Theme::textStrong);
    // State outlines differ per scheme; the scheme cross-fade covers the jump.
    QObject::connect(Theme::notifier(), &Theme::Notifier::changed, this, [this]() {
        loadingStyle = rowStyle(Theme::rowActive);
        queueStyle = rowStyle(Theme::rowQueued);
        errorStyle = rowStyle(Theme::rowError);
        renderingStyle = rowStyle(Theme::rowActive);
        finishedStyle = rowStyle(Theme::rowFinished);
        setState(this->project.state);
        timer->stop();
        style = targetStyle;
        velocity = Style::zero();
        ui->fileBarContentWidget->update();
    });

    frameLoadingBar = new LoadingBar(ui->frameButton);
    frameLoadingBar->setGeometry(0, 0, ui->frameButton->width(), ui->frameButton->height());
    ui->frameButton->installEventFilter(this);
    resolutionLoadingBar = new LoadingBar(ui->resolutionLabel);
    resolutionLoadingBar->setGeometry(0, 0, ui->resolutionLabel->width(), ui->resolutionLabel->height());
    ui->resolutionLabel->installEventFilter(this);
    renderEngineLoadingBar = new LoadingBar(ui->renderEngineLabel);
    renderEngineLoadingBar->setGeometry(0, 0, ui->renderEngineLabel->width(), ui->renderEngineLabel->height());
    ui->renderEngineLabel->installEventFilter(this);

    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(this->project.id);
    });
    QObject::connect(ui->reloadButton, &QPushButton::clicked, this, [this]() {
        emit reloadRequested(this->project.id);
    });
    QObject::connect(ui->frameButton, &QPushButton::clicked, this, &FileBar::onFrameButtonClicked);
    QObject::connect(ui->fileNameButton, &QPushButton::clicked, this, &FileBar::onFileNameButtonClicked);

    dropShadowWidget = new DropShadowWidget(parent, this);
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.6f);
    dropShadowWidget->setBlurRadius(15);
    dropShadowWidget->setHoverLift(true);

    QObject::connect(timer, &QTimer::timeout, this, &FileBar::updateStyle);
    elapsedTimer->start();
    // The event filter paints the rounded background and the state outline,
    // and opens the project page on a click.
    ui->fileBarContentWidget->installEventFilter(this);
    ui->fileBarContentWidget->setAttribute(Qt::WA_Hover);
    ui->fileBarContentWidget->setCursor(Qt::PointingHandCursor);
    ui->fileBarContentWidget->setToolTip(tr("Click to open the project"));

    applySnapshot(project);
    // A new bar starts in its state's colors; only later changes animate.
    timer->stop();
    style = targetStyle;
    velocity = Style::zero();
    show();
    dropShadowWidget->setVisible(DropShadowWidget::shadowsEnabled());
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

void FileBar::setNodeCount(int count)
{
    if (count == nodeCount)
    {
        return;
    }
    nodeCount = count;
    if (project.state != ProjectState::Loading)
    {
        updateProgress();
    }
}

void FileBar::paintDragHandle()
{
    QWidget *handle = ui->dragHandle;
    const qreal hover = Fader::level(handle, 0, handle->underMouse() || dragging);
    QPainter painter(handle);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Look::mix(Theme::handle, Theme::textHover, hover));
    // Two columns of three dots, centered.
    constexpr qreal dot = 2.6;
    constexpr qreal gapX = 5.0;
    constexpr qreal gapY = 5.5;
    const QPointF center = QRectF(handle->rect()).center();
    for (int column = 0; column < 2; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            const QPointF at(center.x() + (column - 0.5) * gapX, center.y() + (row - 1) * gapY);
            painter.drawEllipse(at, dot / 2, dot / 2);
        }
    }
}

bool FileBar::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->dragHandle)
    {
        switch (event->type())
        {
        case QEvent::Paint:
            paintDragHandle();
            return true;
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
            ui->dragHandle->update();
            break;
        case QEvent::MouseButtonPress:
        {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton)
            {
                // The press is taken so the handle keeps the mouse grab and
                // the bar does not open the project page.
                dragging = true;
                ui->dragHandle->setCursor(Qt::ClosedHandCursor);
                ui->dragHandle->update();
                emit dragStarted(project.id, mouse->globalPosition());
            }
            return true;
        }
        case QEvent::MouseMove:
            if (dragging)
            {
                emit dragMoved(project.id, static_cast<QMouseEvent *>(event)->globalPosition());
            }
            return true;
        case QEvent::MouseButtonRelease:
            if (dragging && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
            {
                dragging = false;
                ui->dragHandle->setCursor(Qt::OpenHandCursor);
                ui->dragHandle->update();
                emit dragFinished(project.id);
            }
            return true;
        default:
            break;
        }
        return QWidget::eventFilter(object, event);
    }
    if (object == ui->fileBarContentWidget && event->type() == QEvent::Paint)
    {
        QWidget *content = ui->fileBarContentWidget;
        const qreal hover = Fader::level(content, 0, content->underMouse());
        const qreal press = Fader::level(content, 1, bodyPressed && bodyPressInside);
        QPainter painter(content);
        // Pressed sinks below the resting color, like the pills' pressed state.
        RoundedSurface::paintFrame(painter, QRectF(content->rect()), 16,
            Look::mix(Look::mix(Theme::row, Theme::rowHover, hover), Theme::rowPressed, press),
            style.borderColor.toQColor());
        return true;
    }
    if (object == ui->fileBarContentWidget)
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave
            || event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
        {
            ui->fileBarContentWidget->update();
        }
        else if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
        {
            // Taken so the release comes back here.
            bodyPressed = true;
            bodyPressInside = true;
            ui->fileBarContentWidget->update();
            return true;
        }
        else if (event->type() == QEvent::MouseMove && bodyPressed)
        {
            // Leaving the bar while held lifts the pressed look, as a button
            // does. The pointer stays grabbed, so underMouse() would not tell.
            const bool inside = ui->fileBarContentWidget->rect().contains(
                static_cast<QMouseEvent *>(event)->position().toPoint());
            if (inside != bodyPressInside)
            {
                bodyPressInside = inside;
                ui->fileBarContentWidget->update();
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
        {
            const bool wasPressed = bodyPressed;
            bodyPressed = false;
            ui->fileBarContentWidget->update();
            if (wasPressed && ui->fileBarContentWidget->rect().contains(static_cast<QMouseEvent *>(event)->position().toPoint()))
            {
                emit detailsRequested(project.id);
            }
            return true;
        }
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
    // Opens as a popover under the button.
    FrameRangeDialog dialog(project.frameStart, project.frameEnd, project.frameStep, ui->frameButton);
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
    RenameDialog dialog(project.name, ui->fileNameButton);
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
        + (project.state == ProjectState::Rendering && nodeCount > 1
            ? tr(" · %n node(s)", nullptr, nodeCount)
            : QString())
    );
}

void FileBar::updateStyle()
{
    qint64 elapsed = elapsedTimer->elapsed();
    // A stalled event loop must not hand the spring one huge step: explicit
    // integration diverges once damping * step exceeds 2, which flashes
    // clamped colors. Long gaps are dropped and the rest is cut into 5ms steps.
    float dt = std::min(static_cast<float>(elapsed - lastElapsed) / 1000, 0.05f);
    lastElapsed = elapsed;
    const int iteration = std::max(1, static_cast<int>(std::ceil(dt / 0.005f)));
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
