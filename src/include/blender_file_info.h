// blender_file_info.h

#ifndef BLENDER_FILE_INFO_H
#define BLENDER_FILE_INFO_H

#include <QMetaType>

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
        int renderEngine
    );

    int resolutionX;
    int resolutionY;
    int resolutionScale;
    int frameBegin;
    int frameEnd;
    int frameStep;
    int renderEngine;
};

Q_DECLARE_METATYPE(BlenderFileInfo)

#endif // BLENDER_FILE_INFO_H
