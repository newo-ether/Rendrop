// frame_ledger.h

#ifndef FRAME_LEDGER_H
#define FRAME_LEDGER_H

#include <optional>
#include <utility>
#include <vector>

#include <QHash>
#include <QString>
#include <QUuid>

// Tracks per-frame completion for one render run and hands out contiguous
// leases of pending frames to renderers. Frames are addressed by index:
// frame = frameStart + index * frameStep.
class FrameLedger
{
public:
    // Inclusive [first, last] index range.
    using IndexRange = std::pair<int, int>;

    struct Lease
    {
        QUuid id;
        QString workerId;
        int firstIndex = 0;
        int count = 0;
        // Frames finished counted from the start of the lease.
        int finished = 0;
        qint64 claimedMs = 0;
        qint64 lastSeenMs = 0;
    };

    void reset(int frameStart, int frameEnd, int frameStep, const std::vector<IndexRange> &completed);
    void clear();

    std::optional<Lease> claim(const QString &workerId, int maxFrames, qint64 nowMs);
    bool report(const QUuid &leaseId, int finishedCount, qint64 nowMs);
    bool complete(const QUuid &leaseId);
    bool release(const QUuid &leaseId);
    void releaseAll();
    std::vector<Lease> expire(qint64 nowMs, qint64 timeoutMs);

    std::optional<Lease> lease(const QUuid &leaseId) const;
    int frameAt(int index) const;
    int totalCount() const;
    int doneCount() const;
    int pendingCount() const;
    bool isComplete() const;
    bool hasLeases() const;
    // Distinct holders of open leases.
    int leaseHolderCount() const;
    bool holdsLease(const QString &workerId) const;
    std::vector<IndexRange> completedRanges() const;

private:
    enum class Slot : unsigned char
    {
        Pending,
        Leased,
        Done
    };

    void markDone(int firstIndex, int count);
    void releaseUnfinished(const Lease &lease);

    int start = 0;
    int step = 0;
    // Not named "slots": that is a Qt keyword macro.
    std::vector<Slot> frameSlots;
    int done = 0;
    QHash<QUuid, Lease> leases;
};

#endif // FRAME_LEDGER_H
