// queue_page.cpp

#include "ui/pages/queue_page.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>

#include "app/app_controller.h"
#include "core/blender_version_manager.h"
#include "core/render_queue.h"
#include "ui/motion/list_motion.h"
#include "ui/motion/smooth_scroll.h"
#include "ui/style/drop_shadow_widget.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rounded_surface.h"
#include "ui/widgets/add_file_button.h"
#include "ui/widgets/drop_file_tip.h"
#include "ui/widgets/file_bar.h"
#include "ui/widgets/log_panel.h"
#include "ui/widgets/message_dialog.h"
#include "ui_queue_page.h"

QueuePage::QueuePage(AppController *controller, QWidget *parent):
    QWidget(parent),
    ui(new Ui::queuePage),
    controller(controller),
    renderQueue(controller->queue()),
    blenderVersionManager(controller->blenderVersions())
{
    ui->setupUi(this);
    ui->selectorComboBox->setFont(Look::pixelFont(11, QFont::Medium));
    ui->summaryLabel->setFont(Look::monoFont(9));
    ui->renderButton->setFont(Look::pixelFont(11, QFont::Medium));
    ui->logToggleButton->setFont(Look::pixelFont(9, QFont::Medium));

    QObject::connect(renderQueue, &RenderQueue::jobAdded, this, &QueuePage::onJobAdded);
    QObject::connect(renderQueue, &RenderQueue::jobUpdated, this, &QueuePage::onJobUpdated);
    QObject::connect(renderQueue, &RenderQueue::jobRemoved, this, &QueuePage::onJobRemoved);
    QObject::connect(renderQueue, &RenderQueue::jobMoved, this, &QueuePage::onJobMoved);
    QObject::connect(renderQueue, &RenderQueue::queueChanged, this, &QueuePage::onQueueChanged);
    QObject::connect(renderQueue, &RenderQueue::renderingChanged, this, &QueuePage::onRenderingChanged);
    QObject::connect(renderQueue, &RenderQueue::outputTextUpdated, this, &QueuePage::appendOutput);
    QObject::connect(controller, &AppController::outputText, this, &QueuePage::appendOutput);

    updateBlenderVersions();
    const int selectedIndex = ui->selectorComboBox->findText(controller->selectedBlender());
    if (selectedIndex != -1)
    {
        ui->selectorComboBox->setCurrentIndex(selectedIndex);
    }
    QObject::connect(ui->selectorComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        const bool placeholder = text.isEmpty() || text == tr("Add Blender...");
        this->controller->selectBlender(placeholder ? QString() : text);
    });
    // The placeholder entry of an empty selector leads to where versions
    // are added.
    QObject::connect(ui->selectorComboBox, &QComboBox::activated, this, [this]() {
        if (blenderVersionManager->getBlenderVersionCount() == 0)
        {
            emit addBlenderRequested();
        }
    });

    ui->scrollAreaContent->installEventFilter(this);

    addFileButton = new AddFileButton(this);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &QueuePage::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);
    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Cards lift their shadow under the pointer; controls inside them do not.
    addShadow(this, ui->selectorContainer, 12, 4, 0.8f, 15, true);
    addShadow(ui->selectorWidgetContent, ui->selectorComboBox, 8, 3, 0.6f, 10, false);
    addShadow(ui->selectorWidgetContent, ui->renderButton, 10, 3, 0.6f, 10, false);
    addShadow(this, ui->scrollAreaContainer, 12, 4, 0.8f, 15, true);
    addShadow(this, ui->outputContainer, 15, 4, 0.6f, 10, true);
    ui->logContainer->setShadow(addShadow(this, ui->logContainer, 12, 4, 0.8f, 15, true));

    dropFileTip = new DropFileTip(ui->scrollAreaContent);
    dropFileTip->lower();
    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    QObject::connect(ui->renderButton, &QPushButton::clicked, this, &QueuePage::onRenderButtonClicked);
    // The page's gaps become items of their own, so the log card can carry
    // the gap in front of it while it grows and shrinks; layout spacing next
    // to a zero-height card would appear and vanish in one step.
    {
        QBoxLayout *page = ui->queuePageLayout;
        const int gap = page->spacing();
        page->setSpacing(0);
        page->insertSpacing(page->indexOf(ui->scrollAreaContainer), gap);
        auto *logGap = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
        page->insertItem(page->indexOf(ui->logContainer), logGap);
        page->insertSpacing(page->indexOf(ui->outputContainer), gap);
        ui->logContainer->setLeadingGap(logGap, gap);
    }
    setLogShown(false);
    QObject::connect(ui->logToggleButton, &QPushButton::clicked, this, [this]() {
        setLogShown(!ui->logContainer->isShown());
    });

    installRoundedSurfaces();

    onRenderingChanged(false);
    updateStatisticInfo();
    updateButtonStatus();
    clearOutputText();
}

