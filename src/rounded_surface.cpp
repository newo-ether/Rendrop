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
    if (target->underMouse() && colors.hover.isValid())
    {
        return colors.hover;
    }
    return colors.normal;
}

bool RoundedSurface::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == target && event->type() == QEvent::Paint)
    {
        const QRectF bounds = QRectF(target->rect());
        const qreal clamped = std::min(radius, std::min(bounds.width(), bounds.height()) / 2.0);
        QPainter painter(target);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(currentColor());
        painter.drawRoundedRect(bounds, clamped, clamped);
        // Fall through: the widget paints its own content on top.
    }
    return QObject::eventFilter(watched, event);
}
