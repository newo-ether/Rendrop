// rounded_surface.cpp

#include <algorithm>
#include <cmath>

#include <QAbstractButton>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTransform>
#include <QWidget>

#include "ui/style/rounded_surface.h"

RoundedSurface::RoundedSurface(QWidget *widget, qreal radius, const Colors &colors):
    QObject(widget),
    target(widget),
    radius(radius),
    colors(colors),
    fill(widget),
    outline(widget),
    label(widget)
{
    // Enter and leave must reach the filter even without :hover rules.
    target->setAttribute(Qt::WA_Hover);
    target->installEventFilter(this);
    if (auto *button = qobject_cast<QAbstractButton *>(target))
    {
        // isDown() and isChecked() change after the events the filter sees.
        QObject::connect(button, &QAbstractButton::pressed, this, [this]() { refresh(); });
        QObject::connect(button, &QAbstractButton::released, this, [this]() { refresh(); });
        QObject::connect(button, &QAbstractButton::toggled, this, [this]() { refresh(); });
    }
    fill.jumpTo(currentColor());
    outline.jumpTo(currentBorder());
    label.jumpTo(currentText());
    // The scheme cross-fade covers the switch; the surface just takes the
    // new colors.
    QObject::connect(Theme::notifier(), &Theme::Notifier::changed, this, [this]() {
        setColors(this->colors);
        target->update();
    });
}

RoundedSurface *RoundedSurface::install(QWidget *widget, qreal radius, const Colors &colors)
{
    if (!widget)
    {
        return nullptr;
    }
    // Installing again restyles the existing surface instead of stacking one.
    if (RoundedSurface *existing = find(widget))
    {
        existing->radius = radius;
        existing->setColors(colors);
        return existing;
    }
    return new RoundedSurface(widget, radius, colors);
}

RoundedSurface *RoundedSurface::install(QWidget *widget, qreal radius, const Theme::Tone &color)
{
    return install(widget, radius, Colors {color, {}, {}, {}});
}

RoundedSurface *RoundedSurface::find(const QWidget *widget)
{
    return widget ? widget->findChild<RoundedSurface *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

void RoundedSurface::setColors(const Colors &newColors)
{
    if (&newColors != &colors)
    {
        colors = newColors;
    }
    fill.jumpTo(currentColor());
    outline.jumpTo(currentBorder());
    label.jumpTo(currentText());
}

void RoundedSurface::refresh()
{
    fill.setTarget(currentColor());
    outline.setTarget(currentBorder());
    label.setTarget(currentText());
}

QColor RoundedSurface::textColor() const
{
    return label.value();
}

QColor RoundedSurface::currentColor() const
{
    if (!target->isEnabled())
    {
        return colors.disabled.isValid() ? colors.disabled.color() : colors.normal.color();
    }
    const auto *button = qobject_cast<const QAbstractButton *>(target);
    if (button && button->isDown() && colors.pressed.isValid())
    {
        return colors.pressed;
    }
    if (button && button->isChecked() && colors.checked.isValid())
    {
        return colors.checked;
    }
    if (target->underMouse() && colors.hover.isValid())
    {
        return colors.hover;
    }
    return colors.normal;
}

QColor RoundedSurface::currentBorder() const
{
    if (colors.focusBorder.isValid() && target->isEnabled() && target->hasFocus())
    {
        return colors.focusBorder.color();
    }
    return colors.border.color();
}

QColor RoundedSurface::currentText() const
{
    const QPalette &palette = target->palette();
    if (!target->isEnabled())
    {
        return palette.color(QPalette::Disabled, QPalette::ButtonText);
    }
    const QColor text = colors.text.isValid() ? colors.text.color() : palette.color(QPalette::Active, QPalette::ButtonText);
    const auto *button = qobject_cast<const QAbstractButton *>(target);
    if (button && button->isChecked() && colors.checkedText.isValid())
    {
        return colors.checkedText;
    }
    if (target->underMouse() && colors.hoverText.isValid())
    {
        return colors.hoverText;
    }
    return text;
}

void RoundedSurface::paintFrame(QPainter &painter, const QRectF &rect, qreal radius,
    const QColor &fill, const QColor &border)
{
    const bool hasFill = fill.isValid() && fill.alpha() > 0;
    const bool hasBorder = border.isValid() && border.alpha() > 0;
    if (!hasFill && !hasBorder)
    {
        return;
    }

    // Logical to device pixels. For widgets this already carries the
    // high-DPI scale and the widget's offset in the backing store.
    const QTransform toDevice = painter.deviceTransform();
    const qreal scale = std::abs(toDevice.m11()) > 0.0 ? std::abs(toDevice.m11()) : 1.0;
    bool invertible = false;
    const QTransform toLogical = toDevice.inverted(&invertible);
    if (!invertible)
    {
        return;
    }

    // Snap the edges to whole device pixels, then work back in logical units
    // so the painter's own transform is left alone.
    const QRectF device = toDevice.mapRect(rect);
    const QRectF snapped(QPointF(std::round(device.left()), std::round(device.top())),
        QPointF(std::round(device.right()), std::round(device.bottom())));
    const QRectF outer = toLogical.mapRect(snapped);
    const qreal outerRadius = std::min(radius, std::min(outer.width(), outer.height()) / 2.0);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath outerPath;
    outerPath.addRoundedRect(outer, outerRadius, outerRadius);
    if (!hasBorder)
    {
        painter.fillPath(outerPath, fill);
        painter.restore();
        return;
    }

    // One device pixel at 100% and 125%, two at 200%.
    const qreal width = std::max(1.0, std::round(scale)) / scale;
    const QRectF inner = outer.adjusted(width, width, -width, -width);
    const qreal innerRadius = std::max(0.0, outerRadius - width);
    QPainterPath innerPath;
    innerPath.addRoundedRect(inner, innerRadius, innerRadius);
    if (hasFill && fill.alpha() == 255)
    {
        // Opaque fill over a border-colored base: the inner edge blends
        // border into fill directly, with no background seam between them.
        painter.fillPath(outerPath, border);
        painter.fillPath(innerPath, fill);
    }
    else
    {
        // A translucent fill must not show the border through it, so the
        // border is a ring.
        if (hasFill)
        {
            painter.fillPath(innerPath, fill);
        }
        QPainterPath ring = outerPath;
        ring.addPath(innerPath);
        ring.setFillRule(Qt::OddEvenFill);
        painter.fillPath(ring, border);
    }
    painter.restore();
}

bool RoundedSurface::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != target)
    {
        return QObject::eventFilter(watched, event);
    }
    switch (event->type())
    {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::EnabledChange:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        refresh();
        break;
    case QEvent::PaletteChange:
        // A new palette is a restyle, not a state change.
        label.jumpTo(currentText());
        break;
    case QEvent::Paint:
    {
        QPainter painter(target);
        paintFrame(painter, QRectF(target->rect()), radius, fill.value(), outline.value());
        // Fall through: the widget paints its own content on top.
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
