// nodes_page.cpp

#include <algorithm>
#include <array>

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "farm/load_monitor.h"
#include "ui/pages/nodes_page.h"
#include "core/render_queue.h"
#include "ui/style/rounded_surface.h"
#include "ui/style/look_widgets.h"
#include "ui/motion/smooth_scroll.h"

namespace {

const QColor workingColor(110, 110, 180);
const QColor sharingColor(90, 160, 220);
const QColor idleColor(90, 185, 120);
const QColor busyColor(215, 165, 70);
const QColor offColor(110, 115, 130);
const QColor nameColor(224, 224, 232);
const QColor cellColor(170, 174, 186);

constexpr int headerHeight = 30;
constexpr int rowHeight = 32;
// Share of the table width per column: node, address, state, s/frame,
// lease, leases done.
constexpr std::array<double, 6> columnShares {0.22, 0.17, 0.25, 0.10, 0.14, 0.12};

QWidget *card(QWidget *parent, QLayout *content, int margin)
{
    // Same build as the queue toolbar: a frame around a darker surface.
    auto *frame = new QWidget(parent);
    // Widget gives every frame marked as a card a drop shadow.
    frame->setProperty(Look::cardProperty, true);
    RoundedSurface::install(frame, 12, Look::frameColor);
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(5, 5, 5, 5);
    auto *surface = new QWidget(frame);
    RoundedSurface::install(surface, 10, Look::surfaceColor);
    content->setContentsMargins(margin, margin, margin, margin);
    surface->setLayout(content);
    frameLayout->addWidget(surface);
    return frame;
}

}

NodeTable::NodeTable(QWidget *parent):
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spinTimer = new QTimer(this);
    spinTimer->setInterval(16);
    QObject::connect(spinTimer, &QTimer::timeout, this, [this]() {
        // One turn per second.
        spinAngle = (spinAngle + 6) % 360;
        update();
    });
}

void NodeTable::setRows(const QVector<Row> &newRows, const QString &newEmptyText, bool newSearching)
{
    rows = newRows;
    emptyText = newEmptyText;
    searching = newSearching;
    updateSpinner();
    updateGeometry();
    update();
}

void NodeTable::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateSpinner();
}

void NodeTable::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    updateSpinner();
}

void NodeTable::updateSpinner()
{
    const bool spinning = searching && rows.isEmpty() && isVisible();
    if (spinning && !spinTimer->isActive())
    {
        spinTimer->start();
    }
    else if (!spinning && spinTimer->isActive())
    {
        spinTimer->stop();
    }
}

QSize NodeTable::sizeHint() const
{
    // The searching state stacks a spinner over the text, so give it an extra row of room.
    const int emptyRows = searching ? 4 : 3;
    const int body = rows.isEmpty() ? rowHeight * emptyRows : rowHeight * static_cast<int>(rows.size());
    return {600, headerHeight + body};
}

