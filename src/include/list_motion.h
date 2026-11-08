// list_motion.h

#ifndef LIST_MOTION_H
#define LIST_MOTION_H

#include <QPoint>

#include "drop_shadow_widget.h"

class QWidget;

// Motion for rows of a layout-managed list such as the queue's file bars.
// Rows stay real widgets in their layout; these helpers only animate how
// they get to where the layout puts them.
namespace ListMotion {

// The widget already sits at its new layout position; it eases there from
// `from` instead of jumping, with an ease-in-out glide. `velocity` (px per
// ms) is the speed it already had, as stopSlide() returns it, so a row
// redirected mid-way does not stop and start again.
void slide(QWidget *widget, QPoint from, QPointF velocity = QPointF());
// Stops a running slide, leaves the widget where it is and returns the
// speed it was moving at (zero when it was not sliding).
QPointF stopSlide(QWidget *widget);
// Fades and scales a freshly laid out widget in. Its shadow, if any, is
// drawn by the animation and handed back at the end.
void appear(QWidget *widget, DropShadowWidget *shadow);
// Fades and scales a snapshot of the widget out where it stands. Call right
// before the widget is removed; the snapshot outlives it.
void vanish(QWidget *widget, DropShadowWidget *shadow);

}

#endif // LIST_MOTION_H
