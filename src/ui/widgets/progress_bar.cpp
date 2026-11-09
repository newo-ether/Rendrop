// progress_bar.cpp

#include <QWidget>
#include <QLabel>
#include <QString>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QFont>
#include <QTimer>
#include <QElapsedTimer>
#include <QColor>

#include "ui/widgets/progress_bar.h"
#include "ui/style/rounded_surface.h"
#include "ui/style/theme.h"

ProgressBar::ProgressBar(QWidget *parent):
    QWidget(parent),
    value(0),
    targetValue(0),
    velocity(0),
    stiffness(300),
    damping(50)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ProgressBar::updateProgressBar);

    elapsedTimer = new QElapsedTimer();
    elapsedTimer->start();
    lastElapsed = 0;
}

ProgressBar::~ProgressBar()
{
    if (timer->isActive())
    {
        timer->stop();
    }
    delete timer;
    delete elapsedTimer;
}

void ProgressBar::setProgressBar(float value)
{
    this->targetValue = std::clamp(value, 0.0f, 100.0f);
    if (!timer->isActive())
    {
        timer->start(10);
        lastElapsed = elapsedTimer->elapsed();
    }
}

void ProgressBar::setStiffness(float stiffness)
{
    this->stiffness = stiffness;
}

void ProgressBar::setDamping(float damping)
{
    this->damping = damping;
}

void ProgressBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    int radius = 4;

    RoundedSurface::paintFrame(painter, QRectF(2, 2, w - 4, h - 4), radius,
        Theme::control.color(), Theme::outline.color());

    float ratio = value / 100.0f;
    int progressWidth = qMax(0, int((w - 6) * ratio));

    if (progressWidth > 0)
    {
        QPainterPath progressPath;
        progressPath.addRoundedRect(3, 3, progressWidth, h - 6, radius, radius);
        painter.fillPath(progressPath, Theme::progress.color());
    }
}

void ProgressBar::updateProgressBar()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = static_cast<float>(elapsed - lastElapsed) / 1000;
    lastElapsed = elapsed;

    int iteration = std::max(1, (int)(dt / 0.01f));
    iteration = std::min(iteration, 10);
    dt /= iteration;

    for (int i = 0; i < iteration; i++)
    {
        float delta = targetValue - value;
        float accel = stiffness * delta - damping * velocity;

        velocity += accel * dt;
        value += velocity * dt;
        value = std::clamp(value, 0.0f, 100.0f);

        if (std::abs(delta) < 0.1f && std::abs(velocity) < 0.1f)
        {
            velocity = 0;
            value = targetValue;
            timer->stop();
            break;
        }
    }

    update();
}
