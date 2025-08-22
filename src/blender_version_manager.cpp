// blender_version_manager.cpp

#include <QProcess>
#include <QString>
#include <QTextStream>

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include "blender_version_manager.h"

BlenderVersionManager::BlenderVersionManager(std::string versionConfigPath)
    : versionConfigPath(versionConfigPath)
{
    std::filesystem::path configPath(versionConfigPath);
    std::filesystem::path configDir = configPath.parent_path();
    if (!std::filesystem::exists(configPath.parent_path()))
    {
        std::filesystem::create_directories(configDir);
    }

    std::fstream configFile(versionConfigPath, std::ios::in | std::ios::out | std::ios::app);
    if (!configFile.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(configFile, line))
    {
        size_t delimiterPos = line.find('|');
        if (delimiterPos != std::string::npos) {
            BlenderVersion version;
            version.version = line.substr(0, delimiterPos);
            version.path = line.substr(delimiterPos + 1);
            blenderVersions.push_back(version);
        }
    }

    configFile.close();
}

BlenderVersionManager::~BlenderVersionManager() {}

int BlenderVersionManager::addBlenderVersion(std::string path)
{
    if (!std::filesystem::exists(path))
    {
        return -1;
    }

    QProcess process;
    QString program = QString::fromStdString(path);
    QStringList arguments = {"--version"};

    process.start(program, arguments);

    if (!process.waitForStarted(3000))
    {
        return -2;
    }

    if (!process.waitForFinished(3000))
    {
        return -2;
    }

    QString output = process.readAllStandardOutput();

    QTextStream stream(&output);
    QString versionString = stream.readLine();
    std::string version = versionString.toStdString();

    bool duplicated = std::any_of(
        blenderVersions.begin(),
        blenderVersions.end(),
        [&version](const BlenderVersion &v){ return v.version == version; }
    );

    if (duplicated)
    {
        return -3;
    }

    BlenderVersion newVersion;
    newVersion.version = version;
    newVersion.path = path;
    blenderVersions.push_back(newVersion);

    std::fstream configFile(versionConfigPath, std::ios::out | std::ios::app);
    if (configFile.is_open())
    {
        configFile << version << "|" << path << "\n";
        configFile.close();
    }
    else
    {
        return -4;
    }
    return 0;
}

int BlenderVersionManager::deleteBlenderVersion(std::string version)
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
        std::fstream configFile(versionConfigPath, std::ios::out | std::ios::trunc);
        if (configFile.is_open())
        {
            for (const auto &v : blenderVersions)
            {
                configFile << v.version << "|" << v.path << "\n";
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

std::string BlenderVersionManager::getBlenderPath(std::string version) const
{
    for (const auto &v : blenderVersions)
    {
        if (v.version == version)
        {
            return v.path;
        }
    }
    return "";
}

unsigned int BlenderVersionManager::getBlenderVersionCount() const
{
    return static_cast<unsigned int>(blenderVersions.size());
}
