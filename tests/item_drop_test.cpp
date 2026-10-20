// An item's toss to the floor, pinned against `CEquipment::drop`,
// `CPath::GetSplinePositionAtDistance` and
// `CLevel::randomOpenItemPositionRange`.
#include "core/item_drop.h"
#include "test_support.h"

#include <cmath>

namespace {

using tl::core::DropArc;
using tl::core::RunicRandom;
using tl::core::Vec3;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

}  // namespace

/// The arc's middle point sits over the midpoint, raised 3.5 to 4.5, and the
/// lengths are the straight lines between the three points.
TL_TEST(drop_arc_rises_over_the_midpoint) {
    RunicRandom rng(12345);
    const DropArc arc = tl::core::make_drop_arc({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, rng);
    TL_CHECK(near(arc.points[1].x, 0.5f));
    TL_CHECK(near(arc.points[1].z, 0.5f));
    TL_CHECK(arc.points[1].y >= tl::core::kDropRiseMin &&
             arc.points[1].y <= tl::core::kDropRiseMax);
    TL_CHECK(near(arc.along[0], 0.0f));
    TL_CHECK(near(arc.along[2], arc.length));
    const float first = std::sqrt(0.25f + arc.points[1].y * arc.points[1].y + 0.25f);
    TL_CHECK(near(arc.along[1], first));
}

/// Exactly 0 and exactly the length are the end points; at a point's own
/// distance a Catmull-Rom curve passes through that point.
TL_TEST(drop_spline_passes_through_its_points) {
    RunicRandom rng(7);
    const DropArc arc = tl::core::make_drop_arc({2.0f, 1.0f, -3.0f}, {3.0f, 1.0f, -2.0f}, rng);
    const Vec3 start = tl::core::spline_position_at_distance(arc, 0.0f);
    const Vec3 end = tl::core::spline_position_at_distance(arc, arc.length);
    const Vec3 top = tl::core::spline_position_at_distance(arc, arc.along[1]);
    TL_CHECK(near(start.x, 2.0f) && near(start.y, 1.0f) && near(start.z, -3.0f));
    TL_CHECK(near(end.x, 3.0f) && near(end.y, 1.0f) && near(end.z, -2.0f));
    TL_CHECK(near(top.y, arc.points[1].y));
    // Halfway up the first segment the curve is between the ground and the top.
    const Vec3 rising = tl::core::spline_position_at_distance(arc, arc.along[1] * 0.5f);
    TL_CHECK(rising.y > 1.0f && rising.y < arc.points[1].y + 0.5f);
}

/// A spot the map accepts on the first try is `at` plus the rotated distance;
/// two draws are spent, distance first.
TL_TEST(random_open_item_position_takes_the_first_passable_spot) {
    RunicRandom rng(99);
    RunicRandom mirror(99);
    const Vec3 at{10.0f, 2.0f, 20.0f};
    const Vec3 got =
        tl::core::random_open_item_position(at, 0.0f, 1.5f, [](const Vec3&) { return true; }, rng);
    const float distance = mirror.between(0.0f, 1.5f);
    const float radians = mirror.between(0.0f, 360.0f) * 0.017453292f;
    TL_CHECK(near(got.x, at.x + distance * std::sin(radians)));
    TL_CHECK(near(got.z, at.z + distance * std::cos(radians)));
    TL_CHECK_EQ(got.y, at.y);
    TL_CHECK(std::sqrt((got.x - at.x) * (got.x - at.x) + (got.z - at.z) * (got.z - at.z)) <=
             1.5001f);
}

/// A hundred refusals give `at` back whole.
TL_TEST(random_open_item_position_gives_up_after_a_hundred_tries) {
    RunicRandom rng(3);
    const Vec3 at{1.0f, 2.0f, 3.0f};
    int asked = 0;
    const Vec3 got = tl::core::random_open_item_position(
        at, 0.0f, 1.0f,
        [&](const Vec3&) {
            ++asked;
            return false;
        },
        rng);
    TL_CHECK_EQ(asked, 100);
    TL_CHECK_EQ(got.x, at.x);
    TL_CHECK_EQ(got.y, at.y);
    TL_CHECK_EQ(got.z, at.z);
}

/// The tilts' axes and angles, as `drop` passes them to `matrixRotation*`.
TL_TEST(drop_tilts_are_the_originals_angles) {
    using tl::core::DropTilt;
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::WeaponZ).axis, 2);
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::WeaponZ).radians, 1.5707964f);
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::ShieldZ).radians, -1.5707964f);
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::CrossbowX).axis, 0);
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::CrossbowX).radians, -1.2217305f);
    TL_CHECK_EQ(tl::core::drop_tilt_rotation(DropTilt::None).axis, -1);
}
