// log_panel.h

#ifndef LOG_PANEL_H
#define LOG_PANEL_H

#include <QPointer>
#include <QString>
#include <QWidget>

class DropShadowWidget;
class QPlainTextEdit;
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
    void append(const QString &text);
    // Where the card is headed; it stays visible while it shrinks away.
    bool isShown() const { return shown; }
    void setShown(bool visible);

private:
    // Scrolls to the newest line unless the user scrolled up.
    void followTail();

    QPlainTextEdit *view = nullptr;
    QPointer<QVariantAnimation> animation;
    QPointer<DropShadowWidget> shadow;
    bool shown = false;
    bool follow = true;
};

#endif // LOG_PANEL_H
