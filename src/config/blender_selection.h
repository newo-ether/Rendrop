// blender_selection.h

#ifndef BLENDER_SELECTION_H
#define BLENDER_SELECTION_H

#include <QString>

// The Blender version last picked in the selector, in blender_selection.cfg.
namespace BlenderSelection {

// Empty when nothing was saved.
QString load();
bool save(const QString &version);

}

#endif // BLENDER_SELECTION_H
