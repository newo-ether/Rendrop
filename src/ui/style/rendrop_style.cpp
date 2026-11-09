// rendrop_style.cpp

#include <algorithm>

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QStyledItemDelegate>

#include "ui/style/look_widgets.h"
#include "ui/motion/popup_motion.h"
#include "ui/style/rendrop_style.h"
#include "ui/style/rounded_surface.h"
#include "ui/motion/transition.h"

namespace {

// Set once a widget got what polish() adds, so a repeated polish (palette or
// font changes) does not add it again.
constexpr char polishedProperty[] = "rendropPolished";

constexpr int comboArrowWidth = 24;
constexpr int spinButtonWidth = 18;
const Theme::Tone &popupColor = Theme::popup;
const Theme::Tone &popupBorder = Theme::popupBorder;
const Theme::Tone &rowHover = Theme::menuRowHover;

bool markOnce(QWidget *widget)
{
    if (widget->property(polishedProperty).toBool())
    {
        return false;
    }
    widget->setProperty(polishedProperty, true);
    return true;
}

bool isComboPopup(const QWidget *widget)
{
    return widget && widget->inherits("QComboBoxPrivateContainer");
}

QMargins buttonPadding(const QWidget *widget)
{
    const QVariant value = widget ? widget->property(Look::paddingProperty) : QVariant();
    return value.isValid() ? value.value<QMargins>() : QMargins(6, 3, 6, 3);
}

Qt::Alignment buttonAlignment(const QWidget *widget)
{
    const QVariant value = widget ? widget->property(Look::alignProperty) : QVariant();
    return value.isValid() ? Qt::Alignment(value.toInt()) : Qt::AlignCenter;
}

QColor groupColor(const QStyleOption *option, QPalette::ColorRole role)
{
    const bool enabled = option->state & QStyle::State_Enabled;
    return option->palette.color(enabled ? QPalette::Active : QPalette::Disabled, role);
}

// Open chevron pointing down (or up), centered in area.
void drawChevron(QPainter *painter, const QRectF &area, qreal width, const QColor &color, bool up)
{
    const qreal height = width / 2.0;
    const QPointF center = area.center();
    const qreal tip = up ? -height / 2.0 : height / 2.0;
    QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    const QPointF points[] = {
        QPointF(center.x() - width / 2.0, center.y() - tip),
        QPointF(center.x(), center.y() + tip),
        QPointF(center.x() + width / 2.0, center.y() - tip)
    };
    painter->drawPolyline(points, 3);
    painter->restore();
}

// Rows of the combo popup: a rounded highlight with an accent mark that fades
// in under the pointer, instead of a flat selection band.
class PopupItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
        const QWidget *surface = view ? view->viewport() : option.widget;
        const bool on = (item.state & (QStyle::State_MouseOver | QStyle::State_Selected))
            && (item.state & QStyle::State_Enabled);
        const qreal level = Fader::level(surface, static_cast<quintptr>(index.row()), on);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF row = QRectF(item.rect).adjusted(0, 1, 0, -1);
        if (level > 0.0)
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Look::mix(QColor(), rowHover, level));
            painter->drawRoundedRect(row, 6, 6);
            QColor mark = Look::accentColor.color();
            mark.setAlphaF(static_cast<float>(level));
            painter->setBrush(mark);
            const qreal half = (row.height() / 2.0 - 5.0) * level;
            painter->drawRoundedRect(QRectF(row.left() + 1, row.center().y() - half, 3, half * 2), 1.5, 1.5);
        }
        const bool enabled = item.state & QStyle::State_Enabled;
        painter->setPen(enabled ? Theme::textStrong.color() : Theme::textDisabled.color());
        painter->setFont(item.font);
        const QRect text = item.rect.adjusted(12, 0, -8, 0);
        painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeft,
            item.fontMetrics.elidedText(item.text, Qt::ElideRight, text.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        return {base.width() + 20, std::max(base.height() + 10, 28)};
    }
};

}

RendropStyle::RendropStyle():
    QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
}

void RendropStyle::install()
{
    auto *style = new RendropStyle();
    QApplication::setStyle(style);
    PopupMotion::disableSystemEffects();
    QApplication::setPalette(style->standardPalette());
}

