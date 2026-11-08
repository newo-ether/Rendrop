// smooth_scroll.h

#ifndef SMOOTH_SCROLL_H
#define SMOOTH_SCROLL_H

#include <QObject>
#include <QPointer>

#include "transition.h"

class QAbstractScrollArea;
class QScrollBar;
class QVariantAnimation;

// Eases a scroll area's vertical position to where a mouse wheel notch or a
// jump asks for, like a browser does. Touchpads and other pixel-precise
// devices already scroll smoothly and keep Qt's direct scrolling.
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
    void animateTo(int value, int durationMs);
    void stop();

    QAbstractScrollArea *area;
    QScrollBar *bar;
    QPointer<QVariantAnimation> animation;
    // The running move; its progress is the animation's 0-1 value.
    Look::Move move;
    int moveMs = 0;
    int target = 0;
    // The value this object last set; anything else moved the bar.
    int written = 0;
    bool jumping = false;
};

#endif // SMOOTH_SCROLL_H
