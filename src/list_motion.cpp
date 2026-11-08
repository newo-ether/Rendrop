// list_motion.cpp

#include <algorithm>
#include <cmath>

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QLayout>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

#include "list_motion.h"
#include "transition.h"

namespace {

constexpr int slideMs = 280;
constexpr int appearMs = 260;
constexpr int vanishMs = 180;
constexpr qreal appearScale = 0.94;
constexpr qreal vanishScale = 0.96;
// Room around the snapshot for its shadow.
constexpr int ghostMargin = 24;
constexpr char slideName[] = "rendropListSlide";

// Paints a snapshot of a row (and its shadow) at some opacity and scale
// about its centre. Covers the real row while it fades in, or stands in for
// a removed one while it fades out.
class Ghost : public QWidget
{
public:
    Ghost(QWidget *parent, const QPixmap &snapshot, const QRect &source, bool hasShadow,
        const DropShadowWidget::Params &shadow):
        QWidget(parent),
        snapshot(snapshot),
        source(source),
        hasShadow(hasShadow),
        shadow(shadow)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        place(source);
        raise();
        show();
    }

    void place(const QRect &rect)
    {
        source = rect;
        setGeometry(rect.adjusted(-ghostMargin, -ghostMargin, ghostMargin, ghostMargin));
    }

    void setFrame(qreal newOpacity, qreal newScale)
    {
        opacity = newOpacity;
        scale = newScale;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (opacity <= 0.0)
        {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(opacity);
        const QRectF full(ghostMargin, ghostMargin, source.width(), source.height());
        const QSizeF size = full.size() * scale;
        const QRectF rect(full.center() - QPointF(size.width() / 2.0, size.height() / 2.0), size);
        if (hasShadow)
        {
            DropShadowWidget::Params params = shadow;
            params.borderRadius *= static_cast<float>(scale);
            DropShadowWidget::paintShadow(painter, rect, params, devicePixelRatioF());
        }
        painter.drawPixmap(rect, snapshot, QRectF(snapshot.rect()));
    }

private:
    QPixmap snapshot;
    QRect source;
    bool hasShadow;
    DropShadowWidget::Params shadow;
    qreal opacity = 0.0;
    qreal scale = 1.0;
};

// A row sliding from one slot to another on a cubic ease-in-out Bezier; an
// interrupted slide hands its speed on to the next one.
class SlideAnimation : public QVariantAnimation
{
public:
    explicit SlideAnimation(QWidget *widget): QVariantAnimation(widget) {}

    QPointF position(qreal s) const { return QPointF(x.at(s), y.at(s)); }

    // px per ms at the current time.
    QPointF velocity() const
    {
        const qreal s = std::clamp(static_cast<qreal>(currentTime()) / duration(), 0.0, 1.0);
        return QPointF(x.rate(s), y.rate(s)) / duration();
    }

    Look::Move x;
    Look::Move y;
};

// Rows fading in, so a removal can take over from their ghost.
QHash<QWidget *, QPointer<Ghost>> &appearing()
{
    static QHash<QWidget *, QPointer<Ghost>> ghosts;
    return ghosts;
}

void finishAppear(QWidget *widget, DropShadowWidget *shadow)
{
    if (widget)
    {
        widget->setGraphicsEffect(nullptr);
    }
    if (shadow)
    {
        shadow->setFade(1.0);
    }
}

}

