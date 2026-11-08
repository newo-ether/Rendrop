// node_discovery.h

#ifndef NODE_DISCOVERY_H
#define NODE_DISCOVERY_H

#include <optional>

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;
class QUdpSocket;

// What a node tells the others about itself.
struct NodeBeacon
{
    QString nodeId;
    QString name;
    int httpPort = 0;
    // Foreign load is high or the node refuses remote work: do not count on
    // it as a renderer.
    bool busy = true;
    bool acceptRemoteWork = false;
    // The node coordinates a job with frames a worker could take.
    bool offersWork = false;
    // Rendering anything, its own queue or a lease for another node.
    bool rendering = false;
    qint64 timeMs = 0;
};

struct PeerNode
{
    NodeBeacon beacon;
    // Address the beacon came from, or the configured peer host.
    QString host;
    qint64 lastSeenMs = 0;
    bool configured = false;
};

// Announces this node with signed UDP broadcasts on the discovery port and
// listens for the others. Nodes broadcast cannot reach (Tailscale, other
// subnets) are listed as peers and polled over HTTP at /node/beacon.
//
// Beacons are HMAC-signed with the group token (see worker_auth.h); beacons
// that fail verification, are stale or come from this node are dropped.
//
// Lives on the GUI thread except currentBeacon(), which is thread-safe.
class NodeDiscovery : public QObject
{
    Q_OBJECT

public:
    static constexpr int defaultHttpPort = 28528;
    static constexpr int announceIntervalMs = 3000;
    static constexpr int peerPollIntervalMs = 5000;
    static constexpr int expiryMs = 15000;

    NodeDiscovery(const QByteArray &token, int discoveryPort, const QStringList &peers, QObject *parent = nullptr);
    ~NodeDiscovery() override;

    static QByteArray encode(const QByteArray &token, const NodeBeacon &beacon);
    static std::optional<NodeBeacon> decode(const QByteArray &token, const QByteArray &data, qint64 nowMs);
    // "host", "host:port" or "[v6]:port". Empty host on malformed input.
    static QPair<QString, int> parsePeer(const QString &peer);

    // Opens the socket and starts announcing. False when the discovery port
    // cannot be bound; peer polling still runs.
    bool start();
    // Announces immediately when anything other than the time stamp changed.
    void setLocalBeacon(const NodeBeacon &beacon);
    // Signed beacon for GET /node/beacon. Thread-safe.
    QByteArray currentBeacon() const;
    // Live peers, configured peers first, then by name.
    QList<PeerNode> peers() const;

signals:
    void peersChanged();

private:
    void announce();
    void readDatagrams();
    void pollPeers();
    void expire();
    void accept(const NodeBeacon &beacon, const QString &host, bool configured);

    QByteArray token;
    int discoveryPort;
    QStringList configuredPeers;
    QUdpSocket *socket = nullptr;
    QNetworkAccessManager *network = nullptr;
    QTimer *announceTimer = nullptr;
    QTimer *pollTimer = nullptr;
    NodeBeacon local;
    mutable QMutex beaconMutex;
    QByteArray signedBeacon;
    QHash<QString, PeerNode> known;
};

#endif // NODE_DISCOVERY_H
