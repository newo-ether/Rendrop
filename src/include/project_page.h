// project_page.h

#ifndef PROJECT_PAGE_H
#define PROJECT_PAGE_H

#include <map>
#include <vector>

#include <QCache>
#include <QColor>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QUuid>
#include <QWidget>

#include "project_info.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QVariantAnimation;
class RenderQueue;

namespace ProjectView {

// Same colors as the Android app: a node keeps its color on both, because
// the index comes from Java's String.hashCode of its name.
QColor nodeColor(const QString &name);

// A run of frame indices [start, end) with one owner; an empty node is this
// machine.
struct Segment
{
    int start;
    int end;
    QString node;
    bool leased;
};

// Finished indices in ascending order. Falls back to a prefix of
// finishedFrame indices when the ranges disagree with the count.
std::vector<int> finishedIndices(const ProjectInfo &project);
// Finished runs split where the owner changes or a frame is missing, plus
// the leased runs, sorted by start.
std::vector<Segment> segments(const ProjectInfo &project);

}

// Progress bar with every frame where it sits in the range, in its node's
// color, leased frames faded; a legend of the nodes underneath.
class ProgressOverview : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressOverview(QWidget *parent = nullptr);
    void setProject(const ProjectInfo &project, const QString &localName);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct LegendEntry
    {
        QString name;
        QColor color;
        int frames;
        int leased;
    };

    ProjectInfo project;
    std::vector<ProjectView::Segment> runs;
    std::vector<LegendEntry> legend;
    QColor stateColor;
    bool multiNode = false;
};

// Thumbnails of the finished frames, newest first, grouped by node when more
// than one rendered. Images load in the background as tiles come into view;
// a click opens the frame in the default viewer. A click on a group header
// folds the group; it opens from the bottom up, like the phone app.
class FrameGrid : public QWidget
{
    Q_OBJECT

public:
    explicit FrameGrid(RenderQueue *queue, QWidget *parent = nullptr);
    ~FrameGrid() override;
    void setProject(const ProjectInfo &project, const QString &localName);
    // node is the owner key: empty for this machine. Folds survive refreshes
    // of the same project.
    void setGroupExpanded(const QString &node, bool expanded, bool animated = true);
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Group
    {
        QString node;
        std::vector<int> frames;
    };
    struct Item
    {
        QRect rect;
        // -1 for a group header.
        int frame;
        int group;
        // The visible part of the group while it folds; tiles draw inside it.
        QRect clip;
        qreal opacity;
    };
    struct Fold
    {
        bool expanded = true;
        // 0 folded, 1 open.
        qreal progress = 1.0;
        QVariantAnimation *animation = nullptr;
    };

    int tileSize(int width) const;
    std::vector<Item> layoutFor(int width) const;
    int frameAt(const QPoint &position) const;
    int headerAt(const QPoint &position) const;
    qreal foldProgress(const QString &node) const;
    void clearFolds();
    void relayout();
    QString nodeName(const QString &node) const;
    void requestThumbnail(int frame);
    void thumbnailLoaded(int frame, quint64 generation, const QImage &image);
    void resetThumbnails();

    RenderQueue *queue;
    ProjectInfo project;
    QString localName;
    std::vector<Group> groups;
    std::vector<Item> items;
    std::map<QString, Fold> folds;
    int hovered = -1;
    int pressed = -1;
    int hoveredHeader = -1;
    int pressedHeader = -1;

    QThreadPool loader;
    // Bounded by bytes; evicted thumbnails load again when seen.
    QCache<int, QPixmap> thumbnails;
    QSet<int> loading;
    QSet<int> unreadable;
    // Results from before a reset are dropped.
    quint64 generation = 0;
};

// The page a click on a queue entry opens: what the phone app shows for a
// project, on the desktop.
class ProjectPage : public QWidget
{
    Q_OBJECT

public:
    ProjectPage(RenderQueue *queue, const QString &localName, QWidget *parent = nullptr);
    void showProject(const QUuid &id);
    // Shows a project that is not in the queue; for the UI snapshot tool.
    void showPreview(const ProjectInfo &project);
    QUuid projectId() const;

signals:
    void backRequested();

private:
    void refresh(const ProjectInfo &project);

    RenderQueue *queue;
    QString localName;
    QUuid currentId;
    QLabel *nameLabel;
    QLabel *stateLabel;
    QLabel *percentLabel;
    QLabel *framesLabel;
    QLabel *detailsLabel;
    ProgressOverview *overview;
    FrameGrid *grid;
    QScrollArea *scroll;
    QPushButton *folderButton;
};

#endif // PROJECT_PAGE_H
