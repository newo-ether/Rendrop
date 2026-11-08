// frame_driver.h

#ifndef FRAME_DRIVER_H
#define FRAME_DRIVER_H

#include <QAnimationDriver>
#include <QChronoTimer>
#include <QElapsedTimer>

// Steps every Qt animation at the display's refresh rate. Qt's own driver
// ticks on a fixed 16 ms timer, so scrolling and slides top out at about
// 60 fps even on a 120 Hz or faster screen.
class FrameDriver : public QAnimationDriver
{
public:
    // Installs a driver for the primary screen's refresh rate, kept within
    // 60-240 Hz. Call once, after the QApplication exists.
    static void installForScreen();

    qint64 elapsed() const override;
    // Frames per second the driver steps at.
    double rate() const;

protected:
    void start() override;
    void stop() override;

private:
    explicit FrameDriver(double hz);

    QChronoTimer timer;
    // Time since the last start(), which is what Qt expects from elapsed().
    QElapsedTimer clock;
    double hz;
};

#endif // FRAME_DRIVER_H
