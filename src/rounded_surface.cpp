// rounded_surface.cpp

#include <algorithm>

#include <QAbstractButton>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

#include "rounded_surface.h"

RoundedSurface::RoundedSurface(QWidget *widget, qreal radius, const Colors &colors):
    QObject(widget),
    target(widget),
    radius(radius),
    colors(colors)
{
    if (colors.hover.isValid())
    {
        // Enter and leave must repaint even without :hover style rules.
        target->setAttribute(Qt::WA_Hover);
    }
    target->installEventFilter(this);
    target->update();
}

RoundedSurface *RoundedSurface::install(QWidget *widget, qreal radius, const Colors &colors)
{
    return widget ? new RoundedSurface(widget, radius, colors) : nullptr;
}

RoundedSurface *RoundedSurface::install(QWidget *widget, qreal radius, const QColor &color)
{
    return install(widget, radius, Colors {color, QColor(), QColor(), QColor()});
}

void RoundedSurface::setColors(const Colors &newColors)
{
    colors = newColors;
    target->update();
}

QColor RoundedSurface::currentColor() const
{
    if (!target->isEnabled())
    {
        return colors.disabled.isValid() ? colors.disabled : colors.normal;
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
        return colors.focusBorder;
    }
    return colors.border;
}

bool RoundedSurface::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == target && colors.focusBorder.isValid()
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut))
    {
        target->update();
    }
    if (watched == target && event->type() == QEvent::Paint)
    {
        const QRectF bounds = QRectF(target->rect());
        const qreal clamped = std::min(radius, std::min(bounds.width(), bounds.height()) / 2.0);
        QPainter painter(target);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(currentColor());
        const QColor border = currentBorder();
        if (border.isValid())
        {
            // A 1px stroke on the half-pixel inset lands on whole device
            // pixels instead of smearing over two.
            painter.setPen(QPen(border, 1.0));
            const QRectF inset = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
            painter.drawRoundedRect(inset, std::max(0.0, clamped - 0.5), std::max(0.0, clamped - 0.5));
        }
        else
        {
            painter.drawRoundedRect(bounds, clamped, clamped);
        }
        // Fall through: the widget paints its own content on top.
    }
    return QObject::eventFilter(watched, event);
}
