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

// The palette is Material 3 SchemeTonalSpot from the Nordic seed 0x546E7A,
// the scheme the Android app uses. Role names in the comments refer to it;
// hover and pressed are its state layers (onSurface at 8% and 12%) baked in.

// Surfaces, from the window down to the inset content of a card.
// surfaceContainerLow / surfaceContainer
inline const Tone window {QColor(0x171C1Fu), QColor(0xEAEEF2u)};
// surfaceContainerHigh / surfaceContainerLow
inline const Tone frame {QColor(0x262B2Du), QColor(0xF0F4F8u)};
// surface / surfaceContainerLowest
inline const Tone surface {QColor(0x0F1417u), QColor(0xFFFFFFu)};
// Dialogs and popovers: surfaceContainerHigh / surfaceContainerLowest.
inline const Tone dialog {QColor(0x262B2Du), QColor(0xFFFFFFu)};
// Rows sitting on a card's surface (Blender versions, file rows).
inline const Tone row {QColor(0x262B2Du), QColor(0xF0F4F8u)};
inline const Tone rowHover {QColor(0x353A3Cu), QColor(0xE4E9ECu)};
inline const Tone rowPressed {QColor(0x1B2023u), QColor(0xDEE3E6u)};
// Small pills inside a row.
inline const Tone pill {QColor(0x1B2023u), QColor(0xFFFFFFu)};
inline const Tone pillHover {QColor(0x2A2F32u), QColor(0xF0F4F8u)};
inline const Tone pillPressed {QColor(0x171C1Fu), QColor(0xE4E9ECu)};
inline const Tone popup {QColor(0x262B2Du), QColor(0xFFFFFFu)};
// outlineVariant
inline const Tone popupBorder {QColor(0x40484Cu), QColor(0xC0C8CCu)};
inline const Tone menuRowHover {QColor(0x353A3Cu), QColor(0xEAEEF2u)};
inline const Tone separator {QColor(0x40484Cu), QColor(0xDCE4E9u)};
// Selected entry of the rail and the settings list; the save bar:
// secondaryContainer.
inline const Tone selected {QColor(0x354A53u), QColor(0xCFE6F2u)};
inline const Tone chip {QColor(0x303638u), QColor(0xDEE3E6u)};
inline const Tone tile {QColor(0x262B2Du), QColor(0xF0F4F8u)};
inline const Tone track {QColor(0x303638u), QColor(0xDEE3E6u)};

// Fields and plain buttons: surfaceContainerHighest / surfaceContainer.
inline const Tone control {QColor(0x303638u), QColor(0xEAEEF2u)};
inline const Tone controlHover {QColor(0x3E4446u), QColor(0xD9DDE1u)};
inline const Tone fieldHover {QColor(0x393F41u), QColor(0xDFE4E7u)};
inline const Tone controlPressed {QColor(0x454B4Du), QColor(0xD1D5D9u)};
inline const Tone controlDisabled {QColor(0x1B2023u), QColor(0xF0F4F8u)};
inline const Tone controlBorder {QColor(0x40484Cu), QColor(0xC0C8CCu)};
inline const Tone outline {QColor(0x565E62u), QColor(0xC0C8CCu)};
// onSurface at 12%.
inline const Tone disabledFill {QColor(0x2F3437u), QColor(0xE3E4E4u)};

// Accent: the filled button, focus rings, marks, the switch.
// primary, with onPrimary state layers for hover and pressed.
inline const Tone accent {QColor(0x8AD0EEu), QColor(0x0E6681u)};
inline const Tone accentFill {QColor(0x8AD0EEu), QColor(0x0E6681u)};
inline const Tone accentHover {QColor(0x7FC4E0u), QColor(0x21728Bu)};
inline const Tone accentPressed {QColor(0x79BDDAu), QColor(0x2B7890u)};
inline const Tone accentDisabled {QColor(0x2F3437u), QColor(0xE3E4E4u)};
// Accent drawn as a line or text: primary.
inline const Tone accentInk {QColor(0x8AD0EEu), QColor(0x0E6681u)};
inline const Tone accentGlyph {QColor(0x8AD0EEu), QColor(0x0E6681u)};
// Text selection: primaryContainer and onPrimaryContainer.
inline const Tone highlight {QColor(0x004D63u), QColor(0xBBE9FFu)};
inline const Tone onHighlight {QColor(0xBBE9FFu), QColor(0x004D63u)};

