// node_discovery.cpp

#include <algorithm>
#include <cstdlib>

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>

#include "node_discovery.h"
#include "worker_auth.h"

namespace {

constexpr qsizetype maxDatagramSize = 4096;

QString displayAddress(const QHostAddress &address)
{
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    return isV4 ? QHostAddress(v4).toString() : address.toString();
}

}

NodeDiscovery::NodeDiscovery(const QByteArray &token, int discoveryPort, const QStringList &peers, QObject *parent):
    QObject(parent),
    token(token),
    discoveryPort(discoveryPort),
    configuredPeers(peers)
{
}

NodeDiscovery::~NodeDiscovery() = default;

QByteArray NodeDiscovery::encode(const QByteArray &token, const NodeBeacon &beacon)
{
    const QByteArray payload = QJsonDocument(QJsonObject {
        {QStringLiteral("nodeId"), beacon.nodeId},
        {QStringLiteral("name"), beacon.name},
        {QStringLiteral("httpPort"), beacon.httpPort},
        {QStringLiteral("busy"), beacon.busy},
        {QStringLiteral("acceptRemoteWork"), beacon.acceptRemoteWork},
        {QStringLiteral("offersWork"), beacon.offersWork},
        {QStringLiteral("rendering"), beacon.rendering},
        {QStringLiteral("ts"), static_cast<double>(beacon.timeMs)}
    }).toJson(QJsonDocument::Compact);
    return QJsonDocument(QJsonObject {
        {QStringLiteral("rendrop"), 1},
        {QStringLiteral("payload"), QString::fromUtf8(payload)},
        {QStringLiteral("sig"), QString::fromLatin1(WorkerAuth::beaconSignature(token, payload))}
    }).toJson(QJsonDocument::Compact);
}

