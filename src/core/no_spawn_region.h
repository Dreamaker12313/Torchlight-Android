#pragma once

#include "core/collision.h"

#include <vector>

namespace tl::core {

/// Extent of an axis-aligned bounding box, matching `Ogre::AxisAlignedBox::Extent`
/// (0 = EXTENT_NULL, 1 = EXTENT_FINITE, 2 = EXTENT_INFINITE).
enum class NoSpawnExtent {
    Null = 0,
    Finite = 1,
    Infinite = 2,
};

/// An axis-aligned region where spawning is forbidden (`CPropertyNode +0x120..+0x13b`); a
/// zero-size finite box by default, as `CPropertyNode::CPropertyNode`
/// leaves it.
struct NoSpawnRegion {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};
    NoSpawnExtent extent = NoSpawnExtent::Finite;
};

/// `CPropertyNode::updateBoundingBox`: the world box of
/// a `Property Node`'s corners `(+-scale_x/2, +-1000, +-scale_z/2)`, merged by
/// `MATH::expandBounds`.
NoSpawnRegion property_node_bounds(const Vec3& position, const Vec3& right, const Vec3& up,
                                   const Vec3& forward, float scale_x, float scale_z);

/// `CLevel::isInNoSpawnRegion`: whether `p` (or the box
/// `[p - r, p + r]` when radius > 0 or NaN) touches any region, in list order.
bool is_in_no_spawn_region(const std::vector<NoSpawnRegion>& regions, const Vec3& p, float radius);

}  // namespace tl::core
