// node_panel.cpp

#include <algorithm>

#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "node_panel.h"
#include "rounded_surface.h"

namespace {

const QColor workingColor(110, 110, 180);
const QColor sharingColor(90, 160, 220);
const QColor idleColor(90, 185, 120);
const QColor busyColor(215, 165, 70);
const QColor offColor(110, 115, 130);
const QColor nameColor(224, 224, 232);
const QColor dimColor(110, 115, 130);
const QColor stateColor(150, 155, 170);

constexpr int rowHeight = 24;

QFont pixelFont(int pointSize)
{
    QFont font(QStringLiteral("Pixel Mixed"));
    font.setPointSize(pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

}

ToggleSwitch::ToggleSwitch(QWidget *parent):
    QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFixedSize(sizeHint());
}

QSize ToggleSwitch::sizeHint() const
{
    return {36, 20};
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor trackColor = isChecked() ? workingColor : QColor(0x33, 0x38, 0x42);
    if (!isEnabled())
    {
        trackColor = QColor(0x55, 0x59, 0x5F);
    }
    else if (underMouse())
    {
        trackColor = trackColor.lighter(125);
    }
    painter.setPen(QPen(QColor(65, 65, 85), 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const qreal knob = track.height() - 4;
    const qreal x = isChecked() ? track.right() - 2 - knob : track.left() + 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? QColor(240, 240, 248) : QColor(170, 170, 170));
    painter.drawEllipse(QRectF(x, track.top() + 2, knob, knob));
}

NodeList::NodeList(QWidget *parent):
    QWidget(parent)
{
    setMinimumHeight(rowHeight * 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void NodeList::setRows(const QVector<Row> &newRows, const QString &newEmptyText)
{
    rows = newRows;
    emptyText = newEmptyText;
    update();
}

void NodeList::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QFont nameFont = pixelFont(10);
    const QFont smallFont = pixelFont(9);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics smallMetrics(smallFont);

    const int capacity = std::max(1, height() / rowHeight);
    const int overflow = static_cast<int>(rows.size()) > capacity ? static_cast<int>(rows.size()) - capacity + 1 : 0;
    const int shown = overflow > 0 ? capacity - 1 : static_cast<int>(rows.size());

    int y = 0;
    for (int index = 0; index < shown; ++index)
    {
        const Row &row = rows[index];
        const QRect line(0, y, width(), rowHeight);
        if (row.self)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 10));
            painter.drawRoundedRect(QRectF(line).adjusted(0, 1, 0, -1), 8, 8);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(row.dot);
        painter.drawEllipse(QRectF(8, y + rowHeight / 2.0 - 4, 8, 8));

        // The state text takes what it needs; name and detail share the rest.
        const int stateWidth = std::min(smallMetrics.horizontalAdvance(row.state), width() / 2);
        const QRect stateRect(width() - 8 - stateWidth, y, stateWidth, rowHeight);
        painter.setFont(smallFont);
        painter.setPen(stateColor);
        painter.drawText(stateRect, Qt::AlignVCenter | Qt::AlignRight,
            smallMetrics.elidedText(row.state, Qt::ElideRight, stateWidth));

        const int textLeft = 24;
        const int available = std::max(0, stateRect.left() - 10 - textLeft);
        const QString name = nameMetrics.elidedText(row.name, Qt::ElideRight, available);
        const int nameWidth = nameMetrics.horizontalAdvance(name);
        painter.setFont(nameFont);
        painter.setPen(nameColor);
        painter.drawText(QRect(textLeft, y, nameWidth, rowHeight), Qt::AlignVCenter | Qt::AlignLeft, name);
        const int detailLeft = textLeft + nameWidth + 8;
        const int detailWidth = available - nameWidth - 8;
        if (!row.detail.isEmpty() && detailWidth > 20)
        {
            painter.setFont(smallFont);
            painter.setPen(dimColor);
            painter.drawText(QRect(detailLeft, y, detailWidth, rowHeight), Qt::AlignVCenter | Qt::AlignLeft,
                smallMetrics.elidedText(row.detail, Qt::ElideRight, detailWidth));
        }
        y += rowHeight;
    }
    if (overflow > 0)
    {
        painter.setFont(smallFont);
        painter.setPen(dimColor);
        painter.drawText(QRect(24, y, width() - 32, rowHeight), Qt::AlignVCenter | Qt::AlignLeft,
            tr("+%n more node(s)", nullptr, overflow));
        y += rowHeight;
    }
    if (!emptyText.isEmpty())
    {
        painter.setFont(smallFont);
        painter.setPen(dimColor);
        painter.drawText(QRect(8, y, width() - 16, height() - y),
            Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, emptyText);
    }
}

NodePanel::NodePanel(NodeAgent *agent, QWidget *parent):
    QWidget(parent),
    agent(agent)
{
    setObjectName(QStringLiteral("nodePanel"));
    setStyleSheet(QStringLiteral("background: transparent;"));
    RoundedSurface::install(this, 12, QColor(26, 28, 32));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(11, 11, 11, 11);
    layout->setSpacing(7);

    auto *header = new QHBoxLayout();
    header->setSpacing(7);
    titleLabel = new QLabel(tr("Render Nodes"), this);
    titleLabel->setFont(pixelFont(16));
    titleLabel->setStyleSheet(QStringLiteral("color: rgb(110, 115, 130);"));
    header->addWidget(titleLabel);
    header->addStretch(1);
    acceptLabel = new QLabel(tr("Accept work"), this);
    acceptLabel->setFont(pixelFont(9));
    acceptLabel->setStyleSheet(QStringLiteral("color: rgb(150, 155, 170);"));
    acceptLabel->setToolTip(tr("Let other Rendrop nodes render on this machine while it is idle"));
    header->addWidget(acceptLabel);
    acceptSwitch = new ToggleSwitch(this);
    acceptSwitch->setToolTip(acceptLabel->toolTip());
    header->addWidget(acceptSwitch);
    layout->addLayout(header);

    list = new NodeList(this);
    layout->addWidget(list, 1);

    QObject::connect(acceptSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
        this->agent->setAcceptRemoteWork(checked);
    });
    QObject::connect(agent, &NodeAgent::changed, this, &NodePanel::refresh);
    refresh();
}

