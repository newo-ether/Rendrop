// drop_shadow_widget.cpp

#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ui/style/drop_shadow_widget.h"

namespace {

bool globalShadowsEnabled = true;

// How far a lifted shadow moves away, relative to its resting parameters.
// A little darker too: on the dark theme a farther, softer shadow alone
// barely shows. The cap keeps the umbra from turning into a black rim.
constexpr float liftOffsetScale = 2.0f;
constexpr float liftBlurScale = 1.8f;
constexpr float liftAlphaScale = 1.35f;
constexpr float liftAlphaCap = 0.85f;
constexpr int liftMs = 220;
// Scales the Gaussian profile down to the visible weight of the old curve.
constexpr float profileStrength = 0.55f;

std::vector<DropShadowWidget *> &instances()
{
    static std::vector<DropShadowWidget *> list;
    return list;
}

float lerp(float a, float b, qreal t)
{
    return a + (b - a) * static_cast<float>(t);
}

// Template coordinates are quantized so animation frames that differ by less
// than this share a cached template.
constexpr float quantum = 1.0f / 32.0f;

int quantize(float value)
{
    return static_cast<int>(std::lround(value / quantum));
}

// Shadow of the rounded rect `rect` in an image of `size` device pixels: the
// signed distance to the rect through a Gaussian edge profile. `blur` is the
// whole penumbra, +-2.5 sigma, so the tails at its ends are under 1%.
// (The old 1 - f^0.3 curve kept a solid core with a sharp knee, which read
// as a black rim once the lift pushed the core past the card.)
QImage renderTemplate(QSize size, const QRectF &rect, float radius, float blur, float alphaMax)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    const float halfBlur = blur * 0.5f;
    const float inverseSigmaRoot2 = 1.0f / (std::max(blur, 0.001f) / 5.0f * std::sqrt(2.0f));
    const float centreX = static_cast<float>(rect.center().x());
    const float centreY = static_cast<float>(rect.center().y());
    const float halfWidth = static_cast<float>(rect.width()) * 0.5f;
    const float halfHeight = static_cast<float>(rect.height()) * 0.5f;
    for (int y = 0; y < size.height(); ++y)
    {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const float py = static_cast<float>(y) + 0.5f - centreY;
        const float dy = std::abs(py) - (halfHeight - radius);
        for (int x = 0; x < size.width(); ++x)
        {
            const float px = static_cast<float>(x) + 0.5f - centreX;
            const float dx = std::abs(px) - (halfWidth - radius);
            const float outsideX = std::max(dx, 0.0f);
            const float outsideY = std::max(dy, 0.0f);
            const float distance = std::sqrt(outsideX * outsideX + outsideY * outsideY)
                + std::min(std::max(dx, dy), 0.0f) - radius;
            float alpha = 0.0f;
            if (distance <= -halfBlur)
            {
                alpha = alphaMax;
            }
            else if (distance < halfBlur)
            {
                alpha = 0.5f * std::erfc(distance * inverseSigmaRoot2) * alphaMax;
            }
            // Same perceived weight as the old curve at the card's edge, where
            // the Gaussian sits at nearly full strength.
            alpha *= profileStrength;
            line[x] = qRgba(0, 0, 0, static_cast<int>(std::lround(alpha * 255.0f)));
        }
    }
    return image;
}

struct Segment
{
    int source;
    int sourceLength;
    int target;
    int targetLength;
};

// Splits one axis of the shadow image [0, length) into the part before the
// straight run, its single template pixel stretched over `middle` pixels, and
// the rest. `first` is the first pixel of the run; middle < 2 means no split.
std::vector<Segment> segments(int length, int first, int middle)
{
    std::vector<Segment> result;
    if (middle < 2)
    {
        result.push_back({0, length, 0, length});
        return result;
    }
    if (first > 0)
    {
        result.push_back({0, first, 0, first});
    }
    result.push_back({first, 1, first, middle});
    const int after = length - first - middle;
    if (after > 0)
    {
        result.push_back({first + 1, after, first + middle, after});
    }
    return result;
}

}

DropShadowWidget::DropShadowWidget(QWidget *parent, QWidget *targetWidget):
    QWidget(parent),
    targetWidget(targetWidget),
    lift(this, 0.0, liftMs)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    stackUnder(targetWidget);
    targetWidget->installEventFilter(this);
    instances().push_back(this);
    syncGeometry();
    refreshVisibility();
}

