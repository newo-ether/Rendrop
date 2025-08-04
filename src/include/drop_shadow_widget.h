// drop_shadow_widget.h

#ifndef DROPSHADOWWIDGET_H
#define DROPSHADOWWIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QPixmap>

#include "drop_shadow_renderer.h"

#define ENABLE_SHADOW_CACHE 1

class DropShadowWidget : public QWidget
{
    Q_OBJECT

public:
    DropShadowWidget(
        QWidget *parent,
        QWidget *targetWidget,
        DropShadowRenderer *dropShadowRenderer
    );
    ~DropShadowWidget();

    void setBorderRadius(float _borderRadius);
    void setOffsetX(float offsetX);
    void setOffsetY(float offsetY);
    void setAlphaMax(float alphaMax);
    void setBlurRadius(float blurRadius);

    void setShadowUpdateEnabled(bool updateEnabled);
    void setShadowPixmap(QPixmap *shadowPixmap);

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *targetWidget;
    QPixmap *shadowPixmap;
    DropShadowRenderer *dropShadowRenderer;

    float borderRadius, offsetX, offsetY, alphaMax, blurRadius;
    bool updateEnabled;

#if ENABLE_SHADOW_CACHE
    QPixmap shadowCache;
#endif

};

#endif // DROPSHADOWWIDGET_H
