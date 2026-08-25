// file_bar.h

#ifndef FILE_BAR_H
#define FILE_BAR_H

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "drop_shadow_renderer.h"
#include "drop_shadow_widget.h"
#include "loading_bar.h"
#include "project_info.h"
#include "style.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT

public:
    FileBar(
        QWidget *parent,
        DropShadowRenderer *dropShadowRenderer,
        const ProjectInfo &project
    );
    ~FileBar() override;

    QUuid projectId() const;
    DropShadowWidget *getDropShadowWidget() const;
    void applySnapshot(const ProjectInfo &project);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

signals:
    void moveUpRequested(QUuid id);
    void moveDownRequested(QUuid id);
    void deleteRequested(QUuid id);
    void reloadRequested(QUuid id);
    void renameRequested(QUuid id, QString name);
    void frameRangeRequested(QUuid id, int frameStart, int frameEnd, int frameStep);

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

    Ui::fileBar *ui;
    ProjectInfo project;
    DropShadowWidget *dropShadowWidget;
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
