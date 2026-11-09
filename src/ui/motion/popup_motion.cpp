// popup_motion.cpp

#include <algorithm>

#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include "ui/motion/popup_motion.h"

namespace {

constexpr int openMs = 260;
constexpr int closeMs = 140;
// Height the unroll starts from, so the rounded ends are whole from the
// first frame.
constexpr qreal startHeight = 18.0;
constexpr qreal openScale = 0.94;
constexpr qreal closeScale = 0.97;
constexpr char borderProperty[] = "rendropPopupBorder";
constexpr char radiusProperty[] = "rendropPopupRadius";
constexpr char anchorProperty[] = "rendropPopupAnchor";

// Material's emphasized decelerate: most of the travel happens at once and
// the rest settles in.
QEasingCurve openCurve()
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(0.05, 0.7), QPointF(0.1, 1.0), QPointF(1.0, 1.0));
    return curve;
}

// A popup is only animated while it is still the class it was attached as;
// while its destructor runs it reports a plain QWidget and must not be
// grabbed.
bool isLivePopup(const QWidget *popup)
{
    return qobject_cast<const QMenu *>(popup) || qobject_cast<const QDialog *>(popup)
        || popup->inherits("QComboBoxPrivateContainer");
}

// Where the popup came from, in global coordinates: the combo box for a
// combo popup, the pointer for a menu.
QRect anchorOf(const QWidget *popup)
{
    // A popover dialog remembers what opened it.
    const QVariant anchor = popup->property(anchorProperty);
    if (anchor.isValid())
    {
        return anchor.toRect();
    }
    if (popup->inherits("QComboBoxPrivateContainer"))
    {
        if (const auto *combo = qobject_cast<const QComboBox *>(popup->parentWidget()))
        {
            return QRect(combo->mapToGlobal(QPoint(0, 0)), combo->size());
        }
    }
    return QRect(QCursor::pos(), QSize(1, 1));
}

