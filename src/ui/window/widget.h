// widget.h

#ifndef WIDGET_H
#define WIDGET_H

#include <vector>

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QString>
#include <QWidget>

#include "config/distributed_settings.h"

class AppController;
class BlenderVersionManager;
class CaptionButton;
class DropShadowWidget;
class NavButton;
class NodesPage;
class ProjectPage;
class QSystemTrayIcon;
class QueuePage;
class RenderQueue;
class SettingsPage;

namespace QWK {
class WidgetWindowAgent;
}

QT_BEGIN_NAMESPACE
namespace Ui {
class widget;
}
QT_END_NAMESPACE

// The main window: the frameless title bar with its status chip, the
// navigation rail and the page stack, the tray icon and its notifications.
// The pages do their own work; the window routes between them.
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
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void installRoundedSurfaces();
    void createPages();
    void setupWindowFrame();
    void showPage(int index);
    void updateStatusChip();
    // The Blender versions changed: the selector and the settings list follow.
    void onBlenderVersionsChanged();
    void setupTray();
    // Asks before quitting while files are rendering or loading.
    bool confirmQuit();
    void requestQuit();
    // Windows toast through the tray icon; silent when there is no tray.
    void notify(const QString &title, const QString &text, bool error);

    void onLanguageChanged(int index);
    void onAddBlenderVersionButtonClicked();
    void onDeleteBlenderVersion(QString version);
    // Applies settings saved on the settings page without a restart.
    void applyFarmSettings(DistributedSettings settings);

    Ui::widget *ui;
    AppController *controller;
    // Owned by the controller.
    BlenderVersionManager *blenderVersionManager;
    RenderQueue *renderQueue;
    std::vector<DropShadowWidget *> dropShadowWidgets;
    QWK::WidgetWindowAgent *windowAgent = nullptr;
    CaptionButton *maximizeButton = nullptr;
    std::vector<NavButton *> navButtons;
    QueuePage *queuePage = nullptr;
    NodesPage *nodesPage = nullptr;
    SettingsPage *settingsPage = nullptr;
    ProjectPage *projectPage = nullptr;
    // Queue, nodes and settings come first in the page stack.
    static constexpr int settingsPageIndex = 2;
    static constexpr int projectPageIndex = 3;
    int languageIndex = 0;
    // Closing the window hides it here; only the tray menu quits.
    QSystemTrayIcon *trayIcon = nullptr;
    // At sign-in Explorer may not have its notification area yet.
    int trayAttempts = 0;
    static constexpr int maxTrayAttempts = 60;
    bool backgroundStart = false;
};

#endif // WIDGET_H
