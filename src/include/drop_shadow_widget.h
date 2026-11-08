// drop_shadow_widget.h

#ifndef DROP_SHADOW_WIDGET_H
#define DROP_SHADOW_WIDGET_H

#include <QPaintEvent>
#include <QPointer>
#include <QRectF>
#include <QWidget>

#include "transition.h"

class QPainter;

// Soft shadow of a rounded rect, drawn as a sibling under its target.
//
// The shadow is a nine-slice: one small template per parameter set holds the
// corners and a single pixel of each edge, and painting stretches that pixel
// along the straight parts. Resizing the target therefore costs nothing, and
// the templates are cheap enough to render on the GUI thread, which is what
// lets the hover lift animate the shadow's parameters frame by frame.
class DropShadowWidget : public QWidget
{
    Q_OBJECT

public:
    struct Params
    {
        float borderRadius = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float alphaMax = 0.0f;
        float blurRadius = 0.0f;
    };

    DropShadowWidget(QWidget *parent, QWidget *targetWidget);
    ~DropShadowWidget() override;

    void setBorderRadius(float borderRadius);
    void setOffsetX(float offsetX);
    void setOffsetY(float offsetY);
    void setAlphaMax(float alphaMax);
    void setBlurRadius(float blurRadius);
    Params params() const;
    // While the pointer is over the target the shadow eases further away:
    // larger offset and blur, a little lighter. Off by default.
    void setHoverLift(bool enabled);
    // 0-1 multiplier for list animations that fade the target in or out.
    void setFade(qreal fade);

    // Paints the shadow of target (a rect in the painter's logical
    // coordinates) with the painter's current opacity. dpr is the device
    // pixel ratio of the paint device.
    static void paintShadow(QPainter &painter, const QRectF &target, const Params &params, qreal dpr);

    // Global switch for every shadow in the app (the Settings toggle).
    static void setShadowsEnabled(bool enabled);
    static bool shadowsEnabled();

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Visible only while shadows are on and the target itself is shown.
    void refreshVisibility();
    // Follows the target with room for the lifted shadow.
    void syncGeometry();
    Params liftedParams() const;
    Params currentParams() const;
    int margin() const;

    QPointer<QWidget> targetWidget;
    Params rest;
    bool hoverLift = false;
    qreal fade = 1.0;
    Transition lift;
};

#endif // DROP_SHADOW_WIDGET_H
