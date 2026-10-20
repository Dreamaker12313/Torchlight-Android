#include "core/no_spawn_region.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <vector>

namespace {

using tl::core::NoSpawnExtent;
using tl::core::NoSpawnRegion;
using tl::core::Vec3;

bool near_float(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

}  // namespace

/// A point strictly inside a finite region is refused (returns true).
TL_TEST(point_inside_a_region_is_refused) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{2.0f, -3.0f, 4.0f}, 0.0f));
}

/// A point outside a finite region is allowed (returns false).
TL_TEST(point_outside_is_allowed) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{10.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 15.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, -6.0f}, 0.0f));
}

/// A point on the boundary of each axis is inclusive (refused, returns true).
/// Points just outside (by 0.001) are allowed (returns false).
TL_TEST(point_on_the_boundary) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    // Min and max on each axis are inclusive.
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{-5.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{5.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, -10.0f, 0.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 10.0f, 0.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, -5.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, 5.0f}, 0.0f));

    // Just outside the boundary is allowed.
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{-5.001f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{5.001f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, -10.001f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 10.001f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, -5.001f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, 5.001f}, 0.0f));
}

/// A square overlapping a region is refused (returns true).
TL_TEST(square_overlapping_a_region_is_refused) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    // Center at 6.0, radius 2.0 -> x range [4.0, 8.0] overlaps [-5.0, 5.0].
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{6.0f, 0.0f, 0.0f}, 2.0f));
}

/// Touching boundaries (edges exactly equal) count as an overlap (returns true).
TL_TEST(square_touching_a_region) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    // Center at 7.0, radius 2.0 -> x range [5.0, 9.0], exactly touching box.max.x (5.0).
    // In, separation uses strict `>` (`ja`), so touching is not separated.
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{7.0f, 0.0f, 0.0f}, 2.0f));

    // Center at 7.001, radius 2.0 -> x range [5.001, 9.001] is strictly separated.
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{7.001f, 0.0f, 0.0f}, 2.0f));
}

