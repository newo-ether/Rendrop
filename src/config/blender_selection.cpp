// blender_selection.cpp

#include <QFile>

#include "config/app_paths.h"
#include "config/blender_selection.h"

namespace BlenderSelection {

QString load()
{
    QFile file(AppPaths::blenderSelectionFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

bool save(const QString &version)
{
    if (!AppPaths::ensureConfigDirectory())
    {
        return false;
    }
    QFile file(AppPaths::blenderSelectionFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }
    return file.write((version + QLatin1Char('\n')).toUtf8()) >= 0;
}

}
