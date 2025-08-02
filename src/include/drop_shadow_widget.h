// drop_shadow_widget.h

#ifndef DROPSHADOWWIDGET_H
#define DROPSHADOWWIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QImage>

#include "drop_shadow_renderer.h"

class DropShadowWidget : public QWidget
{
    Q_OBJECT

public:
    DropShadowWidget(QWidget *parent, QWidget *targetWidget, DropShadowRenderer *dropShadowRenderer);
    ~DropShadowWidget();

    void setBorderRadius(float _borderRadius);
    void setOffsetX(float offsetX);
    void setOffsetY(float offsetY);
    void setAlphaMax(float alphaMax);
    void setBlurRadius(float blurRadius);

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    float borderRadius, offsetX, offsetY, alphaMax, blurRadius;

    QWidget *targetWidget;
    DropShadowRenderer *dropShadowRenderer;

};

#endif // DROPSHADOWWIDGET_H
