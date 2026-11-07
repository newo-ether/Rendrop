// caption_button.h

#ifndef CAPTION_BUTTON_H
#define CAPTION_BUTTON_H

#include <QAbstractButton>

// Minimize / maximize / close button of the custom title bar. The glyphs
// are drawn with antialiased lines, so they stay crisp at any display scale
// and match the self-painted rounded surfaces. QWindowKit forwards the
// native hover and press of the maximize button (Windows 11 Snap Layout) as
// ordinary client mouse messages, so plain enter/leave tracking is enough.
class CaptionButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Kind
    {
        Minimize,
        Maximize,
        Close
    };

    explicit CaptionButton(Kind kind, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    // Switches the maximize glyph between "maximize" and "restore".
    void setMaximized(bool maximized);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Kind kind;
    bool maximized = false;
};

#endif // CAPTION_BUTTON_H
