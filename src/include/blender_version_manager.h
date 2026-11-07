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

    // First line of "blender --version"; empty when nothing usable came out.
    static QString parseVersionOutput(const QString &output);
    // Registers a version the caller has already probed. Returns 0, -2 for
    // an empty version, -3 for a duplicate or -4 when the file cannot be
    // written.
    int addBlenderVersion(const QString &path, const QString &version);
    int deleteBlenderVersion(QString version);
    const std::vector<BlenderVersion>& getBlenderVersions() const;
    QString getBlenderPath(QString version) const;
    unsigned int getBlenderVersionCount() const;

private:
    std::vector<BlenderVersion> blenderVersions;
    QString versionConfigPath;
};

#endif // BLENDER_VERSION_MANAGER_H