QueuePage::~QueuePage()
{
    // The back end belongs to the controller and outlives the page.
    QObject::disconnect(controller, nullptr, this, nullptr);
    QObject::disconnect(renderQueue, nullptr, this, nullptr);
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
    delete ui;
}

bool QueuePage::acceptsFiles() const
{
    return addFileButton->isEnabled();
}

void QueuePage::addFiles(const QStringList &paths)
{
    for (const QString &path : paths)
    {
        if (!path.isEmpty())
        {
            const QString nativePath = QDir::toNativeSeparators(path);
            renderQueue->addJob(nativePath, QFileInfo(nativePath).completeBaseName());
        }
    }
}

void QueuePage::refreshBlenderVersions()
{
    updateBlenderVersions();
    updateButtonStatus();
    clearOutputText();
}

void QueuePage::refreshLiveInfo()
{
    updateNodeCounts();
    updateStatisticInfo();
}

bool QueuePage::eventFilter(QObject *watched, QEvent *event)
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

DropShadowWidget *QueuePage::addShadow(
    QWidget *parent,
    QWidget *target,
    float borderRadius,
    float offset,
    float alphaMax,
    float blurRadius,
    bool hoverLift
)
{
    DropShadowWidget *shadow = DropShadowWidget::attach(
        parent, target, {borderRadius, offset, offset, alphaMax, blurRadius}, hoverLift);
    dropShadowWidgets.push_back(shadow);
    return shadow;
}

void QueuePage::installRoundedSurfaces()
{
    // Everything here is painted: RoundedSurface draws the backgrounds and
    // fades them, RendropStyle the controls; palettes carry the text colors.
    // The log card styles itself.
    const Theme::Tone &frame = Theme::frame;
    const Theme::Tone &content = Theme::surface;
    RoundedSurface::install(ui->selectorContainer, 12, frame);
    RoundedSurface::install(ui->selectorWidgetContent, 12, content);
    RoundedSurface::install(ui->scrollAreaContainer, 12, frame);
    RoundedSurface::install(ui->scrollArea, 12, content);
    Look::clearScrollArea(ui->scrollArea);
    SmoothScroll::install(ui->scrollArea);
    RoundedSurface::install(ui->outputContainer, 15, frame);
    RoundedSurface::install(ui->outputText, 15, Theme::outputFill);
    Look::setTextColor(ui->outputText, Theme::outputText);
    ui->outputLabel->setContentsMargins(10, 0, 10, 0);
    // Blender's last line, same face as the log it comes from.
    ui->outputLabel->setFont(Look::monoFont(8));
    Look::setTextColor(ui->outputLabel, Theme::logText);
    RoundedSurface::install(ui->selectorComboBox, 8, RoundedSurface::Colors {
        Theme::control, Theme::controlHover, {}, Theme::disabledFill, Theme::controlBorder});
    Look::setTextColor(ui->selectorComboBox, Theme::textStrong, Theme::textDisabledLight);
    RoundedSurface::Colors logToggle {Qt::transparent, Theme::Tone(QColor(255, 255, 255, 18), QColor(0, 0, 0, 14)), {}, {}};
    logToggle.text = Theme::logToggleText;
    logToggle.hoverText = Theme::textStrong;
    RoundedSurface::install(ui->logToggleButton, 8, logToggle);
    Look::setPadding(ui->logToggleButton, QMargins(8, 2, 8, 2));
    RoundedSurface::install(ui->renderButton, 10, RoundedSurface::Colors {
        Theme::accentFill,
        Theme::accentHover,
        Theme::accentPressed,
        Theme::Tone(QColor(0x3A, 0x3D, 0x45), QColor(0xC9, 0xCC, 0xD3))
    });
    Look::setTextColor(ui->renderButton, Theme::onAccent, Theme::Tone(QColor(0xAA, 0xAA, 0xAA), QColor(0xF4, 0xF5, 0xF7)));
    Look::setPadding(ui->renderButton, QMargins(6, 6, 6, 6));
    // The icon is drawn in a resolved color.
    QObject::connect(Theme::notifier(), &Theme::Notifier::changed, this, [this]() {
        ui->logToggleButton->setIcon(Look::terminalIcon(
            ui->logContainer->isShown() ? Theme::accentInk : Theme::logToggleText));
        // The summary's rich text carries its colors.
        updateStatisticInfo();
    });
}

