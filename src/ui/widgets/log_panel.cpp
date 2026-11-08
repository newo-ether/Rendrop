// log_panel.cpp

#include "ui/widgets/log_panel.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextDocument>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include "ui/style/drop_shadow_widget.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rounded_surface.h"

namespace {

constexpr int logHeight = 160;
constexpr int logMs = 260;
constexpr int logRadius = 12;

} // namespace

LogPanel::LogPanel(QWidget *parent):
    QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(5, 5, 5, 5);
    view = new QPlainTextEdit(this);
    view->setObjectName(QStringLiteral("logView"));
    view->setFrameShape(QFrame::NoFrame);
    view->setReadOnly(true);
    view->setMaximumBlockCount(500);
    layout->addWidget(view);
    setFixedHeight(logHeight);

    // Same frame-around-surface card as the queue list.
    RoundedSurface::install(this, logRadius, QColor(0x2C, 0x31, 0x3C));
    RoundedSurface::install(view, 10, QColor(26, 28, 32));
    Look::clearScrollArea(view);
    view->setFont(Look::monoFont(8));
    view->document()->setDocumentMargin(6);
    Look::setTextColor(view, QColor(180, 180, 200));
    // Whenever the range changes (a line arrives, the card is shown or
    // resized) the view stays at the bottom, unless the user has scrolled up
    // to read; scrolling back to the bottom resumes following. While the
    // card was hidden the range used to be stale, so it opened at the top.
    QScrollBar *bar = view->verticalScrollBar();
    QObject::connect(bar, &QScrollBar::rangeChanged, this, [this, bar](int, int maximum) {
        if (follow)
        {
            bar->setValue(maximum);
        }
    });
    // User scrolling only (wheel, drag, arrows); the editor's own relayouts
    // also move the value and must not stop the following.
    QObject::connect(bar, &QScrollBar::actionTriggered, this, [this, bar](int) {
        // sliderPosition already holds where the action goes.
        follow = bar->sliderPosition() >= bar->maximum();
    });
}

void LogPanel::setShadow(DropShadowWidget *value)
{
    shadow = value;
}

void LogPanel::append(const QString &text)
{
    view->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + text);
    followTail();
}

void LogPanel::setShown(bool visible)
{
    shown = visible;
    if (animation)
    {
        animation->stop();
    }
    if (!parentWidget() || !parentWidget()->isVisible())
    {
        // A fade cut short by the stop above must not stay half-way.
        setGraphicsEffect(nullptr);
        if (shadow)
        {
            shadow->setFade(1.0);
        }
        setFixedHeight(logHeight);
        setVisible(visible);
        followTail();
        return;
    }
    // The card grows from or shrinks to nothing; the cards around it give
    // way or ride along.
    const int from = isVisible() ? height() : 0;
    const int to = visible ? logHeight : 0;
    // Below twice the corner radius the rounded card cannot be drawn as
    // itself, so it stays invisible there and fades in over the next
    // stretch of height.
    constexpr int hiddenBelow = 2 * logRadius;
    constexpr int fadeSpan = 48;
    auto *fade = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
    if (!fade)
    {
        fade = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(fade);
    }
    const auto applyHeight = [this, fade](int value) {
        const qreal alpha = std::clamp(static_cast<qreal>(value - hiddenBelow) / fadeSpan, 0.0, 1.0);
        fade->setOpacity(alpha);
        if (shadow)
        {
            shadow->setFade(alpha);
        }
        setFixedHeight(value);
        followTail();
    };
    applyHeight(from);
    setVisible(true);
    auto *grow = new QVariantAnimation(this);
    grow->setStartValue(from);
    grow->setEndValue(to);
    // A reversal part way only travels what is left.
    grow->setDuration(std::max(120, logMs * std::abs(to - from) / logHeight));
    grow->setEasingCurve(Look::bezierInOut(Look::cubicInOut));
    QObject::connect(grow, &QVariantAnimation::valueChanged, this, [applyHeight](const QVariant &value) {
        applyHeight(value.toInt());
    });
    QObject::connect(grow, &QVariantAnimation::finished, this, [this, visible]() {
        // The effect re-renders the whole card offscreen; drop it at rest.
        setGraphicsEffect(nullptr);
        if (shadow)
        {
            shadow->setFade(1.0);
        }
        setVisible(visible);
        setFixedHeight(logHeight);
        followTail();
    });
    animation = grow;
    grow->start(QAbstractAnimation::DeleteWhenStopped);
}

void LogPanel::followTail()
{
    if (follow)
    {
        QScrollBar *bar = view->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}
