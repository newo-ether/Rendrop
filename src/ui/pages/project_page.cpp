// project_page.cpp

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolTip>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include "ui/style/look_widgets.h"
#include "ui/motion/smooth_scroll.h"
#include "ui/pages/project_page.h"
#include "core/render_queue.h"
#include "ui/style/rounded_surface.h"
#include "ui/motion/transition.h"

namespace {

constexpr int barHeight = 10;
constexpr int legendGap = 12;
constexpr int legendHeight = 18;

constexpr int gridGap = 10;
constexpr int minimumTile = 120;
constexpr int labelHeight = 22;
constexpr int headerHeight = 34;
constexpr int groupGap = 8;
constexpr int emptyHeight = 180;
// Same length and curve as the phone's fold (Compose tween default,
// FastOutSlowIn).
constexpr int foldMs = 400;

QEasingCurve foldCurve()
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(0.4, 0.0), QPointF(0.2, 1.0), QPointF(1.0, 1.0));
    return curve;
}

const QColor helpColor(110, 115, 130);
const QColor tileColor(0x2B, 0x2E, 0x35);

QColor stateColor(ProjectState state)
{
    switch (state)
    {
    case ProjectState::Rendering: return QColor(80, 150, 210);
    case ProjectState::Finished: return QColor(90, 185, 120);
    case ProjectState::Error: return QColor(225, 110, 110);
    case ProjectState::Loading:
    case ProjectState::Queued: break;
    }
    return Look::accentColor.lighter(120);
}

QString stateText(ProjectState state)
{
    switch (state)
    {
    case ProjectState::Loading: return QObject::tr("Loading");
    case ProjectState::Queued: return QObject::tr("Queued");
    case ProjectState::Error: return QObject::tr("Error");
    case ProjectState::Rendering: return QObject::tr("Rendering");
    case ProjectState::Finished: return QObject::tr("Finished");
    }
    return QString();
}

int frameNumber(const ProjectInfo &project, int index)
{
    return project.frameStart + index * project.frameStep;
}

// Remote owner of every frame index; empty for this machine.
std::vector<QString> ownersByIndex(const ProjectInfo &project)
{
    std::vector<QString> owners(static_cast<size_t>(std::max(0, project.totalFrame)));
    for (const FrameOwnerRanges &owner : project.frameOwners)
    {
        if (owner.owner.isEmpty())
        {
            continue;
        }
        for (const auto &[first, last] : owner.ranges)
        {
            for (int index = std::max(0, first); index <= std::min(last, project.totalFrame - 1); ++index)
            {
                owners[static_cast<size_t>(index)] = owner.owner;
            }
        }
    }
    return owners;
}

// Frame, header and nodes page cards: a frame around a darker surface.
QWidget *card(QWidget *parent, QLayout *content, const QMargins &margins)
{
    auto *frame = new QWidget(parent);
    frame->setProperty(Look::cardProperty, true);
    RoundedSurface::install(frame, 12, Look::frameColor);
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(5, 5, 5, 5);
    auto *surface = new QWidget(frame);
    RoundedSurface::install(surface, 10, Look::surfaceColor);
    content->setContentsMargins(margins);
    surface->setLayout(content);
    frameLayout->addWidget(surface);
    return frame;
}

QLabel *label(QWidget *parent, int pointSize, const QColor &color)
{
    auto *result = new QLabel(parent);
    result->setFont(Look::pixelFont(pointSize));
    Look::setTextColor(result, color);
    return result;
}

}

