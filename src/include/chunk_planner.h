// chunk_planner.h

#ifndef CHUNK_PLANNER_H
#define CHUNK_PLANNER_H

#include <QHash>
#include <QString>

// Decides how many frames the next lease of a renderer gets. Workers pull
// work; each claim is sized from that renderer's measured time per frame so a
// lease lasts about targetLeaseSeconds, and capped near the end of the job so
// the last frames are spread over everyone instead of landing on one machine.
// Pure logic, no clock of its own.
class ChunkPlanner
{
public:
    struct Settings
    {
        // Off: every lease is exactly initialChunk frames, as before.
        bool adaptive = true;
        // Lease size before a renderer has been measured.
        int initialChunk = 10;
        int maxChunk = 1000;
        int targetLeaseSeconds = 300;
    };

    void setSettings(const Settings &settings);
    const Settings &settings() const;
    // Forgets all measurements. Frame times differ per scene, so this runs
    // whenever a job starts.
    void reset();

    // finished frames took elapsedMs since the lease was claimed. The time
    // includes Blender start-up and transfers on purpose: they are part of
    // what a lease costs.
    void recordProgress(const QString &workerId, qint64 elapsedMs, int finished);
    // The lease of count frames finished after elapsedMs.
    void recordComplete(const QString &workerId, qint64 elapsedMs, int count);

    // pending: frames nobody holds. renderers: how many renderers are
    // currently sharing the job, including the one asking.
    int chunkFor(const QString &workerId, int pending, int renderers) const;
    // Milliseconds per frame, or a negative value when unknown.
    double msPerFrame(const QString &workerId) const;

private:
    struct Estimate
    {
        // Smoothed over completed leases.
        double committed = -1.0;
        // Running value of the lease in progress.
        double current = -1.0;
    };

    Settings config;
    QHash<QString, Estimate> estimates;
};

#endif // CHUNK_PLANNER_H
