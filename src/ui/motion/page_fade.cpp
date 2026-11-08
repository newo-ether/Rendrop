// page_fade.cpp

#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QStackedWidget>
#include <QVariantAnimation>

#include "ui/motion/page_fade.h"
#include "ui/motion/transition.h"

namespace {

constexpr int fadeMs = 200;
constexpr char overlayName[] = "rendropPageFade";

// The captured old page, drawn on top of the stack at a falling opacity.
class FadeOverlay : public QWidget
{
public:
    FadeOverlay(QStackedWidget *stack, const QPixmap &frame):
        QWidget(stack),
        frame(frame)
    {
        setObjectName(QString::fromLatin1(overlayName));
        // Clicks reach the new page at once; the fade is only a picture.
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(stack->rect());
        raise();
        show();
    }

    void setLevel(qreal value)
    {
        level = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setOpacity(level);
        painter.drawPixmap(0, 0, frame);
    }

private:
    QPixmap frame;
    qreal level = 1.0;
};

}

namespace PageFade
{

void switchTo(QStackedWidget *stack, int index)
{
    if (!stack || index == stack->currentIndex())
    {
        return;
    }
    QWidget *window = stack->window();
    if (!stack->isVisible() || !window || stack->size().isEmpty())
    {
        stack->setCurrentIndex(index);
        return;
    }
    // From the window, so the pages' transparent areas carry the window
    // background and the shadows of cards near the edge come along. A fade
    // still running is part of what is on screen and is captured with it.
    const QRect area(stack->mapTo(window, QPoint(0, 0)), stack->size());
    const QPixmap frame = window->grab(area);
    if (QWidget *previous = stack->findChild<QWidget *>(QString::fromLatin1(overlayName), Qt::FindDirectChildrenOnly))
    {
        delete previous;
    }
    stack->setCurrentIndex(index);

    FadeOverlay *overlay = new FadeOverlay(stack, frame);
    QVariantAnimation *animation = new QVariantAnimation(overlay);
    animation->setDuration(fadeMs);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(Look::easeInOut());
    QObject::connect(animation, &QVariantAnimation::valueChanged, overlay, [overlay](const QVariant &value) {
        overlay->setLevel(value.toReal());
    });
    QPointer<FadeOverlay> guard(overlay);
    QObject::connect(animation, &QVariantAnimation::finished, overlay, [guard]() {
        if (guard)
        {
            guard->deleteLater();
        }
    });
    animation->start();
}

}
