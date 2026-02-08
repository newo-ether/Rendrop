// loading_bar.cpp

#include <QPainter>
#include <QElapsedTimer>
#include <QTimer>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>

#include "loading_bar.h"

LoadingBar::LoadingBar(QWidget *parent):
    QWidget(parent),
    timer(new QTimer()),
    elapsedTimer(new QElapsedTimer()),
    lastElapsed(0),
    progress(0.0f)
{
    elapsedTimer->start();
    connect(timer, &QTimer::timeout, this, &LoadingBar::updateProgress);
    timer->start(20);
}

LoadingBar::~LoadingBar()
{
    delete timer;
    delete elapsedTimer;
}

void LoadingBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    lastElapsed = elapsedTimer->elapsed();
    if (!timer->isActive())
    {
        timer->start(20);
    }
}

void LoadingBar::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (timer->isActive())
    {
        timer->stop();
    }
}

void LoadingBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int dotCount = 3;
    int baseRadius = 4;
    int spacing = 15;
    QColor color(33, 100, 180);

    for (int i = 0; i < dotCount; ++i)
    {
        float phase = fmodf(progress + i * 0.33f, 1.0f);
        float scale = 0.5f + 0.5f * sinf(phase * M_PI);
        float alpha = 0.3f + 0.7f * sinf(phase * M_PI);
        scale = std::clamp(scale, 0.0f, 1.0f);
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        QColor c = color;
        c.setAlphaF(alpha);

        painter.setBrush(c);
        painter.setPen(Qt::NoPen);

        int x = width() / 2 - (dotCount - 1) * spacing / 2 + i * spacing;
        int y = height() / 2;
        painter.drawEllipse(QPointF(x, y), baseRadius * scale, baseRadius * scale);
    }
}

void LoadingBar::updateProgress()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = (elapsed - lastElapsed) / 1000.0f;
    lastElapsed = elapsed;
    progress -= dt * 1.2f;
    progress = fmodf(progress + 1.0f, 1.0f);
    progress = std::clamp(progress, 0.0f, 1.0f);
    update();
}
