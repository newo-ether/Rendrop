// app_controller.h

#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <QObject>
#include <QString>

#include "config/distributed_settings.h"

class BlenderVersionManager;
class HttpServer;
class LoadMonitor;
class NodeAgent;
class RenderQueue;

// Owns everything that runs behind the window: the render queue, the Blender
// versions, the load monitor, the farm server and this machine's node agent.
// The window only drives it, so the back end and its settings files stay out
// of the UI.
//
// GUI thread only.
class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    // Restores the saved queue and starts the monitor, the agent and the
    // server. Call once the window listens to queue(), so the restored jobs
    // reach it.
    void start();

    RenderQueue *queue() const { return renderQueue; }
    BlenderVersionManager *blenderVersions() const { return versionManager; }
    const LoadMonitor *loadMonitor() const { return monitor; }
    // Replaced when the farm settings change; see nodeAgentReplaced.
    NodeAgent *nodeAgent() const { return agent; }

    // The version the queue renders with; empty when none is available.
    QString selectedBlender() const { return selectedVersion; }
    // An empty version clears the Blender of the queue and the agent and
    // keeps the saved choice; any other is saved for the next start.
    void selectBlender(const QString &version);
    QString selectedBlenderPath() const;

    void setMaxConcurrentLoads(int limit);
    // Applies settings saved on the settings page without a restart.
    void applyFarmSettings(const DistributedSettings &settings);

signals:
    // Null first while the old agent goes, then the new one.
    void nodeAgentReplaced(NodeAgent *agent);
    // A line for the log, from the node agent.
    void outputText(QString text);
    // The agent's state moved; the title bar status follows it.
    void nodeStateChanged();

private:
    void createNodeAgent(const DistributedSettings &settings);

    RenderQueue *renderQueue = nullptr;
    BlenderVersionManager *versionManager = nullptr;
    LoadMonitor *monitor = nullptr;
    HttpServer *httpServer = nullptr;
    NodeAgent *agent = nullptr;
    // What the running farm components were last configured with.
    DistributedSettings farmSettings;
    QString selectedVersion;
};

#endif // APP_CONTROLLER_H
