// node_agent.cpp

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include "http_server.h"
#include "load_monitor.h"
#include "node_agent.h"
#include "render_queue.h"

namespace {

constexpr int tickIntervalMs = 1000;
constexpr qint64 noWorkBackoffMs = 5000;
constexpr qint64 unreachableBackoffMs = 15000;
constexpr qint64 rejectedBackoffMs = 5 * 60 * 1000;

}

// Renders leases for one coordinator until it runs out of work, becomes
// unreachable or the agent asks it to yield.
class BuiltInWorkerThread : public QThread
{
public:
    explicit BuiltInWorkerThread(const RemoteWorker::Config &config):
        worker(config)
    {
    }

    void run() override
    {
        while (!worker.yieldRequested())
        {
            last = worker.runOnce();
            if (last != RemoteWorker::Outcome::Worked)
            {
                break;
            }
        }
    }

    RemoteWorker worker;
    RemoteWorker::Outcome last = RemoteWorker::Outcome::NoWork;
};

NodeAgent::NodeAgent(
    RenderQueue *renderQueue,
    const LoadMonitor *loadMonitor,
    const DistributedSettings &settings,
    const QString &settingsPath,
    QObject *parent
):
    QObject(parent),
    renderQueue(renderQueue),
    loadMonitor(loadMonitor),
    settings(settings),
    settingsPath(settingsPath),
    nodeId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
    nodeName(QSysInfo::machineHostName()),
    workerId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    if (settings.enabled)
    {
        discovery = new NodeDiscovery(settings.token.toUtf8(), settings.discoveryPort, settings.peers, this);
        QObject::connect(discovery, &NodeDiscovery::peersChanged, this, &NodeAgent::changed);
    }
}

NodeAgent::~NodeAgent()
{
    shutdown();
}

void NodeAgent::attachServer(HttpServer *server)
{
    httpServer = server;
    if (!httpServer)
    {
        return;
    }
    httpServer->setAcceptRemoteWork(settings.enabled && settings.acceptRemoteWork);
    if (discovery)
    {
        // The server must stop before the agent is destroyed.
        NodeDiscovery *raw = discovery;
        httpServer->setBeaconProvider([raw]() { return raw->currentBeacon(); });
    }
}

void NodeAgent::start()
{
    if (!discovery || timer)
    {
        return;
    }
    listening = discovery->start();
    if (!listening)
    {
        emit outputText(tr("Node discovery: UDP port %1 is unavailable, only configured peers are found").arg(settings.discoveryPort));
    }
    timer = new QTimer(this);
    timer->setInterval(tickIntervalMs);
    QObject::connect(timer, &QTimer::timeout, this, &NodeAgent::tick);
    timer->start();
    tick();
}

void NodeAgent::setBlenderPath(const QString &path)
{
    blenderPath = path;
}

void NodeAgent::setAcceptRemoteWork(bool accept)
{
    if (settings.acceptRemoteWork == accept)
    {
        return;
    }
    settings.acceptRemoteWork = accept;
    // Reload first so edits made to the file while Rendrop runs survive.
    DistributedSettings stored = DistributedConfig::load(settingsPath);
    stored.acceptRemoteWork = accept;
    DistributedConfig::save(settingsPath, stored);
    if (httpServer)
    {
        httpServer->setAcceptRemoteWork(settings.enabled && accept);
    }
    tick();
    emit changed();
}

NodeAgent::Snapshot NodeAgent::snapshot() const
{
    Snapshot result;
    result.enabled = settings.enabled;
    result.acceptRemoteWork = settings.acceptRemoteWork;
    result.discoveryListening = listening;
    const NodeBeacon beacon = localBeacon();
    result.busy = beacon.busy;
    result.offersWork = beacon.offersWork;
    result.name = nodeName;
    result.httpPort = beacon.httpPort;
    result.workerState = worker ? WorkerState::Working : evaluate();
    if (worker)
    {
        result.coordinatorName = workerCoordinatorName;
        result.worker = worker->worker.status();
    }
    if (discovery)
    {
        result.peers = discovery->peers();
    }
    return result;
}

void NodeAgent::shutdown()
{
    stopping = true;
    if (timer)
    {
        timer->stop();
    }
    if (worker)
    {
        worker->worker.requestYield();
        worker->wait();
        delete worker;
        worker = nullptr;
    }
}

