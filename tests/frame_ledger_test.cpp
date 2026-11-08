#include <QtTest>

#include "frame_ledger.h"

class FrameLedgerTest : public QObject
{
    Q_OBJECT

private slots:
    void claimsContiguousPendingFrames();
    void reportIsMonotonicAndClamped();
    void releaseReturnsOnlyUnfinishedFrames();
    void expireReleasesSilentLeases();
    void resetRestoresCompletedRangesWithGaps();
    void completeAndUnknownLeases();
    void creditsFramesToLeaseOwners();
    void resetRestoresOwnersOnlyOnCompletedFrames();
    void listsUnfinishedLeasedFrames();
};

void FrameLedgerTest::claimsContiguousPendingFrames()
{
    FrameLedger ledger;
    ledger.reset(10, 28, 2, {});
    QCOMPARE(ledger.totalCount(), 10);
    const auto first = ledger.claim(QStringLiteral("a"), 4, 0);
    QVERIFY(first.has_value());
    QCOMPARE(first->firstIndex, 0);
    QCOMPARE(first->count, 4);
    QCOMPARE(ledger.frameAt(first->firstIndex + first->count - 1), 16);
    const auto second = ledger.claim(QStringLiteral("b"), 100, 0);
    QVERIFY(second.has_value());
    QCOMPARE(second->firstIndex, 4);
    QCOMPARE(second->count, 6);
    QVERIFY(!ledger.claim(QStringLiteral("c"), 1, 0).has_value());
    QVERIFY(!ledger.claim(QStringLiteral("c"), 0, 0).has_value());
    QCOMPARE(ledger.pendingCount(), 0);
}

void FrameLedgerTest::reportIsMonotonicAndClamped()
{
    FrameLedger ledger;
    ledger.reset(1, 5, 1, {});
    const auto lease = ledger.claim(QStringLiteral("a"), 3, 0);
    QVERIFY(lease.has_value());
    QVERIFY(ledger.report(lease->id, 2, 10));
    QCOMPARE(ledger.doneCount(), 2);
    QVERIFY(ledger.report(lease->id, 1, 20));
    QCOMPARE(ledger.doneCount(), 2);
    QVERIFY(ledger.report(lease->id, 99, 30));
    QCOMPARE(ledger.doneCount(), 3);
    QCOMPARE(ledger.lease(lease->id)->lastSeenMs, qint64(30));
}

void FrameLedgerTest::releaseReturnsOnlyUnfinishedFrames()
{
    FrameLedger ledger;
    ledger.reset(1, 6, 1, {});
    const auto lease = ledger.claim(QStringLiteral("a"), 6, 0);
    QVERIFY(ledger.report(lease->id, 2, 0));
    QVERIFY(ledger.release(lease->id));
    QVERIFY(!ledger.hasLeases());
    QCOMPARE(ledger.doneCount(), 2);
    QCOMPARE(ledger.pendingCount(), 4);
    const auto next = ledger.claim(QStringLiteral("b"), 10, 0);
    QCOMPARE(next->firstIndex, 2);
    QCOMPARE(next->count, 4);
}

void FrameLedgerTest::expireReleasesSilentLeases()
{
    FrameLedger ledger;
    ledger.reset(1, 4, 1, {});
    const auto quiet = ledger.claim(QStringLiteral("quiet"), 2, 0);
    const auto chatty = ledger.claim(QStringLiteral("chatty"), 2, 0);
    QVERIFY(ledger.report(chatty->id, 0, 900));
    const auto expired = ledger.expire(1000, 500);
    QCOMPARE(expired.size(), size_t(1));
    QCOMPARE(expired[0].id, quiet->id);
    QVERIFY(!ledger.lease(quiet->id).has_value());
    QVERIFY(ledger.lease(chatty->id).has_value());
    QCOMPARE(ledger.pendingCount(), 2);
}

void FrameLedgerTest::resetRestoresCompletedRangesWithGaps()
{
    FrameLedger ledger;
    ledger.reset(1, 10, 1, {{0, 1}, {4, 4}, {8, 20}});
    QCOMPARE(ledger.doneCount(), 5);
    const std::vector<FrameLedger::IndexRange> expected = {{0, 1}, {4, 4}, {8, 9}};
    QCOMPARE(ledger.completedRanges(), expected);
    const auto lease = ledger.claim(QStringLiteral("a"), 100, 0);
    QCOMPARE(lease->firstIndex, 2);
    QCOMPARE(lease->count, 2);
}

