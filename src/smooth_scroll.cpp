// smooth_scroll.cpp

#include <algorithm>
#include <cmath>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QEasingCurve>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "smooth_scroll.h"
#include "transition.h"

namespace {

// Both a wheel notch and a jump to a section ease in and out on a quart
// Bezier, firmer than the list's cubic; a notch arriving mid-way carries the
// current speed into the next move.
constexpr int wheelMs = 220;
constexpr int jumpMinMs = 260;
constexpr int jumpMaxMs = 520;
constexpr char handleName[] = "rendropSmoothScroll";

}

SmoothScroll::SmoothScroll(QAbstractScrollArea *area):
    QObject(area),
    area(area),
    bar(area->verticalScrollBar())
{
    setObjectName(QLatin1String(handleName));
    written = bar->value();
    area->viewport()->installEventFilter(this);
    // Dragging the handle, keys or code moving the bar take over.
    QObject::connect(bar, &QScrollBar::sliderPressed, this, &SmoothScroll::stop);
    QObject::connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
        if (animation && value != written)
        {
            stop();
        }
    });
    QObject::connect(bar, &QScrollBar::rangeChanged, this, [this](int minimum, int maximum) {
        target = std::clamp(target, minimum, maximum);
    });
}

SmoothScroll *SmoothScroll::of(const QAbstractScrollArea *area)
{
    // Direct children only: an area nested inside keeps its own.
    return area ? area->findChild<SmoothScroll *>(QLatin1String(handleName), Qt::FindDirectChildrenOnly) : nullptr;
}

void SmoothScroll::install(QAbstractScrollArea *area)
{
    if (area && !of(area))
    {
        new SmoothScroll(area);
    }
}

void SmoothScroll::scrollTo(QAbstractScrollArea *area, int value)
{
    SmoothScroll *scroll = of(area);
    if (!scroll)
    {
        if (area)
        {
            area->verticalScrollBar()->setValue(value);
        }
        return;
    }
    const int clamped = std::clamp(value, scroll->bar->minimum(), scroll->bar->maximum());
    const int distance = std::abs(clamped - scroll->bar->value());
    // Longer trips take a little longer, within bounds.
    const int duration = std::clamp(jumpMinMs + distance / 4, jumpMinMs, jumpMaxMs);
    scroll->animateTo(clamped, duration);
    scroll->jumping = scroll->animation != nullptr;
}

bool SmoothScroll::isJumping(const QAbstractScrollArea *area)
{
    const SmoothScroll *scroll = of(area);
    return scroll && scroll->jumping && scroll->animation;
}

void SmoothScroll::stop()
{
    jumping = false;
    if (animation)
    {
        animation->stop();
    }
}

void SmoothScroll::animateTo(int value, int durationMs)
{
    qreal from = bar->value();
    qreal speed = 0.0; // units per ms
    if (animation)
    {
        if (moveMs > 0)
        {
            const qreal s = std::clamp(static_cast<qreal>(animation->currentTime()) / moveMs, 0.0, 1.0);
            from = move.at(s);
            speed = move.rate(s) / moveMs;
        }
        animation->stop();
    }
    target = value;
    jumping = false;
    if (value == bar->value() && speed == 0.0)
    {
        return;
    }
    move = Look::Move::make(from, static_cast<qreal>(value), Look::quartInOut, speed, durationMs);
    moveMs = durationMs;
    auto *next = new QVariantAnimation(this);
    next->setStartValue(0.0);
    next->setEndValue(1.0);
    next->setDuration(durationMs);
    QObject::connect(next, &QVariantAnimation::valueChanged, this, [this](const QVariant &current) {
        written = std::clamp(static_cast<int>(std::lround(move.at(current.toReal()))), bar->minimum(), bar->maximum());
        bar->setValue(written);
    });
    animation = next;
    next->start(QAbstractAnimation::DeleteWhenStopped);
}

bool SmoothScroll::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != area->viewport() || event->type() != QEvent::Wheel)
    {
        return QObject::eventFilter(watched, event);
    }
    auto *wheel = static_cast<QWheelEvent *>(event);
    const bool precise = !wheel->pixelDelta().isNull() || wheel->phase() != Qt::NoScrollPhase;
    const bool modified = wheel->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    const int notches = wheel->angleDelta().y();
    if (precise || modified || notches == 0 || bar->maximum() <= bar->minimum())
    {
        // Qt scrolls these directly; drop any easing so they do not fight.
        stop();
        return false;
    }
    // Same distance per notch as Qt's own wheel scrolling, only eased.
    const qreal step = QApplication::wheelScrollLines() * bar->singleStep() * (notches / 120.0);
    // Notches in quick succession add up from where the last one was going.
    const int from = animation && !jumping ? target : bar->value();
    const int to = std::clamp(static_cast<int>(std::lround(from - step)), bar->minimum(), bar->maximum());
    if (to != from || animation)
    {
        animateTo(to, wheelMs);
    }
    wheel->accept();
    return true;
}
