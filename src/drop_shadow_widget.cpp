// drop_shadow_widget.cpp

#include <QPaintEvent>
#include <QImage>
#include <QPainter>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

#define ENABLE_SHADOW_CACHE 1

DropShadowWidget::DropShadowWidget(
    QWidget *parent,
    QWidget *targetWidget,
    DropShadowRenderer *dropShadowRenderer
):
    QWidget(parent),
    targetWidget(targetWidget),
    dropShadowRenderer(dropShadowRenderer)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    updateEnabled = true;
    stackUnder(targetWidget);
    targetWidget->installEventFilter(this);
}

DropShadowWidget::~DropShadowWidget() {}

void DropShadowWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

#if ENABLE_SHADOW_CACHE

    if (updateEnabled)
    {
        if (shadowCache.isNull() || !(shadowCache.width() == width()) || !(shadowCache.height() == height()))
        {
            shadowCache = dropShadowRenderer->render(
                width(),
                height(),
                borderRadius,
                offsetX,
                offsetY,
                alphaMax,
                blurRadius
            );
        }
        painter.drawImage(QPoint(0, 0), shadowCache);
    }
    else
    {
        painter.drawImage(QPoint(0, 0), *shadowImage);
    }

#else

        QImage dropShadowImage = dropShadowRenderer->render(
            width(),
            height(),
            borderRadius,
            offsetX,
            offsetY,
            alphaMax,
            blurRadius
        );
        painter.drawImage(QPoint(0, 0), dropShadowImage);

#endif

}

bool DropShadowWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == targetWidget && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        QRect rect = targetWidget->geometry();

        float marginX = std::abs(offsetX) + blurRadius * 0.5f;
        float marginY = std::abs(offsetY) + blurRadius * 0.5f;

        rect.setRect(rect.x() - marginX, rect.y() - marginY, rect.width() + marginX * 2, rect.height() + marginY * 2);
        setGeometry(rect);
        stackUnder(targetWidget);
    }
    return QWidget::eventFilter(watched, event);
}

void DropShadowWidget::setBorderRadius(float borderRadius)
{
    this->borderRadius = borderRadius;
}

void DropShadowWidget::setOffsetX(float offsetX)
{
    this->offsetX = offsetX;
}

void DropShadowWidget::setOffsetY(float offsetY)
{
    this->offsetY = offsetY;
}

void DropShadowWidget::setAlphaMax(float alphaMax)
{
    this->alphaMax = alphaMax;
}

void DropShadowWidget::setBlurRadius(float blurRadius)
{
    this->blurRadius = blurRadius;
}

void DropShadowWidget::setShadowUpdateEnabled(bool updateEnabled)
{
    this->updateEnabled = updateEnabled;
}

void DropShadowWidget::setShadowImage(QImage *shadowImage)
{
    this->shadowImage = shadowImage;
}