std::optional<NodeBeacon> NodeDiscovery::decode(const QByteArray &token, const QByteArray &data, qint64 nowMs)
{
    if (token.isEmpty() || data.size() > maxDatagramSize)
    {
        return std::nullopt;
    }
    const QJsonDocument envelope = QJsonDocument::fromJson(data);
    if (!envelope.isObject() || envelope.object().value(QStringLiteral("rendrop")).toInt() != 1)
    {
        return std::nullopt;
    }
    const QByteArray payload = envelope.object().value(QStringLiteral("payload")).toString().toUtf8();
    const QByteArray signature = envelope.object().value(QStringLiteral("sig")).toString().toLatin1();
    if (!WorkerAuth::constantTimeEquals(WorkerAuth::beaconSignature(token, payload), signature))
    {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
    {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    NodeBeacon beacon;
    beacon.nodeId = object.value(QStringLiteral("nodeId")).toString();
    beacon.name = object.value(QStringLiteral("name")).toString().left(128);
    beacon.httpPort = object.value(QStringLiteral("httpPort")).toInt();
    beacon.busy = object.value(QStringLiteral("busy")).toBool(true);
    beacon.acceptRemoteWork = object.value(QStringLiteral("acceptRemoteWork")).toBool(false);
    beacon.offersWork = object.value(QStringLiteral("offersWork")).toBool(false);
    beacon.rendering = object.value(QStringLiteral("rendering")).toBool(false);
    beacon.timeMs = static_cast<qint64>(object.value(QStringLiteral("ts")).toDouble());
    if (beacon.nodeId.isEmpty() || beacon.nodeId.size() > 64
        || beacon.httpPort <= 0 || beacon.httpPort > 65535
        || std::abs(nowMs - beacon.timeMs) > WorkerAuth::clockWindowMs)
    {
        return std::nullopt;
    }
    return beacon;
}

QPair<QString, int> NodeDiscovery::parsePeer(const QString &peer)
{
    const QString text = peer.trimmed();
    if (text.isEmpty())
    {
        return {QString(), 0};
    }
    QString host = text;
    QString portText;
    if (text.startsWith(QLatin1Char('[')))
    {
        const qsizetype close = text.indexOf(QLatin1Char(']'));
        if (close < 0)
        {
            return {QString(), 0};
        }
        host = text.mid(1, close - 1);
        const QString rest = text.mid(close + 1);
        if (!rest.isEmpty())
        {
            if (!rest.startsWith(QLatin1Char(':')))
            {
                return {QString(), 0};
            }
            portText = rest.mid(1);
        }
    }
    else if (text.count(QLatin1Char(':')) == 1)
    {
        const qsizetype colon = text.indexOf(QLatin1Char(':'));
        host = text.left(colon);
        portText = text.mid(colon + 1);
    }
    int port = defaultHttpPort;
    if (!portText.isEmpty())
    {
        bool ok = false;
        port = portText.toInt(&ok);
        if (!ok || port <= 0 || port > 65535)
        {
            return {QString(), 0};
        }
    }
    if (host.isEmpty() || host.contains(QLatin1Char('/')) || host.contains(QLatin1Char(' ')))
    {
        return {QString(), 0};
    }
    return {host, port};
}

bool NodeDiscovery::start()
{
    network = new QNetworkAccessManager(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(peerPollIntervalMs);
    QObject::connect(pollTimer, &QTimer::timeout, this, [this]() {
        pollPeers();
        expire();
    });
    pollTimer->start();
    pollPeers();

    announceTimer = new QTimer(this);
    announceTimer->setInterval(announceIntervalMs);
    QObject::connect(announceTimer, &QTimer::timeout, this, &NodeDiscovery::announce);
    announceTimer->start();

    if (discoveryPort <= 0)
    {
        return false;
    }
    socket = new QUdpSocket(this);
    if (!socket->bind(QHostAddress::AnyIPv4, static_cast<quint16>(discoveryPort),
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        delete socket;
        socket = nullptr;
        return false;
    }
    QObject::connect(socket, &QUdpSocket::readyRead, this, &NodeDiscovery::readDatagrams);
    return true;
}

void NodeDiscovery::setLocalBeacon(const NodeBeacon &beacon)
{
    const bool changed = beacon.nodeId != local.nodeId
        || beacon.name != local.name
        || beacon.httpPort != local.httpPort
        || beacon.busy != local.busy
        || beacon.acceptRemoteWork != local.acceptRemoteWork
        || beacon.offersWork != local.offersWork
        || beacon.rendering != local.rendering;
    local = beacon;
    if (changed)
    {
        announce();
    }
}

QByteArray NodeDiscovery::currentBeacon() const
{
    QMutexLocker locker(&beaconMutex);
    return signedBeacon;
}

QList<PeerNode> NodeDiscovery::peers() const
{
    QList<PeerNode> result = known.values();
    std::sort(result.begin(), result.end(), [](const PeerNode &left, const PeerNode &right) {
        if (left.configured != right.configured)
        {
            return left.configured;
        }
        return left.beacon.name.compare(right.beacon.name, Qt::CaseInsensitive) < 0;
    });
    return result;
}

void NodeDiscovery::announce()
{
    if (local.nodeId.isEmpty())
    {
        return;
    }
    local.timeMs = QDateTime::currentMSecsSinceEpoch();
    const QByteArray datagram = encode(token, local);
    {
        QMutexLocker locker(&beaconMutex);
        signedBeacon = datagram;
    }
    if (!socket)
    {
        return;
    }
    // Directed broadcasts per interface reach networks where the limited
    // broadcast address only leaves through the default route.
    QSet<QString> sent;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces())
    {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || !(flags & QNetworkInterface::CanBroadcast) || (flags & QNetworkInterface::IsLoopBack))
        {
            continue;
        }
        for (const QNetworkAddressEntry &entry : interface.addressEntries())
        {
            const QHostAddress broadcast = entry.broadcast();
            if (broadcast.isNull() || broadcast.protocol() != QAbstractSocket::IPv4Protocol
                || sent.contains(broadcast.toString()))
            {
                continue;
            }
            sent.insert(broadcast.toString());
            socket->writeDatagram(datagram, broadcast, static_cast<quint16>(discoveryPort));
        }
    }
    socket->writeDatagram(datagram, QHostAddress::Broadcast, static_cast<quint16>(discoveryPort));
}

void NodeDiscovery::readDatagrams()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (socket && socket->hasPendingDatagrams())
    {
        const QNetworkDatagram datagram = socket->receiveDatagram(maxDatagramSize + 1);
        const std::optional<NodeBeacon> beacon = decode(token, datagram.data(), now);
        if (beacon)
        {
            accept(*beacon, displayAddress(datagram.senderAddress()), false);
        }
    }
}

void NodeDiscovery::pollPeers()
{
    for (const QString &peer : configuredPeers)
    {
        const QPair<QString, int> address = parsePeer(peer);
        if (address.first.isEmpty())
        {
            continue;
        }
        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(address.first);
        url.setPort(address.second);
        url.setPath(QStringLiteral("/node/beacon"));
        QNetworkRequest request(url);
        request.setTransferTimeout(3000);
        QNetworkReply *reply = network->get(request);
        const QString host = address.first;
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, host]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
            {
                return;
            }
            const QByteArray body = reply->read(maxDatagramSize + 1);
            const std::optional<NodeBeacon> beacon = decode(token, body, QDateTime::currentMSecsSinceEpoch());
            if (beacon)
            {
                accept(*beacon, host, true);
            }
        });
    }
}

void NodeDiscovery::expire()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto iterator = known.begin(); iterator != known.end();)
    {
        if (now - iterator->lastSeenMs > expiryMs)
        {
            iterator = known.erase(iterator);
            changed = true;
        }
        else
        {
            ++iterator;
        }
    }
    if (changed)
    {
        emit peersChanged();
    }
}

void NodeDiscovery::accept(const NodeBeacon &beacon, const QString &host, bool configured)
{
    if (beacon.nodeId == local.nodeId)
    {
        return;
    }
    PeerNode &peer = known[beacon.nodeId];
    // A configured name keeps working when broadcasts also arrive; it is the
    // address the user chose (for example a Tailscale name).
    if (configured || !peer.configured)
    {
        peer.host = host;
    }
    peer.configured = peer.configured || configured;
    peer.beacon = beacon;
    peer.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    emit peersChanged();
}
