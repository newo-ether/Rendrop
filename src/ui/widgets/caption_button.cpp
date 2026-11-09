// caption_button.cpp

#include <algorithm>

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

#include "ui/widgets/caption_button.h"
#include "ui/motion/transition.h"
#include "ui/style/theme.h"

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

    // Hover and press fade like the native caption buttons.
    const qreal hovered = Fader::level(this, 0, underMouse() && isEnabled());
    const qreal pressed = Fader::level(this, 1, isDown());
    const bool close = kind == Kind::Close;
    const QColor hoverFill = close ? Theme::closeHover.color() : Theme::veil(22);
    const QColor pressFill = close ? Theme::closePressed.color() : Theme::veil(12);
    QColor background = Look::mix(QColor(), hoverFill, hovered);
    background = Look::mix(background, pressFill, pressed);
    // The close glyph turns white on its red fill in both schemes.
    QColor glyph = close
        ? Look::mix(Theme::captionGlyph, Qt::white, std::max(hovered, pressed))
        : Theme::captionGlyph.color();
    if (!isEnabled())
    {
        glyph = Theme::textDisabledLight;
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
