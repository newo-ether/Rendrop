// http_server.h

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "httplib.h"

#include <QThread>

class RenderQueue;

class HttpServer : public QThread
{
    Q_OBJECT

public:
    explicit HttpServer(RenderQueue *renderQueue, int port = 28528, QObject *parent = nullptr);
    ~HttpServer() override;
    void stop();

protected:
    void run() override;

private:
    RenderQueue *renderQueue;
    int port;
    httplib::Server server;
};

#endif // HTTP_SERVER_H
