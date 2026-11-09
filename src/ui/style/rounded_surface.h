// rounded_surface.h

#ifndef ROUNDED_SURFACE_H
#define ROUNDED_SURFACE_H

#include <QColor>
#include <QObject>

#include "ui/motion/transition.h"
#include "ui/style/theme.h"

class QPainter;
class QRectF;
class QWidget;

// Paints an antialiased rounded background under a widget, and for buttons
// the text color RendropStyle draws the label with.
//
// For buttons the hover, pressed, checked and disabled colors follow the
// button state; focusBorder replaces the outline while the widget has focus.
// State changes fade over Look::transitionMs. The colors are Theme tones and
// follow a scheme switch at once.
class RoundedSurface : public QObject
{
    Q_OBJECT

public:
    struct Colors
    {
        Theme::Tone normal;
        // Invalid colors fall back to normal.
        Theme::Tone hover;
        Theme::Tone pressed;
        Theme::Tone disabled;
        // Optional antialiased 1px outline; invalid means none.
        Theme::Tone border = Theme::Tone();
        // Invalid means the outline does not change on focus.
        Theme::Tone focusBorder = Theme::Tone();
        // Checkable buttons; wins over hover. Invalid means no checked look.
        Theme::Tone checked = Theme::Tone();
        // Button label colors. Invalid text means the palette's ButtonText;
        // invalid hover and checked text fall back to text. Disabled buttons
        // always use the palette's disabled ButtonText.
        Theme::Tone text = Theme::Tone();
        Theme::Tone hoverText = Theme::Tone();
        Theme::Tone checkedText = Theme::Tone();
    };

    // The surface is owned by the widget. A widget has at most one: a second
    // install restyles it.
    static RoundedSurface *install(QWidget *widget, qreal radius, const Colors &colors);
    static RoundedSurface *install(QWidget *widget, qreal radius, const Theme::Tone &color);
    static RoundedSurface *find(const QWidget *widget);

    // Takes effect at once, without a fade.
    void setColors(const Colors &colors);
    // Re-reads the widget state and fades to the matching colors.
    void refresh();
    // Current, possibly mid-fade, label color.
    QColor textColor() const;

    // Fills rect as a rounded rectangle with an optional outline, snapped to
    // device pixels. The outline is a whole number of device pixels wide, so
    // under fractional scaling (125%, 150%) it stays one crisp pixel instead
    // of smearing a 1.25px stroke across uneven pixel rows. Invalid or fully
    // transparent colors are skipped. The painter must not be rotated.
    static void paintFrame(QPainter &painter, const QRectF &rect, qreal radius,
        const QColor &fill, const QColor &border);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    RoundedSurface(QWidget *widget, qreal radius, const Colors &colors);
    QColor currentColor() const;
    QColor currentBorder() const;
    QColor currentText() const;

    QWidget *target;
    qreal radius;
    Colors colors;
    ColorTransition fill;
    ColorTransition outline;
    ColorTransition label;
};

#endif // ROUNDED_SURFACE_H
