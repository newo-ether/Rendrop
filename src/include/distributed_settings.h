// distributed_settings.h

#ifndef DISTRIBUTED_SETTINGS_H
#define DISTRIBUTED_SETTINGS_H

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QString>
#include <QStringList>

#include "chunk_planner.h"
#include "load_settings.h"

struct DistributedSettings
{
    // When disabled, the local renderer takes the whole remaining range in one
    // Blender process, exactly like a single-machine queue, and this machine
    // neither announces itself nor renders for others.
    bool enabled = false;
    // Shared secret of the render group. Workers send it as
    // "Authorization: Bearer <token>"; beacons are signed with it.
    QString token;
    // Frames per lease before a renderer has been measured, or always when
    // adaptiveChunks is off.
    int chunkSize = 10;
    // Size leases from each renderer's measured speed so one lease takes
    // about targetLeaseSeconds.
    bool adaptiveChunks = true;
    int targetLeaseSeconds = 300;
    int maxChunkSize = 1000;
    // A lease without a heartbeat for this long goes back to the pending pool.
    int leaseTimeoutSeconds = 120;
    // Off: this machine never renders for other nodes and reports itself as
    // not idle.
    bool acceptRemoteWork = true;
    // UDP port for LAN beacons. Broadcast does not cross Tailscale or routed
    // networks; list such nodes in peers ("host" or "host:port").
    int discoveryPort = 28529;
    QStringList peers;
    // Where this machine keeps downloaded projects while rendering for
    // others. Empty: %TEMP%\rendrop-worker.
    QString workerCacheDirectory;
    // "none": workers get the .blend alone, assets must be packed into it.
    // "bat": external assets are collected with Blender Asset Tracer and sent
    // along. batPath is the directory that contains the blender_asset_tracer
    // package (added to Blender's Python path).
    QString dependencyMode = QStringLiteral("none");
    QString batPath;
    // Busy detection for work that is not Rendrop's own.
    LoadSettings load;

    ChunkPlanner::Settings chunkPlan() const
    {
        ChunkPlanner::Settings plan;
        plan.adaptive = adaptiveChunks;
        plan.initialChunk = chunkSize;
        plan.maxChunk = maxChunkSize;
        plan.targetLeaseSeconds = targetLeaseSeconds;
        return plan;
    }
};

namespace DistributedConfig {

constexpr int minimumTokenLength = 16;

inline QString generateToken()
{
    QString token;
    for (int index = 0; index < 4; ++index)
    {
        token += QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, QChar('0'));
    }
    return token;
}

inline QJsonObject toJson(const DistributedSettings &settings)
{
    return {
        {QStringLiteral("enabled"), settings.enabled},
        {QStringLiteral("token"), settings.token},
        {QStringLiteral("chunkSize"), settings.chunkSize},
        {QStringLiteral("adaptiveChunks"), settings.adaptiveChunks},
        {QStringLiteral("targetLeaseSeconds"), settings.targetLeaseSeconds},
        {QStringLiteral("maxChunkSize"), settings.maxChunkSize},
        {QStringLiteral("leaseTimeoutSeconds"), settings.leaseTimeoutSeconds},
        {QStringLiteral("acceptRemoteWork"), settings.acceptRemoteWork},
        {QStringLiteral("discoveryPort"), settings.discoveryPort},
        {QStringLiteral("peers"), QJsonArray::fromStringList(settings.peers)},
        {QStringLiteral("workerCacheDirectory"), settings.workerCacheDirectory},
        {QStringLiteral("dependencyMode"), settings.dependencyMode},
        {QStringLiteral("batPath"), settings.batPath},
        {QStringLiteral("load"), QJsonObject {
            {QStringLiteral("sampleIntervalMs"), settings.load.sampleIntervalMs},
            {QStringLiteral("busyThresholdPercent"), settings.load.busyThresholdPercent},
            {QStringLiteral("busyWindowSeconds"), settings.load.busyWindowSeconds},
            {QStringLiteral("idleThresholdPercent"), settings.load.idleThresholdPercent},
            {QStringLiteral("idleWindowSeconds"), settings.load.idleWindowSeconds}
        }}
    };
}