// Click-through window that plays the motion with a snapshot of the popup.
class PopupOverlay : public QWidget
{
public:
    PopupOverlay(QWidget *popup, const QPixmap &shot, bool opening):
        QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
            | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus),
        popup(popup),
        shot(shot),
        opening(opening),
        border(popup->property(borderProperty).value<QColor>()),
        radius(popup->property(radiusProperty).toReal())
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_DeleteOnClose);
        const QRect area = popup->geometry();
        setGeometry(area);

        const QRect anchor = anchorOf(popup);
        fromTop = area.center().y() >= anchor.center().y();
        // Grow from the middle of the part that lines up with the anchor.
        const int left = std::max(anchor.left(), area.left());
        const int right = std::min(anchor.right(), area.right());
        qreal x = 0.5;
        if (area.width() > 0)
        {
            if (left <= right)
            {
                x = ((left + right) / 2.0 - area.left()) / area.width();
            }
            else
            {
                x = anchor.right() < area.left() ? 0.0 : 1.0;
            }
        }
        originX = std::clamp(x, 0.0, 1.0);

        animation = new QVariantAnimation(this);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setDuration(opening ? openMs : closeMs);
        animation->setEasingCurve(opening ? openCurve() : QEasingCurve(QEasingCurve::OutCubic));
        QObject::connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            progress = value.toReal();
            update();
        });
        QObject::connect(animation, &QVariantAnimation::finished, this, [this]() { finish(); });
    }

    void start()
    {
        show();
        animation->start();
    }

    // Ends the motion now; an opening one hands over to the popup.
    void finish()
    {
        if (done)
        {
            return;
        }
        done = true;
        animation->stop();
        if (opening && popup)
        {
            popup->setWindowOpacity(1.0);
            popup->update();
        }
        close();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        // shown runs 0 to 1 while opening and 1 to 0 while closing.
        const qreal shown = opening ? progress : 1.0 - progress;
        const qreal w = width();
        const qreal h = height();
        const qreal scale = opening ? openScale + (1.0 - openScale) * shown : closeScale + (1.0 - closeScale) * shown;
        const qreal alpha = opening ? std::min(1.0, shown / 0.35) : shown;
        // Closing keeps the whole surface; only opening unrolls it.
        const qreal minHeight = std::min(h, startHeight);
        const qreal revealed = opening ? minHeight + (h - minHeight) * shown : h;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(alpha);
        const QPointF origin(originX * w, fromTop ? 0.0 : h);
        painter.translate(origin);
        painter.scale(scale, scale);
        painter.translate(-origin);

        const QRectF visible(0.0, fromTop ? 0.0 : h - revealed, w, revealed);
        QPainterPath outline;
        outline.addRoundedRect(visible.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        painter.save();
        painter.setClipPath(outline);
        painter.drawPixmap(QPointF(0.0, 0.0), shot);
        painter.restore();
        if (opening && revealed < h && border.isValid())
        {
            // The popup's own outline is cut off at the moving edge.
            painter.setPen(QPen(border, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(outline);
        }
    }

private:
    QPointer<QWidget> popup;
    QPixmap shot;
    bool opening;
    QColor border;
    qreal radius;
    bool fromTop = true;
    qreal originX = 0.5;
    qreal progress = 0.0;
    bool done = false;
    QVariantAnimation *animation = nullptr;
};

class MotionFilter : public QObject
{
public:
    static MotionFilter *instance()
    {
        static MotionFilter *filter = new MotionFilter(qApp);
        return filter;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!watched->isWidgetType())
        {
            return false;
        }
        auto *popup = static_cast<QWidget *>(watched);
        if (event->type() == QEvent::Show)
        {
            opened(popup);
        }
        else if (event->type() == QEvent::Hide)
        {
            closed(popup);
        }
        return false;
    }

private:
    using QObject::QObject;

    // Keeps the popup's current overlay; a QObject child so it goes with the
    // popup.
    class PopupOverlayHandle : public QObject
    {
    public:
        using QObject::QObject;
        QPointer<PopupOverlay> overlay;
    };

    static PopupOverlayHandle *handleOf(QWidget *popup)
    {
        // Found by name: without Q_OBJECT a typed findChild would match any
        // QObject child.
        static const QString name = QStringLiteral("rendropPopupMotion");
        if (QObject *found = popup->findChild<QObject *>(name, Qt::FindDirectChildrenOnly))
        {
            return static_cast<PopupOverlayHandle *>(found);
        }
        auto *handle = new PopupOverlayHandle(popup);
        handle->setObjectName(name);
        return handle;
    }

    static void stopOverlay(QWidget *popup)
    {
        PopupOverlayHandle *handle = handleOf(popup);
        if (handle->overlay)
        {
            handle->overlay->finish();
            handle->overlay = nullptr;
        }
    }

    void opened(QWidget *popup)
    {
        if (QCoreApplication::closingDown() || !isLivePopup(popup))
        {
            return;
        }
        stopOverlay(popup);
        // Hidden until the snapshot lands, but already taking input. The
        // snapshot waits a turn of the event loop so the list inside has
        // laid out its rows.
        popup->setWindowOpacity(0.0);
        QPointer<QWidget> guard(popup);
        QTimer::singleShot(0, popup, [guard]() {
            if (!guard || !guard->isVisible() || !isLivePopup(guard))
            {
                if (guard)
                {
                    guard->setWindowOpacity(1.0);
                }
                return;
            }
            auto *overlay = new PopupOverlay(guard, guard->grab(), true);
            handleOf(guard)->overlay = overlay;
            overlay->start();
        });
    }

    void closed(QWidget *popup)
    {
        if (!isLivePopup(popup))
        {
            return;
        }
        // An unfinished open never showed the popup, so there is nothing to
        // fade out.
        PopupOverlayHandle *handle = handleOf(popup);
        const bool wasShown = popup->windowOpacity() > 0.0;
        stopOverlay(popup);
        popup->setWindowOpacity(1.0);
        if (!wasShown || QCoreApplication::closingDown())
        {
            return;
        }
        auto *overlay = new PopupOverlay(popup, popup->grab(), false);
        handle->overlay = overlay;
        overlay->start();
    }
};

}

void PopupMotion::disableSystemEffects()
{
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
    QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
}

void PopupMotion::attach(QWidget *popup, const QColor &border, qreal radius)
{
    popup->setProperty(borderProperty, border);
    popup->setProperty(radiusProperty, radius);
    popup->installEventFilter(MotionFilter::instance());
}

void PopupMotion::attach(QDialog *popover, const QColor &border, qreal radius, const QRect &globalAnchor)
{
    popover->setProperty(anchorProperty, globalAnchor);
    attach(static_cast<QWidget *>(popover), border, radius);
}
