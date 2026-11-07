// look_widgets.h

#ifndef LOOK_WIDGETS_H
#define LOOK_WIDGETS_H

#include <QAbstractButton>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QString>
#include <QWidget>

// Shared look of the main window: fonts, colors and style sheets that the
// queue, nodes and settings pages all use.
namespace Look {

QFont pixelFont(int pointSize);

inline const QColor frameColor(0x2C, 0x31, 0x3C);
inline const QColor surfaceColor(26, 28, 32);
// Dynamic property on the outer frame of a large card; Widget adds a drop
// shadow to every direct child of a page that carries it.
inline constexpr char cardProperty[] = "rendropCard";
inline const QColor accentColor(110, 110, 180);
inline const QColor dimTextColor(110, 115, 130);

// Style sheets clip border-radius without antialiasing, so the sheets below
// only carry text, padding and a transparent box; the backgrounds and
// outlines come from RoundedSurface.

// Combo box, line edit and spin box on the dark surfaces. Pair with
// smoothInputs() once the inputs exist.
QString inputStyleSheet();
// Gives every combo box, line edit and spin box under root (root included)
// the antialiased input surface.
void smoothInputs(QWidget *root);
// Filled accent button.
void styleAccentButton(QAbstractButton *button);
// Quiet secondary button.
void stylePlainButton(QAbstractButton *button);
// Thin rounded handle without a groove.
QString scrollBarStyleSheet();
// Terminal glyph for the log toggle.
QIcon terminalIcon(const QColor &color);

}

// Small antialiased on/off switch.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Icon button of the navigation rail. The selected one gets a tinted
// background and an accent mark on its left edge.
class NavButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Icon
    {
        Queue,
        Nodes,
        Settings
    };

    NavButton(Icon icon, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Icon icon;
};

// Thin horizontal progress bar, 0-100.
class LinearProgress : public QWidget
{
    Q_OBJECT

public:
    explicit LinearProgress(QWidget *parent = nullptr);
    void setValue(double percent);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double value = 0.0;
};

// Rounded pill with a colored dot and a short text, used for the global
// status in the title bar.
class StatusChip : public QWidget
{
    Q_OBJECT

public:
    explicit StatusChip(QWidget *parent = nullptr);
    void setStatus(const QColor &dot, const QString &text);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor dot;
    QString text;
};

#endif // LOOK_WIDGETS_H