namespace ProjectView {

QColor nodeColor(const QString &name)
{
    static const std::array<QColor, 6> palette {
        QColor(0x3A, 0x7D, 0x44), QColor(0xC2, 0x7C, 0x2C), QColor(0x3E, 0x6F, 0xB0),
        QColor(0xA8, 0x50, 0x8C), QColor(0x2A, 0x8C, 0x8C), QColor(0xB5, 0x52, 0x3B)
    };
    // Java's String.hashCode over UTF-16 units, with its 32-bit wraparound.
    quint32 hash = 0;
    for (const QChar unit : name)
    {
        hash = 31u * hash + unit.unicode();
    }
    const qint32 value = static_cast<qint32>(hash);
    const int size = static_cast<int>(palette.size());
    return palette[static_cast<size_t>(((value % size) + size) % size)];
}

std::vector<int> finishedIndices(const ProjectInfo &project)
{
    std::vector<int> indices;
    for (const auto &[first, last] : project.completedRanges)
    {
        for (int index = std::max(0, first); index <= std::min(last, project.totalFrame - 1); ++index)
        {
            indices.push_back(index);
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (static_cast<int>(indices.size()) != project.finishedFrame)
    {
        // Ranges missing or out of step with the count: the frames went in order.
        indices.clear();
        for (int index = 0; index < std::min(project.finishedFrame, project.totalFrame); ++index)
        {
            indices.push_back(index);
        }
    }
    return indices;
}

std::vector<Segment> segments(const ProjectInfo &project)
{
    std::vector<Segment> result;
    if (project.totalFrame <= 0)
    {
        return result;
    }
    const std::vector<QString> owners = ownersByIndex(project);
    for (const int index : finishedIndices(project))
    {
        const QString &owner = owners[static_cast<size_t>(index)];
        if (!result.empty() && result.back().end == index && result.back().node == owner)
        {
            ++result.back().end;
        }
        else
        {
            result.push_back(Segment {index, index + 1, owner, false});
        }
    }
    for (const FrameOwnerRanges &lease : project.frameLeases)
    {
        for (const auto &[first, last] : lease.ranges)
        {
            const int start = std::max(0, first);
            const int end = std::min(last, project.totalFrame - 1) + 1;
            if (end > start)
            {
                result.push_back(Segment {start, end, lease.owner, true});
            }
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Segment &a, const Segment &b) {
        return a.start < b.start;
    });
    return result;
}

}

ProgressOverview::ProgressOverview(QWidget *parent):
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProgressOverview::setProject(const ProjectInfo &newProject, const QString &localName)
{
    project = newProject;
    runs = ProjectView::segments(project);
    stateColor = ::stateColor(project.state);
    const bool hadLegend = multiNode;
    multiNode = project.state != ProjectState::Error
        && std::any_of(runs.begin(), runs.end(), [](const ProjectView::Segment &run) { return !run.node.isEmpty(); });

    // Frames and leased frames per node, this machine first.
    std::map<QString, std::pair<int, int>> counts;
    for (const ProjectView::Segment &run : runs)
    {
        auto &entry = counts[run.node];
        (run.leased ? entry.second : entry.first) += run.end - run.start;
    }
    legend.clear();
    for (const auto &[node, count] : counts)
    {
        const QString name = node.isEmpty() ? localName : node;
        legend.push_back(LegendEntry {name, ProjectView::nodeColor(name), count.first, count.second});
        if (node.isEmpty())
        {
            std::rotate(legend.begin(), legend.end() - 1, legend.end());
        }
    }
    // Runs keep their node; the local one paints with the local name's color.
    for (ProjectView::Segment &run : runs)
    {
        if (run.node.isEmpty())
        {
            run.node = localName;
        }
    }
    if (hadLegend != multiNode)
    {
        updateGeometry();
    }
    update();
}

QSize ProgressOverview::sizeHint() const
{
    return {400, barHeight + (multiNode ? legendGap + legendHeight : 0)};
}

QSize ProgressOverview::minimumSizeHint() const
{
    return {100, sizeHint().height()};
}

void ProgressOverview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bar(0, 0, width(), barHeight);
    QPainterPath clip;
    clip.addRoundedRect(bar, barHeight / 2.0, barHeight / 2.0);
    painter.save();
    painter.setClipPath(clip);
    QColor track = multiNode ? QColor(150, 155, 170) : stateColor;
    track.setAlphaF(0.18f);
    painter.fillRect(bar, track);
    const int total = project.totalFrame;
    if (total > 0)
    {
        const qreal unit = bar.width() / total;
        constexpr qreal gap = 2.0;
        // Only touching runs of different nodes need a visible break.
        const auto breaks = [this](const ProjectView::Segment &a, const ProjectView::Segment &b) {
            return multiNode && a.end == b.start && a.node != b.node;
        };
        for (size_t index = 0; index < runs.size(); ++index)
        {
            const ProjectView::Segment &run = runs[index];
            qreal left = run.start * unit;
            qreal right = run.end * unit;
            if (index > 0 && breaks(runs[index - 1], run))
            {
                left += gap / 2.0;
            }
            if (index + 1 < runs.size() && breaks(run, runs[index + 1]))
            {
                right -= gap / 2.0;
            }
            if (right <= left)
            {
                continue;
            }
            QColor fill = multiNode ? ProjectView::nodeColor(run.node) : stateColor;
            if (run.leased)
            {
                fill.setAlphaF(0.38f);
            }
            painter.fillRect(QRectF(left, 0, right - left, barHeight), fill);
        }
    }
    painter.restore();

    if (!multiNode)
    {
        return;
    }
    const QFont nameFont = Look::pixelFont(9, QFont::Medium);
    const QFontMetrics metrics(nameFont);
    painter.setFont(nameFont);
    qreal x = 0;
    const qreal top = barHeight + legendGap;
    for (const LegendEntry &entry : legend)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(entry.color);
        painter.drawEllipse(QRectF(x, top + legendHeight / 2.0 - 4, 8, 8));
        x += 14;
        painter.setPen(Look::textColor);
        painter.drawText(QRectF(x, top, metrics.horizontalAdvance(entry.name) + 1, legendHeight),
            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip, entry.name);
        x += metrics.horizontalAdvance(entry.name) + 8;
        QString count = QString::number(entry.frames);
        if (entry.leased > 0)
        {
            count += QStringLiteral(" +%1").arg(entry.leased);
        }
        painter.setPen(helpColor);
        painter.drawText(QRectF(x, top, metrics.horizontalAdvance(count) + 1, legendHeight),
            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip, count);
        x += metrics.horizontalAdvance(count) + 22;
    }
}

