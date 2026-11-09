// frame_driver.cpp

#include <algorithm>
#include <chrono>

#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QScreen>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

#include "ui/motion/frame_driver.h"

namespace {

QPointer<FrameDriver> current;

}

FrameDriver::FrameDriver(double hz):
    QAnimationDriver(qApp),
    hz(hz)
{
    pacer = std::thread([this]() { run(); });
}

FrameDriver::~FrameDriver()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        quitting = true;
    }
    wake.notify_all();
    pacer.join();
}

void FrameDriver::installForScreen()
{
    if (!current)
    {
        const QScreen *screen = QGuiApplication::primaryScreen();
        const double refresh = screen ? screen->refreshRate() : 60.0;
        current = new FrameDriver(std::clamp(refresh, 60.0, 240.0));
        // Hand back to Qt's driver while the event loop still exists. This
        // runs at the end of every exec(), so a restart installs it again.
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, current, []() {
            if (current && current->installed)
            {
                current->uninstall();
                current->installed = false;
            }
        });
    }
    if (!current->installed)
    {
        current->install();
        current->installed = true;
    }
}

qint64 FrameDriver::elapsed() const
{
    return clock.isValid() ? clock.elapsed() : 0;
}

double FrameDriver::rate() const
{
    return hz;
}

void FrameDriver::start()
{
    // Qt adds this to the time it had when the driver started, so it has to
    // count from this start, not from some earlier one.
    clock.start();
    {
        std::lock_guard<std::mutex> lock(mutex);
        active = true;
    }
    wake.notify_all();
    QAnimationDriver::start();
}

void FrameDriver::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        active = false;
    }
    QAnimationDriver::stop();
}

void FrameDriver::tick()
{
    pending = false;
    if (isRunning())
    {
        advance();
    }
}

void FrameDriver::run()
{
    const auto period = std::chrono::nanoseconds(static_cast<qint64>(1e9 / hz));
    auto last = std::chrono::steady_clock::now();
    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this]() { return active || quitting; });
            if (quitting)
            {
                return;
            }
        }
        bool paced = false;
#ifdef Q_OS_WIN
        // Returns after the next composition, i.e. once per refresh.
        paced = SUCCEEDED(DwmFlush());
#endif
        // Without composition (display off, some remote sessions) DwmFlush
        // fails or returns at once; keep to the screen's rate then.
        const auto early = last + period / 2;
        if (!paced || std::chrono::steady_clock::now() < early)
        {
            std::this_thread::sleep_until(last + period);
        }
        last = std::chrono::steady_clock::now();
        if (!pending.exchange(true))
        {
            QMetaObject::invokeMethod(this, [this]() { tick(); }, Qt::QueuedConnection);
        }
    }
}
