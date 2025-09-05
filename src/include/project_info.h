// project_info.h

#ifndef PROJECT_INFO_H
#define PROJECT_INFO_H

#include <QString>

enum ProjectState
{
    Loading,
    Queued,
    Error,
    Rendering,
    Finished
};

inline QString ProjectStateToString(ProjectState state)
{
    switch (state)
    {
        case Loading: return "Loading";
        case Queued: return "Queued";
        case Error: return "Error";
        case Rendering: return "Rendering";
        case Finished: return "Finished";
    }

    return "";
}

struct ProjectInfo
{
    bool isNull;
    int id;
    QString name;
    QString path;
    QString outputPath;
    ProjectState state;
    int frameStart;
    int frameEnd;
    int frameStep;
    int resolutionX;
    int resolutionY;
    int resolutionScale;
    QString renderEngine;
    int finishedFrame;
    int totalFrame;

    inline ProjectInfo():
        isNull(true),
        id(0),
        name(),
        path(),
        outputPath(),
        state(ProjectState::Error),
        frameStart(0),
        frameEnd(0),
        frameStep(0),
        resolutionX(0),
        resolutionY(0),
        resolutionScale(0),
        renderEngine(),
        finishedFrame(0),
        totalFrame(0) {}

    inline ProjectInfo(
        int id,
        const QString &name,
        const QString &path,
        const QString &outputPath,
        ProjectState state,
        int frameStart,
        int frameEnd,
        int frameStep,
        int resolutionX,
        int resolutionY,
        int resolutionScale,
        QString renderEngine,
        int finishedFrame,
        int totalFrame
    ):
        isNull(false),
        id(id),
        name(name),
        path(path),
        outputPath(outputPath),
        state(state),
        frameStart(frameStart),
        frameEnd(frameEnd),
        frameStep(frameStep),
        resolutionX(resolutionX),
        resolutionY(resolutionY),
        resolutionScale(resolutionScale),
        renderEngine(renderEngine),
        finishedFrame(finishedFrame),
        totalFrame(totalFrame) {}
};

#endif // PROJECT_INFO_H
