// theme.h

#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QObject>
#include <QString>

// Dark and light color schemes. Every color the UI paints is a Tone, a pair
// of the dark and the light value, resolved when it is used; switching the
// scheme repaints the windows and cross-fades from a snapshot of the old look.
namespace Theme {

enum class Mode
{
    System,
    Dark,
    Light
};

QString modeName(Mode mode);
Mode modeFromName(const QString &name);

struct Tone
{
    QColor dark;
    QColor light;

    Tone() = default;
    // One color for both schemes.
    Tone(const QColor &color): dark(color), light(color) {}
    Tone(Qt::GlobalColor color): dark(color), light(color) {}
    Tone(const QColor &dark, const QColor &light): dark(dark), light(light) {}

    // The value for the scheme on screen.
    QColor color() const;
    operator QColor() const { return color(); }
    // Invalid tones mean "none" or "fall back", as invalid QColors do.
    bool isValid() const { return dark.isValid(); }
    QColor lighter(int factor) const { return color().lighter(factor); }
};

// Emits changed() after the scheme switched and the application palette
// follows it. Anything that keeps a resolved color re-reads it there.
class Notifier : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void changed();
};

Notifier *notifier();
bool isLight();
Mode mode();
// Before any window exists: picks the scheme without a fade and starts
// following the system scheme for Mode::System.
void init(Mode mode);
// Switches at once if no window is shown, otherwise with a cross-fade.
void setMode(Mode mode);

// Hover tint over any surface: white on the dark scheme, black on the light.
QColor veil(int alpha);

// Surfaces, from the window down to the inset content of a card.
inline const Tone window {QColor(0x20, 0x21, 0x24), QColor(0xE9, 0xEC, 0xF1)};
inline const Tone frame {QColor(0x2C, 0x31, 0x3C), QColor(0xF6, 0xF7, 0xF9)};
inline const Tone surface {QColor(26, 28, 32), QColor(0xFF, 0xFF, 0xFF)};
// Dialogs and popovers.
inline const Tone dialog {QColor(43, 45, 48), QColor(0xFF, 0xFF, 0xFF)};
// Rows sitting on a card's surface (Blender versions, file rows).
inline const Tone row {QColor(43, 45, 48), QColor(0xF1, 0xF3, 0xF6)};
inline const Tone rowHover {QColor(52, 55, 62), QColor(0xE9, 0xEC, 0xF0)};
inline const Tone rowPressed {QColor(35, 37, 41), QColor(0xE0, 0xE3, 0xE9)};
inline const Tone popup {QColor(0x2B, 0x2E, 0x35), QColor(0xFF, 0xFF, 0xFF)};
inline const Tone popupBorder {QColor(0x45, 0x4A, 0x56), QColor(0xCF, 0xD3, 0xDB)};
inline const Tone menuRowHover {QColor(0x4B, 0x53, 0x62), QColor(0xE6, 0xE8, 0xF2)};
inline const Tone separator {QColor(0x3A, 0x3E, 0x48), QColor(0xE1, 0xE4, 0xEA)};
// Selected entry of the rail and the settings list; the save bar.
inline const Tone selected {QColor(0x2B, 0x2D, 0x40), QColor(0xE2, 0xE3, 0xF6)};
inline const Tone chip {QColor(0x2B, 0x2D, 0x33), QColor(0xDF, 0xE2, 0xE8)};
inline const Tone tile {QColor(0x2B, 0x2E, 0x35), QColor(0xF1, 0xF3, 0xF6)};
inline const Tone track {QColor(0x33, 0x35, 0x3C), QColor(0xE1, 0xE4, 0xEA)};

// Fields and plain buttons.
inline const Tone control {QColor(0x33, 0x38, 0x42), QColor(0xEC, 0xEE, 0xF2)};
inline const Tone controlHover {QColor(0x4B, 0x53, 0x62), QColor(0xE0, 0xE3, 0xE9)};
inline const Tone fieldHover {QColor(0x3C, 0x42, 0x4D), QColor(0xE5, 0xE8, 0xED)};
inline const Tone controlPressed {QColor(0x2B, 0x2F, 0x38), QColor(0xD5, 0xD9, 0xE0)};
inline const Tone controlDisabled {QColor(0x2E, 0x31, 0x38), QColor(0xF2, 0xF3, 0xF5)};
inline const Tone controlBorder {QColor(65, 65, 85), QColor(0xCC, 0xD0, 0xD9)};
inline const Tone outline {QColor(0x55, 0x5A, 0x66), QColor(0xC4, 0xC8, 0xD1)};
inline const Tone disabledFill {QColor(0x55, 0x59, 0x5F), QColor(0xC9, 0xCC, 0xD3)};