NodeBeacon NodeAgent::localBeacon() const
{
    NodeBeacon beacon;
    beacon.nodeId = nodeId;
    beacon.name = nodeName;
    const int bound = httpServer ? httpServer->boundPort() : 0;
    beacon.httpPort = bound > 0 ? bound : NodeDiscovery::defaultHttpPort;
    const bool loadBusy = !loadMonitor || loadMonitor->status().busy;
    // Refusing remote work reads as "not idle" to the rest of the group.
    beacon.busy = loadBusy || !settings.acceptRemoteWork;
    beacon.acceptRemoteWork = settings.acceptRemoteWork;
    beacon.offersWork = renderQueue->offersRemoteWork();
    beacon.rendering = renderQueue->isRendering() || worker != nullptr;
    return beacon;
}

NodeAgent::WorkerState NodeAgent::evaluate() const
{
    if (!settings.enabled)
    {
        return WorkerState::Disabled;
    }
    if (!settings.acceptRemoteWork)
    {
        return WorkerState::Refusing;
    }
    if (blenderPath.isEmpty() || !QFileInfo(blenderPath).isFile())
    {
        return WorkerState::NoBlender;
    }
    if (renderQueue->isRendering())
    {
        return WorkerState::RenderingOwnQueue;
    }
    if (!loadMonitor || loadMonitor->status().busy)
    {
        return WorkerState::Busy;
    }
    return WorkerState::Waiting;
}

void NodeAgent::tick()
{
    if (stopping || !discovery)
    {
        return;
    }
    discovery->setLocalBeacon(localBeacon());
    const WorkerState state = evaluate();
    if (worker)
    {
        if (state != WorkerState::Waiting && !worker->worker.yieldRequested())
        {
            worker->worker.requestYield();
            emit outputText(tr("Handing back the lease from %1: this machine is needed").arg(workerCoordinatorName));
        }
        emit changed();
        return;
    }
    if (state == WorkerState::Waiting)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const PeerNode &peer : discovery->peers())
        {
            if (peer.beacon.offersWork && backoffUntil.value(peer.beacon.nodeId) <= now)
            {
                startWorker(peer);
                break;
            }
        }
    }
    emit changed();
}

void NodeAgent::startWorker(const PeerNode &coordinator)
{
    RemoteWorker::Config config;
    config.host = coordinator.host.toStdString();
    config.port = coordinator.beacon.httpPort;
    config.token = settings.token.toUtf8();
    config.blenderPath = blenderPath;
    config.cacheDirectory = settings.workerCacheDirectory.isEmpty()
        ? QDir::temp().filePath(QStringLiteral("rendrop-worker"))
        : settings.workerCacheDirectory;
    QDir().mkpath(config.cacheDirectory);
    config.workerId = workerId;
    config.name = nodeName;

    worker = new BuiltInWorkerThread(config);
    workerCoordinatorId = coordinator.beacon.nodeId;
    workerCoordinatorName = coordinator.beacon.name.isEmpty() ? coordinator.host : coordinator.beacon.name;
    QPointer<NodeAgent> self(this);
    const QString prefix = QStringLiteral("[%1] ").arg(workerCoordinatorName);
    worker->worker.setLogger([self, prefix](bool, const QString &text) {
        QMetaObject::invokeMethod(qApp, [self, prefix, text]() {
            if (self)
            {
                emit self->outputText(prefix + text);
            }
        }, Qt::QueuedConnection);
    });
    QObject::connect(worker, &QThread::finished, this, &NodeAgent::onWorkerFinished, Qt::QueuedConnection);
    emit outputText(tr("Rendering for %1 (%2:%3)")
        .arg(workerCoordinatorName, coordinator.host)
        .arg(coordinator.beacon.httpPort));
    // Blender runs as a child process; the thread itself only coordinates.
    worker->start(QThread::LowPriority);
}

void NodeAgent::onWorkerFinished()
{
    if (!worker || stopping)
    {
        return;
    }
    // The finished signal can arrive before wait() would return.
    worker->wait();
    const RemoteWorker::Outcome outcome = worker->last;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    switch (outcome)
    {
    case RemoteWorker::Outcome::NoWork:
    case RemoteWorker::Outcome::Worked:
    case RemoteWorker::Outcome::Yielded:
        backoffUntil.insert(workerCoordinatorId, now + noWorkBackoffMs);
        break;
    case RemoteWorker::Outcome::Unreachable:
        backoffUntil.insert(workerCoordinatorId, now + unreachableBackoffMs);
        emit outputText(tr("%1 is unreachable").arg(workerCoordinatorName));
        break;
    case RemoteWorker::Outcome::Rejected:
        backoffUntil.insert(workerCoordinatorId, now + rejectedBackoffMs);
        emit outputText(tr("%1 rejected this machine, check that both use the same token").arg(workerCoordinatorName));
        break;
    }
    delete worker;
    worker = nullptr;
    workerCoordinatorId.clear();
    workerCoordinatorName.clear();
    emit changed();
}
