// frame_driver.cpp

#include <algorithm>
#include <chrono>

#include <QGuiApplication>
#include <QScreen>

#include "frame_driver.h"

FrameDriver::FrameDriver(double hz):
    QAnimationDriver(qApp),
    hz(hz)
{
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(std::chrono::nanoseconds(static_cast<qint64>(1e9 / hz)));
    QObject::connect(&timer, &QChronoTimer::timeout, this, [this]() { advance(); });
}

void FrameDriver::installForScreen()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    const double refresh = screen ? screen->refreshRate() : 60.0;
    auto *driver = new FrameDriver(std::clamp(refresh, 60.0, 240.0));
    driver->install();
    // Hand back to Qt's driver while the event loop still exists; stopping
    // the timer later warns about a destroyed event dispatcher.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, driver, [driver]() { driver->uninstall(); });
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
    timer.start();
    QAnimationDriver::start();
}

void FrameDriver::stop()
{
    timer.stop();
    QAnimationDriver::stop();
}