// Accent: the filled button, focus rings, marks, the switch.
inline const Tone accent {QColor(110, 110, 180), QColor(0x5A, 0x5A, 0xC0)};
inline const Tone accentFill {QColor(90, 90, 150), QColor(0x5E, 0x5E, 0xC4)};
inline const Tone accentHover {QColor(120, 120, 180), QColor(0x70, 0x70, 0xD0)};
inline const Tone accentPressed {QColor(70, 70, 130), QColor(0x4C, 0x4C, 0xA8)};
inline const Tone accentDisabled {QColor(85, 89, 95), QColor(0xC3, 0xC6, 0xCE)};
// Accent drawn as a line or text, lighter on dark and darker on light.
inline const Tone accentInk {QColor(143, 143, 234), QColor(0x4B, 0x4B, 0xB4)};
inline const Tone accentGlyph {QColor(0xB4, 0xB8, 0xFF), QColor(0x4B, 0x4B, 0xB4)};
inline const Tone highlight {QColor(0x5A, 0x5A, 0x96), QColor(0x6A, 0x6A, 0xCC)};

// Text, from the strongest down.
inline const Tone textStrong {QColor(0xFF, 0xFF, 0xFF), QColor(0x14, 0x16, 0x1B)};
inline const Tone text {QColor(0xE0, 0xE0, 0xE0), QColor(0x22, 0x25, 0x2B)};
inline const Tone textName {QColor(224, 224, 232), QColor(0x26, 0x29, 0x30)};
inline const Tone textHover {QColor(0xD0, 0xD3, 0xDC), QColor(0x30, 0x33, 0x3B)};
inline const Tone textCell {QColor(170, 174, 186), QColor(0x50, 0x55, 0x61)};
inline const Tone textQuiet {QColor(150, 155, 170), QColor(0x60, 0x65, 0x73)};
inline const Tone textMuted {QColor(0xA8, 0xAC, 0xB8), QColor(0x5A, 0x5F, 0x6B)};
inline const Tone textHelp {QColor(110, 115, 130), QColor(0x86, 0x8B, 0x98)};
inline const Tone textFaint {QColor(100, 104, 115), QColor(0xA0, 0xA4, 0xAE)};
inline const Tone textDisabled {QColor(0x88, 0x88, 0x88), QColor(0xA6, 0xAA, 0xB3)};
inline const Tone textDisabledLight {QColor(0xAA, 0xAA, 0xAA), QColor(0xA6, 0xAA, 0xB3)};
inline const Tone placeholder {QColor(0x6E, 0x73, 0x82), QColor(0x9C, 0xA1, 0xAC)};
inline const Tone onAccent {QColor(0xFF, 0xFF, 0xFF), QColor(0xFF, 0xFF, 0xFF)};
// Nav rail and caption glyphs at rest and under the pointer.
inline const Tone glyph {QColor(0x8A, 0x8F, 0x9C), QColor(0x7A, 0x7F, 0x8C)};
inline const Tone glyphHover {QColor(0xC4, 0xC8, 0xD4), QColor(0x3A, 0x3E, 0x48)};
inline const Tone captionGlyph {QColor(0xE0, 0xE0, 0xE0), QColor(0x3A, 0x3D, 0x44)};

// The log and the render output preview.
inline const Tone logText {QColor(180, 180, 200), QColor(0x4A, 0x4E, 0x5C)};
inline const Tone outputFill {QColor(80, 80, 130), QColor(0xE4, 0xE4, 0xF6)};
inline const Tone outputText {QColor(200, 200, 240), QColor(0x3A, 0x3A, 0x78)};
inline const Tone logToggleText {QColor(150, 150, 190), QColor(0x60, 0x60, 0x96)};

// States.
inline const Tone ok {QColor(90, 185, 120), QColor(0x2E, 0x9A, 0x55)};
inline const Tone warning {QColor(215, 165, 70), QColor(0xB7, 0x80, 0x1C)};
inline const Tone error {QColor(225, 110, 110), QColor(0xCF, 0x45, 0x45)};
inline const Tone info {QColor(80, 150, 210), QColor(0x2F, 0x7F, 0xC4)};
inline const Tone sharing {QColor(90, 160, 220), QColor(0x2F, 0x86, 0xC8)};
inline const Tone progress {QColor(66, 165, 245), QColor(0x1E, 0x88, 0xE5)};
inline const Tone closeHover {QColor(0xC4, 0x2B, 0x1C), QColor(0xC4, 0x2B, 0x1C)};
inline const Tone closePressed {QColor(0xA3, 0x23, 0x17), QColor(0xA3, 0x23, 0x17)};

// Switch knob.
inline const Tone knob {QColor(240, 240, 248), QColor(0xFF, 0xFF, 0xFF)};
inline const Tone knobDisabled {QColor(170, 170, 170), QColor(0xE4, 0xE6, 0xEA)};
inline const Tone switchOff {QColor(0x33, 0x38, 0x42), QColor(0xCF, 0xD3, 0xDB)};

}

#endif // THEME_H
