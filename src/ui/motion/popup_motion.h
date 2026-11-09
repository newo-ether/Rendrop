// popup_motion.h

#ifndef POPUP_MOTION_H
#define POPUP_MOTION_H

#include <QColor>
#include <QRect>

class QDialog;
class QWidget;

// Open and close motion for menus and combo popups, in place of the system
// roll and fade. A popup opens by unrolling from the side of what opened it
// while it scales up and fades in; it closes with a short fade and shrink.
//
// The popup itself stays live and takes input the whole time. A snapshot of
// it plays the motion in a click-through window on top, and the popup only
// becomes visible when the snapshot lands.
namespace PopupMotion {
// Turns off Qt's own menu and combo effects. Call once after QApplication
// exists.
void disableSystemEffects();

// Animates a menu or a combo popup on every show and hide. border and radius
// match the outline the popup paints, so the edge stays drawn while the
// popup unrolls. Call once per popup.
void attach(QWidget *popup, const QColor &border, qreal radius);
// The same for a dialog shown as a popover (a Qt::Popup window), which
// unrolls from globalAnchor, the control that opened it.
void attach(QDialog *popover, const QColor &border, qreal radius, const QRect &globalAnchor);
}

#endif // POPUP_MOTION_H