DropShadowWidget::~DropShadowWidget()
{
    auto &list = instances();
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
}

void DropShadowWidget::setShadowsEnabled(bool enabled)
{
    globalShadowsEnabled = enabled;
    for (DropShadowWidget *shadow : instances())
    {
        shadow->refreshVisibility();
    }
}

bool DropShadowWidget::shadowsEnabled()
{
    return globalShadowsEnabled;
}

void DropShadowWidget::refreshVisibility()
{
    // isVisibleTo: the target is a sibling, so only its own hidden state
    // matters, not whether the window is shown yet.
    setVisible(globalShadowsEnabled && targetWidget && targetWidget->isVisibleTo(parentWidget()));
}

DropShadowWidget::Params DropShadowWidget::params() const
{
    return rest;
}

DropShadowWidget::Params DropShadowWidget::liftedParams() const
{
    Params lifted = rest;
    lifted.offsetX *= liftOffsetScale;
    lifted.offsetY *= liftOffsetScale;
    lifted.blurRadius *= liftBlurScale;
    lifted.alphaMax = std::max(rest.alphaMax, std::min(liftAlphaCap, lifted.alphaMax * liftAlphaScale));
    return lifted;
}

DropShadowWidget::Params DropShadowWidget::currentParams() const
{
    const qreal t = hoverLift ? lift.value() : 0.0;
    if (t <= 0.0)
    {
        return rest;
    }
    const Params lifted = liftedParams();
    Params result;
    result.borderRadius = rest.borderRadius;
    result.offsetX = lerp(rest.offsetX, lifted.offsetX, t);
    result.offsetY = lerp(rest.offsetY, lifted.offsetY, t);
    result.alphaMax = lerp(rest.alphaMax, lifted.alphaMax, t);
    result.blurRadius = lerp(rest.blurRadius, lifted.blurRadius, t);
    return result;
}

int DropShadowWidget::margin() const
{
    const Params widest = hoverLift ? liftedParams() : rest;
    const float reach = std::max(std::abs(widest.offsetX), std::abs(widest.offsetY)) + widest.blurRadius * 0.5f;
    // One more pixel for the outward snap to whole device pixels.
    return static_cast<int>(std::ceil(reach)) + 1;
}

void DropShadowWidget::syncGeometry()
{
    if (!targetWidget)
    {
        return;
    }
    const int m = margin();
    setGeometry(targetWidget->geometry().adjusted(-m, -m, m, m));
    stackUnder(targetWidget);
}

