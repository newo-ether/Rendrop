// rendrop_style.h

#ifndef RENDROP_STYLE_H
#define RENDROP_STYLE_H

#include <QProxyStyle>

// Application style: Fusion underneath, with Rendrop's dark palette and its
// own painting for the controls the windows use. Push buttons and inputs get
// their antialiased backgrounds from RoundedSurface, which polish() installs
// where a widget has none; this style draws their content, the combo arrow,
// spin buttons, scroll bars, the combo popup and menus. Hover and press
// states fade over Look::transitionMs.
//
// Push buttons read Look::paddingProperty and Look::alignProperty.
class RendropStyle : public QProxyStyle
{
    Q_OBJECT

public:
    RendropStyle();

    // Makes a new RendropStyle the application style. Call once, before any
    // widget exists.
    static void install();

    QPalette standardPalette() const override;
    void polish(QPalette &palette) override;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
        const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
        const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
        const QWidget *widget = nullptr) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
        const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
        const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override;

protected:
    // Paints the rounded background of combo popups.
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // RENDROP_STYLE_H
