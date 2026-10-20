#include "core/no_spawn_region.h"

#include "core/runic_math.h"

#include <cmath>

namespace tl::core {

// CPropertyNode::updateBoundingBox
// MATH::expandBounds
// Remade on a scale change (`CPropertyNodeDescriptor::Set_setScaleX`,
// `CPropertyNode::scaleUpdated`).
NoSpawnRegion property_node_bounds(const Vec3& position, const Vec3& right, const Vec3& up,
                                   const Vec3& forward, float scale_x, float scale_z) {
    const float half_x = scale_x * 0.5f;
    const float half_z = scale_z * 0.5f;
    // The culling bounds' two corners (`CCullingBounds +0x10` and `+0x1c`).
    const Vec3 lo{-half_x, -1000.0f, -half_z};
    const Vec3 hi{half_x, 1000.0f, half_z};

    // Each corner through the node's transform, in this order (`w` is 1: `Matrix4::IDENTITY`).
    const auto turn = [&](float x, float y, float z) {
        return Vec3{right.x * x + up.x * y + forward.x * z + position.x,
                    right.y * x + up.y * y + forward.y * z + position.y,
                    right.z * x + up.z * y + forward.z * z + position.z};
    };

    // The first corner seeds both ends, then the other seven in the original's order.
    Vec3 min_pt = turn(lo.x, lo.y, lo.z);
    Vec3 max_pt = min_pt;
    expand_bounds(min_pt, max_pt, turn(hi.x, hi.y, hi.z));
    expand_bounds(min_pt, max_pt, turn(lo.x, lo.y, hi.z));
    expand_bounds(min_pt, max_pt, turn(lo.x, hi.y, hi.z));
    expand_bounds(min_pt, max_pt, turn(lo.x, hi.y, lo.z));
    expand_bounds(min_pt, max_pt, turn(hi.x, lo.y, lo.z));
    expand_bounds(min_pt, max_pt, turn(hi.x, hi.y, lo.z));
    expand_bounds(min_pt, max_pt, turn(hi.x, lo.y, hi.z));

    return NoSpawnRegion{min_pt, max_pt, NoSpawnExtent::Finite};
}

// CLevel::isInNoSpawnRegion
bool is_in_no_spawn_region(const std::vector<NoSpawnRegion>& regions, const Vec3& p, float radius) {
    if (regions.empty()) {
        return false;
    }

    if (radius > 0.0f || std::isnan(radius)) {
        const float qx_min = p.x - radius;
        const float qx_max = p.x + radius;
        const float qy_min = p.y - radius;
        const float qy_max = p.y + radius;
        const float qz_min = p.z - radius;
        const float qz_max = p.z + radius;

        for (const NoSpawnRegion& region : regions) {
            if (region.extent == NoSpawnExtent::Infinite) {
                return true;
            }
            if (region.extent == NoSpawnExtent::Null) {
                continue;
            }
            // Touching counts; a NaN radius separates nothing.
            if (!(qx_min > region.max.x || qy_min > region.max.y || qz_min > region.max.z ||
                  region.min.x > qx_max || region.min.y > qy_max || region.min.z > qz_max)) {
                return true;
            }
        }
        return false;
    }

    // radius <= 0.0f and not NaN: point containment test.
    for (const NoSpawnRegion& region : regions) {
        if (region.extent == NoSpawnExtent::Null) {
            continue;
        }
        if (region.extent == NoSpawnExtent::Infinite) {
            return true;
        }
        // Extent is Finite: min <= p <= max on all three axes (both ends inclusive).
        if (region.min.x <= p.x && p.x <= region.max.x && region.min.y <= p.y &&
            p.y <= region.max.y && region.min.z <= p.z && p.z <= region.max.z) {
            return true;
        }
    }
    return false;
}

}  // namespace tl::core
