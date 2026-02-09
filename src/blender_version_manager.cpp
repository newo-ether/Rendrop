// blender_version_manager.cpp

#include <QString>
#include <QTextStream>

#include <vector>
#include <QFile>
#include <QDir>

#include "blender_version_manager.h"
#include "simple_process.h"

BlenderVersionManager::BlenderVersionManager(QString versionConfigPath):
    versionConfigPath(versionConfigPath)
{
    QDir configDir = QFileInfo(versionConfigPath).absoluteDir();
    if (!configDir.exists())
    {
        configDir.mkpath(configDir.absolutePath());
    }

    QFile configFile(versionConfigPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&configFile);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        int delimiterPos = line.indexOf('|');
        if (delimiterPos != -1)
        {
            BlenderVersion version;
            version.version = line.left(delimiterPos);
            version.path = line.mid(delimiterPos + 1);
            blenderVersions.push_back(version);
        }
    }

    configFile.close();
}

BlenderVersionManager::~BlenderVersionManager() {}

int BlenderVersionManager::addBlenderVersion(QString path)
{
    if (!QFile::exists(path))
    {
        return -1;
    }

    SimpleProcess process;
    QString program = path;
    QStringList arguments = {"--version"};

    if (process.start(program, arguments) != 0)
    {
        return -2;
    }

    if (!process.waitForFinished(3000))
    {
        return -2;
    }

    QString output = process.readStandardOutput();

    if (output.isEmpty())
    {
        return -2;
    }

    QString versionString = output.split("\n").first().trimmed();
    QString version = versionString;

    if (version.isEmpty())
    {
        return -2;
    }

    bool duplicated = std::any_of(
        blenderVersions.begin(),
        blenderVersions.end(),
        [&version](const BlenderVersion &v) { return v.version == version; }
    );

    if (duplicated)
    {
        return -3;
    }

    BlenderVersion newVersion;
    newVersion.version = version;
    newVersion.path = path;
    blenderVersions.push_back(newVersion);

    QFile configFile(versionConfigPath);
    if (configFile.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&configFile);
        out << version << "|" << path << "\n";
        configFile.close();
    }
    else
    {
        return -4;
    }
    return 0;
}

int BlenderVersionManager::deleteBlenderVersion(QString version)
{
    auto iter = std::remove_if(
        blenderVersions.begin(),
        blenderVersions.end(),
        [&version](const BlenderVersion &v) { return v.version == version; }
    );
    if (iter != blenderVersions.end())
    {
        blenderVersions.erase(iter, blenderVersions.end());

        // Rewrite the configuration file
        QFile configFile(versionConfigPath);
        if (configFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&configFile);
            for (const auto &v : blenderVersions)
            {
                out << v.version << "|" << v.path << "\n";
            }
            configFile.close();
        }
        else
        {
            return -1;
        }
    }
    return 0;
}

const std::vector<BlenderVersionManager::BlenderVersion> & BlenderVersionManager::getBlenderVersions() const
{
    return blenderVersions;
}

QString BlenderVersionManager::getBlenderPath(QString version) const
{
    for (const auto &v : blenderVersions)
    {
        if (v.version == version)
        {
            return QDir::toNativeSeparators(v.path);
        }
    }
    return "";
}

unsigned int BlenderVersionManager::getBlenderVersionCount() const
{
    return static_cast<unsigned int>(blenderVersions.size());
}
