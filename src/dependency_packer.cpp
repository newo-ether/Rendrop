// dependency_packer.cpp

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>

#include "dependency_packer.h"

namespace {

constexpr int keptPacks = 2;
constexpr qsizetype maxLogBytes = 4096;

// Runs BAT's own command line inside Blender. cli_main reads the arguments
// after "--" itself.
const char runnerSource[] =
    "import os\n"
    "import sys\n"
    "sys.path.insert(0, os.environ['RENDROP_BAT_PATH'])\n"
    "from blender_asset_tracer import cli\n"
    "code = cli.cli_main()\n"
    "sys.stdout.flush()\n"
    "sys.exit(code or 0)\n";

QString filesDirectory(const QString &packDirectory)
{
    return QDir(packDirectory).filePath(QStringLiteral("files"));
}

// Manifest entries are served by name, so only accept clean relative paths.
bool isCleanRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char('\\'))
        || path.contains(QLatin1Char(':')))
    {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'));
    return std::none_of(parts.begin(), parts.end(), [](const QString &part) {
        return part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..");
    });
}

}

DependencyPacker::DependencyPacker(const QString &cacheRoot, QObject *parent):
    QObject(parent),
    cacheRoot(cacheRoot)
{
}

DependencyPacker::~DependencyPacker()
{
    stop();
}

void DependencyPacker::setTool(const QString &newBlenderPath, const QString &newBatPath)
{
    if (blenderPath != newBlenderPath || batPath != newBatPath)
    {
        // A different tool may succeed where the old one failed.
        failed.clear();
    }
    blenderPath = newBlenderPath;
    batPath = newBatPath;
}

DependencyPacker::State DependencyPacker::request(const QString &blendPath, const QString &key)
{
    if (ready.contains(key))
    {
        return State::Ready;
    }
    if (failed.contains(key))
    {
        return State::Failed;
    }
    if (process && runningKey == key)
    {
        return State::Packing;
    }
    const QString directory = QDir(cacheRoot).filePath(key);
    if (const std::optional<Pack> existing = readManifest(directory, key))
    {
        ready.insert(key, *existing);
        return State::Ready;
    }
    if (process)
    {
        // Busy with another file; ask again later.
        return State::Packing;
    }
    if (blenderPath.isEmpty() || batPath.isEmpty()
        || !QFileInfo(QDir(batPath).filePath(QStringLiteral("blender_asset_tracer"))).isDir())
    {
        failed.insert(key, tr("Blender Asset Tracer not found at \"%1\" (set batPath in distributed.json)").arg(batPath));
        emit finished(key, false, failed.value(key));
        return State::Failed;
    }
    start(blendPath, key);
    return process ? State::Packing : State::Failed;
}

std::optional<DependencyPacker::Pack> DependencyPacker::pack(const QString &key) const
{
    const auto iterator = ready.constFind(key);
    if (iterator == ready.cend())
    {
        return std::nullopt;
    }
    return iterator.value();
}

QString DependencyPacker::errorText(const QString &key) const
{
    return failed.value(key);
}

void DependencyPacker::stop()
{
    if (!process)
    {
        return;
    }
    QProcess *running = process;
    process = nullptr;
    running->disconnect(this);
    running->kill();
    running->waitForFinished(5000);
    delete running;
    QDir(QDir(cacheRoot).filePath(runningKey)).removeRecursively();
    runningKey.clear();
}

QString DependencyPacker::runnerPath() const
{
    return QDir(cacheRoot).filePath(QStringLiteral("rendrop_bat_runner.py"));
}

void DependencyPacker::start(const QString &blendPath, const QString &key)
{
    const QString directory = QDir(cacheRoot).filePath(key);
    QDir(directory).removeRecursively();
    QFile runner(runnerPath());
    if (!QDir().mkpath(directory)
        || !runner.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || runner.write(runnerSource) < 0)
    {
        failed.insert(key, tr("Cannot prepare the dependency cache in %1").arg(cacheRoot));
        emit finished(key, false, failed.value(key));
        return;
    }
    runner.close();

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("RENDROP_BAT_PATH"), QDir::toNativeSeparators(batPath));
    process->setProcessEnvironment(environment);
    runningKey = key;
    runningBlend = blendPath;
    QObject::connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        onProcessFinished(status == QProcess::NormalExit ? exitCode : -1);
    });
    QObject::connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            onProcessFinished(-1);
        }
    });
    process->start(blenderPath, {
        QStringLiteral("-b"),
        QStringLiteral("--factory-startup"),
        QStringLiteral("--python-exit-code"),
        QStringLiteral("1"),
        QStringLiteral("--python"),
        QDir::toNativeSeparators(runnerPath()),
        QStringLiteral("--"),
        QStringLiteral("pack"),
        QDir::toNativeSeparators(blendPath),
        QDir::toNativeSeparators(filesDirectory(directory))
    });
}

