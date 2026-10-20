// The pathable grid, cast against hand-built collision: which cells
// `CLevel::calculatePassability` leaves open, what it counts, and what its
// erosion pass closes afterwards without uncounting.
#include "core/passability.h"
#include "core/collision.h"
#include "test_support.h"

#include <cmath>
#include <vector>

using tl::core::CollisionWorld;
using tl::core::kNoCollideMaterial;
using tl::core::PassabilityMap;
using tl::core::Vec3;

namespace {

void add_quad(CollisionWorld& world, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
              const Vec3& facing, uint32_t material = 0) {
    const Vec3 u = b - a;
    const Vec3 v = c - a;
    const Vec3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    if (n.x * facing.x + n.y * facing.y + n.z * facing.z >= 0.0f) {
        world.add_triangle(a, b, c, material);
        world.add_triangle(a, c, d, material);
    } else {
        world.add_triangle(a, c, b, material);
        world.add_triangle(a, d, c, material);
    }
}

void add_floor(CollisionWorld& world, float x0, float z0, float x1, float z1, float y) {
    add_quad(world, {x0, y, z0}, {x1, y, z0}, {x1, y, z1}, {x0, y, z1}, {0.0f, 1.0f, 0.0f});
}

/// A two-faced wall along x = `x`, from z0 to z1.
void add_wall_x(CollisionWorld& world, float x, float z0, float z1, float height,
                uint32_t material) {
    for (float side : {-1.0f, 1.0f}) {
        add_quad(world, {x, 0.0f, z0}, {x, 0.0f, z1}, {x, height, z1}, {x, height, z0},
                 {side, 0.0f, 0.0f}, material);
    }
}

size_t open_cells(const PassabilityMap& grid) {
    size_t n = 0;
    for (int x = 0; x < grid.width(); ++x) {
        for (int z = 0; z < grid.height(); ++z) {
            if (grid.map_passable(x, z)) ++n;
        }
    }
    return n;
}

}  // namespace

// Open floor: every cell the pass reaches is cast and counted, and the grid is
// sized from the list's bounds snapped to 0.4.
TL_TEST(passability_counts_every_cell_of_open_floor) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.ready());
    TL_CHECK(std::fabs(grid.origin_x() - -10.0f) < 1e-3f);
    TL_CHECK(grid.width() >= 49 && grid.width() <= 50);

    const uint32_t cells = static_cast<uint32_t>(grid.width() * grid.height());
    TL_CHECK_EQ(grid.passes().size(), size_t{1});
    TL_CHECK_EQ(grid.passes()[0].ray_casts, cells);
    TL_CHECK_EQ(grid.passable_cells(), cells);
    TL_CHECK(std::fabs(grid.pathable_area() - static_cast<float>(cells) / 6.5f) < 1e-3f);

    TL_CHECK(grid.map_passable(Vec3{0.0f, 0.0f, 0.0f}));
    // The erosion closes the corners, but not the count.
    TL_CHECK(!grid.map_passable(0, 0));
    TL_CHECK(open_cells(grid) < cells);
}

// The chest-height probes: a `nocollide` wall closes the cells within 0.3 of it.
TL_TEST(passability_a_nocollide_wall_closes_the_cells_beside_it) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    add_wall_x(world, 0.0f, -10.0f, 10.0f, 3.0f, kNoCollideMaterial);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);

    TL_CHECK(!grid.map_passable(Vec3{-0.2f, 0.0f, 0.0f}));
    TL_CHECK(!grid.map_passable(Vec3{0.2f, 0.0f, 0.0f}));
    TL_CHECK(grid.map_passable(Vec3{-1.0f, 0.0f, 0.0f}));
    TL_CHECK(grid.map_passable(Vec3{1.0f, 0.0f, 0.0f}));

    TL_CHECK(!grid.passable_between({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, true));
    TL_CHECK(grid.passable_between({-2.0f, 0.0f, -3.0f}, {-2.0f, 0.0f, 3.0f}, true));
}

// Only `nocollide` faces stop the probes: the same wall with any other material
// leaves the cells beside it open.
TL_TEST(passability_a_wall_of_another_material_does_not_close_cells) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    add_wall_x(world, 0.0f, -10.0f, 10.0f, 3.0f, 0);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.map_passable(Vec3{-0.2f, 0.0f, 0.0f}));
    TL_CHECK(grid.map_passable(Vec3{0.2f, 0.0f, 0.0f}));
}

// A `nocollide` floor is no floor.
TL_TEST(passability_a_nocollide_floor_is_not_pathable) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 0.0f, 10.0f, 0.0f);
    add_quad(world, {0.0f, 0.0f, -10.0f}, {10.0f, 0.0f, -10.0f}, {10.0f, 0.0f, 10.0f},
             {0.0f, 0.0f, 10.0f}, {0.0f, 1.0f, 0.0f}, kNoCollideMaterial);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.map_passable(Vec3{-5.0f, 0.0f, 0.0f}));
    TL_CHECK(!grid.map_passable(Vec3{5.0f, 0.0f, 0.0f}));
}

// A one-cell column is counted by the first pass and closed by the erosion.
TL_TEST(passability_erosion_closes_a_one_cell_column_it_counted) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    // Cell centres sit at -10 + 0.2 + 0.4k: ..., -0.6, -0.2, 0.2, 0.6, ... A wall at
    // -0.45 closes -0.6 and -0.2; one at 0.65 closes 0.6, leaving 0.2 alone.
    add_wall_x(world, -0.45f, -10.0f, 10.0f, 3.0f, kNoCollideMaterial);
    add_wall_x(world, 0.65f, -10.0f, 10.0f, 3.0f, kNoCollideMaterial);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);

    TL_CHECK(!grid.map_passable(Vec3{-0.2f, 0.0f, 0.0f}));
    TL_CHECK(!grid.map_passable(Vec3{0.6f, 0.0f, 0.0f}));
    TL_CHECK(!grid.map_passable(Vec3{0.2f, 0.0f, 0.0f}));
    TL_CHECK(grid.passable_cells() >= open_cells(grid) + static_cast<size_t>(grid.height() - 2));
}

