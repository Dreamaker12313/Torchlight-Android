// A layout node's pose as the editor wrote it: its turn from the three axes it carries, and its scale.
// The port's own reading of the compiled layout's vectors (formats/cmp.h), in OGRE's maths (core/ogre_types.h).
#pragma once

#include "core/ogre_types.h"
#include "formats/cmp.h"

namespace tl::core {

/// The turn whose local X, Y and Z are `right`, `up` and `forward`; none for a triple that is not unit length
/// or is left-handed (a reflection no rotation represents).
ogre::Quaternion orientation_from_basis(const ogre::Vector3& forward, const ogre::Vector3& right,
                                        const ogre::Vector3& up);

/// A placeable node's turn, from its FORWARD, RIGHT and UP vectors (the node's vec3s 1..3).
ogre::Quaternion node_orientation(const tl::formats::CmpNode& node);

/// A placeable node's authored per-axis scale, or `unit_scale` for a Monster that authors none of its own.
ogre::Vector3 layout_scale(const tl::formats::CmpNode& node, float unit_scale);

}  // namespace tl::core
