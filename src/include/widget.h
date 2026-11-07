// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <vector>

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QPixmap>
#include <QString>
#include <QWidget>

#include "add_file_button.h"
#include "blender_version_manager.h"
#include "caption_button.h"
#include "drop_file_tip.h"
#include "drop_shadow_renderer.h"
#include "file_bar.h"
#include "http_server.h"
#include "load_monitor.h"
#include "node_agent.h"
#include "nodes_page.h"
#include "project_info.h"
#include "render_queue.h"
#include "settings_page.h"
#include "look_widgets.h"

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
    Widget(int languageIndex, QWidget *parent = nullptr);
    ~Widget() override;

    // Brings the window back from the tray, or to the front when another
    // launch of Rendrop asks for it.
    void showFromTray();

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
        float blurRadius
    );
    QString selectedBlenderPath() const;
    void updateBlenderVersions();
    void updateButtonStatus();
    void updateStatisticInfo();
    void clearOutputText();
    void setSelectorEnabled(bool enabled);
    void updateAllFileBarShadow();
    void installRoundedSurfaces();
    void createPages();
    void setupWindowFrame();
    void showPage(int index);
    void updateStatusChip();
    void updateNodeCounts();
    void setLogVisible(bool visible);
    void setupTray();
    void startNodeAgent(const DistributedSettings &settings);
    // Asks before quitting while files are rendering or loading.
    bool confirmQuit();
    void requestQuit();

private slots:
    void onLanguageChanged(int index);
    void onFileBarMoveUp(QUuid id);
    void onFileBarMoveDown(QUuid id);
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
    AddFileButton *addFileButton;
    DropFileTip *dropFileTip;
    BlenderVersionManager *blenderVersionManager;
    RenderQueue *renderQueue;
    std::vector<FileBar *> fileBars;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    DropShadowRenderer *dropShadowRenderer;
    QPixmap fileBarShadowPixmap;
    int shadowBufferHandle;
    HttpServer *httpServer;
    LoadMonitor *loadMonitor = nullptr;
    NodeAgent *nodeAgent = nullptr;
    // What the running farm components were last configured with.
    DistributedSettings farmSettings;
    QWK::WidgetWindowAgent *windowAgent = nullptr;
    CaptionButton *maximizeButton = nullptr;
    std::vector<NavButton *> navButtons;
    NodesPage *nodesPage = nullptr;
    SettingsPage *settingsPage = nullptr;
    int languageIndex = 0;
    // Throughput since rendering started, for the ETA.
    QElapsedTimer renderClock;
    int framesAtRenderStart = 0;
    // Closing the window hides it here; only the tray menu quits.
    QSystemTrayIcon *trayIcon = nullptr;
};

#endif // WIDGET_H
