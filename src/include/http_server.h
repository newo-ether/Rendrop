// http_server.h

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "httplib.h" // Has to be included first
#include "project_info.h"

#include <QThread>
#include <QString>

#include <functional>
#include <vector>

class HttpServer : public QThread
{
    Q_OBJECT

public:
    HttpServer(
        std::function<std::vector<ProjectInfo> ()> getAllProjectInfo,
        std::function<ProjectInfo (int id)> getProjectInfoByID,
        std::function<QString (int id, int frame)> getFramePath,
        QObject *parent = nullptr
    );
    void stop();

protected:
    void run() override;

private:
    std::function<std::vector<ProjectInfo> ()> getAllProjectInfo;
    std::function<ProjectInfo (int id)> getProjectInfoByID;
    std::function<QString (int id, int frame)> getFramePath;
    httplib::Server server;
};

#endif // HTTP_SERVER_H
