// chunk_planner.cpp

#include <algorithm>
#include <cmath>

#include "chunk_planner.h"

namespace {
// Weight of the newest lease in the smoothed estimate.
constexpr double smoothing = 0.5;
}

void ChunkPlanner::setSettings(const Settings &settings)
{
    config = settings;
    config.initialChunk = std::max(config.initialChunk, 1);
    config.maxChunk = std::max(config.maxChunk, config.initialChunk);
    config.targetLeaseSeconds = std::max(config.targetLeaseSeconds, 1);
}

const ChunkPlanner::Settings &ChunkPlanner::settings() const
{
    return config;
}

void ChunkPlanner::reset()
{
    estimates.clear();
}

void ChunkPlanner::recordProgress(const QString &workerId, qint64 elapsedMs, int finished)
{
    if (finished <= 0 || elapsedMs < 0)
    {
        return;
    }
    estimates[workerId].current = static_cast<double>(elapsedMs) / finished;
}

void ChunkPlanner::recordComplete(const QString &workerId, qint64 elapsedMs, int count)
{
    if (count <= 0 || elapsedMs < 0)
    {
        return;
    }
    Estimate &estimate = estimates[workerId];
    const double value = static_cast<double>(elapsedMs) / count;
    estimate.committed = estimate.committed < 0.0
        ? value
        : estimate.committed * (1.0 - smoothing) + value * smoothing;
    estimate.current = -1.0;
}

double ChunkPlanner::msPerFrame(const QString &workerId) const
{
    const auto iterator = estimates.constFind(workerId);
    if (iterator == estimates.cend())
    {
        return -1.0;
    }
    // A running lease that is already slower than the smoothed value is the
    // better predictor: the scene got heavier or the machine got busy.
    return std::max(iterator->committed, iterator->current);
}

int ChunkPlanner::chunkFor(const QString &workerId, int pending, int renderers) const
{
    if (!config.adaptive)
    {
        return config.initialChunk;
    }
    int chunk = config.initialChunk;
    const double perFrame = msPerFrame(workerId);
    if (perFrame > 0.0)
    {
        const double frames = config.targetLeaseSeconds * 1000.0 / perFrame;
        chunk = static_cast<int>(std::clamp(std::floor(frames), 1.0, static_cast<double>(config.maxChunk)));
    }
    else if (perFrame == 0.0)
    {
        chunk = config.maxChunk;
    }
    if (pending > 0)
    {
        // With others around, take half of an even share. Leases then
        // shrink as the job runs out, so a slower machine never ends up
        // holding a big last block while the faster ones sit idle.
        const int sharers = std::max(renderers, 1);
        const int divisor = sharers > 1 ? sharers * 2 : 1;
        const int fairShare = (pending + divisor - 1) / divisor;
        chunk = std::min(chunk, fairShare);
    }
    return std::max(chunk, 1);
}
