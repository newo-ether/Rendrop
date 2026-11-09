// frame_driver.h

#ifndef FRAME_DRIVER_H
#define FRAME_DRIVER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <QAnimationDriver>
#include <QElapsedTimer>

// Steps every Qt animation once per display refresh. Qt's own driver ticks
// on a 16 ms timer, and a Windows timer cannot go below the 15.6 ms system
// tick anyway, so scrolling and slides would top out at about 60 fps even on
// a 120 Hz or faster screen.
//
// A thread waits for each composition with DwmFlush() and posts one advance
// to the GUI thread; a frame still waiting there is not posted again, so a
// slow frame drops ticks instead of queueing them.
class FrameDriver : public QAnimationDriver
{
public:
    ~FrameDriver() override;

    // Installs the driver after the QApplication exists. It comes off when
    // exec() returns, so call it again before every exec().
    static void installForScreen();

    qint64 elapsed() const override;
    // Frames per second of the primary screen, kept within 60-240.
    double rate() const;

protected:
    void start() override;
    void stop() override;

private:
    explicit FrameDriver(double hz);
    void run();
    void tick();

    // Time since the last start(), which is what Qt expects from elapsed().
    QElapsedTimer clock;
    double hz;
    bool installed = false;

    std::thread pacer;
    std::mutex mutex;
    std::condition_variable wake;
    // Guarded by mutex.
    bool active = false;
    bool quitting = false;
    // An advance is posted and has not run yet.
    std::atomic_bool pending {false};
};

#endif // FRAME_DRIVER_H