void DependencyPacker::onProcessFinished(int exitCode)
{
    if (!process)
    {
        return;
    }
    QProcess *finishedProcess = process;
    process = nullptr;
    const QString log = QString::fromLocal8Bit(finishedProcess->readAll()).right(maxLogBytes).trimmed();
    finishedProcess->deleteLater();
    const QString key = runningKey;
    runningKey.clear();
    const QString directory = QDir(cacheRoot).filePath(key);
    const QString mainFile = QFileInfo(runningBlend).fileName();

    if (exitCode != 0 || !QFileInfo(QDir(filesDirectory(directory)).filePath(mainFile)).isFile()
        || !writeManifest(directory, mainFile))
    {
        QDir(directory).removeRecursively();
        const QString lastLine = log.section(QLatin1Char('\n'), -1).trimmed();
        failed.insert(key, tr("Dependency packing failed (exit %1): %2").arg(exitCode).arg(lastLine));
        emit finished(key, false, failed.value(key));
        return;
    }
    if (const std::optional<Pack> result = readManifest(directory, key))
    {
        ready.insert(key, *result);
        prune();
        emit finished(key, true, tr("Packed %1 file(s) for remote workers").arg(result->files.size()));
        return;
    }
    failed.insert(key, tr("Dependency packing wrote an unreadable manifest"));
    emit finished(key, false, failed.value(key));
}

bool DependencyPacker::writeManifest(const QString &directory, const QString &mainFile)
{
    const QDir root(filesDirectory(directory));
    QJsonArray files;
    QDirIterator iterator(root.path(), QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QFileInfo info(iterator.next());
        const QString relative = root.relativeFilePath(info.absoluteFilePath());
        if (!isCleanRelativePath(relative))
        {
            return false;
        }
        files.append(QJsonObject {
            {QStringLiteral("path"), relative},
            {QStringLiteral("size"), info.size()}
        });
    }
    QSaveFile manifest(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (!manifest.open(QIODevice::WriteOnly))
    {
        return false;
    }
    manifest.write(QJsonDocument(QJsonObject {
        {QStringLiteral("main"), mainFile},
        {QStringLiteral("files"), files}
    }).toJson(QJsonDocument::Indented));
    return manifest.commit();
}

std::optional<DependencyPacker::Pack> DependencyPacker::readManifest(const QString &directory, const QString &key)
{
    QFile file(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    Pack pack;
    pack.key = key;
    pack.directory = filesDirectory(directory);
    pack.mainFile = object.value(QStringLiteral("main")).toString();
    bool hasMain = false;
    for (const QJsonValue &value : object.value(QStringLiteral("files")).toArray())
    {
        const QJsonObject entry = value.toObject();
        File packed {entry.value(QStringLiteral("path")).toString(), entry.value(QStringLiteral("size")).toInteger(-1)};
        const QFileInfo info(QDir(pack.directory).filePath(packed.path));
        // A manifest that no longer matches the disk is stale.
        if (!isCleanRelativePath(packed.path) || !info.isFile() || info.size() != packed.size)
        {
            return std::nullopt;
        }
        hasMain = hasMain || packed.path == pack.mainFile;
        pack.files.push_back(packed);
    }
    if (!hasMain)
    {
        return std::nullopt;
    }
    // Mark use so pruning keeps recently requested packs.
    QFile marker(QDir(directory).filePath(QStringLiteral(".used")));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        marker.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
    }
    return pack;
}

void DependencyPacker::prune()
{
    QFileInfoList entries = QDir(cacheRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    const auto lastUsed = [](const QFileInfo &entry) {
        return QFileInfo(QDir(entry.filePath()).filePath(QStringLiteral(".used"))).lastModified();
    };
    std::sort(entries.begin(), entries.end(), [&](const QFileInfo &left, const QFileInfo &right) {
        return lastUsed(left) > lastUsed(right);
    });
    for (int index = keptPacks; index < entries.size(); ++index)
    {
        const QString key = entries[index].fileName();
        if (key == runningKey)
        {
            continue;
        }
        ready.remove(key);
        QDir(entries[index].filePath()).removeRecursively();
    }
}
