// transition.cpp

#include <algorithm>
#include <cmath>

#include <QEasingCurve>
#include <QWidget>

#include "ui/motion/transition.h"

namespace Look {

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    const QColor from = a.isValid() ? a : QColor(b.red(), b.green(), b.blue(), 0);
    const QColor to = b.isValid() ? b : QColor(from.red(), from.green(), from.blue(), 0);
    const auto lerp = [t](qreal x, qreal y) { return x + (y - x) * t; };
    return QColor::fromRgbF(
        static_cast<float>(lerp(from.redF(), to.redF())),
        static_cast<float>(lerp(from.greenF(), to.greenF())),
        static_cast<float>(lerp(from.blueF(), to.blueF())),
        static_cast<float>(lerp(from.alphaF(), to.alphaF()))
    );
}

QEasingCurve easeInOut()
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(0.42, 0.0), QPointF(0.58, 1.0), QPointF(1.0, 1.0));
    return curve;
}

QEasingCurve bezierInOut(qreal x1, qreal startSlope)
{
    // The tangent at 0 points at (x1, y1), so the starting slope is y1 / x1.
    const qreal y1 = std::clamp(startSlope * x1, 0.0, 1.0);
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(x1, y1), QPointF(1.0 - x1, 1.0), QPointF(1.0, 1.0));
    return curve;
}

qreal slopeAt(const QEasingCurve &curve, qreal s)
{
    constexpr qreal h = 0.004;
    const qreal a = std::clamp(s - h, 0.0, 1.0);
    const qreal b = std::clamp(s + h, 0.0, 1.0);
    return b > a ? (curve.valueForProgress(b) - curve.valueForProgress(a)) / (b - a) : 0.0;
}

Move Move::make(qreal from, qreal to, qreal x1, qreal speed, int durationMs)
{
    const qreal distance = to - from;
    // Progress-space slope that matches the current speed; negative or
    // undefined slopes (turning round, no distance) start from rest.
    const qreal slope = std::abs(distance) > 1e-6 ? speed * durationMs / distance : 0.0;
    return Move{from, to, bezierInOut(x1, std::max(0.0, slope))};
}

}

namespace {

QVariantAnimation *makeAnimation(QWidget *owner, int durationMs,
    const QEasingCurve &curve = QEasingCurve(QEasingCurve::OutCubic))
{
    auto *animation = new QVariantAnimation(owner);
    animation->setDuration(durationMs);
    animation->setEasingCurve(curve);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    return animation;
}

}

Transition::Transition(QWidget *owner, qreal initial, int durationMs, const QEasingCurve &curve):
    owner(owner),
    animation(makeAnimation(owner, durationMs, curve)),
    current(initial),
    goal(initial)
{
    QObject::connect(animation, &QVariantAnimation::valueChanged, owner, [this](const QVariant &value) {
        current = value.toReal();
        this->owner->update();
    });
}

Transition::~Transition()
{
    delete animation;
}

qreal Transition::value() const
{
    return current;
}

qreal Transition::target() const
{
    return goal;
}

void Transition::setTarget(qreal target)
{
    if (qFuzzyCompare(target + 1.0, goal + 1.0))
    {
        return;
    }
    goal = target;
    animation->stop();
    if (!owner->isVisible())
    {
        jumpTo(target);
        return;
    }
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->start();
}

void Transition::jumpTo(qreal value)
{
    animation->stop();
    current = value;
    goal = value;
    owner->update();
}

ColorTransition::ColorTransition(QWidget *owner, const QColor &initial, int durationMs):
    owner(owner),
    animation(makeAnimation(owner, durationMs)),
    current(initial),
    goal(initial)
{
    // The animation runs 0 to 1; colors are mixed here so invalid ends work.
    QObject::connect(animation, &QVariantAnimation::valueChanged, owner, [this](const QVariant &) {
        const QColor from = animation->property("rendropFrom").value<QColor>();
        current = Look::mix(from, goal, animation->currentValue().toReal());
        if (animation->currentValue().toReal() >= 1.0)
        {
            current = goal;
        }
        this->owner->update();
    });
}

ColorTransition::~ColorTransition()
{
    delete animation;
}

QColor ColorTransition::value() const
{
    return current;
}

void ColorTransition::setTarget(const QColor &target)
{
    if (target == goal)
    {
        return;
    }
    goal = target;
    animation->stop();
    if (!owner->isVisible())
    {
        jumpTo(target);
        return;
    }
    animation->setProperty("rendropFrom", current);
    animation->start();
}

void ColorTransition::jumpTo(const QColor &value)
{
    animation->stop();
    current = value;
    goal = value;
    owner->update();
}

Fader::Fader(QWidget *owner):
    QObject(owner),
    owner(owner)
{
}

qreal Fader::level(const QWidget *constOwner, quintptr key, qreal target)
{
    if (!constOwner)
    {
        return target;
    }
    // Painting is const in QStyle; the fade state is not part of the widget's
    // observable state, so it lives on a child object.
    QWidget *owner = const_cast<QWidget *>(constOwner);
    Fader *fader = owner->findChild<Fader *>(QString(), Qt::FindDirectChildrenOnly);
    if (!fader)
    {
        fader = new Fader(owner);
    }
    auto found = fader->transitions.find(key);
    if (found == fader->transitions.end())
    {
        fader->transitions.emplace(key, std::make_unique<Transition>(owner, target));
        return target;
    }
    found->second->setTarget(target);
    return found->second->value();
}
