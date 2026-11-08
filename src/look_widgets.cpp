// look_widgets.cpp

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <QScrollArea>

#include "look_widgets.h"

namespace Look {

namespace {
QString &pixelFamilyStorage()
{
    static QString family = QStringLiteral("Mi Outfit");
    return family;
}
}

void selectFontEngine()
{
#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
    }
#endif
}

void applyTextRendering(QFont &font)
{
    static const bool subpixel = qEnvironmentVariable("RENDROP_TEXT_AA") == QLatin1String("subpixel");
    static const QFont::HintingPreference hinting = []() {
        const QString value = qEnvironmentVariable("RENDROP_TEXT_HINT");
        if (value == QLatin1String("default"))
        {
            return QFont::PreferDefaultHinting;
        }
        if (value == QLatin1String("none"))
        {
            return QFont::PreferNoHinting;
        }
        if (value == QLatin1String("full"))
        {
            return QFont::PreferFullHinting;
        }
        return QFont::PreferVerticalHinting;
    }();
    font.setStyleStrategy(subpixel
        ? QFont::PreferAntialias
        : QFont::StyleStrategy(QFont::PreferAntialias | QFont::NoSubpixelAntialias));
    font.setHintingPreference(hinting);
}

void installTextRendering()
{
    QFont font = QApplication::font();
    applyTextRendering(font);
    QApplication::setFont(font);
}

QString pixelFamily()
{
    return pixelFamilyStorage();
}

void setPixelFamily(const QString &family)
{
    pixelFamilyStorage() = family;
}

QFont pixelFont(int pointSize, QFont::Weight weight)
{
    QFont font(pixelFamilyStorage());
    font.setPointSize(pointSize);
    font.setWeight(weight);
    applyTextRendering(font);
    return font;
}

QFont monoFont(int pointSize, QFont::Weight weight)
{
    QFont font(QStringLiteral("JetBrains Mono"));
    // JetBrains Mono has no CJK; Mi Outfit (Outfit + MiSans) carries it, so
    // Chinese in mono text matches the rest of the UI instead of the system
    // fallback.
    font.setFamilies({QStringLiteral("JetBrains Mono"), pixelFamilyStorage()});
    font.setPointSize(pointSize);
    font.setWeight(weight);
    applyTextRendering(font);
    return font;
}

RoundedSurface::Colors plainButtonColors()
{
    RoundedSurface::Colors colors {
        QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(0x2B, 0x2F, 0x38), QColor(0x2E, 0x31, 0x38),
        QColor(65, 65, 85)};
    colors.text = textColor;
    return colors;
}

RoundedSurface::Colors accentButtonColors()
{
    RoundedSurface::Colors colors {
        QColor(90, 90, 150), QColor(120, 120, 180), QColor(70, 70, 130), QColor(85, 89, 95)};
    colors.text = Qt::white;
    return colors;
}

RoundedSurface::Colors fieldColors()
{
    return RoundedSurface::Colors {
        QColor(0x33, 0x38, 0x42), QColor(0x3C, 0x42, 0x4D), QColor(), QColor(0x2E, 0x31, 0x38),
        QColor(65, 65, 85), accentColor};
}

RoundedSurface::Colors comboColors()
{
    RoundedSurface::Colors colors = fieldColors();
    colors.focusBorder = QColor();
    return colors;
}

void styleAccentButton(QAbstractButton *button)
{
    RoundedSurface::install(button, 8, accentButtonColors());
    setPadding(button, QMargins(14, 4, 14, 4));
}

void stylePlainButton(QAbstractButton *button)
{
    RoundedSurface::install(button, 8, plainButtonColors());
    setPadding(button, QMargins(12, 4, 12, 4));
}

void setPadding(QWidget *button, const QMargins &padding, Qt::Alignment align)
{
    button->setProperty(paddingProperty, QVariant::fromValue(padding));
    button->setProperty(alignProperty, static_cast<int>(align));
    button->updateGeometry();
    button->update();
}

void setTextColor(QWidget *widget, const QColor &color, const QColor &disabled)
{
    QPalette palette = widget->palette();
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
    {
        palette.setColor(QPalette::Active, role, color);
        palette.setColor(QPalette::Inactive, role, color);
        if (disabled.isValid())
        {
            palette.setColor(QPalette::Disabled, role, disabled);
        }
    }
    widget->setPalette(palette);
}

void clearScrollArea(QAbstractScrollArea *area)
{
    area->setFrameShape(QFrame::NoFrame);
    area->setAutoFillBackground(false);
    area->viewport()->setAutoFillBackground(false);
    if (auto *scroll = qobject_cast<QScrollArea *>(area); scroll && scroll->widget())
    {
        scroll->widget()->setAutoFillBackground(false);
    }
}

