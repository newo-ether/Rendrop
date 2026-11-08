// http_server.h

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <atomic>
#include <functional>
#include <string>

#include "httplib.h"

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QThread>

#include "farm/worker_auth.h"

class LoadMonitor;
class RenderQueue;

class HttpServer : public QThread
{
    Q_OBJECT

public:
    explicit HttpServer(RenderQueue *renderQueue, int port = 28528, QObject *parent = nullptr);
    ~HttpServer() override;

    // Enables the /workers and /leases endpoints for clients presenting
    // "Authorization: Bearer <token>". An empty token keeps them disabled.
    // Thread-safe; a new token applies to the next request.
    void setWorkerToken(const QString &token);
    // Enables GET /node/status. The monitor must outlive the server.
    // Must be called before start().
    void setLoadMonitor(const LoadMonitor *monitor);
    // Serves GET /node/beacon, the signed announcement other nodes poll when
    // broadcast cannot reach them. Called on the server thread; must be
    // thread-safe. May be replaced while running; returns once no call to the
    // previous provider is in progress.
    void setBeaconProvider(std::function<QByteArray()> provider);
    // When false, /node/status reports the node as busy whatever the load.
    // Thread-safe.
    void setAcceptRemoteWork(bool accept);
    // 0 until listening.
    int boundPort() const;
    void stop();

protected:
    void run() override;

private:
    // Accepts "Rendrop-HMAC" signed requests and, for scripts, the plain
    // bearer token. nonce receives the request nonce of signed requests.
    bool authorizeWorker(const httplib::Request &request, httplib::Response &response, QByteArray *nonce = nullptr);
    void signResponse(httplib::Response &response, const QByteArray &nonce) const;
    std::string currentWorkerToken() const;
    // Content hash of a file about to be served, cached by path, size and
    // modification time. Empty when the file changed while being hashed.
    QByteArray contentHash(const QString &path);
    // Streams path with its content hash headers. False if it cannot be opened.
    bool serveFile(httplib::Response &response, const QString &path, const QByteArray &nonce);

    RenderQueue *renderQueue;
    const LoadMonitor *loadMonitor = nullptr;
    std::function<QByteArray()> beaconProvider;
    int port;
    std::atomic_int listeningPort {0};
    std::atomic_bool acceptRemoteWork {true};
    // Guards workerToken and beaconProvider, which change with the settings.
    mutable QMutex configMutex;
    std::string workerToken;
    WorkerAuth::Verifier verifier;
    QMutex hashMutex;
    // Path -> (size|mtime, content hash).
    QHash<QString, QPair<QByteArray, QByteArray>> hashCache;
    httplib::Server server;
};

#endif // HTTP_SERVER_H