void NodeTable::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QFont headerFont = Look::pixelFont(9, QFont::Medium);
    const QFont nameFont = Look::pixelFont(10, QFont::Medium);
    const QFont cellFont = Look::pixelFont(9);
    // Address, speed and lease counts line up in columns.
    const QFont numberFont = Look::monoFont(8);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics cellMetrics(cellFont);
    const QFontMetrics numberMetrics(numberFont);

    const int left = 8;
    const int usable = std::max(0, width() - left * 2);
    std::array<int, 7> edges {};
    edges[0] = left;
    double accumulated = 0.0;
    for (size_t index = 0; index < columnShares.size(); ++index)
    {
        accumulated += columnShares[index];
        edges[index + 1] = left + static_cast<int>(usable * accumulated);
    }
    const auto cell = [&](int column, int top, int height) {
        return QRect(edges[column], top, edges[column + 1] - edges[column] - 10, height);
    };

    const QStringList headers {
        tr("Node"), tr("Address"), tr("State"), tr("s/frame"), tr("Lease"), tr("Leases done")
    };
    painter.setFont(headerFont);
    painter.setPen(Look::dimTextColor);
    for (int column = 0; column < headers.size(); ++column)
    {
        QRect area = cell(column, 0, headerHeight);
        if (column == 0)
        {
            area.adjust(16, 0, 0, 0);
        }
        painter.drawText(area, Qt::AlignVCenter | Qt::AlignLeft,
            cellMetrics.elidedText(headers[column], Qt::ElideRight, area.width()));
    }
    painter.setPen(QPen(QColor(0x33, 0x35, 0x3C), 1));
    painter.drawLine(QPointF(left, headerHeight - 0.5), QPointF(width() - left, headerHeight - 0.5));

    int y = headerHeight;
    for (int index = 0; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        if (index % 2 == 1)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 6));
            painter.drawRoundedRect(QRectF(left / 2.0, y + 1, width() - left, rowHeight - 2), 6, 6);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(row.dot);
        painter.drawEllipse(QRectF(edges[0] + 2, y + rowHeight / 2.0 - 4, 8, 8));

        const QRect nameArea = cell(0, y, rowHeight).adjusted(16, 0, 0, 0);
        painter.setFont(nameFont);
        painter.setPen(nameColor);
        painter.drawText(nameArea, Qt::AlignVCenter | Qt::AlignLeft,
            nameMetrics.elidedText(row.name, Qt::ElideRight, nameArea.width()));

        const std::array<const QString *, 5> values {&row.address, &row.state, &row.speed, &row.lease, &row.done};
        for (int column = 1; column <= 5; ++column)
        {
            const QRect area = cell(column, y, rowHeight);
            // State is a word; the other columns are numbers.
            const bool number = column != 2;
            painter.setFont(number ? numberFont : cellFont);
            painter.setPen(column == 1 ? Look::dimTextColor : cellColor);
            painter.drawText(area, Qt::AlignVCenter | Qt::AlignLeft,
                (number ? numberMetrics : cellMetrics).elidedText(*values[column - 1], Qt::ElideRight, area.width()));
        }
        painter.setFont(cellFont);
        y += rowHeight;
    }
    if (rows.isEmpty() && !emptyText.isEmpty())
    {
        QRect textArea(left, y, width() - left * 2, height() - y);
        if (searching)
        {
            // A quarter arc turning over a faint ring, above the text.
            constexpr qreal diameter = 22.0;
            constexpr qreal stroke = 2.5;
            constexpr qreal gap = 16.0;
            // Centre spinner + gap + one text line as a group in the empty area.
            const qreal group = diameter + gap + cellMetrics.height();
            const qreal groupTop = y + std::max<qreal>(12.0, (textArea.height() - group) / 2.0);
            const QPointF centre(width() / 2.0, groupTop + diameter / 2.0);
            const QRectF ring(centre.x() - diameter / 2.0, centre.y() - diameter / 2.0, diameter, diameter);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 20), stroke));
            painter.drawEllipse(ring);
            painter.setPen(QPen(workingColor.lighter(130), stroke, Qt::SolidLine, Qt::RoundCap));
            // Qt angles are counter-clockwise in 1/16 degree; negate to spin clockwise.
            painter.drawArc(ring, -spinAngle * 16, 100 * 16);
            textArea.setTop(static_cast<int>(centre.y() + diameter / 2.0 + gap));
            textArea.setHeight(cellMetrics.height() * 3);
        }
        painter.setFont(cellFont);
        painter.setPen(Look::dimTextColor);
        painter.drawText(textArea,
            (searching ? Qt::AlignHCenter | Qt::AlignTop : Qt::AlignCenter) | Qt::TextWordWrap, emptyText);
    }
}

