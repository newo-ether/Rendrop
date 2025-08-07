// ring_progress_bar.cpp

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>

#include "ring_progress_bar.h"

RingProgressBar::RingProgressBar(QWidget *parent):
    QWidget(parent),
    value(0),
    targetValue(0),
    velocity(0),
    stiffness(300),
    damping(50)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &RingProgressBar::updateProgressBar);
    timer->start(8);

    elapsedTimer = new QElapsedTimer();
    elapsedTimer->start();
    lastElapsed = 0;
}

RingProgressBar::~RingProgressBar() {
    delete timer;
}

void RingProgressBar::setProgressBar(float value) {
    this->targetValue = std::clamp(value, 0.0f, 100.0f);
    update();
}

void RingProgressBar::setStiffness(float stiffness) {
    this->stiffness = stiffness;
    update();
}

void RingProgressBar::setDamping(float damping) {
    this->damping = damping;
    update();
}

void RingProgressBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int size = qMin(w, h);
    int penWidth = 12;

    QRectF rect((w - size) / 2 + penWidth / 2, (h - size) / 2 + penWidth / 2,
                size - penWidth, size - penWidth);

    QColor backgroundColor(53, 55, 60);
    QColor progressColor(33, 150, 243);

    QPen pen;
    pen.setColor(backgroundColor);
    pen.setWidth(penWidth);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    p.drawArc(rect, 90 * 16, -360 * 16);

    pen.setColor(progressColor);
    p.setPen(pen);

    float angle = 360.0 * value / 100;
    p.drawArc(rect, 90 * 16, -angle * 16);
}

void RingProgressBar::updateProgressBar()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = static_cast<float>(elapsed - lastElapsed) / 1000;
    lastElapsed = elapsed;

    int iteration = std::max(1, (int)(dt / 0.01f));
    dt /= iteration;

    for (int i = 0; i < iteration; i++)
    {
        float delta = targetValue - value;
        float accel = stiffness * delta - damping * velocity;

        velocity += accel * dt;
        value += velocity * dt;
        value = std::clamp(value, 0.0f, 100.0f);

        if (std::abs(delta) < 0.1f && std::abs(velocity) < 0.1f) {
            velocity = 0;
            value = targetValue;
        }
    }

    update();
}
