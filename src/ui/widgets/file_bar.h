// file_bar.h

#ifndef FILE_BAR_H
#define FILE_BAR_H

#include <QColor>
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "ui/style/drop_shadow_widget.h"
#include "ui/widgets/loading_bar.h"
#include "core/project_info.h"
#include "ui/style/style.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT

public:
    FileBar(QWidget *parent, const ProjectInfo &project);
    ~FileBar() override;

    QUuid projectId() const;
    DropShadowWidget *getDropShadowWidget() const;
    void applySnapshot(const ProjectInfo &project);
    // Machines rendering this project right now, this one included. Shown
    // next to the progress while more than one works on it.
    void setNodeCount(int count);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

signals:
    // Dragging the grip on the left edge reorders the queue. Positions are
    // global so the window can map them into its list.
    void dragStarted(QUuid id, QPointF globalPosition);
    void dragMoved(QUuid id, QPointF globalPosition);
    void dragFinished(QUuid id);
    void deleteRequested(QUuid id);
    void reloadRequested(QUuid id);
    void renameRequested(QUuid id, QString name);
    void frameRangeRequested(QUuid id, int frameStart, int frameEnd, int frameStep);
    // A click on the bar outside its buttons opens the project page.
    void detailsRequested(QUuid id);

private slots:
    void onFrameButtonClicked();
    void onFileNameButtonClicked();
    void updateStyle();

private:
    void setState(ProjectState state);
    void showLoadingBars();
    void hideLoadingBars();
    void updateProjectDetails();
    void updateProgress();
    void paintDragHandle();

    Ui::fileBar *ui;
    bool dragging = false;
    // Left button held on the bar body, and whether the pointer is still on it.
    bool bodyPressed = false;
    bool bodyPressInside = false;
    ProjectInfo project;
    int nodeCount = 0;
    // A sibling under the same parent, which may delete it first (raise()
    // reorders the children), so the bar must not delete it twice.
    QPointer<DropShadowWidget> dropShadowWidget;
    LoadingBar *frameLoadingBar;
    LoadingBar *resolutionLoadingBar;
    LoadingBar *renderEngineLoadingBar;

    Style loadingStyle;
    Style queueStyle;
    Style errorStyle;
    Style renderingStyle;
    Style finishedStyle;
    Style style;
    Style targetStyle;
    Style velocity;
    float stiffness;
    float damping;
    qint64 lastElapsed;
    QTimer *timer;
    QElapsedTimer *elapsedTimer;
};

#endif // FILE_BAR_H