void styleDialog(QWidget *dialog)
{
    QPalette palette = dialog->palette();
    palette.setColor(QPalette::Window, QColor(43, 45, 48));
    dialog->setPalette(palette);
    dialog->setAutoFillBackground(true);
    setTextColor(dialog, Qt::white);
    QFont font = pixelFont(9);
    font.setPixelSize(12);
    dialog->setFont(font);
}

QIcon terminalIcon(const QColor &color)
{
    // Drawn at 2x so it stays crisp on scaled displays.
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(3, 5, 26, 22), 5, 5);
    painter.drawPolyline(QPolygonF({QPointF(9, 12), QPointF(13.5, 16), QPointF(9, 20)}));
    painter.drawLine(QPointF(16.5, 20.5), QPointF(23, 20.5));
    painter.end();
    pixmap.setDevicePixelRatio(2.0);
    return QIcon(pixmap);
}

}

namespace {

bool isHoverEvent(const QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::EnabledChange:
        return true;
    default:
        return false;
    }
}

}

ToggleSwitch::ToggleSwitch(QWidget *parent):
    QAbstractButton(parent),
    // The knob travels: ease in and out over a little longer than a fade.
    position(this, 0.0, 240, Look::easeInOut()),
    hover(this)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(sizeHint());
    // A switch flipped before it is shown jumps; Transition skips hidden owners.
    QObject::connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        position.setTarget(checked ? 1.0 : 0.0);
    });
}

QSize ToggleSwitch::sizeHint() const
{
    return {36, 20};
}

bool ToggleSwitch::event(QEvent *event)
{
    if (isHoverEvent(event))
    {
        hover.setTarget(underMouse() && isEnabled() ? 1.0 : 0.0);
        update();
    }
    return QAbstractButton::event(event);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(1, 1, -1, -1);
    // setChecked() under a QSignalBlocker sends no toggled(); it still repaints.
    position.setTarget(isChecked() ? 1.0 : 0.0);
    const qreal on = position.value();
    QColor trackColor = Look::mix(QColor(0x33, 0x38, 0x42), Look::accentColor, on);
    trackColor = Look::mix(trackColor, trackColor.lighter(125), hover.value());
    const qreal disabled = Fader::level(this, 0, !isEnabled());
    trackColor = Look::mix(trackColor, QColor(0x55, 0x59, 0x5F), disabled);
    painter.setPen(QPen(QColor(65, 65, 85), 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const qreal knob = track.height() - 4;
    // Held down, the knob stretches towards where it will go.
    const qreal stretch = 4.0 * Fader::level(this, 1, isDown());
    const qreal left = track.left() + 2;
    const qreal travel = track.right() - 2 - knob - stretch - left;
    const qreal x = left + travel * on;
    painter.setPen(Qt::NoPen);
    painter.setBrush(Look::mix(QColor(240, 240, 248), QColor(170, 170, 170), disabled));
    const QRectF knobRect(x, track.top() + 2, knob + stretch, knob);
    painter.drawRoundedRect(knobRect, knob / 2, knob / 2);
}

NavButton::NavButton(Icon icon, QWidget *parent):
    QAbstractButton(parent),
    icon(icon),
    hover(this),
    selected(this)
{
    setCheckable(true);
    setAutoExclusive(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(sizeHint());
    QObject::connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        selected.setTarget(checked ? 1.0 : 0.0);
    });
}

QSize NavButton::sizeHint() const
{
    return {40, 40};
}

bool NavButton::event(QEvent *event)
{
    if (isHoverEvent(event))
    {
        hover.setTarget(underMouse() ? 1.0 : 0.0);
    }
    return QAbstractButton::event(event);
}

void NavButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect());
    selected.setTarget(isChecked() ? 1.0 : 0.0);
    const qreal chosen = selected.value();
    // Hover only tints buttons that are not (or no longer) selected.
    const qreal hovered = hover.value() * (1.0 - chosen);
    painter.setPen(Qt::NoPen);
    if (hovered > 0.0)
    {
        painter.setBrush(QColor(255, 255, 255, qRound(14 * hovered)));
        painter.drawRoundedRect(area, 8, 8);
    }
    if (chosen > 0.0)
    {
        QColor fill(0x2B, 0x2D, 0x40);
        fill.setAlphaF(static_cast<float>(chosen));
        painter.setBrush(fill);
        painter.drawRoundedRect(area, 8, 8);
        // The mark grows from the middle as the button becomes selected.
        QColor mark = Look::accentColor.lighter(115);
        mark.setAlphaF(static_cast<float>(chosen));
        painter.setBrush(mark);
        const qreal half = 8.0 * chosen;
        painter.drawRoundedRect(QRectF(0, area.center().y() - half, 3, half * 2), 1.5, 1.5);
    }

    const QColor rest = Look::mix(QColor(0x8A, 0x8F, 0x9C), QColor(0xC4, 0xC8, 0xD4), hover.value());
    const QColor glyph = Look::mix(rest, QColor(0xB4, 0xB8, 0xFF), chosen);
    QPen pen(glyph, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF c = area.center();
    switch (icon)
    {
    case Icon::Queue:
        // Three rows of a list.
        for (int row = -1; row <= 1; ++row)
        {
            const qreal y = c.y() + row * 5.0;
            painter.drawPoint(QPointF(c.x() - 7, y));
            painter.drawLine(QPointF(c.x() - 3, y), QPointF(c.x() + 8, y));
        }
        break;
    case Icon::Nodes:
    {
        // Two stacked rack units, each with a status light and a vent slot.
        constexpr qreal unitWidth = 17.0;
        constexpr qreal unitHeight = 7.0;
        constexpr qreal spacing = 2.0;
        for (int unit = 0; unit < 2; ++unit)
        {
            const qreal top = c.y() - unitHeight - spacing / 2.0 + unit * (unitHeight + spacing);
            const QRectF box(c.x() - unitWidth / 2.0, top, unitWidth, unitHeight);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box, 2.0, 2.0);
            const qreal mid = box.center().y();
            painter.drawLine(QPointF(box.left() + 4.0, mid), QPointF(box.left() + 8.0, mid));
            painter.setPen(Qt::NoPen);
            painter.setBrush(glyph);
            painter.drawEllipse(QPointF(box.right() - 3.6, mid), 1.2, 1.2);
            painter.setPen(pen);
        }
        break;
    }
    case Icon::Settings:
    {
        // Gear: eight teeth around a ring.
        QPainterPath gear;
        constexpr int teeth = 8;
        constexpr qreal outer = 8.5;
        constexpr qreal inner = 6.2;
        for (int index = 0; index < teeth * 4; ++index)
        {
            const qreal angle = (index / static_cast<qreal>(teeth * 4)) * 2.0 * std::numbers::pi;
            const int phase = index % 4;
            const qreal radius = phase == 1 || phase == 2 ? outer : inner;
            const QPointF point(c.x() + radius * std::cos(angle), c.y() + radius * std::sin(angle));
            index == 0 ? gear.moveTo(point) : gear.lineTo(point);
        }
        gear.closeSubpath();
        painter.drawPath(gear);
        painter.drawEllipse(c, 2.6, 2.6);
        break;
    }
    }
}