QPalette RendropStyle::standardPalette() const
{
    QPalette palette;
    const auto both = [&palette](QPalette::ColorRole role, const QColor &color) {
        palette.setColor(QPalette::All, role, color);
    };
    both(QPalette::Window, Look::windowColor);
    both(QPalette::WindowText, Look::textColor);
    both(QPalette::Base, Theme::control);
    both(QPalette::AlternateBase, Theme::chip);
    both(QPalette::ToolTipBase, popupColor);
    both(QPalette::ToolTipText, Look::textColor);
    both(QPalette::PlaceholderText, Theme::placeholder);
    both(QPalette::Text, Theme::textStrong);
    both(QPalette::Button, Theme::control);
    both(QPalette::ButtonText, Theme::textStrong);
    both(QPalette::BrightText, Theme::textStrong);
    both(QPalette::Light, Theme::controlHover);
    both(QPalette::Midlight, Theme::fieldHover);
    both(QPalette::Mid, Theme::controlDisabled);
    both(QPalette::Dark, Theme::outline);
    both(QPalette::Shadow, Qt::black);
    both(QPalette::Highlight, Theme::highlight);
    both(QPalette::HighlightedText, Theme::onHighlight);
    both(QPalette::Link, Theme::accentInk);
    both(QPalette::LinkVisited, Theme::accentInk);
    const QColor disabledText = Theme::textDisabled;
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
    {
        palette.setColor(QPalette::Disabled, role, disabledText);
    }
    palette.setColor(QPalette::Disabled, QPalette::Highlight, Theme::disabledFill.color());
    return palette;
}

void RendropStyle::polish(QPalette &palette)
{
    palette = standardPalette();
}

void RendropStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (auto *button = qobject_cast<QPushButton *>(widget))
    {
        // Buttons styled by their owner keep that look; others, such as the
        // ones of a message box, get the plain one.
        if (!RoundedSurface::find(button) && !button->isFlat())
        {
            Look::stylePlainButton(button);
        }
        button->setAttribute(Qt::WA_Hover);
        return;
    }
    if (auto *spin = qobject_cast<QAbstractSpinBox *>(widget))
    {
        if (!RoundedSurface::find(spin))
        {
            RoundedSurface::install(spin, 8, Look::fieldColors());
        }
        spin->setAttribute(Qt::WA_Hover);
        return;
    }
    if (auto *combo = qobject_cast<QComboBox *>(widget))
    {
        if (!RoundedSurface::find(combo))
        {
            RoundedSurface::install(combo, 8, Look::comboColors());
        }
        combo->setAttribute(Qt::WA_Hover);
        if (markOnce(combo))
        {
            combo->setItemDelegate(new PopupItemDelegate(combo));
            // showPopup() creates the popup's native window before the popup
            // is first polished, and a window only becomes see-through when
            // it is created that way; otherwise its rounded corners show
            // black. view() builds the container now, still without a
            // native window. On Windows Qt only gives a window per-pixel
            // alpha (a layered window) when it is also frameless.
            QWidget *container = combo->view()->window();
            if (isComboPopup(container) && !container->testAttribute(Qt::WA_WState_Created))
            {
                container->setAttribute(Qt::WA_TranslucentBackground);
                container->setWindowFlags(container->windowFlags() | Qt::FramelessWindowHint
                    | Qt::NoDropShadowWindowHint);
            }
        }
        return;
    }
    if (auto *edit = qobject_cast<QLineEdit *>(widget))
    {
        // The editors inside spin and combo boxes sit on their parent's surface.
        const QWidget *parent = edit->parentWidget();
        const bool embedded = qobject_cast<const QAbstractSpinBox *>(parent) || qobject_cast<const QComboBox *>(parent);
        if (!embedded && edit->hasFrame() && !RoundedSurface::find(edit))
        {
            RoundedSurface::install(edit, 8, Look::fieldColors());
        }
        return;
    }
    if (auto *bar = qobject_cast<QScrollBar *>(widget))
    {
        // Hover over the handle has to reach the style.
        bar->setAttribute(Qt::WA_Hover);
        return;
    }
    if (auto *menu = qobject_cast<QMenu *>(widget))
    {
        if (markOnce(menu))
        {
            // Rounded corners need a see-through window; the square system
            // shadow would show around them.
            menu->setAttribute(Qt::WA_TranslucentBackground);
            menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
            PopupMotion::attach(menu, popupBorder, 8);
        }
        return;
    }
    if (widget->inherits("QTipLabel"))
    {
        // Qt polishes a tool tip in its constructor, before its window
        // exists, so it can still become see-through for the rounded
        // corners. A tool tip lives for one showing; its colors are read
        // here, in the scheme on screen.
        widget->setAttribute(Qt::WA_TranslucentBackground);
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        widget->setContentsMargins(4, 1, 4, 1);
        QPalette palette = widget->palette();
        palette.setColor(QPalette::ToolTipText, Look::textColor);
        palette.setColor(QPalette::WindowText, Look::textColor);
        widget->setPalette(palette);
        return;
    }
    if (isComboPopup(widget) && markOnce(widget))
    {
        // Translucency is set up with the combo box above; changing window
        // flags here, on a created window, would hide and recreate it.
        widget->setContentsMargins(4, 4, 4, 4);
        widget->installEventFilter(this);
        PopupMotion::attach(widget, popupBorder, 8);
        if (auto *frame = qobject_cast<QFrame *>(widget))
        {
            frame->setFrameShape(QFrame::NoFrame);
        }
        for (QAbstractItemView *view : widget->findChildren<QAbstractItemView *>())
        {
            view->setFrameShape(QFrame::NoFrame);
            view->setAutoFillBackground(false);
            view->viewport()->setAutoFillBackground(false);
            view->setMouseTracking(true);
        }
    }
}