// With room boxes, only the boxes are cast; everything else ends blocked.
TL_TEST(passability_casts_only_inside_the_room_boxes) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    world.build();

    std::vector<PassabilityMap::Box> rooms(3);
    rooms[0].merge({-10.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 10.0f});
    // rooms[1] stays null and is skipped.
    rooms[2].merge({-10.0f, 0.0f, -10.0f}, {-5.0f, 0.0f, 10.0f});

    PassabilityMap grid;
    grid.bake(world, &rooms);
    TL_CHECK_EQ(grid.passes().size(), size_t{2});
    TL_CHECK(grid.map_passable(Vec3{-3.0f, 0.0f, 0.0f}));
    TL_CHECK(!grid.map_passable(Vec3{5.0f, 0.0f, 0.0f}));
    // The second box lies inside the first: its open cells are cast and counted
    // again.
    TL_CHECK(grid.passes()[1].passable > 0);
    TL_CHECK_EQ(grid.passable_cells(), grid.passes()[0].passable + grid.passes()[1].passable);
}

// CBaseUnit::addToAvoidanceMap's circle on the map grid:
// blocks a route, undone by the decrement.
TL_TEST(passability_increment_map_blocks_a_circle_decrement_reopens_it) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.map_passable(Vec3{0.0f, 0.0f, 0.0f}));

    const float radius = 0.3f + 0.4f;  // COLLISION_RADIUS default + 0.4
    grid.increment_map_passability({-radius, 0.0f, -radius}, {radius, 0.0f, radius}, radius);
    TL_CHECK(!grid.map_passable(Vec3{0.0f, 0.0f, 0.0f}));
    // Well outside the circle: untouched.
    TL_CHECK(grid.map_passable(Vec3{5.0f, 0.0f, 0.0f}));

    grid.decrement_map_passability({-radius, 0.0f, -radius}, {radius, 0.0f, radius}, radius);
    TL_CHECK(grid.map_passable(Vec3{0.0f, 0.0f, 0.0f}));
}

// The object grid: blocks
// `positionPassable` without touching `mapPassable`.
TL_TEST(passability_increment_object_blocks_position_not_map) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);

    const float radius = 0.7f;
    grid.increment_object_passability({-radius, 0.0f, -radius}, {radius, 0.0f, radius}, radius);
    TL_CHECK(grid.map_passable(Vec3{0.0f, 0.0f, 0.0f}));
    TL_CHECK(!grid.position_passable(Vec3{0.0f, 0.0f, 0.0f}));

    grid.decrement_object_passability({-radius, 0.0f, -radius}, {radius, 0.0f, radius}, radius);
    TL_CHECK(grid.position_passable(Vec3{0.0f, 0.0f, 0.0f}));
}

// A door blocks exactly the cells its own collision covers, and opening it
// reopens exactly those.
TL_TEST(passability_door_collision_blocks_only_its_own_footprint) {
    CollisionWorld world;
    add_floor(world, -10.0f, -10.0f, 10.0f, 10.0f, 0.0f);
    world.build();

    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.map_passable(Vec3{0.0f, 1.0f, 0.0f}));
    TL_CHECK(grid.map_passable(Vec3{3.0f, 1.0f, 0.0f}));

    // A slab, not a vertical quad: the straight-down probe must meet a horizontal face. Not
    // square about the origin, so no probed corner lands on a triangle seam.
    CollisionWorld doors;
    doors.set_group(1);
    add_quad(doors, {-0.5f, 1.0f, -0.3f}, {0.9f, 1.0f, -0.3f}, {0.9f, 1.0f, 0.5f},
             {-0.5f, 1.0f, 0.5f}, {0.0f, 1.0f, 0.0f});
    doors.build();

    Vec3 box_low, box_high;
    TL_CHECK(doors.group_bounds(1, box_low, box_high));

    grid.increment_map_passability_collision(box_low, box_high, doors, 1);
    TL_CHECK(!grid.map_passable(Vec3{0.0f, 1.0f, 0.0f}));
    // Outside the door's own footprint in z: untouched.
    TL_CHECK(grid.map_passable(Vec3{0.0f, 1.0f, 3.0f}));
    // Elsewhere in the corridor: untouched.
    TL_CHECK(grid.map_passable(Vec3{3.0f, 1.0f, 0.0f}));

    grid.decrement_map_passability_collision(box_low, box_high, doors, 1);
    TL_CHECK(grid.map_passable(Vec3{0.0f, 1.0f, 0.0f}));
}

// A cell is the coordinate times 2.5 (`CLevel::mapPassable`), not divided by
// 0.4: 5.2 is cell 13, where 5.2f / 0.4f gives 12.
TL_TEST(a_cell_index_is_the_coordinate_times_two_and_a_half) {
    CollisionWorld world;
    add_floor(world, 0.0f, 0.0f, 20.0f, 20.0f, 0.0f);
    world.build();
    PassabilityMap grid;
    grid.bake(world, nullptr);
    TL_CHECK(grid.ready());
    TL_CHECK_EQ(grid.origin_x(), 0.0f);
    TL_CHECK_EQ(grid.cell_x(5.2f), 13);
    TL_CHECK_EQ(grid.cell_z(6.8f), 17);
    TL_CHECK_EQ(grid.cell_x(2.0f), 5);
}
