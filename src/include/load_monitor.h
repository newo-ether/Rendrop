// load_monitor.h

#ifndef LOAD_MONITOR_H
#define LOAD_MONITOR_H

#include <deque>

#include <QMutex>
#include <QThread>

#include "load_settings.h"

// One instantaneous measurement of load not caused by Rendrop.
struct LoadSample
{
    qint64 timeMs = 0;
    double cpuPercent = 0.0;
    double gpuPercent = 0.0;
    // Rendrop's own share, reported for calibration only.
    double ownCpuPercent = 0.0;
    double ownGpuPercent = 0.0;
};

// Sliding-window statistics and the busy/idle state machine. Pure logic so it
// can be tested with synthetic samples.
class LoadWindow
{
public:
    struct Means
    {
        double cpuPercent = 0.0;
        double gpuPercent = 0.0;
        // False until samples span the whole window.
        bool covered = false;
    };

    explicit LoadWindow(const LoadSettings &settings = LoadSettings());

    void setSettings(const LoadSettings &settings);
    const LoadSettings &settings() const;

    // Adds a sample and updates the state. Returns true when busy() changed.
    bool addSample(const LoadSample &sample);
    void clear();

    // Starts busy: a machine is not offered for remote work until a full
    // idle window has shown it is quiet.
    bool busy() const;
    Means busyWindowMeans() const;
    Means idleWindowMeans() const;

private:
    Means means(int windowSeconds) const;

    LoadSettings config;
    std::deque<LoadSample> samples;
    qint64 firstSampleMs = -1;
    bool isBusy = true;
};

struct LoadStatus
{
    bool busy = true;
    // False on platforms or machines without the GPU Engine counters.
    bool gpuAvailable = false;
    LoadSample latest;
    LoadWindow::Means busyWindow;
    LoadWindow::Means idleWindow;
    LoadSettings settings;
};

class LoadSampler;

// Samples foreign load on its own thread. Rendrop's process tree (this
// process, rendrop-worker.exe and everything they spawn) is subtracted from
// the machine totals.
class LoadMonitor : public QThread
{
    Q_OBJECT

public:
    explicit LoadMonitor(const LoadSettings &settings = LoadSettings(), QObject *parent = nullptr);
    ~LoadMonitor() override;

    // Thread-safe.
    LoadStatus status() const;
    void stop();

signals:
    void busyChanged(bool busy);

protected:
    void run() override;

private:
    void sampleOnce(LoadSampler &sampler);

    mutable QMutex mutex;
    LoadWindow window;
    LoadStatus current;
};

#endif // LOAD_MONITOR_H
