// http_server.h

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <atomic>
#include <functional>
#include <string>

#include "httplib.h"

#include <QByteArray>
#include <QString>
#include <QThread>

#include "worker_auth.h"

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
    // Must be called before start().
    void setWorkerToken(const QString &token);
    // Enables GET /node/status. The monitor must outlive the server.
    // Must be called before start().
    void setLoadMonitor(const LoadMonitor *monitor);
    // Serves GET /node/beacon, the signed announcement other nodes poll when
    // broadcast cannot reach them. Called on the server thread; must be
    // thread-safe. Must be called before start().
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

    RenderQueue *renderQueue;
    const LoadMonitor *loadMonitor = nullptr;
    std::function<QByteArray()> beaconProvider;
    int port;
    std::atomic_int listeningPort {0};
    std::atomic_bool acceptRemoteWork {true};
    std::string workerToken;
    WorkerAuth::Verifier verifier;
    httplib::Server server;
};

#endif // HTTP_SERVER_H
