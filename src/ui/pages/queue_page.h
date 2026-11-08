// queue_page.h

#ifndef QUEUE_PAGE_H
#define QUEUE_PAGE_H

#include <functional>
#include <vector>

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QWidget>

#include "core/project_info.h"

class AddFileButton;
class AppController;
class BlenderVersionManager;
class DropFileTip;
class DropShadowWidget;
class FileBar;
class RenderQueue;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class queuePage;
}
QT_END_NAMESPACE

// The render queue page: the Blender selector with the queue summary and the
// render button, the list of file bars with drag reordering, the log and the
// output line under them. Everything it shows comes from the controller's
// queue and Blender versions.
class QueuePage : public QWidget
{
    Q_OBJECT

public:
    // The controller must outlive the page.
    explicit QueuePage(AppController *controller, QWidget *parent = nullptr);
    ~QueuePage() override;

    // Whether .blend files can be queued now; false until a Blender exists.
    bool acceptsFiles() const;
    // Queues each .blend file, named after the file.
    void addFiles(const QStringList &paths);
    // The Blender versions changed elsewhere (the settings page). Keeps the
    // current version selected; the first one only gets picked when nothing
    // was selected before.
    void refreshBlenderVersions();
    // Node counts and the ETA follow lease activity that has no signal of its
    // own; the window calls this once a second.
    void refreshLiveInfo();
    // A line for the output bar and the log.
    void appendOutput(const QString &text);

signals:
    void projectDetailsRequested(QUuid id);
    // The empty selector's placeholder was picked.
    void addBlenderRequested();
    // A toast for the end of a run or a failed job.
    void notificationRequested(QString title, QString text, bool error);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createFileBar(const ProjectInfo &project);
    FileBar *findFileBar(const QUuid &id) const;
    DropShadowWidget *addShadow(QWidget *parent, QWidget *target, float borderRadius, float offset,
                                float alphaMax, float blurRadius, bool hoverLift);
    void installRoundedSurfaces();
    void updateBlenderVersions();
    void rebuildBlenderSelector();
    void updateButtonStatus();
    void updateStatisticInfo();
    void updateNodeCounts();
    void clearOutputText();
    void setLogShown(bool shown);
    // Runs change, which reorders or removes queue bars, and slides every bar
    // that moved from where it was to where the layout puts it.
    void animateQueueLayout(const std::function<void ()> &change);
    // Puts the dragged bar under the pointer and reorders the others live.
    void placeDraggedBar();
    // Scrolls the queue while a drag holds the pointer near its edge.
    void autoScrollDrag();
    void endDrag();

    void onFileBarDragStarted(QUuid id, QPointF globalPosition);
    void onFileBarDragMoved(QUuid id, QPointF globalPosition);
    void onFileBarDragFinished(QUuid id);
    void onFileBarDelete(QUuid id);
    void onJobAdded(ProjectInfo project);
    void onJobUpdated(ProjectInfo project);
    void onJobRemoved(QUuid id);
    void onJobMoved(QUuid id, int fromIndex, int toIndex);
    void onQueueChanged();
    void onRenderingChanged(bool rendering);
    void onAddFileButtonClicked();
    void onRenderButtonClicked();

    Ui::queuePage *ui;
    AppController *controller;
    // Owned by the controller.
    RenderQueue *renderQueue;
    BlenderVersionManager *blenderVersionManager;
    AddFileButton *addFileButton = nullptr;
    DropFileTip *dropFileTip = nullptr;
    std::vector<FileBar *> fileBars;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    // Drag reordering of the queue. Every bar has the same height, so the
    // slots taken at the start stay valid while bars change places.
    QPointer<FileBar> draggedBar;
    std::vector<int> dragSlots;
    int dragStartIndex = -1;
    qreal dragGrabOffset = 0.0;
    // Holds the dragged bar's slot in the layout while the bar itself is out
    // of it, so nothing re-lays the bar out from under the pointer.
    QPointer<QWidget> dragPlaceholder;
    QPointF dragPointer;
    QTimer *dragScrollTimer = nullptr;
    // Throughput since rendering started, for the ETA.
    QElapsedTimer renderClock;
    int framesAtRenderStart = 0;
    // Set by the Stop button so the end of the run is logged as a manual
    // stop and does not raise a "finished" notification.
    bool stopRequested = false;
    // Jobs that entered Error while this run was going, for the summary.
    int failedThisRun = 0;
    int finishedThisRun = 0;
    // Last state seen per job, to notice Rendering -> Finished / Error.
    QHash<QUuid, ProjectState> lastJobStates;
};

#endif // QUEUE_PAGE_H
