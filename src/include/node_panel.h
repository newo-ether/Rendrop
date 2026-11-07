// node_panel.h

#ifndef NODE_PANEL_H
#define NODE_PANEL_H

#include <QAbstractButton>
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

#include "node_agent.h"

class QLabel;

// Small antialiased on/off switch.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Painted list of render nodes: a status dot, the name and what the node is
// doing.
class NodeList : public QWidget
{
    Q_OBJECT

public:
    struct Row
    {
        QColor dot;
        QString name;
        QString detail;
        QString state;
        bool self = false;
    };

    explicit NodeList(QWidget *parent = nullptr);
    void setRows(const QVector<Row> &rows, const QString &emptyText);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Row> rows;
    QString emptyText;
};

// The "Render Nodes" card: this machine and the peers it found, plus the
// switch that lets other nodes use this machine.
class NodePanel : public QWidget
{
    Q_OBJECT

public:
    NodePanel(NodeAgent *agent, QWidget *parent = nullptr);

private:
    void refresh();

    NodeAgent *agent;
    QLabel *titleLabel;
    QLabel *acceptLabel;
    ToggleSwitch *acceptSwitch;
    NodeList *list;
};

#endif // NODE_PANEL_H
