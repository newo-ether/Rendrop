// node_agent.h

#ifndef NODE_AGENT_H
#define NODE_AGENT_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "distributed_settings.h"
#include "node_discovery.h"
#include "remote_worker.h"

class HttpServer;
class LoadMonitor;
class QTimer;
class RenderQueue;
class BuiltInWorkerThread;

// Runs this machine's part of the render group: announces it, tracks the
// other nodes and, while the machine is idle, renders leases for nodes that
// offer work. When foreign load makes the machine busy, the running lease is
// handed back at once (frames already uploaded stay done).
//
// GUI thread only.
class NodeAgent : public QObject
{
    Q_OBJECT

public:
    enum class WorkerState
    {
        Disabled,
        Refusing,
        NoBlender,
        Busy,
        RenderingOwnQueue,
        Waiting,
        Working
    };

    struct Snapshot
    {
        bool enabled = false;
        bool acceptRemoteWork = false;
        bool discoveryListening = false;
        bool busy = true;
        bool offersWork = false;
        QString name;
        int httpPort = 0;
        WorkerState workerState = WorkerState::Disabled;
        // Valid while workerState is Working.
        QString coordinatorName;
        RemoteWorker::Status worker;
        QList<PeerNode> peers;
    };

    NodeAgent(
        RenderQueue *renderQueue,
        const LoadMonitor *loadMonitor,
        const DistributedSettings &settings,
        const QString &settingsPath,
        QObject *parent = nullptr
    );
    ~NodeAgent() override;

    // Installs the beacon provider. Call before the server starts.
    void attachServer(HttpServer *server);
    // Starts announcing and the worker policy. No-op when disabled.
    void start();
    void setBlenderPath(const QString &path);
    // Persists the choice to distributed.json.
    void setAcceptRemoteWork(bool accept);
    Snapshot snapshot() const;
    // Hands back the running lease and waits for the worker. Called on exit.
    void shutdown();

signals:
    void changed();
    void outputText(QString text);

private:
    void tick();
    WorkerState evaluate() const;
    void startWorker(const PeerNode &coordinator);
    void onWorkerFinished();
    NodeBeacon localBeacon() const;

    RenderQueue *renderQueue;
    const LoadMonitor *loadMonitor;
    HttpServer *httpServer = nullptr;
    DistributedSettings settings;
    QString settingsPath;
    QString nodeId;
    QString nodeName;
    QString workerId;
    QString blenderPath;
    NodeDiscovery *discovery = nullptr;
    bool listening = false;
    QTimer *timer = nullptr;
    BuiltInWorkerThread *worker = nullptr;
    QString workerCoordinatorId;
    QString workerCoordinatorName;
    // Coordinator id -> time before which it is not tried again.
    QHash<QString, qint64> backoffUntil;
    bool stopping = false;
};

#endif // NODE_AGENT_H