void QueuePage::setLogShown(bool shown)
{
    ui->logContainer->setShown(shown);
    // Icon only; the accent tint marks the open state and the tooltip names it.
    ui->logToggleButton->setText(QString());
    ui->logToggleButton->setFixedSize(32, 24);
    ui->logToggleButton->setIconSize(QSize(16, 16));
    ui->logToggleButton->setIcon(Look::terminalIcon(shown ? Theme::accentInk : Theme::logToggleText));
    ui->logToggleButton->setToolTip(shown ? tr("Hide log") : tr("Show log"));
    ui->logToggleButton->setAccessibleName(ui->logToggleButton->toolTip());
}

void QueuePage::appendOutput(const QString &text)
{
    ui->outputLabel->setText(text);
    ui->logContainer->append(text);
}

void QueuePage::createFileBar(const ProjectInfo &project)
{
    FileBar *fileBar = new FileBar(ui->scrollAreaContent, project);
    ui->scrollAreaVerticalLayout->insertWidget(static_cast<int>(fileBars.size()), fileBar);
    // Restored jobs at startup just appear; ones added later fade in.
    if (isVisible() && ui->scrollAreaContent->isVisible())
    {
        ui->scrollAreaVerticalLayout->activate();
        ListMotion::appear(fileBar, fileBar->getDropShadowWidget());
    }
    QObject::connect(fileBar, &FileBar::dragStarted, this, &QueuePage::onFileBarDragStarted);
    QObject::connect(fileBar, &FileBar::dragMoved, this, &QueuePage::onFileBarDragMoved);
    QObject::connect(fileBar, &FileBar::dragFinished, this, &QueuePage::onFileBarDragFinished);
    QObject::connect(fileBar, &FileBar::deleteRequested, this, &QueuePage::onFileBarDelete);
    QObject::connect(fileBar, &FileBar::reloadRequested, this, [this](QUuid id) {
        renderQueue->reloadJob(id);
    });
    QObject::connect(fileBar, &FileBar::renameRequested, this, [this](QUuid id, QString name) {
        renderQueue->renameJob(id, name);
    });
    QObject::connect(fileBar, &FileBar::frameRangeRequested, this, [this](QUuid id, int frameStart, int frameEnd, int frameStep) {
        renderQueue->setFrameRange(id, frameStart, frameEnd, frameStep);
    });
    QObject::connect(fileBar, &FileBar::detailsRequested, this, &QueuePage::projectDetailsRequested);
    fileBars.push_back(fileBar);
    dropFileTip->hide();
}

FileBar *QueuePage::findFileBar(const QUuid &id) const
{
    const auto iterator = std::find_if(fileBars.begin(), fileBars.end(), [id](const FileBar *fileBar) {
        return fileBar->projectId() == id;
    });
    return iterator == fileBars.end() ? nullptr : *iterator;
}

