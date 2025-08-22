// drop_shadow_widget.h

#ifndef DROP_SHADOW_WIDGET_H
#define DROP_SHADOW_WIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QPixmap>

#include "drop_shadow_renderer.h"

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

    QPixmap shadowCache;
};

#endif // DROP_SHADOW_WIDGET_H
