// remote_worker.h

#ifndef REMOTE_WORKER_H
#define REMOTE_WORKER_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <QByteArray>
#include <QMutex>
#include <QString>

namespace httplib {
class Client;
}

// Renders leases for one coordinator: claims a frame range, downloads the
// project over HTTP (cached by blend key), renders it with the local Blender
// and uploads every frame into the coordinator's output directory. Used by the
// rendrop-worker command line tool and by the worker built into Rendrop.
//
// All requests are HMAC-signed (see worker_auth.h); assignments are only
// accepted with a valid coordinator signature.
class RemoteWorker
{
public:
    struct Config
    {
        std::string host;
        int port = 28528;
        QByteArray token;
        QString blenderPath;
        QString cacheDirectory;
        // Stable for the lifetime of the process.
        QString workerId;
        QString name;
        // 0: the coordinator decides.
        int maxFrames = 0;
    };

    enum class Outcome
    {
        // Rendered a lease (successfully or not, the coordinator was told).
        Worked,
        NoWork,
        Unreachable,
        // The coordinator refused our credentials or failed to prove it knows
        // the token. Retrying does not help.
        Rejected,
        // requestYield() interrupted the lease and it was handed back.
        Yielded
    };

    struct Status
    {
        bool active = false;
        QString projectName;
        int frameStart = 0;
        int frameEnd = 0;
        int finished = 0;
        int total = 0;
        // "downloading", "rendering", "uploading"
        QString phase;
    };

    using Logger = std::function<void(bool error, const QString &text)>;

    explicit RemoteWorker(const Config &config);
    ~RemoteWorker();

    void setLogger(Logger logger);
    // Claims one lease and renders it.
    Outcome runOnce();
    // Thread-safe. Stops the running lease as soon as possible, keeps the
    // frames that already reached the coordinator and returns the rest.
    void requestYield();
    void clearYield();
    bool yieldRequested() const;
    // Thread-safe snapshot for display.
    Status status() const;
    const Config &config() const;

private:
    struct Assignment;
    class Uploader;
    using HttpStatus = int;

    bool renderLease(const Assignment &assignment, Outcome &outcome);
    bool renderInto(const Assignment &assignment, const QString &blendPath, const QString &outputDirectory, Outcome &outcome);
    HttpStatus ensureBlend(const Assignment &assignment, QString &blendPath);
    HttpStatus ensurePack(const Assignment &assignment, QString &blendPath);
    HttpStatus download(const std::string &path, const QString &target, const Assignment &assignment, QString *servedKey);
    HttpStatus uploadFile(const Assignment &assignment, const QString &filePath, const QString &name);
    HttpStatus post(httplib::Client &client, const Assignment &assignment, const char *action, const QByteArray &body);
    bool finishFailed(const Assignment &assignment, HttpStatus status, const QString &message, Outcome &outcome);
    void heartbeatIfDue(const Assignment &assignment, qint64 &lastBeatMs);
    std::string authorization(const char *method, const std::string &path, QByteArray *nonce = nullptr) const;
    void log(bool error, const QString &text) const;
    void setStatus(const Status &status);
    void setPhase(const QString &phase, int finished = -1);

    Config settings;
    std::unique_ptr<httplib::Client> client;
    std::unique_ptr<httplib::Client> heartbeatClient;
    Logger logger;
    std::atomic_bool yielding {false};
    mutable QMutex statusMutex;
    Status current;
};

#endif // REMOTE_WORKER_H
