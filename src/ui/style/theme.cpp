// theme.cpp

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QStyle>
#include <QStyleHints>
#include <QVariantAnimation>
#include <QWidget>

#include "ui/style/theme.h"

namespace Theme {

namespace {

constexpr int fadeMs = 320;
constexpr char crossfadeName[] = "rendropCrossfade";

Mode currentMode = Mode::Dark;
bool lightScheme = false;
bool following = false;

bool resolve(Mode mode)
{
    if (mode == Mode::System)
    {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
    }
    return mode == Mode::Light;
}

// A snapshot of a window in the old scheme, laid over it and faded out, so
// the new colors show through as one smooth change. Painting a pixmap with a
// falling opacity over the repainted window is the same as interpolating
// every color, without restyling hundreds of widgets on each frame.
class Crossfade : public QWidget
{
public:
    Crossfade(QWidget *window, const QPixmap &snapshot):
        QWidget(window),
        snapshot(snapshot)
    {
        setObjectName(QLatin1String(crossfadeName));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(window->rect());
        window->installEventFilter(this);
        auto *animation = new QVariantAnimation(this);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        animation->setDuration(fadeMs);
        animation->setEasingCurve(QEasingCurve::InOutQuad);
        QObject::connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            opacity = value.toReal();
            update();
        });
        QObject::connect(animation, &QVariantAnimation::finished, this, &QObject::deleteLater);
        raise();
        show();
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setOpacity(opacity);
        painter.drawPixmap(0, 0, snapshot);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // A resize mid-fade would stretch the old look; just let it go.
        if (watched == parent() && event->type() == QEvent::Resize)
        {
            deleteLater();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QPixmap snapshot;
    qreal opacity = 1.0;
};

bool fades(const QWidget *window)
{
    if (!window->isVisible() || window->isMinimized())
    {
        return false;
    }
    const Qt::WindowType type = window->windowType();
    // Popups, menus and tool tips are short-lived; they switch at once.
    return type != Qt::Popup && type != Qt::ToolTip && !window->testAttribute(Qt::WA_TranslucentBackground);
}

void apply(bool light, bool animate)
{
    if (light == lightScheme)
    {
        return;
    }
    if (animate)
    {
        for (QWidget *window : QApplication::topLevelWidgets())
        {
            if (!fades(window))
            {
                continue;
            }
            // Includes a fade still running, which then goes.
            const QPixmap snapshot = window->grab();
            for (QWidget *old : window->findChildren<QWidget *>(QLatin1String(crossfadeName), Qt::FindDirectChildrenOnly))
            {
                old->deleteLater();
                old->hide();
            }
            new Crossfade(window, snapshot);
        }
    }
    lightScheme = light;
    if (QStyle *style = QApplication::style())
    {
        QApplication::setPalette(style->standardPalette());
    }
    emit notifier()->changed();
    for (QWidget *widget : QApplication::allWidgets())
    {
        widget->update();
    }
}

void follow()
{
    if (following)
    {
        return;
    }
    following = true;
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, notifier(), []() {
        if (currentMode == Mode::System)
        {
            apply(resolve(Mode::System), true);
        }
    });
}

}

QString modeName(Mode mode)
{
    switch (mode)
    {
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::System:
        break;
    }
    return QStringLiteral("system");
}

Mode modeFromName(const QString &name)
{
    if (name == QLatin1String("dark"))
    {
        return Mode::Dark;
    }
    if (name == QLatin1String("light"))
    {
        return Mode::Light;
    }
    return Mode::System;
}

QColor Tone::color() const
{
    return lightScheme ? light : dark;
}

Notifier *notifier()
{
    static Notifier *instance = new Notifier(qApp);
    return instance;
}

bool isLight()
{
    return lightScheme;
}

Mode mode()
{
    return currentMode;
}

void init(Mode mode)
{
    currentMode = mode;
    follow();
    apply(resolve(mode), false);
}

void setMode(Mode mode)
{
    currentMode = mode;
    follow();
    apply(resolve(mode), true);
}

QColor veil(int alpha)
{
    return lightScheme ? QColor(0, 0, 0, alpha * 4 / 5) : QColor(255, 255, 255, alpha);
}

}