namespace ListMotion {

void slide(QWidget *widget, QPoint from, QPointF velocity)
{
    stopSlide(widget);
    const QPoint to = widget->pos();
    if (from == to && velocity.isNull())
    {
        return;
    }
    auto *animation = new SlideAnimation(widget);
    animation->setObjectName(QLatin1String(slideName));
    animation->x = Look::Move::make(from.x(), to.x(), Look::cubicInOut, velocity.x(), slideMs);
    animation->y = Look::Move::make(from.y(), to.y(), Look::cubicInOut, velocity.y(), slideMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(slideMs);
    QPointer<QWidget> guard(widget);
    QObject::connect(animation, &QVariantAnimation::valueChanged, widget, [guard, animation](const QVariant &value) {
        if (guard)
        {
            guard->move(animation->position(value.toReal()).toPoint());
        }
    });
    QObject::connect(animation, &QVariantAnimation::finished, widget, [guard]() {
        // A resize during the slide may have moved the row's slot; let the
        // layout put it back where it belongs now.
        if (guard && guard->parentWidget() && guard->parentWidget()->layout())
        {
            guard->parentWidget()->layout()->invalidate();
        }
    });
    widget->move(from);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

QPointF stopSlide(QWidget *widget)
{
    QPointF velocity;
    const auto animations = widget->findChildren<QVariantAnimation *>(QLatin1String(slideName), Qt::FindDirectChildrenOnly);
    for (QVariantAnimation *animation : animations)
    {
        // Only SlideAnimation carries this name.
        if (animation->state() == QAbstractAnimation::Running)
        {
            velocity = static_cast<SlideAnimation *>(animation)->velocity();
        }
        animation->stop();
    }
    return velocity;
}

void appear(QWidget *widget, DropShadowWidget *shadow)
{
    QWidget *parent = widget->parentWidget();
    if (!parent || widget->size().isEmpty())
    {
        return;
    }
    const QPixmap snapshot = widget->grab();
    const bool hasShadow = shadow && DropShadowWidget::shadowsEnabled();
    auto *ghost = new Ghost(parent, snapshot, widget->geometry(), hasShadow,
        shadow ? shadow->params() : DropShadowWidget::Params());
    ghost->setFrame(0.0, appearScale);
    appearing().insert(widget, ghost);

    // The real row keeps its slot and its input but is not seen until the
    // ghost has grown into it.
    auto *hidden = new QGraphicsOpacityEffect(widget);
    hidden->setOpacity(0.0);
    widget->setGraphicsEffect(hidden);
    if (shadow)
    {
        shadow->setFade(0.0);
    }

    auto *animation = new QVariantAnimation(ghost);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(appearMs);
    animation->setEasingCurve(QEasingCurve(QEasingCurve::OutCubic));
    QPointer<QWidget> target(widget);
    QPointer<DropShadowWidget> targetShadow(shadow);
    QObject::connect(animation, &QVariantAnimation::valueChanged, ghost, [ghost, target](const QVariant &value) {
        if (!target)
        {
            ghost->deleteLater();
            return;
        }
        const qreal t = value.toReal();
        // The row may slide while it fades in.
        ghost->place(target->geometry());
        ghost->setFrame(std::min(1.0, t / 0.6), appearScale + (1.0 - appearScale) * t);
    });
    QObject::connect(animation, &QVariantAnimation::finished, ghost, [ghost, target, targetShadow]() {
        finishAppear(target, targetShadow);
        appearing().remove(target.data());
        ghost->deleteLater();
    });
    QObject::connect(ghost, &QObject::destroyed, widget, [widget, ghost]() {
        auto &ghosts = appearing();
        auto iterator = ghosts.find(widget);
        if (iterator != ghosts.end() && (!iterator.value() || iterator.value() == ghost))
        {
            ghosts.erase(iterator);
        }
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void vanish(QWidget *widget, DropShadowWidget *shadow)
{
    QWidget *parent = widget->parentWidget();
    // Cut a running fade-in short and show the row as it is.
    if (QPointer<Ghost> ghost = appearing().take(widget))
    {
        delete ghost.data();
    }
    finishAppear(widget, shadow);
    if (!parent || !widget->isVisible() || widget->size().isEmpty())
    {
        return;
    }
    stopSlide(widget);
    const QPixmap snapshot = widget->grab();
    const bool hasShadow = shadow && DropShadowWidget::shadowsEnabled();
    auto *ghost = new Ghost(parent, snapshot, widget->geometry(), hasShadow,
        shadow ? shadow->params() : DropShadowWidget::Params());
    ghost->setFrame(1.0, 1.0);
    auto *animation = new QVariantAnimation(ghost);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setDuration(vanishMs);
    animation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    QObject::connect(animation, &QVariantAnimation::valueChanged, ghost, [ghost](const QVariant &value) {
        const qreal t = value.toReal();
        ghost->setFrame(t, vanishScale + (1.0 - vanishScale) * t);
    });
    QObject::connect(animation, &QVariantAnimation::finished, ghost, &QObject::deleteLater);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

}
