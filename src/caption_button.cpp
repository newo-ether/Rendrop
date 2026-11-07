// caption_button.cpp

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

#include "caption_button.h"

namespace
{
// Windows 11 caption button metrics: full title bar height, square hover
// fill, flush with the window's top-right corner.
constexpr int ButtonWidth = 46;
constexpr int ButtonHeight = 40;
constexpr qreal GlyphSize = 10.0;
}

CaptionButton::CaptionButton(Kind kind, QWidget *parent):
    QAbstractButton(parent),
    kind(kind)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(ButtonWidth, ButtonHeight);
}

QSize CaptionButton::sizeHint() const
{
    return QSize(ButtonWidth, ButtonHeight);
}

void CaptionButton::setMaximized(bool value)
{
    if (maximized == value)
    {
        return;
    }
    maximized = value;
    update();
}

bool CaptionButton::event(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
        update();
        break;
    default:
        break;
    }
    return QAbstractButton::event(event);
}

void CaptionButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool hovered = underMouse() && isEnabled();
    const bool pressed = isDown();
    QColor background(Qt::transparent);
    QColor glyph(0xE0, 0xE0, 0xE0);
    if (kind == Kind::Close)
    {
        if (pressed)
        {
            background = QColor(0xA3, 0x23, 0x17);
            glyph = Qt::white;
        }
        else if (hovered)
        {
            background = QColor(0xC4, 0x2B, 0x1C);
            glyph = Qt::white;
        }
    }
    else if (pressed)
    {
        background = QColor(255, 255, 255, 12);
    }
    else if (hovered)
    {
        background = QColor(255, 255, 255, 22);
    }
    if (!isEnabled())
    {
        glyph = QColor(0xAA, 0xAA, 0xAA);
    }

    if (background.alpha() > 0)
    {
        painter.fillRect(rect(), background);
    }

    QPen pen(glyph, 1.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPointF center = QRectF(rect()).center();
    const qreal half = GlyphSize / 2.0;
    const QRectF box(center.x() - half, center.y() - half, GlyphSize, GlyphSize);
    switch (kind)
    {
    case Kind::Minimize:
        painter.drawLine(QPointF(box.left(), center.y()), QPointF(box.right(), center.y()));
        break;
    case Kind::Maximize:
        if (maximized)
        {
            // Restore: a front window with the back one peeking out top-right.
            constexpr qreal offset = 2.5;
            const QRectF front(box.left(), box.top() + offset, GlyphSize - offset, GlyphSize - offset);
            painter.drawRoundedRect(front, 1.5, 1.5);
            QPainterPath back;
            back.moveTo(box.left() + offset, box.top() + offset);
            back.lineTo(box.left() + offset, box.top() + 1.5);
            back.quadTo(box.left() + offset, box.top(), box.left() + offset + 1.5, box.top());
            back.lineTo(box.right() - 1.5, box.top());
            back.quadTo(box.right(), box.top(), box.right(), box.top() + 1.5);
            back.lineTo(box.right(), box.bottom() - offset - 1.5);
            back.quadTo(box.right(), box.bottom() - offset, box.right() - 1.5, box.bottom() - offset);
            back.lineTo(front.right(), box.bottom() - offset);
            painter.drawPath(back);
        }
        else
        {
            painter.drawRoundedRect(box, 1.5, 1.5);
        }
        break;
    case Kind::Close:
        painter.drawLine(box.topLeft(), box.bottomRight());
        painter.drawLine(box.topRight(), box.bottomLeft());
        break;
    }
}