void DropShadowWidget::paintShadow(QPainter &painter, const QRectF &target, const Params &params, qreal dpr)
{
    if (params.alphaMax <= 0.0f || target.isEmpty())
    {
        return;
    }
    // Everything below is in device pixels, so the stretched slices land on
    // whole pixels and never blend with their neighbours.
    const QPointF origin = painter.worldTransform().map(QPointF(0, 0));
    const float scale = static_cast<float>(std::max<qreal>(0.1, dpr));
    const QRectF rect(
        (target.left() + params.offsetX + origin.x()) * scale,
        (target.top() + params.offsetY + origin.y()) * scale,
        target.width() * scale,
        target.height() * scale);
    const float blur = std::max(0.001f, params.blurRadius * scale);
    const float halfBlur = blur * 0.5f;
    const float radius = std::clamp(params.borderRadius * scale, 0.0f,
        static_cast<float>(std::min(rect.width(), rect.height())) * 0.5f);
    const float alphaMax = std::clamp(params.alphaMax, 0.0f, 1.0f);
    // Columns and rows this far inside the rect are constant along the edge.
    const float inset = std::max(radius, halfBlur);

    const int x0 = static_cast<int>(std::floor(rect.left() - halfBlur));
    const int y0 = static_cast<int>(std::floor(rect.top() - halfBlur));
    const int width = static_cast<int>(std::ceil(rect.right() + halfBlur)) - x0;
    const int height = static_cast<int>(std::ceil(rect.bottom() + halfBlur)) - y0;
    const QRectF local = rect.translated(-x0, -y0);

    // Pixel centres c + 0.5 inside [edge + inset, far edge - inset].
    const int firstX = static_cast<int>(std::ceil(local.left() + inset - 0.5));
    const int middleX = static_cast<int>(std::floor(local.right() - inset - 0.5)) - firstX + 1;
    const int firstY = static_cast<int>(std::ceil(local.top() + inset - 0.5));
    const int middleY = static_cast<int>(std::floor(local.bottom() - inset - 0.5)) - firstY + 1;
    const int shrinkX = middleX >= 2 ? middleX - 1 : 0;
    const int shrinkY = middleY >= 2 ? middleY - 1 : 0;

    // The template is the same shadow with the straight runs cut down to a
    // single pixel; cutting whole pixels keeps the far edges' subpixel phase.
    const QSize templateSize(width - shrinkX, height - shrinkY);
    const QRectF templateRect(local.left(), local.top(), local.width() - shrinkX, local.height() - shrinkY);
    const QString key = QStringLiteral("rendrop-shadow:%1x%2:%3,%4,%5,%6:%7:%8:%9")
        .arg(templateSize.width())
        .arg(templateSize.height())
        .arg(quantize(static_cast<float>(templateRect.left())))
        .arg(quantize(static_cast<float>(templateRect.top())))
        .arg(quantize(static_cast<float>(templateRect.right())))
        .arg(quantize(static_cast<float>(templateRect.bottom())))
        .arg(quantize(radius))
        .arg(quantize(blur))
        .arg(static_cast<int>(std::lround(alphaMax * 255.0f)));
    QPixmap shadow;
    if (!QPixmapCache::find(key, &shadow))
    {
        const auto snap = [](qreal value) { return std::round(value / quantum) * quantum; };
        const QRectF snapped(QPointF(snap(templateRect.left()), snap(templateRect.top())),
            QPointF(snap(templateRect.right()), snap(templateRect.bottom())));
        shadow = QPixmap::fromImage(renderTemplate(templateSize, snapped,
            static_cast<float>(snap(radius)), static_cast<float>(snap(blur)), alphaMax));
        QPixmapCache::insert(key, shadow);
    }

    painter.save();
    painter.resetTransform();
    painter.scale(1.0 / scale, 1.0 / scale);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const Segment &row : segments(height, firstY, middleY))
    {
        for (const Segment &column : segments(width, firstX, middleX))
        {
            painter.drawPixmap(
                QRectF(x0 + column.target, y0 + row.target, column.targetLength, row.targetLength),
                shadow,
                QRectF(column.source, row.source, column.sourceLength, row.sourceLength));
        }
    }
    painter.restore();
}

void DropShadowWidget::paintEvent(QPaintEvent *)
{
    if (!targetWidget || fade <= 0.0)
    {
        return;
    }
    QPainter painter(this);
    painter.setOpacity(fade);
    const int m = margin();
    paintShadow(painter, QRectF(m, m, targetWidget->width(), targetWidget->height()), currentParams(), devicePixelRatioF());
}

bool DropShadowWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == targetWidget)
    {
        switch (event->type())
        {
        case QEvent::Move:
        case QEvent::Resize:
            syncGeometry();
            break;
        case QEvent::Show:
            refreshVisibility();
            break;
        case QEvent::Hide:
            lift.jumpTo(0.0);
            refreshVisibility();
            break;
        case QEvent::Enter:
            if (hoverLift)
            {
                lift.setTarget(1.0);
            }
            break;
        case QEvent::Leave:
            if (hoverLift)
            {
                lift.setTarget(0.0);
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DropShadowWidget::setBorderRadius(float borderRadius)
{
    rest.borderRadius = borderRadius;
    update();
}

void DropShadowWidget::setOffsetX(float offsetX)
{
    rest.offsetX = offsetX;
    syncGeometry();
}

void DropShadowWidget::setOffsetY(float offsetY)
{
    rest.offsetY = offsetY;
    syncGeometry();
}

void DropShadowWidget::setAlphaMax(float alphaMax)
{
    rest.alphaMax = alphaMax;
    update();
}

void DropShadowWidget::setBlurRadius(float blurRadius)
{
    rest.blurRadius = blurRadius;
    syncGeometry();
}

void DropShadowWidget::setHoverLift(bool enabled)
{
    hoverLift = enabled;
    if (!enabled)
    {
        lift.jumpTo(0.0);
    }
    syncGeometry();
}

void DropShadowWidget::setFade(qreal value)
{
    value = std::clamp(value, 0.0, 1.0);
    if (std::abs(fade - value) < 1e-4)
    {
        return;
    }
    fade = value;
    update();
}
