// drop_shadow_widget.cpp

#include <QPaintEvent>
#include <QPixmap>
#include <QPainter>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

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
    handle = dropShadowRenderer->createWidgetBuffer([this]() { this->update(); });
    stackUnder(targetWidget);
    targetWidget->installEventFilter(this);
}

DropShadowWidget::~DropShadowWidget()
{
    dropShadowRenderer->deleteWidgetBuffer(handle);
}

void DropShadowWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (updateEnabled)
    {
        shadowCache = dropShadowRenderer->getPixmap(handle);
        int expectedWidth = std::ceil(width() * devicePixelRatio());
        int expectedHeight = std::ceil(height() * devicePixelRatio());

        if (shadowCache.isNull() ||
            shadowCache.width() != expectedWidth ||
            shadowCache.height() != expectedHeight ||
            !qFuzzyCompare(static_cast<float>(shadowCache.devicePixelRatio()), static_cast<float>(devicePixelRatio()))
        ) {
            dropShadowRenderer->setWidgetBuffer(
                handle,
                width(),
                height(),
                borderRadius,
                offsetX,
                offsetY,
                alphaMax,
                blurRadius,
                devicePixelRatio()
            );
            if (shadowCache.isNull())
            {
                return;
            }
            shadowCache = shadowCache.scaled(QSize(expectedWidth, expectedHeight), Qt::IgnoreAspectRatio, Qt::FastTransformation);
            shadowCache.setDevicePixelRatio(devicePixelRatio());
        }
        painter.drawPixmap(QPoint(0, 0), shadowCache);
    }
    else
    {
        if (shadowPixmap && !shadowPixmap->isNull())
        {
            painter.drawPixmap(QPoint(0, 0), *shadowPixmap);
        }
    }
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
    dropShadowRenderer->setWidgetBufferUpdateEnabled(handle, updateEnabled);
}

void DropShadowWidget::setShadowPixmap(QPixmap *shadowPixmap)
{
    this->shadowPixmap = shadowPixmap;
}
