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

QTEST_GUILESS_MAIN(FrameLedgerTest)

#include "frame_ledger_test.moc"
