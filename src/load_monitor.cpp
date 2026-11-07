// load_monitor.cpp

#include "load_monitor.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <pdhmsg.h>
#endif

namespace {

LoadSettings sanitized(LoadSettings settings)
{
    settings.sampleIntervalMs = std::clamp(settings.sampleIntervalMs, 200, 10000);
    settings.busyWindowSeconds = std::clamp(settings.busyWindowSeconds, 1, 3600);
    settings.idleWindowSeconds = std::clamp(settings.idleWindowSeconds, 1, 3600);
    settings.busyThresholdPercent = std::clamp(settings.busyThresholdPercent, 0.0, 100.0);
    settings.idleThresholdPercent = std::clamp(
        settings.idleThresholdPercent,
        0.0,
        settings.busyThresholdPercent
    );
    return settings;
}

double peak(const LoadWindow::Means &means)
{
    return std::max(means.cpuPercent, means.gpuPercent);
}

}

// ---------------------------------------------------------------------------
// LoadWindow

LoadWindow::LoadWindow(const LoadSettings &settings) :
    config(sanitized(settings))
{
}

void LoadWindow::setSettings(const LoadSettings &settings)
{
    config = sanitized(settings);
}

const LoadSettings &LoadWindow::settings() const
{
    return config;
}

void LoadWindow::clear()
{
    samples.clear();
    firstSampleMs = -1;
}

bool LoadWindow::busy() const
{
    return isBusy;
}

LoadWindow::Means LoadWindow::means(int windowSeconds) const
{
    Means result;
    if (samples.empty())
    {
        return result;
    }
    const qint64 windowMs = static_cast<qint64>(windowSeconds) * 1000;
    const qint64 lastMs = samples.back().timeMs;
    double cpuSum = 0.0;
    double gpuSum = 0.0;
    int count = 0;
    for (auto it = samples.rbegin(); it != samples.rend() && it->timeMs > lastMs - windowMs; ++it)
    {
        cpuSum += it->cpuPercent;
        gpuSum += it->gpuPercent;
        ++count;
    }
    result.cpuPercent = cpuSum / count;
    result.gpuPercent = gpuSum / count;
    // Each sample describes the interval that ends at its timestamp, so the
    // first one already covers one interval before it.
    result.covered = lastMs - firstSampleMs + config.sampleIntervalMs >= windowMs;
    return result;
}

LoadWindow::Means LoadWindow::busyWindowMeans() const
{
    return means(config.busyWindowSeconds);
}

LoadWindow::Means LoadWindow::idleWindowMeans() const
{
    return means(config.idleWindowSeconds);
}

bool LoadWindow::addSample(const LoadSample &sample)
{
    if (!samples.empty())
    {
        const qint64 gap = sample.timeMs - samples.back().timeMs;
        // A clock going backwards or a long stall (sleep, hibernation) makes
        // old samples meaningless; restart the window but keep the state.
        if (gap <= 0 || gap > std::max<qint64>(5000, 5LL * config.sampleIntervalMs))
        {
            clear();
        }
    }
    if (firstSampleMs < 0)
    {
        firstSampleMs = sample.timeMs;
    }
    samples.push_back(sample);
    const qint64 keepMs = static_cast<qint64>(
        std::max(config.busyWindowSeconds, config.idleWindowSeconds)
    ) * 1000;
    while (!samples.empty() && samples.front().timeMs <= sample.timeMs - keepMs)
    {
        samples.pop_front();
    }

    const bool wasBusy = isBusy;
    if (isBusy)
    {
        const Means idle = idleWindowMeans();
        if (idle.covered && peak(idle) < config.idleThresholdPercent)
        {
            isBusy = false;
        }
    }
    else
    {
        const Means busyMeans = busyWindowMeans();
        if (busyMeans.covered && peak(busyMeans) > config.busyThresholdPercent)
        {
            isBusy = true;
        }
    }
    return isBusy != wasBusy;
}

// ---------------------------------------------------------------------------
// LoadSampler

#ifdef Q_OS_WIN

namespace {

quint64 fileTimeValue(const FILETIME &time)
{
    return (static_cast<quint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

struct ProcessTimes
{
    quint64 creation = 0;
    quint64 cpu = 0;
};

std::optional<ProcessTimes> queryProcessTimes(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        return std::nullopt;
    }
    FILETIME creation {};
    FILETIME exit {};
    FILETIME kernel {};
    FILETIME user {};
    const BOOL ok = GetProcessTimes(process, &creation, &exit, &kernel, &user);
    CloseHandle(process);
    if (!ok)
    {
        return std::nullopt;
    }
    return ProcessTimes {fileTimeValue(creation), fileTimeValue(kernel) + fileTimeValue(user)};
}

// Engine types that do not represent rendering or viewport work. Video
// decode in particular would mark a machine busy while a browser plays video.
bool ignoredEngineType(const QString &type)
{
    return type.startsWith(QStringLiteral("Copy"), Qt::CaseInsensitive)
        || type.startsWith(QStringLiteral("Video"), Qt::CaseInsensitive)
        || type.startsWith(QStringLiteral("Security"), Qt::CaseInsensitive)
        || type.startsWith(QStringLiteral("Timer"), Qt::CaseInsensitive)
        || type.startsWith(QStringLiteral("OFA"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("Overlay"), Qt::CaseInsensitive);
}

}

class LoadSampler
{
public:
    LoadSampler()
    {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
        {
            query = nullptr;
            return;
        }
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) != ERROR_SUCCESS)
        {
            PdhCloseQuery(query);
            query = nullptr;
            return;
        }
        // Rate counters need two collections before the first value.
        PdhCollectQueryData(query);
    }

