// rounded_surface.cpp

#include <algorithm>

#include <QAbstractButton>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
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
        const QColor color = fill.value();
        const QColor border = outline.value();
        const QRectF bounds = QRectF(target->rect());
        const qreal clamped = std::min(radius, std::min(bounds.width(), bounds.height()) / 2.0);
        QPainter painter(target);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color.isValid() ? QBrush(color) : QBrush(Qt::NoBrush));
        if (border.isValid() && border.alpha() > 0)
        {
            // A 1px stroke on the half-pixel inset lands on whole device
            // pixels instead of smearing over two.
            painter.setPen(QPen(border, 1.0));
            const QRectF inset = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
            painter.drawRoundedRect(inset, std::max(0.0, clamped - 0.5), std::max(0.0, clamped - 0.5));
        }
        else if (color.isValid() && color.alpha() > 0)
        {
            painter.drawRoundedRect(bounds, clamped, clamped);
        }
        // Fall through: the widget paints its own content on top.
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