FrameGrid::FrameGrid(RenderQueue *queue, QWidget *parent):
    QWidget(parent),
    queue(queue)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setMouseTracking(true);
    loader.setMaxThreadCount(2);
    // Cost is in KiB.
    thumbnails.setMaxCost(96 * 1024);
}

FrameGrid::~FrameGrid()
{
    loader.clear();
    loader.waitForDone();
    clearFolds();
}

void FrameGrid::clearFolds()
{
    for (auto &[node, fold] : folds)
    {
        delete fold.animation;
    }
    folds.clear();
}

qreal FrameGrid::foldProgress(const QString &node) const
{
    const auto found = folds.find(node);
    return found == folds.end() ? 1.0 : found->second.progress;
}

void FrameGrid::relayout()
{
    items = layoutFor(width());
    updateGeometry();
    update();
}

void FrameGrid::setGroupExpanded(const QString &node, bool expanded, bool animated)
{
    Fold &fold = folds[node];
    fold.expanded = expanded;
    const qreal target = expanded ? 1.0 : 0.0;
    if (!animated)
    {
        if (fold.animation)
        {
            fold.animation->stop();
        }
        fold.progress = target;
        relayout();
        return;
    }
    if (!fold.animation)
    {
        fold.animation = new QVariantAnimation(this);
        fold.animation->setEasingCurve(foldCurve());
        QObject::connect(fold.animation, &QVariantAnimation::valueChanged, this, [this, node](const QVariant &value) {
            const auto found = folds.find(node);
            if (found != folds.end())
            {
                found->second.progress = value.toReal();
                relayout();
            }
        });
    }
    fold.animation->stop();
    // A reversal mid-way takes only the part of the trip that is left.
    const int duration = static_cast<int>(std::round(foldMs * std::abs(target - fold.progress)));
    if (duration <= 0)
    {
        fold.progress = target;
        relayout();
        return;
    }
    fold.animation->setDuration(duration);
    fold.animation->setStartValue(fold.progress);
    fold.animation->setEndValue(target);
    fold.animation->start();
}

void FrameGrid::setProject(const ProjectInfo &newProject, const QString &newLocalName)
{
    // A reload or a new range renders the files again; old thumbnails would
    // show stale images.
    const bool restarted = newProject.id != project.id
        || newProject.frameStart != project.frameStart
        || newProject.frameStep != project.frameStep
        || newProject.outputPath != project.outputPath
        || newProject.finishedFrame < project.finishedFrame;
    if (newProject.id != project.id)
    {
        clearFolds();
    }
    project = newProject;
    localName = newLocalName;
    if (restarted)
    {
        resetThumbnails();
    }

    const std::vector<QString> owners = ownersByIndex(project);
    std::map<QString, std::vector<int>> byNode;
    for (const int index : ProjectView::finishedIndices(project))
    {
        byNode[owners[static_cast<size_t>(index)]].push_back(frameNumber(project, index));
    }
    groups.clear();
    for (auto &[node, frames] : byNode)
    {
        // Newest first, as on the phone.
        std::reverse(frames.begin(), frames.end());
        groups.push_back(Group {node, std::move(frames)});
        if (node.isEmpty())
        {
            std::rotate(groups.begin(), groups.end() - 1, groups.end());
        }
    }
    items = layoutFor(width());
    updateGeometry();
    update();
}

