// project_info.h

#ifndef PROJECT_INFO_H
#define PROJECT_INFO_H

#include <optional>
#include <utility>
#include <vector>

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QUuid>

#include "frame_owner.h"

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
        case Loading: return QStringLiteral("Loading");
        case Queued: return QStringLiteral("Queued");
        case Error: return QStringLiteral("Error");
        case Rendering: return QStringLiteral("Rendering");
        case Finished: return QStringLiteral("Finished");
    }
    return QString();
}

struct ProjectInfo
{
    bool isNull;
    QUuid id;
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
    // Completed frame indices as sorted, non-overlapping inclusive ranges.
    // finishedFrame is the number of indices covered by these ranges.
    std::vector<std::pair<int, int>> completedRanges;
    // Completed frames credited to remote nodes. Unlisted completed frames
    // belong to this machine.
    std::vector<FrameOwnerRanges> frameOwners;

    inline ProjectInfo():
        isNull(true),
        id(),
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
        const QUuid &id,
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

    QJsonObject toJson() const;
    static std::optional<ProjectInfo> fromJson(const QJsonObject &object);
    static std::vector<std::pair<int, int>> prefixRanges(int count);
    void updateCompletionState();
};

Q_DECLARE_METATYPE(ProjectInfo)

#endif // PROJECT_INFO_H
