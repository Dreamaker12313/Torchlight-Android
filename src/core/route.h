// A path: the original's `CPath` as a character's route
// (`CCharacter::setDestination` builds it, `CCharacter::updatePathing` walks it) and the
// three-point arcs of a jump down, a death flight, a dropped item and a missile.
// Only the open path with a zero origin, as every console construction passes
// (`CCharacter::CCharacter`, `attemptJumpDown`, `die`,
// `CEquipment::drop`, `CPlayer::CPlayer`,
// `CMissile::calculateLaunchOrientation`). The zero origin is still added
// where the console adds it (-0 becomes +0). Radii, angles and bounds are left out.
// Console layout (`CPath::CPath`): +0x24 length, +0x34 points, +0x74 running lengths.
// No OGRE and no file I/O.
#pragma once

#include "core/collision.h"

#include <cstddef>
#include <vector>

namespace tl::core {

class Route {
public:
    /// `CPath::Clear`.
    void clear();

    /// `CPath::AddPoint(Vector3, float, float)`:
    /// the running length of each point and
    /// the total (+0x24).
    void add_point(const Vec3& point);

    // PORT: an accessor of the port's own bookkeeping.
    size_t size() const { return points_.size(); }
    // PORT: an accessor of the port's own bookkeeping.
    bool empty() const { return points_.empty(); }

    /// `+0x24`, the total length.
    float length() const { return length_; }

    /// `CPath::GetPoint(unsigned)`: the
    /// point plus the origin; the origin past the end.
    Vec3 point(size_t index) const;
    Vec3 back() const { return points_.empty() ? Vec3{} : points_.back(); }

    /// `CPath::FindNearestPoint(Vector3, Vector3&, Vector3&, unsigned&, float&, float)`,
    /// open path: the nearest point and its distance
    /// along the route. False with fewer than two points (outputs untouched), or when none is
    /// nearer than 99999 (the first point, 0). The minimum-distance argument is always 0 on the
    /// console (`CCharacter::updatePathing`).
    bool find_nearest_point(const Vec3& at, Vec3& nearest, float& distance) const;

    /// `CPath::GetPositionAtDistance(float)`.
    Vec3 position_at_distance(float distance) const;

    /// `CPath::GetSplinePositionAtDistance(float)`:
    /// Catmull-Rom between the points either side of `distance`, the ends repeated.
    Vec3 spline_position_at_distance(float distance) const;

private:
    std::vector<Vec3> points_;    ///< X360 +0x34 (Linux +0x28)
    std::vector<float> running_;  ///< X360 +0x74 (Linux +0x88), the length up to each point
    float length_ = 0.0f;         ///< X360 +0x24 (Linux +0x18)
    /// X360 +0x28: zero for every path the console builds (above).
    static constexpr Vec3 kOrigin{0.0f, 0.0f, 0.0f};
};

}  // namespace tl::core
