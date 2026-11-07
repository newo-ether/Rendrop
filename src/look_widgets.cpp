// look_widgets.cpp

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include "look_widgets.h"
#include "rounded_surface.h"

namespace Look {

namespace {

// Set on widgets that already carry a RoundedSurface from these helpers, so
// calling them twice does not stack two surfaces.
constexpr char smoothProperty[] = "rendropSmooth";

bool markSmooth(QWidget *widget)
{
    if (!widget || widget->property(smoothProperty).toBool())
    {
        return false;
    }
    widget->setProperty(smoothProperty, true);
    return true;
}

}

QFont pixelFont(int pointSize)
{
    QFont font(QStringLiteral("Pixel Mixed"));
    font.setPointSize(pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QString inputStyleSheet()
{
    return QStringLiteral(
        // The 1px transparent border keeps the box model of the outline
        // RoundedSurface draws.
        "QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  padding: 4px 10px;"
        "  color: white;"
        "  selection-background-color: #5A5A96;"
        // Style-sheeted inputs drop the inherited family; pin it here.
        "  font-family: 'Pixel Mixed';"
        "}"
        "QComboBox { padding-right: 28px; }"
        "QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: #888888; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border-left: none; }"
        "QComboBox::down-arrow { image: url(:/icon/down_arrow.ico); width: 10px; height: 10px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #333842; border: 1px solid #555A66; border-radius: 6px;"
        "  padding: 4px; color: white; selection-background-color: #4B5362; selection-color: white; outline: none;"
        "  font-family: 'Pixel Mixed';"
        "}"
        "QComboBox QAbstractItemView::item:hover, QComboBox QAbstractItemView::item:selected {"
        "  background-color: #4B5362; border-left: 3px solid rgb(110, 110, 180); border-radius: 6px; padding: 2px;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  width: 0px; border: none;"
        "}"
    );
}

void smoothInputs(QWidget *root)
{
    if (!root)
    {
        return;
    }
    const RoundedSurface::Colors field {
        QColor(0x33, 0x38, 0x42), QColor(0x3C, 0x42, 0x4D), QColor(), QColor(0x2E, 0x31, 0x38),
        QColor(65, 65, 85), QColor(110, 110, 180)};
    RoundedSurface::Colors combo = field;
    // Combo boxes never showed a focus outline.
    combo.focusBorder = QColor();

    QList<QWidget *> candidates = root->findChildren<QWidget *>();
    candidates.prepend(root);
    for (QWidget *widget : candidates)
    {
        const bool isCombo = qobject_cast<QComboBox *>(widget) != nullptr;
        const bool isSpin = qobject_cast<QAbstractSpinBox *>(widget) != nullptr;
        bool isEdit = qobject_cast<QLineEdit *>(widget) != nullptr;
        // The editors embedded in spin and combo boxes sit on their parent's surface.
        if (isEdit && (qobject_cast<QAbstractSpinBox *>(widget->parentWidget())
                || qobject_cast<QComboBox *>(widget->parentWidget())))
        {
            isEdit = false;
        }
        if ((isCombo || isSpin || isEdit) && markSmooth(widget))
        {
            RoundedSurface::install(widget, 8, isCombo ? combo : field);
        }
    }
}

void styleAccentButton(QAbstractButton *button)
{
    if (!markSmooth(button))
    {
        return;
    }
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: white; padding: 4px 14px; }"
        "QPushButton:disabled { color: #888888; }"));
    RoundedSurface::install(button, 8, RoundedSurface::Colors {
        QColor(90, 90, 150), QColor(120, 120, 180), QColor(70, 70, 130), QColor(85, 89, 95)});
}

void stylePlainButton(QAbstractButton *button)
{
    if (!markSmooth(button))
    {
        return;
    }
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid transparent; color: #E0E0E0; padding: 4px 12px; }"
        "QPushButton:disabled { color: #777777; }"));
    RoundedSurface::install(button, 8, RoundedSurface::Colors {
        QColor(0x33, 0x38, 0x42), QColor(0x4B, 0x53, 0x62), QColor(0x2B, 0x2F, 0x38), QColor(0x2E, 0x31, 0x38),
        QColor(65, 65, 85)});
}

QString scrollBarStyleSheet()
{
    return QStringLiteral(
        "QScrollBar:vertical { background: transparent; border: none; width: 10px; margin: 4px 2px 4px 2px; }"
        "QScrollBar:horizontal { background: transparent; border: none; height: 10px; margin: 2px 4px 2px 4px; }"
        "QScrollBar::handle:vertical { background: rgba(255, 255, 255, 40); min-height: 28px; border-radius: 3px; }"
        "QScrollBar::handle:horizontal { background: rgba(255, 255, 255, 40); min-width: 28px; border-radius: 3px; }"
        "QScrollBar::handle:hover { background: rgba(255, 255, 255, 70); }"
        "QScrollBar::handle:pressed { background: rgb(110, 110, 180); }"
        "QScrollBar::sub-line, QScrollBar::add-line { background: none; width: 0px; height: 0px; }"
        "QScrollBar::sub-page, QScrollBar::add-page { background: none; }"
    );
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

ToggleSwitch::ToggleSwitch(QWidget *parent):
    QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(sizeHint());
}

QSize ToggleSwitch::sizeHint() const
{
    return {36, 20};
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor trackColor = isChecked() ? Look::accentColor : QColor(0x33, 0x38, 0x42);
    if (!isEnabled())
    {
        trackColor = QColor(0x55, 0x59, 0x5F);
    }
    else if (underMouse())
    {
        trackColor = trackColor.lighter(125);
    }
    painter.setPen(QPen(QColor(65, 65, 85), 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const qreal knob = track.height() - 4;
    const qreal x = isChecked() ? track.right() - 2 - knob : track.left() + 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? QColor(240, 240, 248) : QColor(170, 170, 170));
    painter.drawEllipse(QRectF(x, track.top() + 2, knob, knob));
}

NavButton::NavButton(Icon icon, QWidget *parent):
    QAbstractButton(parent),
    icon(icon)
{
    setCheckable(true);
    setAutoExclusive(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(sizeHint());
}

QSize NavButton::sizeHint() const
{
    return {40, 40};
}

bool NavButton::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
    {
        update();
    }
    return QAbstractButton::event(event);
}

void NavButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect());
    if (isChecked())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x2B, 0x2D, 0x40));
        painter.drawRoundedRect(area, 8, 8);
        painter.setBrush(Look::accentColor.lighter(115));
        painter.drawRoundedRect(QRectF(0, area.center().y() - 8, 3, 16), 1.5, 1.5);
    }
    else if (underMouse())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 14));
        painter.drawRoundedRect(area, 8, 8);
    }

    const QColor glyph = isChecked() ? QColor(0xB4, 0xB8, 0xFF) : QColor(0x8A, 0x8F, 0x9C);
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
    setFont(Look::pixelFont(9));
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
