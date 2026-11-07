// rounded_surface.h

#ifndef ROUNDED_SURFACE_H
#define ROUNDED_SURFACE_H

#include <QColor>
#include <QObject>

class QWidget;

// Paints an antialiased rounded background under a widget. Style sheets clip
// border-radius backgrounds without antialiasing, which leaves visibly
// stepped corners at large radii; widgets using this keep their style sheet
// for text color, font and padding but set "background: transparent".
//
// For buttons the hover, pressed, checked and disabled colors follow the
// button state; focusBorder replaces the outline while the widget has focus.
class RoundedSurface : public QObject
{
    Q_OBJECT

public:
    struct Colors
    {
        QColor normal;
        // Invalid colors fall back to normal.
        QColor hover;
        QColor pressed;
        QColor disabled;
        // Optional antialiased 1px outline; invalid means none.
        QColor border = QColor();
        // Invalid means the outline does not change on focus.
        QColor focusBorder = QColor();
        // Checkable buttons; wins over hover. Invalid means no checked look.
        QColor checked = QColor();
    };

    // The surface is owned by the widget.
    static RoundedSurface *install(QWidget *widget, qreal radius, const Colors &colors);
    static RoundedSurface *install(QWidget *widget, qreal radius, const QColor &color);

    void setColors(const Colors &colors);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    RoundedSurface(QWidget *widget, qreal radius, const Colors &colors);
    QColor currentColor() const;
    QColor currentBorder() const;

    QWidget *target;
    qreal radius;
    Colors colors;
};

#endif // ROUNDED_SURFACE_H
