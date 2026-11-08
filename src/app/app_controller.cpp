// app_controller.cpp

#include <QThread>

#include "app/app_controller.h"
#include "config/app_paths.h"
#include "config/app_preferences.h"
#include "config/blender_selection.h"
#include "core/blender_version_manager.h"
#include "core/render_queue.h"
#include "farm/http_server.h"
#include "farm/load_monitor.h"
#include "farm/node_agent.h"

AppController::AppController(QObject *parent):
    QObject(parent)
{
    renderQueue = new RenderQueue();
    versionManager = new BlenderVersionManager(AppPaths::blenderVersionsFile());

    // The saved choice when it is still installed, else the first version.
    const QString saved = BlenderSelection::load();
    const auto &versions = versionManager->getBlenderVersions();
    for (const auto &version : versions)
    {
        if (version.version == saved)
        {
            selectedVersion = saved;
        }
    }
    if (selectedVersion.isEmpty() && !versions.empty())
    {
        selectedVersion = versions.front().version;
    }

    farmSettings = DistributedConfig::load(AppPaths::distributedFile());
    renderQueue->setBlenderPathForAll(selectedBlenderPath());
    renderQueue->setMaxConcurrentLoads(AppPreferencesFile::load().maxConcurrentLoads);
    renderQueue->setDistributedSettings(farmSettings);
    monitor = new LoadMonitor(farmSettings.load);
    // The server outlives settings changes; only the token it checks moves.
    httpServer = new HttpServer(renderQueue);
    httpServer->setLoadMonitor(monitor);
    httpServer->setWorkerToken(farmSettings.enabled ? farmSettings.token : QString());
    createNodeAgent(farmSettings);
}

AppController::~AppController()
{
    // Hands a lease rendered for another node back before anything stops.
    if (agent)
    {
        agent->shutdown();
    }
    if (httpServer)
    {
        httpServer->stop();
        httpServer->wait();
        delete httpServer;
    }
    // After the server: its beacon handler reads the agent's discovery.
    delete agent;
    agent = nullptr;
    // After the server and the agent: both read the monitor.
    delete monitor;
    if (renderQueue)
    {
        renderQueue->shutdown();
        delete renderQueue;
    }
    delete versionManager;
}

void AppController::start()
{
    renderQueue->restorePersistedJobs(selectedBlenderPath());
    // Normal priority: a starved sampler under full render load would leave
    // gaps that reset the window.
    monitor->start();
    agent->start();
    httpServer->start(QThread::LowestPriority);
}

QString AppController::selectedBlenderPath() const
{
    return selectedVersion.isEmpty() ? QString() : versionManager->getBlenderPath(selectedVersion);
}

void AppController::selectBlender(const QString &version)
{
    selectedVersion = version;
    const QString path = selectedBlenderPath();
    renderQueue->setBlenderPathForAll(path);
    if (agent)
    {
        agent->setBlenderPath(path);
    }
    if (!version.isEmpty())
    {
        BlenderSelection::save(version);
    }
}

void AppController::setMaxConcurrentLoads(int limit)
{
    renderQueue->setMaxConcurrentLoads(limit);
}

void AppController::createNodeAgent(const DistributedSettings &settings)
{
    agent = new NodeAgent(renderQueue, monitor, settings, AppPaths::distributedFile(), this);
    agent->setBlenderPath(selectedBlenderPath());
    agent->attachServer(httpServer);
    QObject::connect(agent, &NodeAgent::outputText, this, &AppController::outputText);
    QObject::connect(agent, &NodeAgent::changed, this, &AppController::nodeStateChanged);
}

void AppController::applyFarmSettings(const DistributedSettings &settings)
{
    // Thresholds, chunking and the key apply in place; nothing restarts.
    monitor->setSettings(settings.load);
    renderQueue->setDistributedSettings(settings);
    httpServer->setWorkerToken(settings.enabled ? settings.token : QString());

    // Discovery binds its port and reads the peers once, so a change there
    // swaps the agent. acceptRemoteWork is the Nodes page's switch, which the
    // agent already follows; the load thresholds were applied above.
    const auto agentPart = [](DistributedSettings value) {
        value.acceptRemoteWork = true;
        value.load = LoadSettings();
        return DistributedConfig::toJson(value);
    };
    const bool rebuild = agentPart(settings) != agentPart(farmSettings);
    farmSettings = settings;
    if (!rebuild)
    {
        return;
    }
    emit nodeAgentReplaced(nullptr);
    // Hands a lease taken for another node back before the agent goes.
    agent->shutdown();
    httpServer->setBeaconProvider({});
    delete agent;
    agent = nullptr;
    createNodeAgent(settings);
    agent->start();
    emit nodeAgentReplaced(agent);
}