bool FrameGrid::hasHeightForWidth() const
{
    return true;
}

int FrameGrid::tileSize(int width) const
{
    const int columns = std::max(1, (width + gridGap) / (minimumTile + gridGap));
    return std::max(40, (width - gridGap * (columns - 1)) / columns);
}

std::vector<FrameGrid::Item> FrameGrid::layoutFor(int width) const
{
    std::vector<Item> result;
    const int tile = tileSize(width);
    const int columns = std::max(1, (width + gridGap) / (tile + gridGap));
    const bool grouped = groups.size() > 1;
    // A folding group moves at most a window's height, as on the phone, so a
    // long group does not race past; it settles to its full height at the end.
    const int travelCap = std::max(200, window() ? window()->height() : 800);
    int y = 0;
    for (size_t group = 0; group < groups.size(); ++group)
    {
        if (grouped)
        {
            if (group > 0)
            {
                y += groupGap;
            }
            const QRect header(0, y, width, headerHeight);
            result.push_back(Item {header, -1, static_cast<int>(group), header, 1.0});
            y += headerHeight;
        }
        const std::vector<int> &frames = groups[group].frames;
        const int rows = (static_cast<int>(frames.size()) + columns - 1) / columns;
        const int contentHeight = rows * (tile + labelHeight + gridGap);
        const qreal progress = grouped ? foldProgress(groups[group].node) : 1.0;
        int visible = contentHeight;
        int shift = 0;
        if (progress < 1.0)
        {
            // The group's bottom shows first and its top comes down last.
            const int travel = std::min(contentHeight, travelCap);
            visible = static_cast<int>(std::round(travel * progress));
            shift = visible - travel;
        }
        const QRect clip(0, y, width, visible);
        if (visible > 0)
        {
            for (size_t index = 0; index < frames.size(); ++index)
            {
                const int column = static_cast<int>(index) % columns;
                const int row = static_cast<int>(index) / columns;
                const QRect rect(column * (tile + gridGap), y + shift + row * (tile + labelHeight + gridGap),
                    tile, tile + labelHeight);
                if (rect.intersects(clip))
                {
                    result.push_back(Item {rect, frames[index], static_cast<int>(group), clip, progress});
                }
            }
        }
        y += visible;
    }
    return result;
}

int FrameGrid::heightForWidth(int width) const
{
    if (groups.empty())
    {
        return emptyHeight;
    }
    const std::vector<Item> layout = layoutFor(width);
    int bottom = 0;
    for (const Item &item : layout)
    {
        bottom = std::max(bottom, item.clip.bottom() + 1);
    }
    return bottom;
}

QSize FrameGrid::sizeHint() const
{
    const int width = std::max(this->width(), 400);
    return {width, heightForWidth(width)};
}

void FrameGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    items = layoutFor(width());
}

QString FrameGrid::nodeName(const QString &node) const
{
    return node.isEmpty() ? localName : node;
}

