// frame_owner.h

#ifndef FRAME_OWNER_H
#define FRAME_OWNER_H

#include <utility>
#include <vector>

#include <QString>

// Frames one remote node rendered, as sorted, non-overlapping inclusive index
// ranges. Completed frames that appear under no owner were rendered by the
// coordinator itself (or predate ownership tracking).
struct FrameOwnerRanges
{
    QString owner;
    std::vector<std::pair<int, int>> ranges;

    bool operator==(const FrameOwnerRanges &other) const = default;
};

#endif // FRAME_OWNER_H
