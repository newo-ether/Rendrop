// look_widgets.h

#ifndef LOOK_WIDGETS_H
#define LOOK_WIDGETS_H

#include <QAbstractButton>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QMargins>
#include <QString>
#include <QWidget>

#include <functional>

#include "ui/style/rounded_surface.h"
#include "ui/style/theme.h"
#include "ui/motion/transition.h"

class QAbstractScrollArea;
class QLabel;
class QDialog;

// Shared look of the main window: fonts, colors and surfaces that the queue,
// nodes and settings pages all use. Nothing here uses style sheets; the
// painting is RendropStyle's and RoundedSurface's.
namespace Look {

// Registers the fonts in the resources (Mi Outfit, JetBrains Mono, Bitcount
// Single) and Mi Outfit as the fallback for Chinese. Call after QApplication.
void addBundledFonts();
// Family behind pixelFont(); "Mi Outfit" unless a test swaps it.
QString pixelFamily();
void setPixelFamily(const QString &family);
// The UI font. Weights: DemiBold for headings, Medium for labels, names and
// buttons, Normal for help and body text.
QFont pixelFont(int pointSize, QFont::Weight weight = QFont::Normal);
// JetBrains Mono, for numbers that line up or change in place: frame
// ranges, resolutions, progress counts, ports, addresses, the log.
QFont monoFont(int pointSize, QFont::Weight weight = QFont::Normal);
// Selects the FreeType font engine on Windows. Call before QApplication.
// Qt's DirectWrite engine (6.11) draws ClearType even for fonts that ask for
// NoSubpixelAntialias, so only FreeType gives the grayscale text below. An
// explicit QT_QPA_PLATFORM is left alone.
void selectFontEngine();
// Text rendering every font in the app shares: grayscale antialiasing
// (ClearType's colour fringes show on the dark surfaces) and vertical-only
// hinting. RENDROP_TEXT_AA=subpixel and RENDROP_TEXT_HINT=default|none|
// vertical|full override them for comparisons.
void applyTextRendering(QFont &font);
// Puts the shared rendering on the application font, which every widget font
// that does not choose its own strategy inherits. Call before any widget.
void installTextRendering();

inline const Theme::Tone &frameColor = Theme::frame;
inline const Theme::Tone &surfaceColor = Theme::surface;
// Dynamic property on the outer frame of a large card; Widget adds a drop
// shadow to every direct child of a page that carries it.
inline constexpr char cardProperty[] = "rendropCard";
inline const Theme::Tone &accentColor = Theme::accent;
inline const Theme::Tone &dimTextColor = Theme::textHelp;
inline const Theme::Tone &textColor = Theme::text;
inline const Theme::Tone &windowColor = Theme::window;

// Dynamic properties RendropStyle reads.
// QMargins between a push button's edge and its label.
inline constexpr char paddingProperty[] = "rendropPadding";
// int, Qt::Alignment of a push button's label; centered when unset.
inline constexpr char alignProperty[] = "rendropAlign";

RoundedSurface::Colors plainButtonColors();
RoundedSurface::Colors accentButtonColors();
// Line edits and spin boxes; combo boxes are the same without the focus ring.
RoundedSurface::Colors fieldColors();
RoundedSurface::Colors comboColors();

// Filled accent button.
void styleAccentButton(QAbstractButton *button);
// Quiet secondary button.
void stylePlainButton(QAbstractButton *button);
void setPadding(QWidget *button, const QMargins &padding, Qt::Alignment align = Qt::AlignCenter);
// Text color of a label or similar for every color group; disabled defaults
// to the palette's disabled text. The widget follows a scheme switch.
void setTextColor(QWidget *widget, const Theme::Tone &color, const Theme::Tone &disabled = Theme::Tone());
// No frame and no background: the scroll area shows the surface it sits on.
// Call after setWidget(), which turns the content's background back on.
void clearScrollArea(QAbstractScrollArea *area);
// Small modal dialog: dark gray background, white 12px Pixel Mixed for every
// child that does not set its own font or colors.
void styleDialog(QWidget *dialog);
// styleDialog as a frameless, rounded popover next to anchor, the control
// that opened it (centered by Qt when anchor is null). A click outside
// closes it as Cancel would. Call last in the constructor, once the
// dialog's contents have their final size.
void stylePopover(QDialog *dialog, const QWidget *anchor);
// Terminal glyph for the log toggle.
QIcon terminalIcon(const QColor &color);
// A white glyph from the resources (an alpha mask) in the tone's color for
// the scheme on screen; the dark side of the tone left invalid keeps the
// glyph as drawn.
QIcon glyphIcon(const QString &path, const Theme::Tone &tone);
// glyphIcon on a button or a label's pixmap, redone after scheme switches.
void setGlyphIcon(QAbstractButton *button, const QString &path, const Theme::Tone &tone);
void setGlyphPixmap(QLabel *label, const QString &path, const Theme::Tone &tone);
// Runs `apply` now and after every scheme switch while `context` lives, for
// state built from resolved colors (icons, palettes).
void onThemeChanged(QObject *context, const std::function<void()> &apply);

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
    bool event(QEvent *event) override;

private:
    // 0 off, 1 on; the knob slides and the track fades with it.
    Transition position;
    Transition hover;
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
    Transition hover;
    Transition selected;
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
    void setStatus(const Theme::Tone &dot, const QString &text);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Theme::Tone dot;
    QString text;
};

#endif // LOOK_WIDGETS_H
