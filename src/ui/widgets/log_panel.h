// log_panel.h

#ifndef LOG_PANEL_H
#define LOG_PANEL_H

#include <QPointer>
#include <QString>
#include <QWidget>

class DropShadowWidget;
class QPlainTextEdit;
class QSpacerItem;
class QVariantAnimation;

// The log card: timestamped lines in a read-only view that follows its
// newest line like a terminal, unless the user scrolled up to read.
// Showing and hiding grows or shrinks the card once its page is on screen.
class LogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);

    // Fades with the card while it grows or shrinks.
    void setShadow(DropShadowWidget *shadow);
    // The layout gap in front of the card. The card brings it along when it
    // grows in and takes it away when it shrinks out, so the cards around it
    // never jump by a gap at either end. The page layout itself has no
    // spacing next to the card.
    void setLeadingGap(QSpacerItem *gap, int size);
    void append(const QString &text);
    // Where the card is headed; it stays visible while it shrinks away.
    bool isShown() const { return shown; }
    void setShown(bool visible);

private:
    // Scrolls to the newest line unless the user scrolled up.
    void followTail();
    // Gap plus card height, 0 to logHeight + gap.
    void applySlot(int value);

    QPlainTextEdit *view = nullptr;
    QPointer<QVariantAnimation> animation;
    QPointer<DropShadowWidget> shadow;
    QSpacerItem *gap = nullptr;
    int gapSize = 0;
    int slot = 0;
    bool shown = false;
    bool follow = true;
};

#endif // LOG_PANEL_H
