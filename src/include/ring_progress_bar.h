// ring_progress_bar.h

#ifndef RING_PROGRESS_BAR_H
#define RING_PROGRESS_BAR_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>

class RingProgressBar : public QWidget
{
    Q_OBJECT

public:
    RingProgressBar(QWidget *parent = nullptr);
    ~RingProgressBar();

    void setProgressBar(float value);
    void setStiffness(float stiffness);
    void setDamping(float damping);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void updateProgressBar();

private:
    float value, targetValue, velocity;
    float stiffness, damping;
    qint64 lastElapsed;

    QTimer *timer;
    QElapsedTimer *elapsedTimer;
};

#endif // RING_PROGRESS_BAR_H
