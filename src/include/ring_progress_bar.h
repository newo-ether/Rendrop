// ring_progress_bar.h

#ifndef RING_PROGRESS_BAR_H
#define RING_PROGRESS_BAR_H

#include <QWidget>
#include <QPainter>
#include <QTimer>

class RingProgressBar : public QWidget
{
    Q_OBJECT

public:
    RingProgressBar(QWidget *parent = nullptr);
    ~RingProgressBar();

    void setValue(int value);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int value;
};

#endif // RING_PROGRESS_BAR_H
