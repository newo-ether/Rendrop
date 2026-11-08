// progress_bar.h

#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>

class ProgressBar : public QWidget
{
    Q_OBJECT

public:
    ProgressBar(QWidget *parent = nullptr);
    ~ProgressBar();

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

#endif // PROGRESS_BAR_H