NodesPage::NodesPage(NodeAgent *agent, RenderQueue *queue, const LoadMonitor *monitor, QWidget *parent):
    QWidget(parent),
    agent(nullptr),
    queue(queue),
    monitor(monitor)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto *selfLayout = new QHBoxLayout();
    selfLayout->setSpacing(14);
    auto *selfText = new QVBoxLayout();
    selfText->setSpacing(4);
    selfName = new QLabel(this);
    selfName->setFont(Look::pixelFont(13, QFont::DemiBold));
    Look::setTextColor(selfName, Qt::white);
    selfState = new QLabel(this);
    selfState->setFont(Look::pixelFont(10));
    Look::setTextColor(selfState, cellColor);
    selfText->addWidget(selfName);
    selfText->addWidget(selfState);
    selfLayout->addLayout(selfText, 1);
    selfLoad = new QLabel(this);
    selfLoad->setFont(Look::monoFont(9));
    Look::setTextColor(selfLoad, QColor(150, 155, 170));
    selfLoad->setToolTip(tr("Load from programs other than Rendrop, averaged over the busy window"));
    selfLayout->addWidget(selfLoad);
    selfLayout->addSpacing(18);
    acceptLabel = new QLabel(tr("Accept work"), this);
    acceptLabel->setFont(Look::pixelFont(10, QFont::Medium));
    Look::setTextColor(acceptLabel, cellColor, QColor(100, 104, 115));
    acceptLabel->setToolTip(tr("Let other Rendrop nodes render on this machine while it is idle"));
    selfLayout->addWidget(acceptLabel);
    acceptSwitch = new ToggleSwitch(this);
    acceptSwitch->setToolTip(acceptLabel->toolTip());
    selfLayout->addWidget(acceptSwitch);
    layout->addWidget(card(this, selfLayout, 14));

    auto *tableLayout = new QVBoxLayout();
    tableLayout->setSpacing(8);
    auto *tableHeader = new QHBoxLayout();
    auto *tableTitle = new QLabel(tr("Render group"), this);
    tableTitle->setFont(Look::pixelFont(12, QFont::DemiBold));
    Look::setTextColor(tableTitle, QColor(150, 155, 170));
    tableHeader->addWidget(tableTitle);
    tableHeader->addStretch(1);
    manageButton = new QPushButton(tr("Manage nodes..."), this);
    manageButton->setFont(Look::pixelFont(9, QFont::Medium));
    Look::stylePlainButton(manageButton);
    manageButton->setCursor(Qt::PointingHandCursor);
    manageButton->setToolTip(tr("Add nodes that broadcast cannot reach, such as Tailscale peers"));
    tableHeader->addWidget(manageButton);
    tableLayout->addLayout(tableHeader);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *scrollContent = new QWidget(scroll);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    table = new NodeTable(scrollContent);
    scrollLayout->addWidget(table);
    scrollLayout->addStretch(1);
    scroll->setWidget(scrollContent);
    Look::clearScrollArea(scroll);
    SmoothScroll::install(scroll);
    tableLayout->addWidget(scroll, 1);
    layout->addWidget(card(this, tableLayout, 12), 1);

    QObject::connect(acceptSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
        if (this->agent)
        {
            this->agent->setAcceptRemoteWork(checked);
        }
    });
    QObject::connect(manageButton, &QPushButton::clicked, this, &NodesPage::manageNodesRequested);
    // Load numbers and worker speeds change without an agent signal.
    timer = new QTimer(this);
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, this, &NodesPage::refresh);
    timer->start();
    setAgent(agent);
}

void NodesPage::setAgent(NodeAgent *newAgent)
{
    if (agent)
    {
        QObject::disconnect(agent, nullptr, this, nullptr);
    }
    agent = newAgent;
    // A rebuilt agent starts a fresh search.
    searchClock.invalidate();
    if (agent)
    {
        QObject::connect(agent, &NodeAgent::changed, this, &NodesPage::refresh);
        refresh();
    }
}

QString NodesPage::describeSelf(const NodeAgent::Snapshot &snapshot, QColor *dot)
{
    QColor color = offColor;
    QString text;
    switch (snapshot.workerState)
    {
    case NodeAgent::WorkerState::Working:
    {
        color = workingColor;
        const RemoteWorker::Status &status = snapshot.worker;
        text = status.active && status.total > 0
            ? tr("Rendering for %1, %2/%3").arg(snapshot.coordinatorName).arg(status.finished).arg(status.total)
            : tr("Rendering for %1").arg(snapshot.coordinatorName);
        break;
    }
    case NodeAgent::WorkerState::RenderingOwnQueue:
        color = snapshot.offersWork ? sharingColor : workingColor;
        text = snapshot.offersWork ? tr("Rendering, sharing frames") : tr("Rendering own queue");
        break;
    case NodeAgent::WorkerState::Busy:
        color = busyColor;
        text = tr("Busy with other programs");
        break;
    case NodeAgent::WorkerState::Refusing:
        text = tr("Not accepting work");
        break;
    case NodeAgent::WorkerState::NoBlender:
        text = tr("No Blender selected");
        break;
    case NodeAgent::WorkerState::Waiting:
        color = idleColor;
        text = tr("Idle");
        break;
    case NodeAgent::WorkerState::Disabled:
        text = tr("Distributed rendering off");
        break;
    }
    if (dot)
    {
        *dot = color;
    }
    return text;
}

