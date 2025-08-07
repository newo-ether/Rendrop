// blender_version_manager.h

#ifndef BLENDER_VERSION_MANAGER_H
#define BLENDER_VERSION_MANAGER_H

#include <vector>
#include <string>

class BlenderVersionManager
{
public:
    struct BlenderVersion
    {
        std::string version;
        std::string path;
    };

    BlenderVersionManager(std::string versionConfigPath);
    ~BlenderVersionManager();

    int addBlenderVersion(std::string path);
    int deleteBlenderVersion(std::string version);
    const std::vector<BlenderVersion>& getBlenderVersions() const;
    std::string getBlenderPath(std::string version) const;

private:
    std::vector<BlenderVersion> blenderVersions;
    std::string versionConfigPath;

};

#endif // BLENDER_VERSION_MANAGER_H