    ~LoadSampler()
    {
        if (query)
        {
            PdhCloseQuery(query);
        }
    }

    LoadSampler(const LoadSampler &) = delete;
    LoadSampler &operator=(const LoadSampler &) = delete;

    bool gpuAvailable() const
    {
        return query != nullptr;
    }

    // Returns nothing on the first call, which only establishes baselines.
    std::optional<LoadSample> sample(qint64 nowMs)
    {
        FILETIME idleTime {};
        FILETIME kernelTime {};
        FILETIME userTime {};
        FILETIME nowTime {};
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            return std::nullopt;
        }
        GetSystemTimeAsFileTime(&nowTime);
        const quint64 idle = fileTimeValue(idleTime);
        // Kernel time includes idle time.
        const quint64 total = fileTimeValue(kernelTime) + fileTimeValue(userTime);
        const quint64 now = fileTimeValue(nowTime);

        const std::unordered_set<DWORD> tree = ownProcessTree();
        const quint64 ownCpu = ownCpuDelta(tree, previousWallTime);
        const double gpuForeign = gpuLoad(tree, false);
        const double gpuOwn = gpuLoad(tree, true);

        const bool hasBaseline = previousTotal != 0;
        const quint64 totalDelta = total - previousTotal;
        const quint64 busyDelta = totalDelta - (idle - previousIdle);
        previousTotal = total;
        previousIdle = idle;
        previousWallTime = now;
        if (!hasBaseline || totalDelta == 0)
        {
            return std::nullopt;
        }

        LoadSample result;
        result.timeMs = nowMs;
        const double busyFraction = static_cast<double>(busyDelta) / totalDelta;
        const double ownFraction = static_cast<double>(ownCpu) / totalDelta;
        result.cpuPercent = std::clamp((busyFraction - ownFraction) * 100.0, 0.0, 100.0);
        result.ownCpuPercent = std::clamp(ownFraction * 100.0, 0.0, 100.0);
        result.gpuPercent = gpuForeign;
        result.ownGpuPercent = gpuOwn;
        return result;
    }

