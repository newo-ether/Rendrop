// transition.h

#ifndef TRANSITION_H
#define TRANSITION_H

#include <memory>
#include <unordered_map>

#include <QColor>
#include <QEasingCurve>
#include <QObject>
#include <QPointer>
#include <QVariantAnimation>

class QWidget;

// Hover, press and state colors ease into each other instead of switching.
// Both classes repaint their owner on every step and are owned by it.
namespace Look {
// Default length of a hover or state change.
inline constexpr int transitionMs = 160;

// a at t = 0, b at t = 1, alpha included.
QColor mix(const QColor &a, const QColor &b, qreal t);

// Cubic Bezier ease-in-out (0.42, 0, 0.58, 1), for things that travel, like
// a switch knob: it starts and lands softly. Color fades keep OutCubic so a
// hover answers at once.
QEasingCurve easeInOut();

// Ease-in-out Beziers for things that move a distance: cubic
// (0.65, 0, 0.35, 1) for list rows and the log card, quart (0.76, 0, 0.24, 1)
// for scrolling.
inline constexpr qreal cubicInOut = 0.65;
inline constexpr qreal quartInOut = 0.76;

// cubic-bezier(x1, y1, 1 - x1, 1). From rest y1 is 0; a move redirected
// half-way passes its current speed as `startSlope` (output per progress at
// 0) so it carries on instead of stopping and starting again. The slope is
// capped so the curve never overshoots.
QEasingCurve bezierInOut(qreal x1, qreal startSlope = 0.0);
// Output per progress of `curve` at progress s.
qreal slopeAt(const QEasingCurve &curve, qreal s);

// One coordinate travelling from `from` to `to` along bezierInOut(x1).
struct Move
{
    qreal from = 0.0;
    qreal to = 0.0;
    QEasingCurve curve;

    // `speed` is the current speed in units per ms; it is carried on when it
    // points the same way as the trip, otherwise the move starts from rest.
    static Move make(qreal from, qreal to, qreal x1, qreal speed, int durationMs);
    qreal at(qreal s) const { return from + (to - from) * curve.valueForProgress(s); }
    // Units per progress at s.
    qreal rate(qreal s) const { return (to - from) * slopeAt(curve, s); }
};
}

// A number easing towards its target, typically a 0-1 state progress.
class Transition
{
public:
    explicit Transition(QWidget *owner, qreal initial = 0.0, int durationMs = Look::transitionMs,
        const QEasingCurve &curve = QEasingCurve(QEasingCurve::OutCubic));
    ~Transition();
    Transition(const Transition &) = delete;
    Transition &operator=(const Transition &) = delete;

    qreal value() const;
    qreal target() const;
    // Starts easing from the current value; a no-op when already heading there.
    void setTarget(qreal target);
    // Jumps without animating, e.g. before the owner is first shown.
    void jumpTo(qreal value);

private:
    QWidget *owner;
    // Child of the owner, which may delete it first.
    QPointer<QVariantAnimation> animation;
    qreal current;
    qreal goal;
};

// A color easing towards its target. Invalid colors count as transparent
// versions of the other end, so a surface fades in instead of popping.
class ColorTransition
{
public:
    explicit ColorTransition(QWidget *owner, const QColor &initial = QColor(), int durationMs = Look::transitionMs);
    ~ColorTransition();
    ColorTransition(const ColorTransition &) = delete;
    ColorTransition &operator=(const ColorTransition &) = delete;

    QColor value() const;
    void setTarget(const QColor &target);
    void jumpTo(const QColor &value);

private:
    QWidget *owner;
    QPointer<QVariantAnimation> animation;
    QColor current;
    QColor goal;
};

// Transitions keyed per part of one widget, for painters that fade several
// parts on their own (a scroll bar handle, spin buttons, list rows, menu
// items). The paint code passes the state it sees and draws with the value it
// gets back; the transition repaints the widget until it settles. A key seen
// for the first time starts at its target, so nothing fades in on first paint.
class Fader : public QObject
{
    Q_OBJECT

public:
    static qreal level(const QWidget *owner, quintptr key, qreal target);
    static qreal level(const QWidget *owner, quintptr key, bool on)
    {
        return level(owner, key, on ? 1.0 : 0.0);
    }

private:
    explicit Fader(QWidget *owner);

    QWidget *owner;
    std::unordered_map<quintptr, std::unique_ptr<Transition>> transitions;
};

#endif // TRANSITION_H