inline bool save(const QString &filePath, const DistributedSettings &settings)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    file.write(QJsonDocument(toJson(settings)).toJson(QJsonDocument::Indented));
    return file.commit();
}

// Loads distributed.json, writing a disabled default when it does not exist.
// Enabling it without a usable token generates one and writes it back so the
// user can copy it to the other machines. Keys missing from older files are
// written back with their defaults so every option is discoverable.
inline DistributedSettings load(const QString &filePath)
{
    DistributedSettings settings;
    QFile file(filePath);
    if (!file.exists())
    {
        save(filePath, settings);
        return settings;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        return settings;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject())
    {
        return settings;
    }
    const QJsonObject object = document.object();
    const DistributedSettings defaults;
    settings.enabled = object.value(QStringLiteral("enabled")).toBool(false);
    settings.token = object.value(QStringLiteral("token")).toString().trimmed();
    settings.chunkSize = std::clamp(object.value(QStringLiteral("chunkSize")).toInt(defaults.chunkSize), 1, 100000);
    settings.adaptiveChunks = object.value(QStringLiteral("adaptiveChunks")).toBool(defaults.adaptiveChunks);
    settings.targetLeaseSeconds = std::clamp(
        object.value(QStringLiteral("targetLeaseSeconds")).toInt(defaults.targetLeaseSeconds),
        10,
        86400
    );
    settings.maxChunkSize = std::clamp(
        object.value(QStringLiteral("maxChunkSize")).toInt(defaults.maxChunkSize),
        settings.chunkSize,
        100000
    );
    settings.leaseTimeoutSeconds = std::clamp(
        object.value(QStringLiteral("leaseTimeoutSeconds")).toInt(defaults.leaseTimeoutSeconds),
        5,
        86400
    );
    settings.acceptRemoteWork = object.value(QStringLiteral("acceptRemoteWork")).toBool(defaults.acceptRemoteWork);
    settings.discoveryPort = std::clamp(
        object.value(QStringLiteral("discoveryPort")).toInt(defaults.discoveryPort),
        0,
        65535
    );
    for (const QJsonValue &peer : object.value(QStringLiteral("peers")).toArray())
    {
        const QString text = peer.toString().trimmed();
        if (!text.isEmpty())
        {
            settings.peers.append(text);
        }
    }
    settings.workerCacheDirectory = object.value(QStringLiteral("workerCacheDirectory")).toString().trimmed();
    const QString mode = object.value(QStringLiteral("dependencyMode")).toString(defaults.dependencyMode).trimmed().toLower();
    settings.dependencyMode = mode == QStringLiteral("bat") ? mode : QStringLiteral("none");
    settings.batPath = object.value(QStringLiteral("batPath")).toString().trimmed();
    // Range checks happen in LoadWindow; only types are handled here.
    const QJsonObject load = object.value(QStringLiteral("load")).toObject();
    settings.load.sampleIntervalMs = load.value(QStringLiteral("sampleIntervalMs")).toInt(defaults.load.sampleIntervalMs);
    settings.load.busyThresholdPercent =
        load.value(QStringLiteral("busyThresholdPercent")).toDouble(defaults.load.busyThresholdPercent);
    settings.load.busyWindowSeconds = load.value(QStringLiteral("busyWindowSeconds")).toInt(defaults.load.busyWindowSeconds);
    settings.load.idleThresholdPercent =
        load.value(QStringLiteral("idleThresholdPercent")).toDouble(defaults.load.idleThresholdPercent);
    settings.load.idleWindowSeconds = load.value(QStringLiteral("idleWindowSeconds")).toInt(defaults.load.idleWindowSeconds);

    bool missingKey = false;
    const QJsonObject defaultObject = toJson(defaults);
    for (auto iterator = defaultObject.constBegin(); iterator != defaultObject.constEnd(); ++iterator)
    {
        missingKey = missingKey || !object.contains(iterator.key());
    }
    if (settings.enabled && settings.token.size() < minimumTokenLength)
    {
        settings.token = generateToken();
        if (!save(filePath, settings))
        {
            // Never run worker endpoints with a token nobody can read back.
            settings.enabled = false;
        }
    }
    else if (missingKey)
    {
        save(filePath, settings);
    }
    return settings;
}

}

#endif // DISTRIBUTED_SETTINGS_H
