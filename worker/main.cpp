// Rendrop render worker: claims frame ranges from a Rendrop coordinator,
// downloads the project over HTTP, renders locally and uploads every frame
// back into the coordinator's output directory. No shared storage needed.
// The protocol lives in RemoteWorker, which Rendrop itself also uses.

#include <cstdio>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QSysInfo>
#include <QThread>
#include <QUuid>

#include "remote_worker.h"

namespace {

void printLine(FILE *stream, const QString &text)
{
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss "))
        + text + QLatin1Char('\n');
    const QByteArray bytes = line.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stream);
    std::fflush(stream);
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("rendrop-worker"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0.2"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Rendrop render worker. Claims frame ranges from a Rendrop instance with\n"
        "distributed rendering enabled, downloads the project over HTTP, renders it\n"
        "with a local Blender and uploads the frames back to the coordinator.\n"
        "External assets travel along when the coordinator packs dependencies;\n"
        "otherwise they must be packed into the .blend.\n\n"
        "The token is read from --token or the RENDROP_WORKER_TOKEN environment\n"
        "variable. Prefer the environment variable: command lines are visible to\n"
        "other users of the machine."
    ));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    const QCommandLineOption hostOption(QStringLiteral("host"), QStringLiteral("Coordinator host."), QStringLiteral("host"));
    const QCommandLineOption portOption(QStringLiteral("port"), QStringLiteral("Coordinator port."), QStringLiteral("port"), QStringLiteral("28528"));
    const QCommandLineOption tokenOption(QStringLiteral("token"), QStringLiteral("Worker token from distributed.json."), QStringLiteral("token"));
    const QCommandLineOption blenderOption(QStringLiteral("blender"), QStringLiteral("Path to blender.exe on this machine."), QStringLiteral("path"));
    const QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("Display name. Defaults to the host name."), QStringLiteral("name"));
    const QCommandLineOption cacheOption(
        QStringLiteral("cache-dir"),
        QStringLiteral("Where downloaded projects and local frames go. Defaults to %TEMP%\\rendrop-worker."),
        QStringLiteral("path")
    );
    const QCommandLineOption maxFramesOption(QStringLiteral("max-frames"), QStringLiteral("Upper bound on frames per lease."), QStringLiteral("count"));
    const QCommandLineOption pollOption(QStringLiteral("poll"), QStringLiteral("Seconds to wait when there is no work."), QStringLiteral("seconds"), QStringLiteral("5"));
    const QCommandLineOption exitWhenIdleOption(QStringLiteral("exit-when-idle"), QStringLiteral("Exit instead of polling when there is no work."));
    parser.addOptions({hostOption, portOption, tokenOption, blenderOption, nameOption, cacheOption, maxFramesOption, pollOption, exitWhenIdleOption});
    if (!parser.parse(application.arguments()))
    {
        printLine(stderr, parser.errorText());
        return 1;
    }
    if (parser.isSet(helpOption))
    {
        std::fputs(parser.helpText().toUtf8().constData(), stdout);
        return 0;
    }
    if (parser.isSet(versionOption))
    {
        std::fputs("rendrop-worker 2.0.2\n", stdout);
        return 0;
    }

    const QString host = parser.value(hostOption);
    const QString blenderPath = parser.value(blenderOption);
    QString token = parser.value(tokenOption);
    if (token.isEmpty())
    {
        token = qEnvironmentVariable("RENDROP_WORKER_TOKEN");
    }
    bool portOk = false;
    const int port = parser.value(portOption).toInt(&portOk);
    bool pollOk = false;
    const int pollSeconds = parser.value(pollOption).toInt(&pollOk);
    int maxFrames = 0;
    if (parser.isSet(maxFramesOption))
    {
        bool maxOk = false;
        maxFrames = parser.value(maxFramesOption).toInt(&maxOk);
        if (!maxOk || maxFrames <= 0)
        {
            printLine(stderr, QStringLiteral("--max-frames must be a positive integer"));
            return 1;
        }
    }
    if (host.isEmpty() || token.isEmpty() || blenderPath.isEmpty())
    {
        printLine(stderr, QStringLiteral("--host, --blender and a token are required"));
        return 1;
    }
    if (!portOk || port <= 0 || port > 65535 || !pollOk || pollSeconds <= 0)
    {
        printLine(stderr, QStringLiteral("Invalid --port or --poll"));
        return 1;
    }
    if (!QFileInfo::exists(blenderPath))
    {
        printLine(stderr, QStringLiteral("Blender not found: ") + blenderPath);
        return 1;
    }
    const QString cacheDirectory = QDir::cleanPath(QFileInfo(parser.isSet(cacheOption)
        ? parser.value(cacheOption)
        : QDir::temp().filePath(QStringLiteral("rendrop-worker"))).absoluteFilePath());
    if (!QDir().mkpath(cacheDirectory))
    {
        printLine(stderr, QStringLiteral("Cannot create cache directory: ") + cacheDirectory);
        return 1;
    }

    RemoteWorker::Config config;
    config.host = host.toStdString();
    config.port = port;
    config.token = token.toUtf8();
    config.blenderPath = blenderPath;
    config.cacheDirectory = cacheDirectory;
    // A fresh id per process: a restarted worker never resumes a stale lease.
    config.workerId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    config.name = parser.isSet(nameOption) ? parser.value(nameOption) : QSysInfo::machineHostName();
    config.maxFrames = maxFrames;

    RemoteWorker worker(config);
    worker.setLogger([](bool error, const QString &text) {
        printLine(error ? stderr : stdout, text);
    });
    printLine(stdout, QStringLiteral("Worker %1 polling %2:%3, cache %4")
        .arg(config.name, host)
        .arg(port)
        .arg(QDir::toNativeSeparators(cacheDirectory)));
    bool reportedUnreachable = false;
    while (true)
    {
        const RemoteWorker::Outcome outcome = worker.runOnce();
        switch (outcome)
        {
        case RemoteWorker::Outcome::Rejected:
            printLine(stderr, QStringLiteral("Coordinator rejected this worker, check the token"));
            return 2;
        case RemoteWorker::Outcome::Worked:
        case RemoteWorker::Outcome::Yielded:
            reportedUnreachable = false;
            continue;
        case RemoteWorker::Outcome::NoWork:
            reportedUnreachable = false;
            if (parser.isSet(exitWhenIdleOption))
            {
                printLine(stdout, QStringLiteral("No work, exiting"));
                return 0;
            }
            break;
        case RemoteWorker::Outcome::Unreachable:
            if (!reportedUnreachable)
            {
                printLine(stderr, QStringLiteral("Coordinator unreachable, retrying"));
                reportedUnreachable = true;
            }
            break;
        }
        QThread::sleep(static_cast<unsigned long>(pollSeconds));
    }
}
