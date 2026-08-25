#include <chrono>
#include <cstdio>
#include <string>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUuid>

#include "httplib.h"
#include "nlohmann/json.hpp"

using nlohmann::json;

namespace {

std::string utf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

void printLine(FILE *stream, const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stream);
    std::fwrite("\n", 1, 1, stream);
}

int printResponse(const httplib::Result &result)
{
    if (!result)
    {
        printLine(stderr, QStringLiteral("HTTP request failed"));
        return 1;
    }
    if (!result->body.empty())
    {
        std::fwrite(result->body.data(), 1, result->body.size(), stdout);
        std::fwrite("\n", 1, 1, stdout);
    }
    return result->status >= 200 && result->status < 300 ? 0 : 1;
}

bool parseProjectId(const QString &text, QString &canonical)
{
    const QUuid id(text);
    if (id.isNull())
    {
        return false;
    }
    canonical = id.toString(QUuid::WithoutBraces);
    return true;
}

int usageError(QCommandLineParser &parser, const QString &message)
{
    printLine(stderr, message);
    printLine(stderr, parser.helpText());
    return 1;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("rendropctl"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.34"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral(
            "Rendrop command line client.\n\n"
            "Commands:\n"
            "  launch [executable]\n"
            "  status\n"
            "  projects list\n"
            "  projects get <uuid>\n"
            "  projects add <path> [--name <name>]\n"
            "  projects reload <uuid>\n"
            "  projects move <uuid> --index <index>\n"
            "  projects delete <uuid>\n"
            "  queue start\n"
            "  queue stop"
        )
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
        QStringLiteral("host"),
        QStringLiteral("HTTP server host."),
        QStringLiteral("host"),
        QStringLiteral("127.0.0.1")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("port"),
        QStringLiteral("HTTP server port."),
        QStringLiteral("port"),
        QStringLiteral("28528")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("name"),
        QStringLiteral("Project name for projects add."),
        QStringLiteral("name")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("index"),
        QStringLiteral("Target queue index for projects move."),
        QStringLiteral("index")
    ));
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("Command and arguments."));
    parser.process(application);

    const QStringList arguments = parser.positionalArguments();
    if (arguments.isEmpty())
    {
        return usageError(parser, QStringLiteral("Missing command"));
    }

    const QString command = arguments[0];
    if (command == QStringLiteral("launch"))
    {
        if (arguments.size() > 2)
        {
            return usageError(parser, QStringLiteral("Usage: rendropctl launch [executable]"));
        }
        const QString executable = arguments.size() == 2
            ? QFileInfo(arguments[1]).absoluteFilePath()
            : QCoreApplication::applicationDirPath() + QStringLiteral("/Rendrop.exe");
        if (!QFileInfo::exists(executable))
        {
            printLine(stderr, QStringLiteral("Rendrop executable not found: ") + executable);
            return 1;
        }
        if (!QProcess::startDetached(executable, {}, QFileInfo(executable).absolutePath()))
        {
            printLine(stderr, QStringLiteral("Failed to start Rendrop"));
            return 1;
        }
        printLine(stdout, QStringLiteral("Started Rendrop"));
        return 0;
    }

    bool portOk = false;
    const int port = parser.value(QStringLiteral("port")).toInt(&portOk);
    if (!portOk || port <= 0 || port > 65535)
    {
        return usageError(parser, QStringLiteral("Invalid port"));
    }
    httplib::Client client(utf8(parser.value(QStringLiteral("host"))), port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(30));

    if (command == QStringLiteral("status"))
    {
        if (arguments.size() != 1)
        {
            return usageError(parser, QStringLiteral("Usage: rendropctl status"));
        }
        return printResponse(client.Get("/health"));
    }

    if (command == QStringLiteral("queue"))
    {
        if (arguments.size() != 2
            || (arguments[1] != QStringLiteral("start") && arguments[1] != QStringLiteral("stop")))
        {
            return usageError(parser, QStringLiteral("Usage: rendropctl queue <start|stop>"));
        }
        return printResponse(client.Post("/queue/" + utf8(arguments[1])));
    }

    if (command != QStringLiteral("projects") || arguments.size() < 2)
    {
        return usageError(parser, QStringLiteral("Unknown command"));
    }

    const QString action = arguments[1];
    if (action == QStringLiteral("list"))
    {
        if (arguments.size() != 2)
        {
            return usageError(parser, QStringLiteral("Usage: rendropctl projects list"));
        }
        return printResponse(client.Get("/projects"));
    }

    if (action == QStringLiteral("add"))
    {
        if (arguments.size() != 3)
        {
            return usageError(parser, QStringLiteral("Usage: rendropctl projects add <path> [--name <name>]"));
        }
        json body = {{"path", utf8(arguments[2])}};
        if (parser.isSet(QStringLiteral("name")))
        {
            body["name"] = utf8(parser.value(QStringLiteral("name")));
        }
        return printResponse(client.Post("/projects", body.dump(), "application/json"));
    }

    if (arguments.size() != 3)
    {
        return usageError(parser, QStringLiteral("Project command requires a UUID"));
    }
    QString projectId;
    if (!parseProjectId(arguments[2], projectId))
    {
        return usageError(parser, QStringLiteral("Invalid project UUID"));
    }
    const std::string path = "/projects/" + utf8(projectId);

    if (action == QStringLiteral("get"))
    {
        return printResponse(client.Get(path));
    }
    if (action == QStringLiteral("reload"))
    {
        return printResponse(client.Post(path + "/reload"));
    }
    if (action == QStringLiteral("delete"))
    {
        return printResponse(client.Delete(path));
    }
    if (action == QStringLiteral("move"))
    {
        bool indexOk = false;
        const int index = parser.value(QStringLiteral("index")).toInt(&indexOk);
        if (!parser.isSet(QStringLiteral("index")) || !indexOk || index < 0)
        {
            return usageError(parser, QStringLiteral("projects move requires a non-negative --index"));
        }
        return printResponse(client.Put(
            path + "/order",
            json({{"index", index}}).dump(),
            "application/json"
        ));
    }

    return usageError(parser, QStringLiteral("Unknown projects command"));
}
