// http_server.cpp

#include "http_server.h" // Has to be included first
#include "nlohmann/json.hpp"

#include <QImage>
#include <QString>
#include <QByteArray>
#include <QBuffer>

#include <functional>
#include <vector>
#include <string>

using namespace nlohmann;

HttpServer::HttpServer(
    std::function<std::vector<ProjectInfo> ()> getAllProjectInfo,
    std::function<ProjectInfo (int id)> getProjectInfoByID,
    std::function<QString (int id, int frame)> getFramePath,
    QObject *parent
):
    QThread(parent),
    getAllProjectInfo(getAllProjectInfo),
    getProjectInfoByID(getProjectInfoByID),
    getFramePath(getFramePath) {}

void HttpServer::stop()
{
    if (server.is_running())
    {
        server.stop();
    }
}

void HttpServer::run()
{
    server.Get("/projects", [this](const httplib::Request&, httplib::Response& res) {
        auto projects = this->getAllProjectInfo();
        json js = json::array();
        for (const auto &project : projects)
        {
            js.push_back({
                {"id", project.id},
                {"state", ProjectStateToString(project.state).toStdWString()},
                {"name", project.name.toStdWString()},
                {"path", project.path.toStdWString()},
                {"outputPath", project.outputPath.toStdWString()},
                {"frameStart", project.frameStart},
                {"frameEnd", project.frameEnd},
                {"frameStep", project.frameStep},
                {"resolutionX", project.resolutionX},
                {"resolutionX", project.resolutionY},
                {"resolutionScale", project.resolutionScale},
                {"renderEngine", project.renderEngine},
                {"finishedFrame", project.finishedFrame},
                {"totalFrame", project.totalFrame}
            });
        }
        res.set_content(js.dump(4), "application/json");
    });

    server.Get("/projects/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/projects");
    });

    server.Get(R"(/projects/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto project = this->getProjectInfoByID(id);

        if (project.isNull) {
            json js = {{"error", "not found"}};
            res.status = 404;
            res.set_content(js.dump(4), "application/json");
            return;
        }

        json js = {
            {"id", project.id},
            {"state", ProjectStateToString(project.state).toStdWString()},
            {"name", project.name.toStdWString()},
            {"path", project.path.toStdWString()},
            {"outputPath", project.outputPath.toStdWString()},
            {"frameStart", project.frameStart},
            {"frameEnd", project.frameEnd},
            {"frameStep", project.frameStep},
            {"resolutionX", project.resolutionX},
            {"resolutionX", project.resolutionY},
            {"resolutionScale", project.resolutionScale},
            {"renderEngine", project.renderEngine},
            {"finishedFrame", project.finishedFrame},
            {"totalFrame", project.totalFrame}
        };
        res.set_content(js.dump(4), "application/json");
    });

    server.Get("/frame", [this](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.get_param_value("id"));
        int frame = std::stoi(req.get_param_value("frame"));
        int thumb = std::stoi(req.get_param_value("thumb"));
        QString framePath = this->getFramePath(id, frame);
        if (framePath.isEmpty())
        {
            json js = {{"error", "not found"}};
            res.status = 404;
            res.set_content(js.dump(4), "application/json");
            return;
        }

        QImage image(framePath);
        if (image.isNull()) {
            json js = {{"error", "not found"}};
            res.status = 404;
            res.set_content(js.dump(4), "application/json");
            return;
        }

        if (thumb)
        {
            image = image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");

        res.set_content(byteArray.constData(), byteArray.size(), "image/png");
    });

    server.listen("0.0.0.0", 28528);
}