void FrameLedgerTest::completeAndUnknownLeases()
{
    FrameLedger ledger;
    ledger.reset(1, 3, 1, {});
    const auto lease = ledger.claim(QStringLiteral("a"), 3, 0);
    QVERIFY(ledger.complete(lease->id));
    QVERIFY(ledger.isComplete());
    QVERIFY(!ledger.complete(lease->id));
    QVERIFY(!ledger.report(lease->id, 1, 0));
    QVERIFY(!ledger.release(QUuid::createUuid()));
    ledger.reset(1, 0, 1, {});
    QCOMPARE(ledger.totalCount(), 0);
    QVERIFY(!ledger.isComplete());
}

void FrameLedgerTest::creditsFramesToLeaseOwners()
{
    FrameLedger ledger;
    ledger.reset(1, 10, 1, {});
    const auto local = ledger.claim(QStringLiteral("local"), 3, 0);
    const auto quantum = ledger.claim(QStringLiteral("q-id"), 4, 0, QStringLiteral("Quantum"));
    const auto rest = ledger.claim(QStringLiteral("local"), 3, 0);
    QVERIFY(ledger.complete(local->id));
    // Partial progress is credited as it is reported, then the rest on complete.
    QVERIFY(ledger.report(quantum->id, 2, 0));
    std::vector<FrameOwnerRanges> expected = {{QStringLiteral("Quantum"), {{3, 4}}}};
    QCOMPARE(ledger.ownerRanges(), expected);
    QVERIFY(ledger.complete(quantum->id));
    QVERIFY(ledger.complete(rest->id));
    expected = {{QStringLiteral("Quantum"), {{3, 6}}}};
    QCOMPARE(ledger.ownerRanges(), expected);
    QCOMPARE(ledger.doneCount(), 10);
}

void FrameLedgerTest::resetRestoresOwnersOnlyOnCompletedFrames()
{
    FrameLedger ledger;
    const std::vector<FrameOwnerRanges> owners = {
        {QStringLiteral("QW"), {{0, 1}, {5, 7}}},
        {QStringLiteral(""), {{2, 2}}},
        {QStringLiteral("Quantum"), {{8, 9}}}
    };
    ledger.reset(1, 10, 1, {{0, 2}, {5, 6}}, owners);
    // Frame 7 is not done, so its stale owner entry is dropped, and the
    // anonymous entry never creates an owner.
    const std::vector<FrameOwnerRanges> expected = {{QStringLiteral("QW"), {{0, 1}, {5, 6}}}};
    QCOMPARE(ledger.ownerRanges(), expected);
    const auto lease = ledger.claim(QStringLiteral("b"), 1, 0, QStringLiteral("Quantum"));
    QCOMPARE(lease->firstIndex, 3);
    QVERIFY(ledger.complete(lease->id));
    const std::vector<FrameOwnerRanges> after = {
        {QStringLiteral("QW"), {{0, 1}, {5, 6}}},
        {QStringLiteral("Quantum"), {{3, 3}}}
    };
    QCOMPARE(ledger.ownerRanges(), after);
}

void FrameLedgerTest::listsUnfinishedLeasedFrames()
{
    FrameLedger ledger;
    ledger.reset(1, 12, 1, {});
    const auto local = ledger.claim(QStringLiteral("local"), 3, 0);
    const auto qw = ledger.claim(QStringLiteral("qw-id"), 4, 0, QStringLiteral("QW"));
    const auto later = ledger.claim(QStringLiteral("qw-id"), 2, 0, QStringLiteral("QW"));
    QVERIFY(ledger.report(qw->id, 1, 0));
    std::vector<FrameOwnerRanges> expected = {
        {QString(), {{0, 2}}},
        {QStringLiteral("QW"), {{4, 6}, {7, 8}}}
    };
    QCOMPARE(ledger.leasedRanges(), expected);
    QVERIFY(ledger.complete(local->id));
    QVERIFY(ledger.release(later->id));
    expected = {{QStringLiteral("QW"), {{4, 6}}}};
    QCOMPARE(ledger.leasedRanges(), expected);
    QVERIFY(ledger.complete(qw->id));
    QVERIFY(ledger.leasedRanges().empty());
}

QTEST_GUILESS_MAIN(FrameLedgerTest)

#include "frame_ledger_test.moc"
