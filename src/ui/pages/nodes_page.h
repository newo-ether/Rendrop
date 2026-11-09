// nodes_page.h

#ifndef NODES_PAGE_H
#define NODES_PAGE_H

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "farm/node_agent.h"
#include "ui/style/theme.h"

class LoadMonitor;
class QLabel;
class QPushButton;
class QTimer;
class RenderQueue;
class ToggleSwitch;

// Painted table of render nodes. Columns shrink and elide from the right so
// the name and the state always stay readable.
class NodeTable : public QWidget
{
    Q_OBJECT

public:
    struct Row
    {
        Theme::Tone dot;
        QString name;
        QString address;
        QString state;
        QString speed;
        QString lease;
        QString done;
    };

    explicit NodeTable(QWidget *parent = nullptr);
    // searching draws a spinner above emptyText while the table is empty.
    void setRows(const QVector<Row> &rows, const QString &emptyText, bool searching = false);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // Runs only while the spinner is on screen.
    void updateSpinner();

    QVector<Row> rows;
    QString emptyText;
    bool searching = false;
    QTimer *spinTimer;
    int spinAngle = 0;
};

// The "Nodes" page: this machine's card (state, foreign load, the switch
// that lets other nodes use it) above every node of the render group with
// what it is doing for this machine's queue.
class NodesPage : public QWidget
{
    Q_OBJECT

public:
    NodesPage(NodeAgent *agent, RenderQueue *queue, const LoadMonitor *monitor, QWidget *parent = nullptr);

    // Short state of this machine, shared with the title bar.
    static QString describeSelf(const NodeAgent::Snapshot &snapshot, Theme::Tone *dot);
    // Follows a rebuilt agent after the farm settings change. May be null
    // while the old one shuts down.
    void setAgent(NodeAgent *newAgent);

signals:
    void manageNodesRequested();

private:
    void refresh();

    NodeAgent *agent;
    RenderQueue *queue;
    const LoadMonitor *monitor;
    QLabel *selfName;
    QLabel *selfState;
    QLabel *selfLoad;
    QLabel *acceptLabel;
    ToggleSwitch *acceptSwitch;
    NodeTable *table;
    QPushButton *manageButton;
    QTimer *timer;
    // Runs while the table is empty; past searchTimeoutMs the spinner gives
    // way to "No other nodes found".
    QElapsedTimer searchClock;
    static constexpr qint64 searchTimeoutMs = 10000;
};

#endif // NODES_PAGE_H
