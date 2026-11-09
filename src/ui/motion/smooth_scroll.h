// smooth_scroll.h

#ifndef SMOOTH_SCROLL_H
#define SMOOTH_SCROLL_H

#include <QObject>
#include <QPointer>

class QAbstractAnimation;
class QAbstractScrollArea;
class QScrollBar;

// Smooth vertical scrolling for a scroll area, like a browser does.
//
// Wheel notches move a target, and the view chases it every frame, closing
// a fixed share of the remaining distance per unit of time (exponential
// decay). It starts moving at full speed on the frame after the notch and
// slows down only as it arrives; notches in quick succession push the
// target further and the view keeps its momentum.
//
// Jumps to a section (scrollTo) chase their target the same way.
// Touchpads and other pixel-precise devices already scroll smoothly and keep
// Qt's direct scrolling.
class SmoothScroll : public QObject
{
    Q_OBJECT

public:
    // Owned by the area; once per area.
    static void install(QAbstractScrollArea *area);
    // Eases to value, or jumps when the area has no SmoothScroll.
    static void scrollTo(QAbstractScrollArea *area, int value);
    // True while a scrollTo() is still on its way and nothing took over.
    static bool isJumping(const QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit SmoothScroll(QAbstractScrollArea *area);
    static SmoothScroll *of(const QAbstractScrollArea *area);
    // Moves the wheel target; starts the chase when it is not running.
    void chaseTo(qreal value);
    void chaseStep(int elapsedMs);
    void write(qreal value);
    void stop();

    QAbstractScrollArea *area;
    QScrollBar *bar;
    // The chase is heading for a scrollTo() target, not wheel notches.
    bool jumping = false;
    // The chase. position keeps the fraction the bar's integer value drops.
    QPointer<QAbstractAnimation> chase;
    qreal position = 0.0;
    qreal goal = 0.0;
    // Time constant of the running chase: wheel or section jump.
    qreal tauMs = 45.0;
    int chaseMs = 0;
    // The value this object last set; anything else moved the bar.
    int written = 0;
};

#endif // SMOOTH_SCROLL_H