void NodePanel::refresh()
{
    const NodeAgent::Snapshot snapshot = agent->snapshot();
    {
        const QSignalBlocker blocker(acceptSwitch);
        acceptSwitch->setChecked(snapshot.enabled && snapshot.acceptRemoteWork);
    }
    acceptSwitch->setEnabled(snapshot.enabled);
    acceptLabel->setEnabled(snapshot.enabled);

    QVector<NodeList::Row> rows;
    if (!snapshot.enabled)
    {
        list->setRows(rows, tr("Distributed rendering is off.\nSet \"enabled\" in distributed.json to join a render group."));
        return;
    }

    NodeList::Row self;
    self.self = true;
    // The highlighted first row is this machine.
    self.name = snapshot.name;
    switch (snapshot.workerState)
    {
    case NodeAgent::WorkerState::Working:
    {
        self.dot = workingColor;
        const RemoteWorker::Status &status = snapshot.worker;
        self.state = status.active && status.total > 0
            ? tr("For %1 %2/%3").arg(snapshot.coordinatorName).arg(status.finished).arg(status.total)
            : tr("For %1").arg(snapshot.coordinatorName);
        break;
    }
    case NodeAgent::WorkerState::RenderingOwnQueue:
        self.dot = snapshot.offersWork ? sharingColor : workingColor;
        self.state = snapshot.offersWork ? tr("Rendering, sharing frames") : tr("Rendering own queue");
        break;
    case NodeAgent::WorkerState::Busy:
        self.dot = busyColor;
        self.state = tr("Busy");
        break;
    case NodeAgent::WorkerState::Refusing:
        self.dot = offColor;
        self.state = tr("Not accepting work");
        break;
    case NodeAgent::WorkerState::NoBlender:
        self.dot = offColor;
        self.state = tr("No Blender selected");
        break;
    case NodeAgent::WorkerState::Waiting:
        self.dot = idleColor;
        self.state = tr("Idle");
        break;
    case NodeAgent::WorkerState::Disabled:
        self.dot = offColor;
        self.state = tr("Off");
        break;
    }
    rows.append(self);

    for (const PeerNode &peer : snapshot.peers)
    {
        NodeList::Row row;
        row.name = peer.beacon.name.isEmpty() ? peer.host : peer.beacon.name;
        row.detail = peer.host;
        if (peer.beacon.offersWork)
        {
            row.dot = sharingColor;
            row.state = tr("Sharing frames");
        }
        else if (peer.beacon.rendering)
        {
            row.dot = workingColor;
            row.state = tr("Rendering");
        }
        else if (!peer.beacon.acceptRemoteWork)
        {
            row.dot = offColor;
            row.state = tr("Not accepting work");
        }
        else if (peer.beacon.busy)
        {
            row.dot = busyColor;
            row.state = tr("Busy");
        }
        else
        {
            row.dot = idleColor;
            row.state = tr("Idle");
        }
        rows.append(row);
    }
    QString empty;
    if (snapshot.peers.isEmpty())
    {
        empty = snapshot.discoveryListening
            ? tr("Looking for other nodes on the LAN...")
            : tr("Discovery port unavailable. Add nodes to \"peers\" in distributed.json.");
    }
    list->setRows(rows, empty);
}
