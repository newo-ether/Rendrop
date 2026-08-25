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
}

#endif // APP_PATHS_H
