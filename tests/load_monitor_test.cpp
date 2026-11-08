// load_monitor_test.cpp

#include <QElapsedTimer>
#include <QtTest>

#include "farm/load_monitor.h"

namespace {

LoadSettings testSettings()
{
    LoadSettings settings;
    settings.sampleIntervalMs = 1000;
    settings.busyThresholdPercent = 25.0;
    settings.busyWindowSeconds = 10;
    settings.idleThresholdPercent = 10.0;
    settings.idleWindowSeconds = 60;
    return settings;
}

// Feeds `seconds` one-second samples starting after `startMs`; returns the
// timestamp of the last one.
qint64 feed(LoadWindow &window, qint64 startMs, int seconds, double cpu, double gpu)
{
    qint64 time = startMs;
    for (int index = 0; index < seconds; ++index)
    {
        time += 1000;
        window.addSample({time, cpu, gpu});
    }
    return time;
}

}

class LoadMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void startsIdleUntilLoadShowsUp();
    void becomesBusyFromEitherCpuOrGpu();
    void shortSpikesDoNotFlipState();
    void hysteresisHoldsBusyBetweenThresholds();
    void longGapRestartsWindow();
    void settingsAreSanitized();
    void liveMonitorProducesSamples();
    void liveProbe();
};

void LoadMonitorTest::startsIdleUntilLoadShowsUp()
{
    LoadWindow quiet(testSettings());
    QVERIFY(!quiet.busy());
    feed(quiet, 0, 60, 0.0, 0.0);
    QVERIFY(!quiet.busy());

    // A machine already loaded at startup turns busy once the busy window fills.
    LoadWindow loaded(testSettings());
    QVERIFY(!loaded.busy());
    feed(loaded, 0, 10, 80.0, 0.0);
    QVERIFY(loaded.busy());
}

void LoadMonitorTest::becomesBusyFromEitherCpuOrGpu()
{
    for (const bool useGpu : {false, true})
    {
        LoadWindow window(testSettings());
        qint64 time = feed(window, 0, 60, 0.0, 0.0);
        QVERIFY(!window.busy());
        // Four seconds at 60 % average to 24 % over 10 s: not yet.
        time = feed(window, time, 4, useGpu ? 0.0 : 60.0, useGpu ? 60.0 : 0.0);
        QVERIFY(!window.busy());
        // The fifth reaches 30 %.
        time = feed(window, time, 1, useGpu ? 0.0 : 60.0, useGpu ? 60.0 : 0.0);
        QVERIFY(window.busy());
        QVERIFY(useGpu ? window.busyWindowMeans().gpuPercent > 20.0
                       : window.busyWindowMeans().cpuPercent > 20.0);
    }
}

void LoadMonitorTest::shortSpikesDoNotFlipState()
{
    LoadWindow window(testSettings());
    qint64 time = feed(window, 0, 60, 0.0, 0.0);
    QVERIFY(!window.busy());
    // A 2 s burst at 100 % averages to 20 % over 10 s: below 25 %.
    time = feed(window, time, 2, 100.0, 0.0);
    QVERIFY(!window.busy());
    time = feed(window, time, 20, 0.0, 0.0);
    QVERIFY(!window.busy());
}

void LoadMonitorTest::hysteresisHoldsBusyBetweenThresholds()
{
    LoadWindow window(testSettings());
    qint64 time = feed(window, 0, 60, 0.0, 0.0);
    time = feed(window, time, 10, 80.0, 0.0);
    QVERIFY(window.busy());
    // 15 % sits between the thresholds: stays busy indefinitely.
    time = feed(window, time, 120, 15.0, 0.0);
    QVERIFY(window.busy());
    // 19 quiet seconds leave the 60 s mean at 10.25 %: still busy.
    time = feed(window, time, 19, 0.0, 0.0);
    QVERIFY(window.busy());
    // 22 quiet seconds bring it to 9.5 %.
    time = feed(window, time, 3, 0.0, 0.0);
    QVERIFY(!window.busy());
}

void LoadMonitorTest::longGapRestartsWindow()
{
    LoadWindow window(testSettings());
    qint64 time = feed(window, 0, 10, 80.0, 0.0);
    QVERIFY(window.busy());
    time = feed(window, time, 50, 0.0, 0.0);
    QVERIFY(window.busy());
    // Sleep for an hour; the 50 quiet seconds before it must not count.
    time += 3600 * 1000;
    window.addSample({time, 0.0, 0.0});
    QVERIFY(!window.idleWindowMeans().covered);
    time = feed(window, time, 30, 0.0, 0.0);
    QVERIFY(window.busy());
    time = feed(window, time, 30, 0.0, 0.0);
    QVERIFY(!window.busy());
}