void QueuePage::updateNodeCounts()
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

void QueuePage::onFileBarDragStarted(QUuid id, QPointF globalPosition)
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
        QObject::connect(dragScrollTimer, &QTimer::timeout, this, &QueuePage::autoScrollDrag);
    }
    dragScrollTimer->start();
}

void QueuePage::onFileBarDragMoved(QUuid id, QPointF globalPosition)
{
    if (!draggedBar || draggedBar->projectId() != id)
    {
        return;
    }
    dragPointer = globalPosition;
    placeDraggedBar();
}

void QueuePage::onFileBarDragFinished(QUuid id)
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

void QueuePage::endDrag()
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

void QueuePage::placeDraggedBar()
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

void QueuePage::autoScrollDrag()
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

void QueuePage::onFileBarDelete(QUuid id)
{
    const ProjectInfo project = renderQueue->snapshotById(id);
    if (project.isNull)
    {
        return;
    }
    if (project.state == ProjectState::Loading)
    {
        const auto result = MessageDialog::warning(
            this,
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
        const auto result = MessageDialog::warning(
            this,
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

void QueuePage::onJobAdded(ProjectInfo project)
{
    createFileBar(project);
}

void QueuePage::onJobUpdated(ProjectInfo project)
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
        emit notificationRequested(
            tr("Render failed"),
            tr("%1 stopped at frame %2 / %3. See the log for details.")
                .arg(project.name).arg(project.finishedFrame).arg(project.totalFrame),
            true
        );
    }
}

void QueuePage::onJobRemoved(QUuid id)
{
    lastJobStates.remove(id);
    FileBar *fileBar = findFileBar(id);
    if (!fileBar)
    {
        return;
    }
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

void QueuePage::onJobMoved(QUuid id, int fromIndex, int toIndex)
{
    Q_UNUSED(fromIndex);
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
}

void QueuePage::animateQueueLayout(const std::function<void ()> &change)
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

void QueuePage::onQueueChanged()
{
    updateButtonStatus();
    updateStatisticInfo();
    clearOutputText();
}

void QueuePage::onRenderingChanged(bool rendering)
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
            appendOutput(summary);
            // A lone failure already raised its own toast.
            if (finishedThisRun > 0 || failedThisRun == 0)
            {
                emit notificationRequested(
                    failedThisRun == 0 ? tr("Render complete") : tr("Render ended with errors"),
                    summary,
                    failedThisRun > 0
                );
            }
        }
        stopRequested = false;
    }
    ui->selectorComboBox->setEnabled(!rendering);
    updateStatisticInfo();
    updateButtonStatus();
    if (!rendering)
    {
        clearOutputText();
    }
}

void QueuePage::onAddFileButtonClicked()
{
    addFiles(QFileDialog::getOpenFileNames(
        this,
        tr("Open File"),
        "",
        tr("Blender Projects (*.blend)")
    ));
}

void QueuePage::onRenderButtonClicked()
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
        appendOutput(tr("Render stopped manually") + where);
        renderQueue->stop();
    }
    else
    {
        renderQueue->start();
    }
    clearOutputText();
}

void QueuePage::updateBlenderVersions()
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

void QueuePage::rebuildBlenderSelector()
{
    ui->selectorComboBox->clear();
    const auto &versions = blenderVersionManager->getBlenderVersions();
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

void QueuePage::updateButtonStatus()
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

void QueuePage::updateStatisticInfo()
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
        QStringLiteral("<span style=\"color:%1\">%2</span>&nbsp;&nbsp;<span style=\"color:%3\">%4</span>")
            .arg(Theme::textStrong.color().name(),
                 tr("Frames %1 / %2").arg(finishedFrames).arg(totalFrames).toHtmlEscaped(),
                 Theme::placeholder.color().name(),
                 detail.toHtmlEscaped())
    );
    ui->summaryProgress->setValue(totalFrames == 0 ? 0.0 : 100.0 * finishedFrames / totalFrames);
}

void QueuePage::clearOutputText()
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