/// Extent 0 (Null) is skipped; extent 2 (Infinite) refuses everything, including
/// after a null region and after a non-matching finite region.
TL_TEST(null_region_is_skipped) {
    std::vector<NoSpawnRegion> null_only{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Null}};
    // Point inside the null box coordinates is not refused because extent 0 is skipped.
    TL_CHECK(!tl::core::is_in_no_spawn_region(null_only, Vec3{0.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(null_only, Vec3{0.0f, 0.0f, 0.0f}, 2.0f));
}

TL_TEST(infinite_region_refuses_everything) {
    // 1. Single infinite region refuses points outside its coordinates.
    std::vector<NoSpawnRegion> inf_only{
        NoSpawnRegion{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, NoSpawnExtent::Infinite}};
    TL_CHECK(tl::core::is_in_no_spawn_region(inf_only, Vec3{100.0f, 200.0f, 300.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(inf_only, Vec3{100.0f, 200.0f, 300.0f}, 5.0f));

    // 2. Infinite region after a null region.
    std::vector<NoSpawnRegion> null_then_inf{
        NoSpawnRegion{Vec3{-5.0f, -5.0f, -5.0f}, Vec3{5.0f, 5.0f, 5.0f}, NoSpawnExtent::Null},
        NoSpawnRegion{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, NoSpawnExtent::Infinite}};
    TL_CHECK(tl::core::is_in_no_spawn_region(null_then_inf, Vec3{50.0f, 50.0f, 50.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(null_then_inf, Vec3{50.0f, 50.0f, 50.0f}, 1.0f));

    // 3. Infinite region after a non-matching finite region.
    std::vector<NoSpawnRegion> finite_then_inf{
        NoSpawnRegion{Vec3{-5.0f, -5.0f, -5.0f}, Vec3{5.0f, 5.0f, 5.0f}, NoSpawnExtent::Finite},
        NoSpawnRegion{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, NoSpawnExtent::Infinite}};
    TL_CHECK(tl::core::is_in_no_spawn_region(finite_then_inf, Vec3{50.0f, 50.0f, 50.0f}, 0.0f));
    TL_CHECK(tl::core::is_in_no_spawn_region(finite_then_inf, Vec3{50.0f, 50.0f, 50.0f}, 1.0f));
}

/// A NaN radius routes to the square test, where all separation comparisons
/// fail, returning true for a finite box.
TL_TEST(nan_radius) {
    std::vector<NoSpawnRegion> regions{
        NoSpawnRegion{Vec3{-5.0f, -10.0f, -5.0f}, Vec3{5.0f, 10.0f, 5.0f}, NoSpawnExtent::Finite}};
    const float nan_r = std::numeric_limits<float>::quiet_NaN();
    // Even a point far outside returns true when radius is NaN.
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{100.0f, 100.0f, 100.0f}, nan_r));

    // But an empty list still allows it.
    std::vector<NoSpawnRegion> empty;
    TL_CHECK(!tl::core::is_in_no_spawn_region(empty, Vec3{100.0f, 100.0f, 100.0f}, nan_r));
}

/// An empty list allows everything for both radius branches.
TL_TEST(empty_list_allows_everything) {
    std::vector<NoSpawnRegion> empty;
    TL_CHECK(!tl::core::is_in_no_spawn_region(empty, Vec3{0.0f, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::is_in_no_spawn_region(empty, Vec3{0.0f, 0.0f, 0.0f}, 1.0f));
}

/// `node_bounds_unturned`: scale X 6.5, scale Z 2, at (10, 0, 20), facing +Z:
/// The box is 6.5 across X (+-3.25), 2 across Z (+-1.0), and fixed +-1000 in Y.
TL_TEST(node_bounds_unturned) {
    const Vec3 pos{10.0f, 0.0f, 20.0f};
    const Vec3 right{1.0f, 0.0f, 0.0f};
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 forward{0.0f, 0.0f, 1.0f};
    const float scale_x = 6.5f;
    const float scale_z = 2.0f;

    const NoSpawnRegion box =
        tl::core::property_node_bounds(pos, right, up, forward, scale_x, scale_z);
    TL_CHECK_EQ(box.extent, NoSpawnExtent::Finite);
    TL_CHECK(near_float(box.min.x, 10.0f - 3.25f));  // 6.75
    TL_CHECK(near_float(box.max.x, 10.0f + 3.25f));  // 13.25
    TL_CHECK(near_float(box.min.y, -1000.0f));
    TL_CHECK(near_float(box.max.y, 1000.0f));
    TL_CHECK(near_float(box.min.z, 20.0f - 1.0f));  // 19.0
    TL_CHECK(near_float(box.max.z, 20.0f + 1.0f));  // 21.0
}

/// The node at (10, 0, 20), scale X 6.5 and Z 2, turned 45 degrees about Y: its bounds reach
/// 4.25 * sqrt(2)/2 either side of the centre on x and z.
TL_TEST(node_bounds_turned_45_degrees) {
    const Vec3 pos{10.0f, 0.0f, 20.0f};
    const float c = 0.7071067811865475f;
    const Vec3 right{c, 0.0f, -c};
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 forward{c, 0.0f, c};
    const float scale_x = 6.5f;
    const float scale_z = 2.0f;

    const NoSpawnRegion box =
        tl::core::property_node_bounds(pos, right, up, forward, scale_x, scale_z);
    TL_CHECK_EQ(box.extent, NoSpawnExtent::Finite);
    TL_CHECK(near_float(box.min.x, 6.9947962f));
    TL_CHECK(near_float(box.max.x, 13.0052038f));
    TL_CHECK(near_float(box.min.y, -1000.0f));
    TL_CHECK(near_float(box.max.y, 1000.0f));
    TL_CHECK(near_float(box.min.z, 16.9947962f));
    TL_CHECK(near_float(box.max.z, 23.0052038f));
}

/// `node_never_scaled`: constructor defaults of `CPropertyNode`
/// leave a zero-size finite box at (0, 0, 0).
TL_TEST(node_never_scaled) {
    const NoSpawnRegion unscaled;
    TL_CHECK_EQ(unscaled.extent, NoSpawnExtent::Finite);
    TL_CHECK_EQ(unscaled.min.x, 0.0f);
    TL_CHECK_EQ(unscaled.min.y, 0.0f);
    TL_CHECK_EQ(unscaled.min.z, 0.0f);
    TL_CHECK_EQ(unscaled.max.x, 0.0f);
    TL_CHECK_EQ(unscaled.max.y, 0.0f);
    TL_CHECK_EQ(unscaled.max.z, 0.0f);

    std::vector<NoSpawnRegion> regions{unscaled};
    // Exactly at (0, 0, 0) is refused (point inside/on boundary).
    TL_CHECK(tl::core::is_in_no_spawn_region(regions, Vec3{0.0f, 0.0f, 0.0f}, 0.0f));
    // Any point away is allowed.
    TL_CHECK(!tl::core::is_in_no_spawn_region(regions, Vec3{0.1f, 0.0f, 0.0f}, 0.0f));
}
