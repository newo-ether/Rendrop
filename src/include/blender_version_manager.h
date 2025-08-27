// blender_version_manager.h

#ifndef BLENDER_VERSION_MANAGER_H
#define BLENDER_VERSION_MANAGER_H

#include <vector>
#include <QString>

class BlenderVersionManager
{
public:
    struct BlenderVersion
    {
        QString version;
        QString path;
    };

    BlenderVersionManager(QString versionConfigPath);
    ~BlenderVersionManager();

    int addBlenderVersion(QString path);
    int deleteBlenderVersion(QString version);
    const std::vector<BlenderVersion>& getBlenderVersions() const;
    QString getBlenderPath(QString version) const;
    unsigned int getBlenderVersionCount() const;

private:
    std::vector<BlenderVersion> blenderVersions;
    QString versionConfigPath;
};

#endif // BLENDER_VERSION_MANAGER_H
