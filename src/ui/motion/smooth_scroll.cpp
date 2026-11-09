// smooth_scroll.cpp

#include <algorithm>
#include <cmath>
#include <functional>

#include <QAbstractAnimation>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QScrollBar>
#include <QWheelEvent>

#include "ui/motion/smooth_scroll.h"

namespace {

// Time constant of the wheel chase: every chaseTauMs the view closes about
// 63% of what is left, so a notch is 90% there after ~2.3 tau and the view
// answers on the very next frame. Browsers land in the same 100-200 ms.
constexpr qreal chaseTauMs = 45.0;
// A jump to a section follows the same curve, slower: it covers a long way
// at once and would otherwise look like a cut.
constexpr qreal jumpTauMs = 90.0;
// Closer than this the chase lands on the target.
constexpr qreal chaseLand = 0.5;
constexpr char handleName[] = "rendropSmoothScroll";

// An open-ended animation that hands each frame's time to a callback, so the
// chase runs on the same refresh-rate ticks as every other animation.
class FrameTicker : public QAbstractAnimation
{
public:
    FrameTicker(QObject *parent, std::function<void (int)> step):
        QAbstractAnimation(parent),
        step(std::move(step))
    {
    }

    int duration() const override { return -1; }

protected:
    void updateCurrentTime(int currentTime) override { step(currentTime); }

private:
    std::function<void (int)> step;
};

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
        if (chase && value != written)
        {
            stop();
        }
    });
    QObject::connect(bar, &QScrollBar::rangeChanged, this, [this](int minimum, int maximum) {
        goal = std::clamp(goal, static_cast<qreal>(minimum), static_cast<qreal>(maximum));
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
    // The same chase as the wheel, so both move on one curve; a chase already
    // running just gets the new goal.
    scroll->tauMs = jumpTauMs;
    scroll->chaseTo(value);
    scroll->jumping = scroll->chase != nullptr;
}

bool SmoothScroll::isJumping(const QAbstractScrollArea *area)
{
    const SmoothScroll *scroll = of(area);
    return scroll && scroll->jumping && scroll->chase;
}

void SmoothScroll::stop()
{
    jumping = false;
    if (chase)
    {
        chase->stop();
    }
}

void SmoothScroll::write(qreal value)
{
    written = std::clamp(static_cast<int>(std::lround(value)), bar->minimum(), bar->maximum());
    bar->setValue(written);
}

void SmoothScroll::chaseTo(qreal value)
{
    goal = std::clamp(value, static_cast<qreal>(bar->minimum()), static_cast<qreal>(bar->maximum()));
    if (chase)
    {
        return;
    }
    position = bar->value();
    if (std::abs(goal - position) < chaseLand)
    {
        return;
    }
    chaseMs = 0;
    auto *ticker = new FrameTicker(this, [this](int ms) { chaseStep(ms); });
    chase = ticker;
    ticker->start(QAbstractAnimation::DeleteWhenStopped);
}

void SmoothScroll::chaseStep(int elapsedMs)
{
    const int dt = elapsedMs - chaseMs;
    chaseMs = elapsedMs;
    if (dt <= 0)
    {
        return;
    }
    // Frame-rate independent: two 4 ms frames cover what one 8 ms frame does.
    position = goal + (position - goal) * std::exp(-dt / tauMs);
    if (std::abs(goal - position) < chaseLand)
    {
        position = goal;
    }
    write(position);
    if (position == goal && chase)
    {
        chase->stop();
    }
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
    // Same distance per notch as Qt's own wheel scrolling.
    const qreal step = QApplication::wheelScrollLines() * bar->singleStep() * (notches / 120.0);
    // Notches in quick succession add up from where the last one was going.
    // A section jump on its way becomes wheel scrolling from where it was going.
    const qreal from = chase ? goal : static_cast<qreal>(bar->value());
    jumping = false;
    tauMs = chaseTauMs;
    chaseTo(from - step);
    wheel->accept();
    return true;
}