// Text, from the strongest down: onSurface, onSurfaceVariant, outline.
inline const Tone textStrong {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone text {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone textName {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone textHover {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone textCell {QColor(0xC0C8CCu), QColor(0x40484Cu)};
inline const Tone textQuiet {QColor(0xA3AAAEu), QColor(0x555C61u)};
inline const Tone textMuted {QColor(0xC0C8CCu), QColor(0x40484Cu)};
inline const Tone textHelp {QColor(0x8A9296u), QColor(0x70787Du)};
inline const Tone textFaint {QColor(0x8A9296u), QColor(0x70787Du)};
// onSurface at 38%.
inline const Tone textDisabled {QColor(0x63686Bu), QColor(0xA7A9AAu)};
inline const Tone textDisabledLight {QColor(0x63686Bu), QColor(0xA7A9AAu)};
inline const Tone placeholder {QColor(0x8A9296u), QColor(0x70787Du)};
// onPrimary
inline const Tone onAccent {QColor(0x003545u), QColor(0xFFFFFFu)};
// Nav rail and caption glyphs at rest and under the pointer.
inline const Tone glyph {QColor(0x8A9296u), QColor(0x70787Du)};
inline const Tone glyphHover {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone captionGlyph {QColor(0xDEE3E6u), QColor(0x40484Cu)};
// Drag handle dots, on a row.
inline const Tone handle {QColor(0x656D71u), QColor(0xA8B0B4u)};
// White glyph icons recolored for the light scheme; invalid dark keeps them
// white on the dark scheme.
inline const Tone iconGlyph {QColor(), QColor(0x40484Cu)};
inline const Tone iconFaint {QColor(), QColor(0xC0C8CCu)};

// The log and the render output preview.
inline const Tone logText {QColor(0xC0C8CCu), QColor(0x40484Cu)};
// secondaryContainer, onSecondaryContainer, secondary.
inline const Tone outputFill {QColor(0x354A53u), QColor(0xCFE6F2u)};
inline const Tone outputText {QColor(0xCFE6F2u), QColor(0x354A53u)};
inline const Tone logToggleText {QColor(0xB4CAD5u), QColor(0x4C616Bu)};

// States.
inline const Tone ok {QColor(90, 185, 120), QColor(0x2E, 0x9A, 0x55)};
inline const Tone warning {QColor(215, 165, 70), QColor(0xB7, 0x80, 0x1C)};
// error
inline const Tone error {QColor(0xFFB4ABu), QColor(0xBA1A1Au)};
inline const Tone info {QColor(0x8AD0EEu), QColor(0x0E6681u)};
inline const Tone sharing {QColor(0x8AD0EEu), QColor(0x0E6681u)};
// inversePrimary: the label on top keeps its contrast in both schemes.
inline const Tone progress {QColor(0x0E6681u), QColor(0x8AD0EEu)};
// Queue row outlines by project state.
inline const Tone rowQueued {QColor(0x40484Cu), QColor(0xC0C8CCu)};
inline const Tone rowActive {QColor(0x8AD0EEu), QColor(0x0E6681u)};
inline const Tone rowError {QColor(0xFFB4ABu), QColor(0xBA1A1Au)};
inline const Tone rowFinished {QColor(60, 150, 80), QColor(60, 150, 80)};
inline const Tone closeHover {QColor(0xC4, 0x2B, 0x1C), QColor(0xC4, 0x2B, 0x1C)};
inline const Tone closePressed {QColor(0xA3, 0x23, 0x17), QColor(0xA3, 0x23, 0x17)};

// Material 3 switch. Off: surfaceContainerHighest track with an outline
// border and an outline handle. On: primary track, onPrimary handle.
// Hovered or pressed, the handle turns onSurfaceVariant / primaryContainer.
inline const Tone switchTrackOff {QColor(0x303638u), QColor(0xDEE3E6u)};
inline const Tone switchOutline {QColor(0x8A9296u), QColor(0x70787Du)};
inline const Tone switchHandleOn {QColor(0x003545u), QColor(0xFFFFFFu)};
inline const Tone switchHandleOnHover {QColor(0x004D63u), QColor(0xBBE9FFu)};
inline const Tone switchHandleOffHover {QColor(0xC0C8CCu), QColor(0x40484Cu)};
// State layer bases: onSurface off, primary on.
inline const Tone switchStateOff {QColor(0xDEE3E6u), QColor(0x171C1Fu)};
inline const Tone switchStateOn {QColor(0x8AD0EEu), QColor(0x0E6681u)};
// Disabled: onSurface at 12% for the track and border, 38% for the off
// handle; the on handle takes the surface color.
inline const Tone switchDisabledTrack {QColor(0x2F3437u), QColor(0xE3E4E4u)};
inline const Tone switchDisabledHandleOff {QColor(0x63686Bu), QColor(0xA7A9AAu)};
inline const Tone switchDisabledHandleOn {QColor(0x0F1417u), QColor(0xF5FAFDu)};

}

#endif // THEME_H
