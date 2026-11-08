#ifndef APP_PATHS_H
#define APP_PATHS_H

#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace AppPaths {
inline QString configDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

inline QString configFile(const QString &fileName)
{
    return QDir(configDirectory()).filePath(fileName);
}

inline bool ensureConfigDirectory()
{
    QDir directory(configDirectory());
    return directory.exists() || directory.mkpath(QStringLiteral("."));
}

// The files Rendrop keeps in the config directory.
inline QString blenderVersionsFile()
{
    return configFile(QStringLiteral("blender_versions.cfg"));
}

inline QString blenderSelectionFile()
{
    return configFile(QStringLiteral("blender_selection.cfg"));
}

inline QString languageFile()
{
    return configFile(QStringLiteral("language.cfg"));
}

inline QString distributedFile()
{
    return configFile(QStringLiteral("distributed.json"));
}
}

#endif // APP_PATHS_H
