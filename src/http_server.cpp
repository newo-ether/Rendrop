// http_server.cpp

#include "http_server.h"

#include <limits>
#include <string>

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QMetaObject>
#include <QString>
#include <QUuid>

#include "nlohmann/json.hpp"
#include "project_info.h"
#include "render_queue.h"

using nlohmann::json;

namespace {

std::string toUtf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString fromUtf8(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string uuidText(const QUuid &id)
{
    return toUtf8(id.toString(QUuid::WithoutBraces));
}

json projectToJson(const ProjectInfo &project)
{
    return {
        {"id", uuidText(project.id)},
        {"state", toUtf8(ProjectStateToString(project.state))},
        {"name", toUtf8(project.name)},
        {"path", toUtf8(project.path)},
        {"outputPath", toUtf8(project.outputPath)},
        {"frameStart", project.frameStart},
        {"frameEnd", project.frameEnd},
        {"frameStep", project.frameStep},
        {"resolutionX", project.resolutionX},
        {"resolutionY", project.resolutionY},
        {"resolutionScale", project.resolutionScale},
        {"renderEngine", toUtf8(project.renderEngine)},
        {"finishedFrame", project.finishedFrame},
        {"totalFrame", project.totalFrame}
    };
}

void setJson(httplib::Response &response, int status, const json &body)
{
    response.status = status;
    response.set_content(body.dump(4), "application/json");
}

void setJsonError(httplib::Response &response, int status, const char *message)
{
    setJson(response, status, json({{"error", message}}));
}

void setAccepted(httplib::Response &response, json body = json::object())
{
    body["accepted"] = true;
    setJson(response, 202, body);
}

bool parseJsonObject(const httplib::Request &request, httplib::Response &response, json &body)
{
    body = json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object())
    {
        setJsonError(response, 400, "invalid JSON object");
        return false;
    }
    return true;
}

bool parseProjectId(const httplib::Request &request, httplib::Response &response, QUuid &id)
{
    id = QUuid(fromUtf8(request.matches[1].str()));
    if (id.isNull())
    {
        setJsonError(response, 400, "invalid project id");
        return false;
    }
    return true;
}

bool projectExists(const RenderQueue *renderQueue, const QUuid &id, httplib::Response &response)
{
    if (renderQueue->snapshotById(id).isNull)
    {
        setJsonError(response, 404, "not found");
        return false;
    }
    return true;
}

}

HttpServer::HttpServer(RenderQueue *renderQueue, int port, QObject *parent):
    QThread(parent),
    renderQueue(renderQueue),
    port(port)
{
}

HttpServer::~HttpServer()
{
    stop();
    wait();
}

void HttpServer::stop()
{
    server.stop();
}

void HttpServer::run()
{
    server.Get("/health", [this](const httplib::Request &, httplib::Response &response) {
        const auto projects = renderQueue->snapshots();
        setJson(response, 200, {
            {"status", "ok"},
            {"projectCount", projects.size()}
        });
    });

    server.Get("/projects", [this](const httplib::Request &, httplib::Response &response) {
        json projects = json::array();
        for (const ProjectInfo &project : renderQueue->snapshots())
        {
            projects.push_back(projectToJson(project));
        }
        setJson(response, 200, projects);
    });

    server.Get("/projects/", [](const httplib::Request &, httplib::Response &response) {
        response.set_redirect("/projects");
    });

    server.Get(R"(/projects/([^/]+))", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id))
        {
            return;
        }
        const ProjectInfo project = renderQueue->snapshotById(id);
        if (project.isNull)
        {
            setJsonError(response, 404, "not found");
            return;
        }
        setJson(response, 200, projectToJson(project));
    });

    server.Post("/projects", [this](const httplib::Request &request, httplib::Response &response) {
        json body;
        if (!parseJsonObject(request, response, body)
            || !body.contains("path")
            || !body["path"].is_string())
        {
            if (response.status == -1)
            {
                setJsonError(response, 400, "path must be a string");
            }
            return;
        }
        const QString path = fromUtf8(body["path"].get<std::string>());
        if (path.trimmed().isEmpty())
        {
            setJsonError(response, 400, "path must not be empty");
            return;
        }
        QString name;
        if (body.contains("name"))
        {
            if (!body["name"].is_string())
            {
                setJsonError(response, 400, "name must be a string");
                return;
            }
            name = fromUtf8(body["name"].get<std::string>());
        }
        const QUuid id = QUuid::createUuid();
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id, path, name]() {
                queue->addJob(id, path, name);
            },
            Qt::QueuedConnection
        );
        setAccepted(response, {{"projectId", uuidText(id)}});
    });

    server.Delete(R"(/projects/([^/]+))", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id]() { queue->removeJob(id); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Put(R"(/projects/([^/]+)/order)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        json body;
        if (!parseJsonObject(request, response, body)
            || !body.contains("index")
            || (!body["index"].is_number_integer() && !body["index"].is_number_unsigned()))
        {
            if (response.status == -1)
            {
                setJsonError(response, 400, "index must be an integer");
            }
            return;
        }
        long long indexValue = 0;
        try
        {
            indexValue = body["index"].get<long long>();
        }
        catch (...)
        {
            setJsonError(response, 400, "index is out of range");
            return;
        }
        const auto projects = renderQueue->snapshots();
        if (indexValue < 0
            || indexValue > std::numeric_limits<int>::max()
            || indexValue >= static_cast<long long>(projects.size()))
        {
            setJsonError(response, 400, "index is out of range");
            return;
        }
        const int targetIndex = static_cast<int>(indexValue);
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id, targetIndex]() { queue->moveTo(id, targetIndex); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post(R"(/projects/([^/]+)/reload)", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        if (!parseProjectId(request, response, id) || !projectExists(renderQueue, id, response))
        {
            return;
        }
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue, id]() { queue->reloadJob(id); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post("/queue/start", [this](const httplib::Request &, httplib::Response &response) {
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue]() { queue->start(); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Post("/queue/stop", [this](const httplib::Request &, httplib::Response &response) {
        QMetaObject::invokeMethod(
            renderQueue,
            [queue = renderQueue]() { queue->stop(); },
            Qt::QueuedConnection
        );
        setAccepted(response);
    });

    server.Get("/frame", [this](const httplib::Request &request, httplib::Response &response) {
        QUuid id;
        int frame = 0;
        int thumbnail = 0;
        try
        {
            if (!request.has_param("id") || !request.has_param("frame"))
            {
                throw std::invalid_argument("missing parameters");
            }
            id = QUuid(fromUtf8(request.get_param_value("id")));
            if (id.isNull())
            {
                throw std::invalid_argument("invalid project id");
            }
            frame = std::stoi(request.get_param_value("frame"));
            if (request.has_param("thumb"))
            {
                thumbnail = std::stoi(request.get_param_value("thumb"));
            }
        }
        catch (...)
        {
            setJsonError(response, 400, "invalid parameters");
            return;
        }
        const QString framePath = renderQueue->framePath(id, frame);
        if (framePath.isEmpty())
        {
            setJsonError(response, 404, "not found");
            return;
        }
        QImage image(framePath);
        if (image.isNull())
        {
            setJsonError(response, 404, "not found");
            return;
        }
        if (thumbnail != 0)
        {
            image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        response.set_content(bytes.constData(), static_cast<size_t>(bytes.size()), "image/png");
    });

    server.listen("0.0.0.0", port);
}