void FrameGrid::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (groups.empty())
    {
        painter.setFont(Look::pixelFont(10));
        painter.setPen(helpColor);
        painter.drawText(rect(), Qt::AlignCenter, tr("No finished frames yet."));
        return;
    }
    const QFont headerFont = Look::pixelFont(10, QFont::Medium);
    const QFont countFont = Look::monoFont(8);
    const QRect exposed = event->rect();
    for (const Item &item : items)
    {
        if (!item.rect.intersects(exposed) || !item.clip.intersects(exposed))
        {
            continue;
        }
        const Group &group = groups[static_cast<size_t>(item.group)];
        if (item.frame < 0)
        {
            // Keys above any frame number keep header fades apart from tiles.
            const qreal headerHover = Fader::level(this, static_cast<quintptr>(-2 - item.group),
                item.group == hoveredHeader);
            if (headerHover > 0.0)
            {
                QColor veil(255, 255, 255);
                veil.setAlphaF(static_cast<float>(0.05 * headerHover));
                painter.setPen(Qt::NoPen);
                painter.setBrush(veil);
                painter.drawRoundedRect(QRectF(item.rect).adjusted(0, 2, 0, -2), 8, 8);
            }
            const QString name = nodeName(group.node);
            painter.setPen(Qt::NoPen);
            painter.setBrush(ProjectView::nodeColor(name));
            painter.drawEllipse(QRectF(item.rect.left() + 10, item.rect.center().y() - 4, 9, 9));
            painter.setFont(headerFont);
            painter.setPen(Qt::white);
            painter.drawText(item.rect.adjusted(28, 0, -110, 0), Qt::AlignVCenter | Qt::AlignLeft, name);
            painter.setFont(countFont);
            painter.setPen(helpColor);
            painter.drawText(item.rect.adjusted(0, 0, -32, 0), Qt::AlignVCenter | Qt::AlignRight,
                tr("%n frame(s)", nullptr, static_cast<int>(group.frames.size())));

            // Chevron: down when open, turning to point right as it folds.
            const qreal open = foldProgress(group.node);
            painter.save();
            painter.translate(item.rect.right() - 14.0, item.rect.center().y() + 0.5);
            painter.rotate(-90.0 * (1.0 - open));
            painter.setPen(QPen(Look::mix(helpColor, Qt::white, headerHover), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            const QPointF chevron[] = {QPointF(-4.0, -2.0), QPointF(0.0, 2.0), QPointF(4.0, -2.0)};
            painter.drawPolyline(chevron, 3);
            painter.restore();
            continue;
        }

        painter.save();
        painter.setClipRect(item.clip, Qt::IntersectClip);
        painter.setOpacity(item.opacity);
        const qreal hover = Fader::level(this, static_cast<quintptr>(item.frame), item.frame == hovered);
        const QRectF tile(item.rect.left(), item.rect.top(), item.rect.width(), item.rect.width());
        QPainterPath shape;
        shape.addRoundedRect(tile, 10, 10);
        painter.fillPath(shape, tileColor);
        const QPixmap *thumbnail = thumbnails.object(item.frame);
        if (thumbnail && !thumbnail->isNull())
        {
            // Cover the square tile, cropping the longer side.
            const QSizeF source = thumbnail->deviceIndependentSize();
            const qreal side = std::min(source.width(), source.height());
            const QRectF crop((source.width() - side) / 2.0, (source.height() - side) / 2.0, side, side);
            const qreal ratio = thumbnail->devicePixelRatio();
            painter.save();
            painter.setClipPath(shape, Qt::IntersectClip);
            painter.drawPixmap(tile, *thumbnail,
                QRectF(crop.topLeft() * ratio, crop.size() * ratio));
            painter.restore();
        }
        else
        {
            if (!unreadable.contains(item.frame))
            {
                requestThumbnail(item.frame);
            }
            painter.setFont(countFont);
            painter.setPen(helpColor);
            const QString text = unreadable.contains(item.frame)
                ? QFileInfo(queue->framePath(project.id, item.frame)).suffix().toUpper()
                : QStringLiteral("...");
            painter.drawText(tile, Qt::AlignCenter, text);
        }
        if (hover > 0.0)
        {
            QColor veil(255, 255, 255);
            veil.setAlphaF(static_cast<float>(0.08 * hover));
            painter.fillPath(shape, veil);
            QColor ring = Look::accentColor.lighter(130);
            ring.setAlphaF(static_cast<float>(hover));
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(tile.adjusted(0.75, 0.75, -0.75, -0.75), 9.5, 9.5);
        }
        painter.setFont(countFont);
        painter.setPen(Look::mix(helpColor, Qt::white, hover));
        painter.drawText(QRectF(item.rect.left(), tile.bottom(), item.rect.width(), labelHeight),
            Qt::AlignCenter, QString::number(item.frame));
        painter.restore();
    }
}

int FrameGrid::frameAt(const QPoint &position) const
{
    for (const Item &item : items)
    {
        if (item.frame >= 0 && item.rect.contains(position) && item.clip.contains(position))
        {
            return item.frame;
        }
    }
    return -1;
}

int FrameGrid::headerAt(const QPoint &position) const
{
    for (const Item &item : items)
    {
        if (item.frame < 0 && item.rect.contains(position))
        {
            return item.group;
        }
    }
    return -1;
}

void FrameGrid::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint position = event->position().toPoint();
    const int frame = frameAt(position);
    const int header = headerAt(position);
    if (frame != hovered || header != hoveredHeader)
    {
        hovered = frame;
        hoveredHeader = header;
        setCursor(frame >= 0 || header >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void FrameGrid::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        pressed = frameAt(event->position().toPoint());
        pressedHeader = headerAt(event->position().toPoint());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FrameGrid::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const int frame = frameAt(event->position().toPoint());
        if (frame >= 0 && frame == pressed)
        {
            const QString path = queue->framePath(project.id, frame);
            if (!path.isEmpty() && QFileInfo::exists(path))
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        }
        const int header = headerAt(event->position().toPoint());
        if (header >= 0 && header == pressedHeader && header < static_cast<int>(groups.size()))
        {
            const QString &node = groups[static_cast<size_t>(header)].node;
            const auto found = folds.find(node);
            setGroupExpanded(node, found != folds.end() && !found->second.expanded);
        }
        pressed = -1;
        pressedHeader = -1;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FrameGrid::leaveEvent(QEvent *event)
{
    hovered = -1;
    hoveredHeader = -1;
    update();
    QWidget::leaveEvent(event);
}

bool FrameGrid::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto *help = static_cast<QHelpEvent *>(event);
        const QPoint position = help->pos();
        for (const Item &item : items)
        {
            if (item.frame >= 0 && item.rect.contains(position) && item.clip.contains(position))
            {
                const QString node = nodeName(groups[static_cast<size_t>(item.group)].node);
                QToolTip::showText(help->globalPos(), tr("Frame %1, rendered by %2").arg(item.frame).arg(node), this);
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

void FrameGrid::requestThumbnail(int frame)
{
    if (loading.contains(frame))
    {
        return;
    }
    const QString path = queue->framePath(project.id, frame);
    if (path.isEmpty())
    {
        unreadable.insert(frame);
        return;
    }
    loading.insert(frame);
    const QPointer<FrameGrid> self(this);
    const quint64 batch = generation;
    const int edge = static_cast<int>(std::ceil(tileSize(width()) * devicePixelRatioF()));
    const qreal ratio = devicePixelRatioF();
    loader.start([self, frame, path, batch, edge, ratio]() {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (size.isValid())
        {
            // Decodes straight to about the tile size where the format allows.
            reader.setScaledSize(size.scaled(edge, edge, Qt::KeepAspectRatioByExpanding));
        }
        QImage image = reader.read();
        image.setDevicePixelRatio(ratio);
        QMetaObject::invokeMethod(qApp, [self, frame, batch, image]() {
            if (self)
            {
                self->thumbnailLoaded(frame, batch, image);
            }
        }, Qt::QueuedConnection);
    });
}

void FrameGrid::thumbnailLoaded(int frame, quint64 batch, const QImage &image)
{
    if (batch != generation)
    {
        return;
    }
    loading.remove(frame);
    if (image.isNull())
    {
        unreadable.insert(frame);
    }
    else
    {
        auto *pixmap = new QPixmap(QPixmap::fromImage(image));
        thumbnails.insert(frame, pixmap, std::max<qsizetype>(1, image.sizeInBytes() / 1024));
    }
    for (const Item &item : items)
    {
        if (item.frame == frame)
        {
            update(item.rect);
            break;
        }
    }
}

void FrameGrid::resetThumbnails()
{
    ++generation;
    loader.clear();
    thumbnails.clear();
    loading.clear();
    unreadable.clear();
}

ProjectPage::ProjectPage(RenderQueue *queue, const QString &localName, QWidget *parent):
    QWidget(parent),
    queue(queue),
    localName(localName)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto *header = new QVBoxLayout();
    header->setSpacing(12);
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(14);
    auto *backButton = new QPushButton(tr("Back"), this);
    backButton->setFont(Look::pixelFont(9, QFont::Medium));
    Look::stylePlainButton(backButton);
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setToolTip(tr("Back to the queue"));
    titleRow->addWidget(backButton);
    nameLabel = label(this, 14, Qt::white);
    nameLabel->setFont(Look::pixelFont(14, QFont::DemiBold));
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleRow->addWidget(nameLabel, 1);
    stateLabel = label(this, 10, Look::textColor);
    titleRow->addWidget(stateLabel);
    percentLabel = label(this, 16, Qt::white);
    percentLabel->setFont(Look::monoFont(15, QFont::Bold));
    percentLabel->setMinimumWidth(QFontMetrics(percentLabel->font()).horizontalAdvance(QStringLiteral("100%")) + 4);
    percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(percentLabel);
    header->addLayout(titleRow);

    overview = new ProgressOverview(this);
    header->addWidget(overview);

    auto *infoRow = new QHBoxLayout();
    infoRow->setSpacing(18);
    framesLabel = label(this, 10, Look::textColor);
    framesLabel->setFont(Look::monoFont(9));
    infoRow->addWidget(framesLabel);
    detailsLabel = label(this, 9, helpColor);
    infoRow->addWidget(detailsLabel, 1);
    folderButton = new QPushButton(tr("Open folder"), this);
    folderButton->setFont(Look::pixelFont(9, QFont::Medium));
    Look::stylePlainButton(folderButton);
    folderButton->setCursor(Qt::PointingHandCursor);
    folderButton->setToolTip(tr("Show the rendered frames in Explorer"));
    infoRow->addWidget(folderButton);
    header->addLayout(infoRow);
    layout->addWidget(card(this, header, QMargins(16, 14, 16, 14)));

    auto *gridLayout = new QVBoxLayout();
    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    grid = new FrameGrid(queue, content);
    contentLayout->addWidget(grid);
    contentLayout->addStretch(1);
    scroll->setWidget(content);
    Look::clearScrollArea(scroll);
    SmoothScroll::install(scroll);
    gridLayout->addWidget(scroll);
    layout->addWidget(card(this, gridLayout, QMargins(4, 4, 4, 4)), 1);

    QObject::connect(backButton, &QPushButton::clicked, this, &ProjectPage::backRequested);
    QObject::connect(folderButton, &QPushButton::clicked, this, [this]() {
        const ProjectInfo project = this->queue->snapshotById(currentId);
        const QString path = this->queue->framePath(currentId, project.frameStart);
        const QString directory = QFileInfo(path).absolutePath();
        if (!path.isEmpty() && QFileInfo(directory).isDir())
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
        }
    });
    QObject::connect(queue, &RenderQueue::jobUpdated, this, [this](ProjectInfo project) {
        if (project.id == currentId)
        {
            refresh(project);
        }
    });
    QObject::connect(queue, &RenderQueue::jobRemoved, this, [this](QUuid id) {
        if (id == currentId)
        {
            currentId = QUuid();
            emit backRequested();
        }
    });
}

void ProjectPage::showProject(const QUuid &id)
{
    const bool other = id != currentId;
    currentId = id;
    refresh(queue->snapshotById(id));
    if (other)
    {
        scroll->verticalScrollBar()->setValue(0);
    }
}

void ProjectPage::showPreview(const ProjectInfo &project)
{
    currentId = project.id;
    refresh(project);
}

QUuid ProjectPage::projectId() const
{
    return currentId;
}

void ProjectPage::refresh(const ProjectInfo &project)
{
    if (project.isNull)
    {
        return;
    }
    nameLabel->setText(project.name);
    nameLabel->setToolTip(project.path);
    stateLabel->setText(stateText(project.state));
    Look::setTextColor(stateLabel, stateColor(project.state));
    const int percent = project.totalFrame > 0 ? project.finishedFrame * 100 / project.totalFrame : 0;
    percentLabel->setText(QStringLiteral("%1%").arg(percent));
    framesLabel->setText(tr("%1 / %2 frames").arg(project.finishedFrame).arg(project.totalFrame));
    detailsLabel->setText(tr("Frames %1-%2, step %3   %4   %5x%6 (%7%)")
        .arg(project.frameStart).arg(project.frameEnd).arg(project.frameStep)
        .arg(project.renderEngine)
        .arg(project.resolutionX).arg(project.resolutionY).arg(project.resolutionScale));
    folderButton->setEnabled(!project.outputPath.isEmpty());
    overview->setProject(project, localName);
    grid->setProject(project, localName);
}
