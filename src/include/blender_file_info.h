// blender_file_info.h

#ifndef BLENDER_FILE_INFO_H
#define BLENDER_FILE_INFO_H

#include <QMetaType>
#include <QString>

struct BlenderFileInfo
{
    BlenderFileInfo();

    BlenderFileInfo(
        int resolutionX,
        int resolutionY,
        int resolutionScale,
        int frameStart,
        int frameEnd,
        int frameStep,
        int renderEngine,
        const QString &outputPath
    );

    int resolutionX;
    int resolutionY;
    int resolutionScale;
    int frameBegin;
    int frameEnd;
    int frameStep;
    int renderEngine;
    QString outputPath;
};

Q_DECLARE_METATYPE(BlenderFileInfo)

#endif // BLENDER_FILE_INFO_H