void LoadMonitorTest::settingsAreSanitized()
{
    LoadSettings settings;
    settings.sampleIntervalMs = 1;
    settings.busyThresholdPercent = 150.0;
    settings.idleThresholdPercent = 120.0;
    settings.busyWindowSeconds = 0;
    settings.idleWindowSeconds = -5;
    const LoadWindow window(settings);
    QCOMPARE(window.settings().sampleIntervalMs, 200);
    QCOMPARE(window.settings().busyThresholdPercent, 100.0);
    QCOMPARE(window.settings().idleThresholdPercent, 100.0);
    QCOMPARE(window.settings().busyWindowSeconds, 1);
    QCOMPARE(window.settings().idleWindowSeconds, 1);

    settings.busyThresholdPercent = 20.0;
    settings.idleThresholdPercent = 30.0;
    QCOMPARE(LoadWindow(settings).settings().idleThresholdPercent, 20.0);
}

void LoadMonitorTest::liveMonitorProducesSamples()
{
#ifndef Q_OS_WIN
    QSKIP("load sampling is implemented for Windows only");
#endif
    LoadSettings settings;
    settings.sampleIntervalMs = 500;
    settings.busyWindowSeconds = 1;
    settings.idleWindowSeconds = 2;
    LoadMonitor monitor(settings);
    monitor.start();
    QTRY_VERIFY_WITH_TIMEOUT(monitor.status().busyWindow.covered, 5000);
    // Burn one core in this process; it must show up as own, not foreign.
    QElapsedTimer burn;
    burn.start();
    volatile double sink = 0.0;
    while (burn.elapsed() < 1500)
    {
        sink = sink + 1.0;
    }
    const LoadStatus status = monitor.status();
    qInfo().nospace()
        << "foreign cpu " << status.latest.cpuPercent
        << "% gpu " << status.latest.gpuPercent
        << "% | own cpu " << status.latest.ownCpuPercent
        << "% gpu " << status.latest.ownGpuPercent
        << "% | gpu counters " << status.gpuAvailable
        << " | busy " << status.busy;
    QVERIFY(status.latest.ownCpuPercent > 0.0);
    QVERIFY(status.latest.cpuPercent >= 0.0 && status.latest.cpuPercent <= 100.0);
    QVERIFY(status.latest.gpuPercent >= 0.0 && status.latest.gpuPercent <= 100.0);
    monitor.stop();
    monitor.wait();
}

// Manual calibration aid: RENDROP_LOAD_PROBE_SECONDS=N prints one line per
// second with the default thresholds while you run other workloads.
void LoadMonitorTest::liveProbe()
{
    const int seconds = qEnvironmentVariableIntValue("RENDROP_LOAD_PROBE_SECONDS");
    if (seconds <= 0)
    {
        QSKIP("set RENDROP_LOAD_PROBE_SECONDS to run");
    }
    LoadMonitor monitor;
    monitor.start();
    for (int second = 0; second < seconds; ++second)
    {
        QTest::qWait(1000);
        const LoadStatus status = monitor.status();
        qInfo().noquote() << QStringLiteral(
            "t=%1 foreign cpu %2 gpu %3 | own cpu %4 gpu %5 | 10s %6/%7 | 60s %8/%9 | %10")
            .arg(second + 1)
            .arg(status.latest.cpuPercent, 5, 'f', 1)
            .arg(status.latest.gpuPercent, 5, 'f', 1)
            .arg(status.latest.ownCpuPercent, 5, 'f', 1)
            .arg(status.latest.ownGpuPercent, 5, 'f', 1)
            .arg(status.busyWindow.cpuPercent, 5, 'f', 1)
            .arg(status.busyWindow.gpuPercent, 5, 'f', 1)
            .arg(status.idleWindow.cpuPercent, 5, 'f', 1)
            .arg(status.idleWindow.gpuPercent, 5, 'f', 1)
            .arg(status.busy ? QStringLiteral("BUSY") : QStringLiteral("idle"));
    }
    monitor.stop();
    monitor.wait();
}

QTEST_GUILESS_MAIN(LoadMonitorTest)

#include "load_monitor_test.moc"
