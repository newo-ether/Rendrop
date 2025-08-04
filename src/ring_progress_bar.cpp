// ring_progress_bar.cpp

#include <QWidget>
#include <QPainter>
#include <QTimer>

#include "ring_progress_bar.h"

RingProgressBar::RingProgressBar(QWidget *parent):
    QWidget(parent),
    value(0)
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [&]() {
        value = (value + 1) % (100 + 1);
        update();
    });
    timer->start(20);
}

RingProgressBar::~RingProgressBar() {}

void RingProgressBar::setValue(int value) {
    this->value = qBound(0, value, 100);
    update();
}

void RingProgressBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int size = qMin(w, h);
    int penWidth = 12;

    QRectF rect((w - size) / 2 + penWidth / 2,
                (h - size) / 2 + penWidth / 2,
                size - penWidth,
                size - penWidth);

    QColor progressColor(33, 150, 243);
    QPen pen(progressColor);
    pen.setWidth(penWidth);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    float angle = 360.0 * value / 100;
    p.drawArc(rect, 90 * 16, -angle * 16);
}