private:
    // This process, every rendrop-worker.exe, and all their descendants.
    std::unordered_set<DWORD> ownProcessTree()
    {
        std::unordered_set<DWORD> tree;
        const DWORD self = GetCurrentProcessId();
        tree.insert(self);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return tree;
        }
        std::unordered_map<DWORD, std::vector<DWORD>> children;
        std::vector<DWORD> roots {self};
        PROCESSENTRY32W entry {};
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry))
        {
            if (entry.th32ProcessID == 0)
            {
                continue;
            }
            children[entry.th32ParentProcessID].push_back(entry.th32ProcessID);
            if (entry.th32ProcessID != self
                && QString::fromWCharArray(entry.szExeFile).compare(
                       QStringLiteral("rendrop-worker.exe"), Qt::CaseInsensitive) == 0)
            {
                roots.push_back(entry.th32ProcessID);
            }
        }
        CloseHandle(snapshot);

        std::vector<std::pair<DWORD, quint64>> pending;
        for (DWORD root : roots)
        {
            tree.insert(root);
            const std::optional<ProcessTimes> times = queryProcessTimes(root);
            pending.emplace_back(root, times ? times->creation : 0);
        }
        while (!pending.empty())
        {
            const auto [parent, parentCreation] = pending.back();
            pending.pop_back();
            const auto found = children.find(parent);
            if (found == children.end())
            {
                continue;
            }
            for (DWORD child : found->second)
            {
                if (tree.count(child))
                {
                    continue;
                }
                const std::optional<ProcessTimes> times = queryProcessTimes(child);
                // A recycled parent pid would otherwise adopt unrelated
                // processes that outlived the original parent.
                if (times && parentCreation != 0 && times->creation < parentCreation)
                {
                    continue;
                }
                tree.insert(child);
                pending.emplace_back(child, times ? times->creation : 0);
            }
        }
        return tree;
    }

    // CPU time (100 ns units, summed over cores) the tree used since the
    // previous sample.
    quint64 ownCpuDelta(const std::unordered_set<DWORD> &tree, quint64 sinceWallTime)
    {
        std::unordered_map<DWORD, ProcessTimes> next;
        quint64 delta = 0;
        for (DWORD pid : tree)
        {
            const std::optional<ProcessTimes> times = queryProcessTimes(pid);
            if (!times)
            {
                continue;
            }
            const auto previous = processTimes.find(pid);
            if (previous != processTimes.end() && previous->second.creation == times->creation)
            {
                delta += times->cpu - previous->second.cpu;
            }
            else if (sinceWallTime != 0 && times->creation >= sinceWallTime)
            {
                // Started during this interval: all of its time is new.
                delta += times->cpu;
            }
            next.emplace(pid, *times);
        }
        processTimes = std::move(next);
        return delta;
    }

    // Busiest GPU engine's utilisation counting only processes inside
    // (own == true) or outside the tree. Collects once per sample call.
    double gpuLoad(const std::unordered_set<DWORD> &tree, bool own)
    {
        if (!query)
        {
            return 0.0;
        }
        if (!own)
        {
            engineItems.clear();
            if (PdhCollectQueryData(query) != ERROR_SUCCESS)
            {
                return 0.0;
            }
            DWORD size = 0;
            DWORD count = 0;
            PDH_STATUS status = PDH_MORE_DATA;
            std::vector<BYTE> buffer;
            for (int attempt = 0; attempt < 3 && status == PDH_MORE_DATA; ++attempt)
            {
                status = PdhGetFormattedCounterArrayW(
                    counter,
                    PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
                    &size,
                    &count,
                    buffer.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data())
                );
                if (status == PDH_MORE_DATA)
                {
                    buffer.assign(size, 0);
                }
            }
            if (status != ERROR_SUCCESS)
            {
                return 0.0;
            }
            const auto *items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
            for (DWORD index = 0; index < count; ++index)
            {
                const PDH_FMT_COUNTERVALUE &value = items[index].FmtValue;
                if (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA)
                {
                    continue;
                }
                // pid_1234_luid_0x..._0x..._phys_0_eng_3_engtype_3D
                const QString name = QString::fromWCharArray(items[index].szName);
                if (!name.startsWith(QStringLiteral("pid_")))
                {
                    continue;
                }
                const qsizetype pidEnd = name.indexOf(QChar('_'), 4);
                const qsizetype luid = name.indexOf(QStringLiteral("luid_"));
                const qsizetype engtype = name.indexOf(QStringLiteral("_engtype_"));
                if (pidEnd < 0 || luid < 0 || engtype < luid)
                {
                    continue;
                }
                bool pidOk = false;
                const DWORD pid = name.mid(4, pidEnd - 4).toULong(&pidOk);
                const QString type = name.mid(engtype + 9);
                if (!pidOk || ignoredEngineType(type))
                {
                    continue;
                }
                engineItems.push_back({pid, name.mid(luid, engtype - luid), value.doubleValue});
            }
        }
        std::unordered_map<QString, double> perEngine;
        for (const EngineItem &item : engineItems)
        {
            if ((tree.count(item.pid) != 0) == own)
            {
                perEngine[item.engine] += item.percent;
            }
        }
        double busiest = 0.0;
        for (const auto &[engine, percent] : perEngine)
        {
            busiest = std::max(busiest, percent);
        }
        return std::clamp(busiest, 0.0, 100.0);
    }

    struct EngineItem
    {
        DWORD pid;
        QString engine;
        double percent;
    };

    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    std::vector<EngineItem> engineItems;
    std::unordered_map<DWORD, ProcessTimes> processTimes;
    quint64 previousIdle = 0;
    quint64 previousTotal = 0;
    quint64 previousWallTime = 0;
};

#else

class LoadSampler
{
public:
    bool gpuAvailable() const
    {
        return false;
    }

    std::optional<LoadSample> sample(qint64)
    {
        return std::nullopt;
    }
};

#endif

// ---------------------------------------------------------------------------
// LoadMonitor

LoadMonitor::LoadMonitor(const LoadSettings &settings, QObject *parent) :
    QThread(parent),
    window(settings)
{
    current.settings = window.settings();
}

LoadMonitor::~LoadMonitor()
{
    stop();
    wait();
}

LoadStatus LoadMonitor::status() const
{
    QMutexLocker locker(&mutex);
    return current;
}

void LoadMonitor::stop()
{
    quit();
}

void LoadMonitor::run()
{
    LoadSampler sampler;
    {
        QMutexLocker locker(&mutex);
        current.gpuAvailable = sampler.gpuAvailable();
    }
    QElapsedTimer clock;
    clock.start();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &timer, [&]() {
        const std::optional<LoadSample> sample = sampler.sample(clock.elapsed());
        if (!sample)
        {
            return;
        }
        bool changed = false;
        bool busy = false;
        {
            QMutexLocker locker(&mutex);
            changed = window.addSample(*sample);
            busy = window.busy();
            current.busy = busy;
            current.latest = *sample;
            current.busyWindow = window.busyWindowMeans();
            current.idleWindow = window.idleWindowMeans();
        }
        if (changed)
        {
            emit busyChanged(busy);
        }
    });
    sampler.sample(clock.elapsed());
    timer.start(window.settings().sampleIntervalMs);
    exec();
}
