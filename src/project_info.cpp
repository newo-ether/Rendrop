// project_info.cpp

#include <algorithm>
#include <cmath>
#include <limits>

#include <QJsonValue>

#include "project_info.h"

namespace {
constexpr int schemaVersion = 2;

bool readInt(const QJsonObject &object, const char *key, int &value)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key));
    if (!jsonValue.isDouble())
    {
        return false;
    }
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number)
        || std::floor(number) != number
        || number < std::numeric_limits<int>::min()
        || number > std::numeric_limits<int>::max())
    {
        return false;
    }
    value = static_cast<int>(number);
    return true;
}

bool readString(const QJsonObject &object, const char *key, QString &value)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key));
    if (!jsonValue.isString())
    {
        return false;
    }
    value = jsonValue.toString();
    return true;
}

std::optional<ProjectState> projectStateFromString(const QString &value)
{
    if (value == QStringLiteral("Loading"))
    {
        return ProjectState::Loading;
    }
    if (value == QStringLiteral("Queued"))
    {
        return ProjectState::Queued;
    }
    if (value == QStringLiteral("Error"))
    {
        return ProjectState::Error;
    }
    if (value == QStringLiteral("Rendering"))
    {
        return ProjectState::Rendering;
    }
    if (value == QStringLiteral("Finished"))
    {
        return ProjectState::Finished;
    }
    return std::nullopt;
}
}

QJsonObject ProjectInfo::toJson() const
{
    return {
        {QStringLiteral("schemaVersion"), schemaVersion},
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("path"), path},
        {QStringLiteral("outputPath"), outputPath},
        {QStringLiteral("state"), ProjectStateToString(state)},
        {QStringLiteral("frameStart"), frameStart},
        {QStringLiteral("frameEnd"), frameEnd},
        {QStringLiteral("frameStep"), frameStep},
        {QStringLiteral("resolutionX"), resolutionX},
        {QStringLiteral("resolutionY"), resolutionY},
        {QStringLiteral("resolutionScale"), resolutionScale},
        {QStringLiteral("renderEngine"), renderEngine},
        {QStringLiteral("finishedFrame"), finishedFrame},
        {QStringLiteral("totalFrame"), totalFrame}
    };
}

std::optional<ProjectInfo> ProjectInfo::fromJson(const QJsonObject &object)
{
    int version = 0;
    QUuid id;
    int frameStart = 0;
    int frameEnd = 0;
    int frameStep = 0;
    int resolutionX = 0;
    int resolutionY = 0;
    int resolutionScale = 0;
    int finishedFrame = 0;
    int totalFrame = 0;
    QString name;
    QString path;
    QString outputPath;
    QString stateText;
    QString renderEngine;
    if (!readInt(object, "schemaVersion", version)
        || (version != 1 && version != schemaVersion)
        || !readString(object, "name", name)
        || !readString(object, "path", path)
        || path.isEmpty()
        || !readString(object, "outputPath", outputPath)
        || !readString(object, "state", stateText)
        || !readInt(object, "frameStart", frameStart)
        || !readInt(object, "frameEnd", frameEnd)
        || !readInt(object, "frameStep", frameStep)
        || !readInt(object, "resolutionX", resolutionX)
        || !readInt(object, "resolutionY", resolutionY)
        || !readInt(object, "resolutionScale", resolutionScale)
        || !readString(object, "renderEngine", renderEngine)
        || !readInt(object, "finishedFrame", finishedFrame)
        || !readInt(object, "totalFrame", totalFrame)
        || finishedFrame < 0
        || totalFrame < 0)
    {
        return std::nullopt;
    }
    if (version == 1)
    {
        int legacyId = 0;
        if (!readInt(object, "id", legacyId) || legacyId <= 0)
        {
            return std::nullopt;
        }
        id = QUuid::createUuid();
    }
    else
    {
        QString idText;
        if (!readString(object, "id", idText))
        {
            return std::nullopt;
        }
        id = QUuid(idText);
        if (id.isNull())
        {
            return std::nullopt;
        }
    }
    const std::optional<ProjectState> state = projectStateFromString(stateText);
    if (!state.has_value())
    {
        return std::nullopt;
    }
    ProjectInfo project(
        id,
        name,
        path,
        outputPath,
        *state,
        frameStart,
        frameEnd,
        frameStep,
        resolutionX,
        resolutionY,
        resolutionScale,
        renderEngine,
        finishedFrame,
        totalFrame
    );
    project.totalFrame = project.frameStep > 0 && project.frameEnd >= project.frameStart
        ? (project.frameEnd - project.frameStart) / project.frameStep + 1
        : 0;
    project.finishedFrame = std::min(project.finishedFrame, project.totalFrame);
    project.updateCompletionState();
    if (project.state == ProjectState::Loading
        || project.state == ProjectState::Rendering
        || project.state == ProjectState::Finished)
    {
        project.state = ProjectState::Queued;
    }
    project.updateCompletionState();
    return project;
}

void ProjectInfo::updateCompletionState()
{
    if (totalFrame > 0 && finishedFrame >= totalFrame)
    {
        finishedFrame = totalFrame;
        state = ProjectState::Finished;
    }
}
