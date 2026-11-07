// frame_ledger.cpp

#include <algorithm>

#include <QSet>

#include "frame_ledger.h"

void FrameLedger::reset(
    int frameStart,
    int frameEnd,
    int frameStep,
    const std::vector<IndexRange> &completed
)
{
    clear();
    if (frameStep <= 0 || frameEnd < frameStart)
    {
        return;
    }
    start = frameStart;
    step = frameStep;
    const int total = (frameEnd - frameStart) / frameStep + 1;
    frameSlots.assign(static_cast<size_t>(total), Slot::Pending);
    for (const IndexRange &range : completed)
    {
        const int first = std::max(range.first, 0);
        const int last = std::min(range.second, total - 1);
        if (first <= last)
        {
            markDone(first, last - first + 1);
        }
    }
}

void FrameLedger::clear()
{
    start = 0;
    step = 0;
    frameSlots.clear();
    done = 0;
    leases.clear();
}

std::optional<FrameLedger::Lease> FrameLedger::claim(
    const QString &workerId,
    int maxFrames,
    qint64 nowMs
)
{
    if (maxFrames <= 0)
    {
        return std::nullopt;
    }
    const auto firstPending = std::find(frameSlots.begin(), frameSlots.end(), Slot::Pending);
    if (firstPending == frameSlots.end())
    {
        return std::nullopt;
    }
    const int firstIndex = static_cast<int>(std::distance(frameSlots.begin(), firstPending));
    int count = 0;
    while (count < maxFrames
        && firstIndex + count < static_cast<int>(frameSlots.size())
        && frameSlots[static_cast<size_t>(firstIndex + count)] == Slot::Pending)
    {
        frameSlots[static_cast<size_t>(firstIndex + count)] = Slot::Leased;
        ++count;
    }
    Lease lease;
    lease.id = QUuid::createUuid();
    lease.workerId = workerId;
    lease.firstIndex = firstIndex;
    lease.count = count;
    lease.claimedMs = nowMs;
    lease.lastSeenMs = nowMs;
    leases.insert(lease.id, lease);
    return lease;
}

bool FrameLedger::report(const QUuid &leaseId, int finishedCount, qint64 nowMs)
{
    const auto iterator = leases.find(leaseId);
    if (iterator == leases.end())
    {
        return false;
    }
    Lease &lease = iterator.value();
    const int clamped = std::clamp(finishedCount, 0, lease.count);
    // Progress is monotonic; a renderer never un-finishes a frame.
    if (clamped > lease.finished)
    {
        markDone(lease.firstIndex + lease.finished, clamped - lease.finished);
        lease.finished = clamped;
    }
    lease.lastSeenMs = nowMs;
    return true;
}

bool FrameLedger::complete(const QUuid &leaseId)
{
    const auto iterator = leases.find(leaseId);
    if (iterator == leases.end())
    {
        return false;
    }
    const Lease &lease = iterator.value();
    markDone(lease.firstIndex + lease.finished, lease.count - lease.finished);
    leases.erase(iterator);
    return true;
}

bool FrameLedger::release(const QUuid &leaseId)
{
    const auto iterator = leases.find(leaseId);
    if (iterator == leases.end())
    {
        return false;
    }
    releaseUnfinished(iterator.value());
    leases.erase(iterator);
    return true;
}

void FrameLedger::releaseAll()
{
    for (const Lease &lease : std::as_const(leases))
    {
        releaseUnfinished(lease);
    }
    leases.clear();
}

std::vector<FrameLedger::Lease> FrameLedger::expire(qint64 nowMs, qint64 timeoutMs)
{
    std::vector<Lease> expired;
    for (auto iterator = leases.begin(); iterator != leases.end();)
    {
        if (nowMs - iterator.value().lastSeenMs > timeoutMs)
        {
            expired.push_back(iterator.value());
            releaseUnfinished(iterator.value());
            iterator = leases.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
    return expired;
}

std::optional<FrameLedger::Lease> FrameLedger::lease(const QUuid &leaseId) const
{
    const auto iterator = leases.constFind(leaseId);
    if (iterator == leases.cend())
    {
        return std::nullopt;
    }
    return iterator.value();
}

int FrameLedger::frameAt(int index) const
{
    return start + index * step;
}

int FrameLedger::totalCount() const
{
    return static_cast<int>(frameSlots.size());
}

int FrameLedger::doneCount() const
{
    return done;
}

int FrameLedger::pendingCount() const
{
    return static_cast<int>(std::count(frameSlots.begin(), frameSlots.end(), Slot::Pending));
}

bool FrameLedger::isComplete() const
{
    return !frameSlots.empty() && done == static_cast<int>(frameSlots.size());
}

bool FrameLedger::hasLeases() const
{
    return !leases.isEmpty();
}

int FrameLedger::leaseHolderCount() const
{
    QSet<QString> holders;
    for (const Lease &lease : leases)
    {
        holders.insert(lease.workerId);
    }
    return static_cast<int>(holders.size());
}

bool FrameLedger::holdsLease(const QString &workerId) const
{
    return std::any_of(leases.cbegin(), leases.cend(), [&workerId](const Lease &lease) {
        return lease.workerId == workerId;
    });
}

std::vector<FrameLedger::IndexRange> FrameLedger::completedRanges() const
{
    std::vector<IndexRange> ranges;
    int rangeStart = -1;
    for (int index = 0; index < static_cast<int>(frameSlots.size()); ++index)
    {
        const bool isDone = frameSlots[static_cast<size_t>(index)] == Slot::Done;
        if (isDone && rangeStart < 0)
        {
            rangeStart = index;
        }
        else if (!isDone && rangeStart >= 0)
        {
            ranges.emplace_back(rangeStart, index - 1);
            rangeStart = -1;
        }
    }
    if (rangeStart >= 0)
    {
        ranges.emplace_back(rangeStart, static_cast<int>(frameSlots.size()) - 1);
    }
    return ranges;
}

void FrameLedger::markDone(int firstIndex, int count)
{
    for (int index = firstIndex; index < firstIndex + count; ++index)
    {
        Slot &slot = frameSlots[static_cast<size_t>(index)];
        if (slot != Slot::Done)
        {
            slot = Slot::Done;
            ++done;
        }
    }
}

void FrameLedger::releaseUnfinished(const Lease &lease)
{
    for (int index = lease.firstIndex + lease.finished; index < lease.firstIndex + lease.count; ++index)
    {
        Slot &slot = frameSlots[static_cast<size_t>(index)];
        if (slot == Slot::Leased)
        {
            slot = Slot::Pending;
        }
    }
}
