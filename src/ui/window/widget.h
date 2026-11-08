// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <functional>
#include <vector>

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QVariantAnimation>
#include <QString>
#include <QWidget>

#include "ui/widgets/add_file_button.h"
#include "core/blender_version_manager.h"
#include "ui/widgets/caption_button.h"
#include "ui/widgets/drop_file_tip.h"
#include "ui/widgets/file_bar.h"
#include "farm/node_agent.h"
#include "ui/pages/nodes_page.h"
#include "core/project_info.h"
#include "ui/pages/project_page.h"
#include "core/render_queue.h"
#include "ui/pages/settings_page.h"
#include "ui/style/look_widgets.h"

class AppController;
class QSystemTrayIcon;

namespace QWK {
class WidgetWindowAgent;
}

QT_BEGIN_NAMESPACE
namespace Ui {
class widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    // Drives the controller's back end, which must outlive the window.
    Widget(AppController *controller, int languageIndex, QWidget *parent = nullptr);
    ~Widget() override;

    // Brings the window back from the tray, or to the front when another
    // launch of Rendrop asks for it.
    void showFromTray();
    // Login launch (--tray): the window stays hidden. When the taskbar is
    // not up yet, the tray icon is retried; only if it never appears does
    // the window show, minimized, so Rendrop stays reachable.
    void startInBackground();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QMessageBox::StandardButton iconMessageBox(
        QMessageBox::Icon icon,
        QString title,
        QString text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok
        );
    QMessageBox::StandardButton errorMessageBox(
        QString title,
        QString text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok
    );
    QMessageBox::StandardButton warningMessageBox(
        QString title,
        QString text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok
    );
    QMessageBox::StandardButton infoMessageBox(
        QString title,
        QString text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok
    );

    void addProject(const QString &fileName, const QString &filePath);
    void createFileBar(const ProjectInfo &project);
    FileBar *findFileBar(const QUuid &id) const;
    void createDropShadowWidget(
        QWidget *parent,
        QWidget *target,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius,
        bool hoverLift
    );
    // Refills the selector and keeps the current version selected; the first
    // version only gets picked when nothing was selected before.
    void updateBlenderVersions();
    void rebuildBlenderSelector();
    void updateButtonStatus();
    void updateStatisticInfo();
    void clearOutputText();
    void setSelectorEnabled(bool enabled);
    void installRoundedSurfaces();
    void createPages();
    void setupWindowFrame();
    void showPage(int index);
    void updateStatusChip();
    void updateNodeCounts();
    // Animated once the window is up; the log card grows or shrinks.
    void setLogVisible(bool visible);
    // Runs change, which reorders or removes queue bars, and slides every bar
    // that moved from where it was to where the layout puts it.
    void animateQueueLayout(const std::function<void ()> &change);
    // Scrolls the log to its newest line unless the user scrolled up.
    void followLogTail();
    // Puts the dragged bar under the pointer and reorders the others live.
    void placeDraggedBar();
    // Scrolls the queue while a drag holds the pointer near its edge.
    void autoScrollDrag();
    void endDrag();
    void setupTray();
    // Asks before quitting while files are rendering or loading.
    bool confirmQuit();
    void requestQuit();

private slots:
    void onLanguageChanged(int index);
    void onFileBarDragStarted(QUuid id, QPointF globalPosition);
    void onFileBarDragMoved(QUuid id, QPointF globalPosition);
    void onFileBarDragFinished(QUuid id);
    void onFileBarDelete(QUuid id);
    void onFileBarReload(QUuid id);
    void onFileBarRename(QUuid id, QString name);
    void onFileBarFrameRange(QUuid id, int frameStart, int frameEnd, int frameStep);
    void onJobAdded(ProjectInfo project);
    void onJobUpdated(ProjectInfo project);
    void onJobRemoved(QUuid id);
    void onJobMoved(QUuid id, int fromIndex, int toIndex);
    void onQueueChanged();
    void onRenderingChanged(bool rendering);
    void onAddFileButtonClicked();
    void onAddBlenderVersionButtonClicked();
    void onDeleteBlenderVersion(QString version);
    // Applies settings saved on the settings page without a restart.
    void applyFarmSettings(DistributedSettings settings);
    void onRenderButtonClicked();
    void onOutputTextUpdated(QString text);

private:
    Ui::widget *ui;
    AppController *controller;
    AddFileButton *addFileButton;
    DropFileTip *dropFileTip;
    // Owned by the controller.
    BlenderVersionManager *blenderVersionManager;
    RenderQueue *renderQueue;
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
    QPointer<QVariantAnimation> logAnimation;
    // Fades with the log card while it grows or shrinks.
    QPointer<DropShadowWidget> logShadow;
    // Where the log is headed; it stays visible while it shrinks away.
    bool logShown = false;
    // The log keeps its newest line in view until the user scrolls up.
    bool logFollow = true;
    QWK::WidgetWindowAgent *windowAgent = nullptr;
    CaptionButton *maximizeButton = nullptr;
    std::vector<NavButton *> navButtons;
    NodesPage *nodesPage = nullptr;
    SettingsPage *settingsPage = nullptr;
    ProjectPage *projectPage = nullptr;
    // Queue, nodes and settings come first in the page stack.
    static constexpr int projectPageIndex = 3;
    int languageIndex = 0;
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
    // Windows toast through the tray icon; silent when there is no tray.
    void notify(const QString &title, const QString &text, bool error);
    // Closing the window hides it here; only the tray menu quits.
    QSystemTrayIcon *trayIcon = nullptr;
    // At sign-in Explorer may not have its notification area yet.
    int trayAttempts = 0;
    static constexpr int maxTrayAttempts = 60;
    bool backgroundStart = false;
};

#endif // WIDGET_H
