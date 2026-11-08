// project_info.cpp

#include <algorithm>
#include <cmath>
#include <limits>

#include <QJsonArray>
#include <QJsonValue>

#include "core/project_info.h"

namespace {
constexpr int schemaVersion = 4;

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

bool readIntValue(const QJsonValue &jsonValue, int &value)
{
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

// Accepts only sorted, non-overlapping ranges inside [0, totalFrame).
bool readRanges(const QJsonValue &jsonValue, int totalFrame, std::vector<std::pair<int, int>> &ranges)
{
    if (!jsonValue.isArray())
    {
        return false;
    }
    int previousLast = -1;
    for (const QJsonValue &item : jsonValue.toArray())
    {
        const QJsonArray pair = item.toArray();
        int first = 0;
        int last = 0;
        if (!item.isArray()
            || pair.size() != 2
            || !readIntValue(pair[0], first)
            || !readIntValue(pair[1], last)
            || first <= previousLast
            || first > last
            || last >= totalFrame)
        {
            ranges.clear();
            return false;
        }
        ranges.emplace_back(first, last);
        previousLast = last;
    }
    return true;
}

// Owners are advisory: any malformed entry drops all of them, which only
// moves frames back under this machine.
bool readOwners(const QJsonValue &jsonValue, int totalFrame, std::vector<FrameOwnerRanges> &owners)
{
    if (!jsonValue.isArray())
    {
        return false;
    }
    for (const QJsonValue &item : jsonValue.toArray())
    {
        const QJsonObject object = item.toObject();
        FrameOwnerRanges entry;
        if (!item.isObject()
            || !readString(object, "node", entry.owner)
            || entry.owner.isEmpty()
            || !readRanges(object.value(QStringLiteral("ranges")), totalFrame, entry.ranges))
        {
            owners.clear();
            return false;
        }
        owners.push_back(std::move(entry));
    }
    return true;
}

int rangeCount(const std::vector<std::pair<int, int>> &ranges)
{
    int count = 0;
    for (const auto &range : ranges)
    {
        count += range.second - range.first + 1;
    }
    return count;
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
    // A snapshot built only from a frame count is treated as sequential
    // progress, matching how schema 2 interpreted finishedFrame.
    const std::vector<std::pair<int, int>> effectiveRanges = rangeCount(completedRanges) == finishedFrame
        ? completedRanges
        : prefixRanges(finishedFrame);
    QJsonArray ranges;
    for (const auto &range : effectiveRanges)
    {
        ranges.append(QJsonArray{range.first, range.second});
    }
    QJsonArray owners;
    for (const FrameOwnerRanges &entry : frameOwners)
    {
        QJsonArray ownerRanges;
        for (const auto &range : entry.ranges)
        {
            ownerRanges.append(QJsonArray{range.first, range.second});
        }
        owners.append(QJsonObject{
            {QStringLiteral("node"), entry.owner},
            {QStringLiteral("ranges"), ownerRanges}
        });
    }
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
        {QStringLiteral("totalFrame"), totalFrame},
        {QStringLiteral("completedRanges"), ranges},
        {QStringLiteral("frameOwners"), owners}
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
        || version < 1
        || version > schemaVersion
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
    if (version >= 3 && object.contains(QStringLiteral("completedRanges")))
    {
        // Invalid ranges fall back to nothing completed; re-rendering is safe,
        // skipping frames is not.
        readRanges(object.value(QStringLiteral("completedRanges")), project.totalFrame, project.completedRanges);
    }
    else
    {
        // Schema 1 and 2 only rendered sequentially, so progress is a prefix.
        // The same applies to a schema 3 object written without ranges.
        project.completedRanges = prefixRanges(std::min(project.finishedFrame, project.totalFrame));
    }
    if (version >= 4 && object.contains(QStringLiteral("frameOwners")))
    {
        readOwners(object.value(QStringLiteral("frameOwners")), project.totalFrame, project.frameOwners);
    }
    project.finishedFrame = rangeCount(project.completedRanges);
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

std::vector<std::pair<int, int>> ProjectInfo::prefixRanges(int count)
{
    if (count <= 0)
    {
        return {};
    }
    return {{0, count - 1}};
}

void ProjectInfo::updateCompletionState()
{
    if (totalFrame > 0 && finishedFrame >= totalFrame)
    {
        finishedFrame = totalFrame;
        completedRanges = prefixRanges(totalFrame);
        state = ProjectState::Finished;
    }
}