void NodesPage::refresh()
{
    // The page only needs live numbers while it is on screen.
    if (!isVisible() && sender() == timer)
    {
        return;
    }
    if (!agent)
    {
        return;
    }
    const NodeAgent::Snapshot snapshot = agent->snapshot();
    selfName->setText(snapshot.name);
    QColor dot;
    selfState->setText(describeSelf(snapshot, &dot));
    Look::setTextColor(selfState, dot.lighter(130));

    const LoadStatus load = monitor->status();
    const QString gpu = load.gpuAvailable
        ? QString::number(qRound(load.busyWindow.gpuPercent)) + QStringLiteral("%")
        : tr("n/a");
    selfLoad->setText(tr("CPU %1%   GPU %2")
        .arg(qRound(load.busyWindow.cpuPercent))
        .arg(gpu));

    {
        const QSignalBlocker blocker(acceptSwitch);
        acceptSwitch->setChecked(snapshot.enabled && snapshot.acceptRemoteWork);
    }
    acceptSwitch->setEnabled(snapshot.enabled);
    acceptLabel->setEnabled(snapshot.enabled);

    if (!snapshot.enabled)
    {
        table->setRows({}, tr("Distributed rendering is off. Turn it on in Settings > Farm to join a render group."));
        return;
    }

    // Workers that took leases from this machine, keyed by the name they
    // report (the built-in worker uses the host name, like its beacon).
    QHash<QString, WorkerInfo> workers;
    for (const WorkerInfo &worker : queue->workers())
    {
        workers.insert(worker.name, worker);
    }
    const auto fillWork = [](NodeTable::Row &row, const WorkerInfo &worker) {
        row.speed = worker.msPerFrame > 0 ? QString::number(worker.msPerFrame / 1000.0, 'f', 1) : QStringLiteral("-");
        row.lease = worker.leaseId.isNull()
            ? QStringLiteral("-")
            : QStringLiteral("%1-%2").arg(worker.leaseFrameStart).arg(worker.leaseFrameEnd);
        row.done = QString::number(worker.completedLeases);
    };

    QVector<NodeTable::Row> rows;
    for (const PeerNode &peer : snapshot.peers)
    {
        NodeTable::Row row;
        row.name = peer.beacon.name.isEmpty() ? peer.host : peer.beacon.name;
        row.address = peer.host;
        row.speed = row.lease = row.done = QStringLiteral("-");
        const auto worker = workers.constFind(peer.beacon.name);
        const bool leasedHere = worker != workers.constEnd() && !worker->leaseId.isNull();
        if (leasedHere)
        {
            row.dot = workingColor;
            row.state = tr("Rendering for this machine");
        }
        else if (peer.beacon.offersWork)
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
        if (worker != workers.constEnd())
        {
            fillWork(row, *worker);
            workers.remove(peer.beacon.name);
        }
        rows.append(row);
    }
    // Standalone rendrop-worker processes do not announce themselves.
    for (const WorkerInfo &worker : std::as_const(workers))
    {
        NodeTable::Row row;
        row.name = worker.name;
        row.address = QStringLiteral("-");
        row.dot = worker.leaseId.isNull() ? idleColor : workingColor;
        row.state = worker.leaseId.isNull() ? tr("Worker, waiting") : tr("Rendering for this machine");
        fillWork(row, worker);
        rows.append(row);
    }
    QString empty;
    bool searching = false;
    if (!rows.isEmpty())
    {
        searchClock.invalidate();
    }
    else if (!snapshot.discoveryListening)
    {
        empty = tr("Discovery port unavailable. Add nodes manually with \"Manage nodes...\".");
    }
    else
    {
        if (!searchClock.isValid())
        {
            searchClock.start();
        }
        // A few announce and poll rounds without an answer means nobody is there.
        searching = searchClock.elapsed() < searchTimeoutMs;
        empty = searching
            ? tr("Looking for other nodes on the LAN...")
            : tr("No other nodes found.");
    }
    table->setRows(rows, empty, searching);
}
