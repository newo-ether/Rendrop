// frame_ledger.h

#ifndef FRAME_LEDGER_H
#define FRAME_LEDGER_H

#include <optional>
#include <utility>
#include <vector>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QUuid>

#include "frame_owner.h"

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
        // Display name credited with the frames; empty for the coordinator.
        QString owner;
        int firstIndex = 0;
        int count = 0;
        // Frames finished counted from the start of the lease.
        int finished = 0;
        qint64 claimedMs = 0;
        qint64 lastSeenMs = 0;
    };

    // owners credits already completed frames; entries outside completed are ignored.
    void reset(
        int frameStart,
        int frameEnd,
        int frameStep,
        const std::vector<IndexRange> &completed,
        const std::vector<FrameOwnerRanges> &owners = {}
    );
    void clear();

    std::optional<Lease> claim(const QString &workerId, int maxFrames, qint64 nowMs, const QString &owner = QString());
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
    // Completed frames per non-empty owner, in order of first appearance.
    std::vector<FrameOwnerRanges> ownerRanges() const;

private:
    enum class Slot : unsigned char
    {
        Pending,
        Leased,
        Done
    };

    void markDone(int firstIndex, int count, int ownerIndex);
    int ownerIndexFor(const QString &owner);
    void releaseUnfinished(const Lease &lease);

    int start = 0;
    int step = 0;
    // Not named "slots": that is a Qt keyword macro.
    std::vector<Slot> frameSlots;
    // Per slot index into ownerNames; -1 means the coordinator.
    std::vector<int> frameOwners;
    QStringList ownerNames;
    int done = 0;
    QHash<QUuid, Lease> leases;
};

#endif // FRAME_LEDGER_H