LinearProgress::LinearProgress(QWidget *parent):
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(6);
}

void LinearProgress::setValue(double percent)
{
    const double clamped = std::clamp(percent, 0.0, 100.0);
    if (clamped == value)
    {
        return;
    }
    value = clamped;
    update();
}

QSize LinearProgress::sizeHint() const
{
    return {200, 6};
}

void LinearProgress::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect());
    const qreal radius = track.height() / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x33, 0x35, 0x3C));
    painter.drawRoundedRect(track, radius, radius);
    if (value > 0.0)
    {
        QRectF fill = track;
        fill.setWidth(std::max(track.height(), track.width() * value / 100.0));
        painter.setBrush(Look::accentColor.lighter(110));
        painter.drawRoundedRect(fill, radius, radius);
    }
}

StatusChip::StatusChip(QWidget *parent):
    QWidget(parent),
    dot(Look::dimTextColor)
{
    setFont(Look::pixelFont(9, QFont::Medium));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // Keep the pill short inside the 40 px title bar instead of filling it.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StatusChip::setStatus(const QColor &newDot, const QString &newText)
{
    if (dot == newDot && text == newText)
    {
        return;
    }
    dot = newDot;
    text = newText;
    updateGeometry();
    update();
}

QSize StatusChip::sizeHint() const
{
    const QFontMetrics metrics(font());
    return {metrics.horizontalAdvance(text) + 32, 24};
}

void StatusChip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x2B, 0x2D, 0x33));
    painter.drawRoundedRect(area, area.height() / 2, area.height() / 2);
    painter.setBrush(dot);
    painter.drawEllipse(QRectF(11, area.center().y() - 3.5, 7, 7));
    painter.setPen(QColor(0xA8, 0xAC, 0xB8));
    painter.drawText(rect().adjusted(24, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}
