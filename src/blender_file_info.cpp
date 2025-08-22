// blender_file_info.cpp

#include "blender_file_info.h"

BlenderFileInfo::BlenderFileInfo():
    resolutionX(0),
    resolutionY(0),
    resolutionScale(0),
    frameBegin(0),
    frameEnd(0),
    frameStep(0),
    renderEngine(0)
{}

BlenderFileInfo::BlenderFileInfo(
    int resolutionX,
    int resolutionY,
    int resolutionScale,
    int frameBegin,
    int frameEnd,
    int frameStep,
    int renderEngine
):
    resolutionX(resolutionX),
    resolutionY(resolutionY),
    resolutionScale(resolutionScale),
    frameBegin(frameBegin),
    frameEnd(frameEnd),
    frameStep(frameStep),
    renderEngine(renderEngine)
{}