void RendropStyle::unpolish(QWidget *widget)
{
    if (isComboPopup(widget))
    {
        widget->removeEventFilter(this);
    }
    QProxyStyle::unpolish(widget);
}

bool RendropStyle::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Paint && watched->isWidgetType())
    {
        auto *popup = static_cast<QWidget *>(watched);
        if (isComboPopup(popup))
        {
            QPainter painter(popup);
            RoundedSurface::paintFrame(painter, QRectF(popup->rect()), 8, popupColor.color(), popupBorder.color());
            return true;
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

void RendropStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
    const QWidget *widget) const
{
    switch (element)
    {
    // RoundedSurface paints these backgrounds and outlines.
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel:
    case PE_FrameButtonBevel:
    case PE_FrameDefaultButton:
    case PE_PanelLineEdit:
    case PE_FrameLineEdit:
    case PE_FrameFocusRect:
    case PE_FrameMenu:
        return;
    case PE_PanelTipLabel:
        RoundedSurface::paintFrame(*painter, QRectF(option->rect), 6, popupColor.color(), popupBorder.color());
        return;
    case PE_PanelMenu:
    {
        RoundedSurface::paintFrame(*painter, QRectF(option->rect), 8, popupColor.color(), popupBorder.color());
        return;
    }
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowUp:
        drawChevron(painter, option->rect, std::min(8, option->rect.width()), groupColor(option, QPalette::ButtonText),
            element == PE_IndicatorArrowUp);
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void RendropStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
    const QWidget *widget) const
{
    switch (element)
    {
    case CE_PushButtonBevel:
        return;
    case CE_PushButtonLabel:
    {
        const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
        if (!button)
        {
            break;
        }
        const QRect area = button->rect.marginsRemoved(buttonPadding(widget));
        const RoundedSurface *surface = RoundedSurface::find(widget);
        const QColor color = surface ? surface->textColor() : groupColor(option, QPalette::ButtonText);
        const bool hasIcon = !button->icon.isNull();
        const bool hasText = !button->text.isEmpty();
        const int gap = hasIcon && hasText ? 6 : 0;
        const int iconWidth = hasIcon ? button->iconSize.width() : 0;
        const int available = std::max(0, area.width() - iconWidth - gap);
        // elidedText() counts the '&' of a mnemonic, so a label such as
        // "是(&Y)" that fits would still be cut; only elide what does not fit.
        QString text = button->text;
        if (hasText && button->fontMetrics.size(Qt::TextShowMnemonic, text).width() > available)
        {
            text = button->fontMetrics.elidedText(text, Qt::ElideRight, available, Qt::TextShowMnemonic);
        }
        const int textWidth = hasText ? button->fontMetrics.size(Qt::TextShowMnemonic, text).width() : 0;
        const int total = iconWidth + gap + textWidth;
        const Qt::Alignment align = buttonAlignment(widget);
        int x = area.left() + (area.width() - total) / 2;
        if (align & Qt::AlignLeft)
        {
            x = area.left();
        }
        else if (align & Qt::AlignRight)
        {
            x = area.right() + 1 - total;
        }
        painter->save();
        if (hasIcon)
        {
            const QIcon::Mode mode = button->state & State_Enabled ? QIcon::Normal : QIcon::Disabled;
            const QIcon::State state = button->state & State_On ? QIcon::On : QIcon::Off;
            const QSize size = button->iconSize;
            const QPixmap pixmap = button->icon.pixmap(size, painter->device()->devicePixelRatio(), mode, state);
            const QRect target(x, button->rect.top() + (button->rect.height() - size.height()) / 2, size.width(), size.height());
            painter->drawPixmap(target, pixmap);
        }
        if (hasText)
        {
            painter->setPen(color);
            painter->drawText(QRect(x + iconWidth + gap, area.top(), textWidth + 1, area.height()),
                // FreeType draws at fractional advances, a little wider than
                // the integer metrics: clipping to them cut off the last glyph.
                Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic | Qt::TextDontClip, text);
        }
        painter->restore();
        return;
    }
    case CE_ComboBoxLabel:
    {
        const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option);
        if (!combo || combo->editable)
        {
            break;
        }
        const QRect field = subControlRect(CC_ComboBox, combo, SC_ComboBoxEditField, widget);
        painter->save();
        painter->setPen(groupColor(option, QPalette::Text));
        painter->drawText(field, Qt::AlignVCenter | Qt::AlignLeft,
            combo->fontMetrics.elidedText(combo->currentText, Qt::ElideRight, field.width()));
        painter->restore();
        return;
    }
    case CE_MenuEmptyArea:
        return;
    case CE_MenuItem:
    {
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (!item)
        {
            break;
        }
        if (item->menuItemType == QStyleOptionMenuItem::Separator)
        {
            painter->save();
            painter->setPen(QPen(Theme::separator.color(), 1.0));
            const qreal y = item->rect.center().y() + 0.5;
            painter->drawLine(QPointF(item->rect.left() + 8, y), QPointF(item->rect.right() - 8, y));
            painter->restore();
            return;
        }
        const bool on = (item->state & State_Selected) && (item->state & State_Enabled);
        const qreal level = Fader::level(widget, static_cast<quintptr>(item->rect.top()), on);
        if (level > 0.0)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(Look::mix(QColor(), rowHover, level));
            painter->drawRoundedRect(QRectF(item->rect).adjusted(0, 0.5, 0, -0.5), 6, 6);
            painter->restore();
        }
        // Fusion would paint its square highlight over the faded one.
        QStyleOptionMenuItem plain(*item);
        plain.state &= ~State_Selected;
        QProxyStyle::drawControl(element, &plain, painter, widget);
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void RendropStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
    const QWidget *widget) const
{
    switch (control)
    {
    case CC_ComboBox:
    {
        const QRect arrow = subControlRect(control, option, SC_ComboBoxArrow, widget);
        const qreal hover = Fader::level(widget, 0, (option->state & State_MouseOver) && (option->state & State_Enabled));
        const QColor color = option->state & State_Enabled
            ? Look::mix(Theme::textMuted, Theme::textStrong, hover)
            : option->palette.color(QPalette::Disabled, QPalette::ButtonText);
        drawChevron(painter, arrow, 8, color, false);
        return;
    }
    case CC_SpinBox:
    {
        const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
        if (!spin || spin->buttonSymbols == QAbstractSpinBox::NoButtons)
        {
            return;
        }
        const QRect up = subControlRect(control, option, SC_SpinBoxUp, widget);
        const QRect down = subControlRect(control, option, SC_SpinBoxDown, widget);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        // The button fills follow the rounded outline of the box.
        QPainterPath outline;
        const QRectF bounds = QRectF(option->rect).adjusted(1, 1, -1, -1);
        outline.addRoundedRect(bounds, 5, 5);
        painter->setClipPath(outline);
        const bool hovered = option->state & State_MouseOver;
        const bool sunken = option->state & State_Sunken;
        const struct
        {
            QRect area;
            SubControl part;
            bool enabled;
        } buttons[] = {
            {up, SC_SpinBoxUp, bool(spin->stepEnabled & QAbstractSpinBox::StepUpEnabled)},
            {down, SC_SpinBoxDown, bool(spin->stepEnabled & QAbstractSpinBox::StepDownEnabled)}
        };
        const bool enabled = option->state & State_Enabled;
        for (const auto &button : buttons)
        {
            const bool active = enabled && button.enabled && (spin->activeSubControls & button.part);
            const qreal hover = Fader::level(widget, static_cast<quintptr>(button.part), active && hovered);
            const qreal press = Fader::level(widget, static_cast<quintptr>(button.part) << 8, active && sunken);
            QColor fill = Look::mix(QColor(), Theme::controlHover, hover);
            fill = Look::mix(fill, Theme::controlPressed, press);
            if (fill.alpha() > 0)
            {
                painter->fillRect(button.area, fill);
            }
            const QColor glyph = enabled && button.enabled
                ? Theme::textHover.color()
                : option->palette.color(QPalette::Disabled, QPalette::ButtonText);
            drawChevron(painter, button.area, 7, glyph, button.part == SC_SpinBoxUp);
        }
        painter->setClipping(false);
        painter->setPen(QPen(Theme::outline.color(), 1.0));
        const qreal x = up.left() + 0.5;
        painter->drawLine(QPointF(x, bounds.top()), QPointF(x, bounds.bottom()));
        const qreal y = down.top() + 0.5;
        painter->drawLine(QPointF(x, y), QPointF(bounds.right(), y));
        painter->restore();
        return;
    }
    case CC_ScrollBar:
    {
        const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (!bar)
        {
            break;
        }
        const QRect handle = subControlRect(control, option, SC_ScrollBarSlider, widget);
        const bool onHandle = bar->activeSubControls & SC_ScrollBarSlider;
        const qreal hover = Fader::level(widget, 0, onHandle && (bar->state & State_MouseOver));
        const qreal press = Fader::level(widget, 1, onHandle && (bar->state & State_Sunken));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor color = Look::mix(Theme::veil(Theme::isLight() ? 60 : 40), Theme::veil(Theme::isLight() ? 105 : 70), hover);
        color = Look::mix(color, Look::accentColor, press);
        if (bar->maximum > bar->minimum && !handle.isEmpty())
        {
            const qreal radius = std::min(handle.width(), handle.height()) / 2.0;
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            painter->drawRoundedRect(QRectF(handle), radius, radius);
        }
        painter->restore();
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

QRect RendropStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
    const QWidget *widget) const
{
    const QRect rect = option->rect;
    switch (control)
    {
    case CC_ComboBox:
        switch (subControl)
        {
        case SC_ComboBoxFrame:
        case SC_ComboBoxListBoxPopup:
            return rect;
        case SC_ComboBoxArrow:
            return QRect(rect.x() + rect.width() - comboArrowWidth - 4, rect.y(), comboArrowWidth, rect.height());
        case SC_ComboBoxEditField:
            return rect.adjusted(12, 1, -(comboArrowWidth + 4), -1);
        default:
            break;
        }
        break;
    case CC_SpinBox:
    {
        const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
        const bool buttons = spin && spin->buttonSymbols != QAbstractSpinBox::NoButtons;
        const int buttonLeft = rect.x() + rect.width() - spinButtonWidth;
        switch (subControl)
        {
        case SC_SpinBoxFrame:
            return rect;
        case SC_SpinBoxEditField:
            return buttons ? rect.adjusted(8, 1, -(spinButtonWidth + 4), -1) : rect.adjusted(10, 1, -10, -1);
        case SC_SpinBoxUp:
            return buttons ? QRect(buttonLeft, rect.y(), spinButtonWidth, rect.height() / 2) : QRect();
        case SC_SpinBoxDown:
            return buttons
                ? QRect(buttonLeft, rect.y() + rect.height() / 2, spinButtonWidth, rect.height() - rect.height() / 2)
                : QRect();
        default:
            break;
        }
        break;
    }
    case CC_ScrollBar:
    {
        const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (!bar)
        {
            break;
        }
        const bool horizontal = bar->orientation == Qt::Horizontal;
        // The track the handle moves in; QScrollBar maps drags through it.
        const QRect track = horizontal ? rect.adjusted(4, 2, -4, -2) : rect.adjusted(2, 4, -2, -4);
        const int length = horizontal ? track.width() : track.height();
        const int span = bar->maximum - bar->minimum;
        const int minimum = std::min(length, proxy()->pixelMetric(PM_ScrollBarSliderMin, option, widget));
        int handleLength = length;
        if (span > 0)
        {
            const qint64 page = std::max(1, bar->pageStep);
            handleLength = static_cast<int>(static_cast<qint64>(length) * page / (span + page));
            handleLength = std::clamp(handleLength, minimum, length);
        }
        const int offset = sliderPositionFromValue(bar->minimum, bar->maximum, bar->sliderPosition,
            length - handleLength, bar->upsideDown);
        const QRect handle = horizontal
            ? QRect(track.x() + offset, track.y(), handleLength, track.height())
            : QRect(track.x(), track.y() + offset, track.width(), handleLength);
        switch (subControl)
        {
        case SC_ScrollBarGroove:
            return track;
        case SC_ScrollBarSlider:
            return handle;
        case SC_ScrollBarSubPage:
            return horizontal
                ? QRect(track.x(), track.y(), handle.x() - track.x(), track.height())
                : QRect(track.x(), track.y(), track.width(), handle.y() - track.y());
        case SC_ScrollBarAddPage:
            return horizontal
                ? QRect(handle.right() + 1, track.y(), track.right() - handle.right(), track.height())
                : QRect(track.x(), handle.bottom() + 1, track.width(), track.bottom() - handle.bottom());
        default:
            // No step buttons.
            return QRect();
        }
    }
    default:
        break;
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

QRect RendropStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    if (element == SE_LineEditContents)
    {
        // Framed line edits sit on a RoundedSurface and need room inside it;
        // the editors of spin and combo boxes get theirs from the parent.
        const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (frame && frame->lineWidth > 0)
        {
            return option->rect.adjusted(10, 1, -10, -1);
        }
        return option->rect;
    }
    return QProxyStyle::subElementRect(element, option, widget);
}

int RendropStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric)
    {
    case PM_ScrollBarExtent:
        return 10;
    case PM_ScrollBarSliderMin:
        return 28;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0;
    case PM_MenuPanelWidth:
        return 1;
    case PM_ToolTipLabelFrameWidth:
        // Room inside the rounded outline; the label adds one more pixel.
        return 5;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return 4;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize RendropStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
    const QWidget *widget) const
{
    switch (type)
    {
    case CT_PushButton:
    {
        const QMargins padding = buttonPadding(widget);
        return {contentsSize.width() + padding.left() + padding.right() + 2,
            std::max(24, contentsSize.height() + padding.top() + padding.bottom() + 2)};
    }
    case CT_ComboBox:
        return {contentsSize.width() + 12 + comboArrowWidth + 4, std::max(28, contentsSize.height() + 10)};
    case CT_SpinBox:
    {
        const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
        const bool buttons = spin && spin->buttonSymbols != QAbstractSpinBox::NoButtons;
        return {contentsSize.width() + (buttons ? 8 + spinButtonWidth + 4 : 20), std::max(28, contentsSize.height() + 10)};
    }
    case CT_LineEdit:
        return {contentsSize.width() + 4, std::max(26, contentsSize.height() + 8)};
    default:
        break;
    }
    return QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
}

int RendropStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
    QStyleHintReturn *returnData) const
{
    switch (hint)
    {
    case SH_ComboBox_Popup:
        // A list below the box, which the delegate and the rounded popup draw.
        return 0;
    case SH_ComboBox_PopupFrameStyle:
        return QFrame::NoFrame;
    case SH_ComboBox_ListMouseTracking:
        return 1;
    case SH_LineEdit_PasswordCharacter:
        // Pixel Mixed draws the default mask (U+25CF) almost full height; the
        // bullet U+2022 keeps a hidden key the size of a visible one.
        return 0x2022;
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return 0;
    case SH_ScrollBar_Transient:
        return 0;
    default:
        break;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
