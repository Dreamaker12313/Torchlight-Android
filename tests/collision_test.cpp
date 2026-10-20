// Collision and the player controller on hand-built triangles.
#include "core/collision.h"
#include "core/input.h"
#include "core/motion.h"
#include "core/passability.h"
#include "core/player.h"
#include "core/random.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using tl::core::CollisionHit;
using tl::core::CollisionWorld;
using tl::core::InputState;
using tl::core::kNoCollideMaterial;
using tl::core::Player;
using tl::core::Vec3;

namespace {

/// A quad `a b c d`, wound so its faces point along `facing`.
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

/// An upward-facing floor over [x0,x1] x [z0,z1] at height y.
void add_floor(CollisionWorld& world, float x0, float z0, float x1, float z1, float y,
               uint32_t material = 0) {
    add_quad(world, {x0, y, z0}, {x1, y, z0}, {x1, y, z1}, {x0, y, z1}, {0.0f, 1.0f, 0.0f},
             material);
}

/// A wall across z = `z` from x0 to x1, facing -z, of both faces when `both`.
void add_wall_z(CollisionWorld& world, float x0, float x1, float z, float base, float height,
                uint32_t material, bool both = true) {
    const float top = base + height;
    add_quad(world, {x0, base, z}, {x1, base, z}, {x1, top, z}, {x0, top, z}, {0.0f, 0.0f, -1.0f},
             material);
    if (both) {
        add_quad(world, {x0, base, z}, {x1, base, z}, {x1, top, z}, {x0, top, z},
                 {0.0f, 0.0f, 1.0f}, material);
    }
}

bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

void run(Player& player, const InputState& in, float yaw, const CollisionWorld& world,
         float seconds) {
    constexpr float kStep = 1.0f / 60.0f;
    const int steps = static_cast<int>(seconds / kStep + 0.5f);
    for (int i = 0; i < steps; ++i) player.update(kStep, in, yaw, world);
}

}  // namespace

// MATH::getMeshInformation: `nocollide` anywhere in the name makes a face 100,
// case-sensitively; anything else is the submesh's index.
TL_TEST(collision_material_is_100_for_nocollide_else_the_submesh_index) {
    TL_CHECK_EQ(tl::core::collision_material("multi_collision/nocollide", 3), kNoCollideMaterial);
    TL_CHECK_EQ(tl::core::collision_material("Material_#221/nocollide", 0), kNoCollideMaterial);
    TL_CHECK_EQ(tl::core::collision_material("collisions/collision", 2), uint32_t{2});
    TL_CHECK_EQ(tl::core::collision_material("collisions/NoCollide", 1), uint32_t{1});
}

// A ray meets a face only from the side its normal points to
// (CCollisionList::rayCollision: the start must not be behind the plane).
TL_TEST(collision_a_ray_meets_a_face_only_from_its_front) {
    CollisionWorld world;
    add_floor(world, -5.0f, -5.0f, 5.0f, 5.0f, 1.0f);
    world.build();

    CollisionHit hit;
    TL_CHECK(world.ray_collision({0.0f, 10.0f, 0.0f}, {0.0f, -10.0f, 0.0f}, false, hit));
    TL_CHECK(near(hit.point.y, 1.0f));
    TL_CHECK(near(hit.normal.y, 1.0f));

    CollisionHit from_below;
    TL_CHECK(!world.ray_collision({0.0f, -10.0f, 0.0f}, {0.0f, 10.0f, 0.0f}, false, from_below));
}

TL_TEST(collision_a_ray_down_meets_the_nearest_floor) {
    CollisionWorld world;
    add_floor(world, -5.0f, -5.0f, 5.0f, 5.0f, 0.0f);
    add_floor(world, -2.0f, -2.0f, 2.0f, 2.0f, 3.0f);
    world.build();

    float y = -1.0f;
    TL_CHECK(world.ground_under(0.0f, 0.0f, 10.0f, y));
    TL_CHECK(near(y, 3.0f));
    TL_CHECK(world.ground_under(4.0f, 0.0f, 10.0f, y));
    TL_CHECK(near(y, 0.0f));
}

// CCharacter::dropToGround: a material-100 face is not ground.
TL_TEST(collision_a_nocollide_floor_is_not_ground) {
    CollisionWorld world;
    add_floor(world, -5.0f, -5.0f, 5.0f, 5.0f, 0.0f, kNoCollideMaterial);
    world.build();
    TL_CHECK_EQ(world.nocollide_count(), size_t{2});

    float y = 0.0f;
    TL_CHECK(!world.ground_under(0.0f, 0.0f, 10.0f, y));
}

// A sphere swept down onto a floor stops with its centre one radius above it.
TL_TEST(collision_a_falling_sphere_rests_one_radius_above_the_floor) {
    CollisionWorld world;
    add_floor(world, -5.0f, -5.0f, 5.0f, 5.0f, 0.0f);
    world.build();

    world.sort_for_collision({-1.0f, -10.0f, -1.0f}, {1.0f, 10.0f, 1.0f});
    CollisionHit hit;
    TL_CHECK(world.pre_sorted_sphere_collision({0.5f, 5.0f, 0.5f}, {0.5f, -5.0f, 0.5f}, 0.3f, false,
                                               hit));
    TL_CHECK(near(hit.point.y, 0.0f));
    TL_CHECK(near(hit.centre.y, 0.3f));
    TL_CHECK(near(hit.centre.x, 0.5f));
}

// Another group is a unit's collision model: cast only for objects, never once switched off;
// a ray meets only `BLOCK` (`CLevel::objectRayCollision`), as material 100.
TL_TEST(collision_a_group_is_an_object_and_can_be_switched_off) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.set_group(4242);
    add_wall_z(world, -3.0f, 3.0f, 0.0f, 0.0f, 4.0f, 0);
    world.set_group(0);
    world.build();

    TL_CHECK(world.has_group(4242));
    TL_CHECK(!world.has_group(0));

    const Vec3 from{0.0f, 1.0f, -2.0f};
    const Vec3 to{0.0f, 1.0f, 2.0f};
    world.sort_for_collision({-1.0f, -5.0f, -3.0f}, {1.0f, 5.0f, 3.0f});
    CollisionHit hit;
    // Not `BLOCK`: the ray goes through, the sphere does not.
    TL_CHECK(!world.group_blocks(4242));
    TL_CHECK(!world.pre_sorted_ray_collision(from, to, true, hit));
    TL_CHECK(!world.ray_collision(from, to, true, hit));
    CollisionHit swept;
    TL_CHECK(world.pre_sorted_sphere_collision(from, to, 0.3f, true, swept));
    TL_CHECK_EQ(swept.group, int64_t{4242});
    TL_CHECK_EQ(swept.material, kNoCollideMaterial);
    TL_CHECK(!world.pre_sorted_sphere_collision(from, to, 0.3f, false, swept));

    world.set_group_blocks(4242, true);
    TL_CHECK(world.pre_sorted_ray_collision(from, to, true, hit));
    TL_CHECK_EQ(hit.group, int64_t{4242});
    TL_CHECK_EQ(hit.material, kNoCollideMaterial);
    TL_CHECK(near(hit.point.z, 0.0f));
    CollisionHit own;
    TL_CHECK(world.ray_collision(from, to, true, own));
    TL_CHECK_EQ(own.group, int64_t{4242});
    CollisionHit level_only;
    TL_CHECK(!world.pre_sorted_ray_collision(from, to, false, level_only));

    world.set_group_enabled(4242, false);
    CollisionHit opened;
    TL_CHECK(!world.pre_sorted_ray_collision(from, to, true, opened));

    // The level's own bounds leave the group out.
    Vec3 low, high;
    TL_CHECK(world.level_bounds(low, high));
    TL_CHECK(near(high.y, 0.0f));
}

TL_TEST(player_uses_the_authored_speeds) {
    CollisionWorld world;
    add_floor(world, -50.0f, -50.0f, 50.0f, 50.0f, 0.0f);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);

    // The figures media/units/players/base.dat authors.
    TL_CHECK(near(player.stats().walking_speed, 2.5f));
    TL_CHECK(near(player.stats().running_speed, 6.25f));

    InputState full;
    full.move_y = 1.0f;
    player.update(1.0f / 60.0f, full, 0.0f, world);
    TL_CHECK(near(player.speed(), 6.25f));

    // A dead stick is a stop, not a drift.
    player.update(1.0f / 60.0f, InputState{}, 0.0f, world);
    TL_CHECK(near(player.speed(), 0.0f));
    TL_CHECK(!player.moving());
}

// A move keeps its whole length along the floor, fall included, so a second covers slightly
// more than RUNNINGSPEED.
TL_TEST(player_moves_in_the_cameras_frame) {
    CollisionWorld world;
    add_floor(world, -50.0f, -50.0f, 50.0f, 50.0f, 0.0f);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);

    InputState forward;
    forward.move_y = 1.0f;

    // Camera looking along +Z: "up" on the stick is +Z.
    run(player, forward, 0.0f, world, 1.0f);
    TL_CHECK(near(player.position().z, 6.25f, 0.15f));
    TL_CHECK(near(player.position().x, 0.0f, 0.01f));
    TL_CHECK(near(player.position().y, 0.0f, 0.01f));

    // Camera turned a quarter turn: the same stick now sends the character
    // along +X.
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, forward, 1.57079633f, world, 1.0f);
    TL_CHECK(near(player.position().x, 6.25f, 0.15f));
    TL_CHECK(near(player.position().z, 0.0f, 0.01f));
}

TL_TEST(player_strafes_to_the_screens_right_not_its_mirror) {
    CollisionWorld world;
    add_floor(world, -50.0f, -50.0f, 50.0f, 50.0f, 0.0f);
    world.build();

    Player player;
    InputState right;
    right.move_x = 1.0f;

    // Forward is (sin yaw, 0, cos yaw); at yaw 0 the screen's right is world -X.
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, right, 0.0f, world, 1.0f);
    TL_CHECK(near(player.position().x, -6.25f, 0.15f));
    TL_CHECK(near(player.position().z, 0.0f, 0.01f));

    InputState left;
    left.move_x = -1.0f;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, left, 0.0f, world, 1.0f);
    TL_CHECK(near(player.position().x, 6.25f, 0.15f));

    // Looking along +X, the screen's right is world +Z.
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, right, 1.57079633f, world, 1.0f);
    TL_CHECK(near(player.position().z, 6.25f, 0.15f));
    TL_CHECK(near(player.position().x, 0.0f, 0.01f));
}

TL_TEST(player_forward_and_right_stay_perpendicular_at_any_camera_angle) {
    CollisionWorld world;
    add_floor(world, -80.0f, -80.0f, 80.0f, 80.0f, 0.0f);
    world.build();

    InputState fwd;
    fwd.move_y = 1.0f;
    InputState right;
    right.move_x = 1.0f;

    for (int step = 0; step < 8; ++step) {
        const float yaw = static_cast<float>(step) * 0.7853981634f;

        Player a;
        a.place({0.0f, 0.0f, 0.0f}, 0.0f);
        run(a, fwd, yaw, world, 0.5f);

        Player b;
        b.place({0.0f, 0.0f, 0.0f}, 0.0f);
        run(b, right, yaw, world, 0.5f);

        const float dot = a.position().x * b.position().x + a.position().z * b.position().z;
        TL_CHECK(near(dot, 0.0f, 0.05f));
        TL_CHECK(near(a.speed(), b.speed()));
    }
}

TL_TEST(player_plays_run_when_moving_and_idle_when_not) {
    CollisionWorld world;
    add_floor(world, -50.0f, -50.0f, 50.0f, 50.0f, 0.0f);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    TL_CHECK_EQ(std::string(player.animation()), std::string("Idle"));

    InputState half;
    half.move_y = 0.5f;
    player.update(1.0f / 60.0f, half, 0.0f, world);
    TL_CHECK_EQ(std::string(player.animation()), std::string("Run"));

    TL_CHECK(player.speed() > 2.5f && player.speed() < 6.25f);
    TL_CHECK(near(player.animation_rate(), player.speed() / 6.25f));
}

// The stick is held to the cells a route could use, and the motion refuses the
// step that leaves nothing under the sphere.
TL_TEST(player_will_not_walk_off_the_collision_mesh) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 5.0f, 0.0f);
    world.build();
    tl::core::PassabilityMap grid;
    grid.bake(world, nullptr);

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);

    InputState forward;
    forward.move_y = 1.0f;
    constexpr float kStep = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) player.update(kStep, forward, 0.0f, world, &grid);

    TL_CHECK(player.position().z <= 5.05f);
    TL_CHECK(player.position().z > 4.5f);
    TL_CHECK(player.blocked());
}

// A floor a little higher than the one stood on is climbed: the ray under the
// sphere finds it within a radius and a tenth, and puts the sphere on it.
TL_TEST(player_follows_the_ground_height) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 2.0f, 0.0f);
    add_floor(world, -20.0f, 2.0f, 20.0f, 20.0f, 0.2f);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);

    InputState forward;
    forward.move_y = 1.0f;
    run(player, forward, 0.0f, world, 1.0f);

    TL_CHECK(player.position().z > 2.5f);
    TL_CHECK(near(player.position().y, 0.2f, 0.02f));
}

// A `nocollide` wall stops the sphere a radius short of it.
TL_TEST(player_is_stopped_by_a_nocollide_wall) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    add_wall_z(world, -10.0f, 10.0f, 3.0f, 0.0f, 3.0f, kNoCollideMaterial);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    InputState forward;
    forward.move_y = 1.0f;
    run(player, forward, 0.0f, world, 2.0f);

    TL_CHECK(player.position().z < 3.0f - 0.25f);
    TL_CHECK(player.position().z > 2.0f);
    TL_CHECK(player.blocked());
}

// The player's sphere is `COLLISION_RADIUS` 0.3, `CCharacter::unitInit`'s default.
TL_TEST(player_passes_a_gap_wider_than_its_sphere) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    add_wall_z(world, -10.0f, -0.4f, 3.0f, 0.0f, 3.0f, kNoCollideMaterial);
    add_wall_z(world, 0.4f, 10.0f, 3.0f, 0.0f, 3.0f, kNoCollideMaterial);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    InputState forward;
    forward.move_y = 1.0f;
    run(player, forward, 0.0f, world, 2.0f);

    TL_CHECK(player.position().z > 5.0f);
}

TL_TEST(scripted_walk_is_a_closed_square_in_open_ground) {
    CollisionWorld world;
    add_floor(world, -80.0f, -80.0f, 80.0f, 80.0f, 0.0f);
    world.build();

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);

    constexpr float kStep = 1.0f / 60.0f;
    const int steps = static_cast<int>(12.0f / kStep);
    for (int i = 0; i < steps; ++i) {
        player.update(kStep, tl::core::scripted_walk(static_cast<float>(i) * kStep), 0.0f, world);
    }

    TL_CHECK(near(player.position().x, 0.0f, 0.2f));
    TL_CHECK(near(player.position().z, 0.0f, 0.2f));
    TL_CHECK(!player.blocked());
}

// **A door that opens has to stop blocking.** A `Unit Trigger`'s collision is
// its own group; switching the group off lets the player through.
TL_TEST(a_door_group_blocks_until_it_is_switched_off) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.set_group(4242);
    add_wall_z(world, -3.0f, 3.0f, 2.0f, 0.0f, 4.0f, kNoCollideMaterial);
    world.set_group(0);
    world.build();

    InputState forward;
    forward.move_y = 1.0f;

    Player player;
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, forward, 0.0f, world, 1.0f);
    TL_CHECK(player.position().z < 2.0f);

    world.set_group_enabled(4242, false);
    player.place({0.0f, 0.0f, 0.0f}, 0.0f);
    run(player, forward, 0.0f, world, 1.0f);
    TL_CHECK(player.position().z > 4.0f);

    // Group 0 is the level and refuses to be switched off.
    world.set_group_enabled(0, false);
    float surface = 0.0f;
    TL_CHECK(world.ground_under(-2.0f, 0.0f, 5.0f, surface));
}

namespace {

// The bits, not the values: -0 against 0 and one NaN against another count.
bool same_bits(float a, float b) {
    uint32_t a_bits = 0;
    uint32_t b_bits = 0;
    std::memcpy(&a_bits, &a, sizeof a_bits);
    std::memcpy(&b_bits, &b, sizeof b_bits);
    return a_bits == b_bits;
}

bool same_hit(bool hit_a, const tl::core::CollisionHit& a, bool hit_b,
              const tl::core::CollisionHit& b) {
    if (hit_a != hit_b) return false;
    if (!hit_a) return true;
    return same_bits(a.point.x, b.point.x) && same_bits(a.point.y, b.point.y) &&
           same_bits(a.point.z, b.point.z) && same_bits(a.normal.x, b.normal.x) &&
           same_bits(a.normal.y, b.normal.y) && same_bits(a.normal.z, b.normal.z) &&
           a.material == b.material && a.group == b.group;
}

}  // namespace

// `ray_collision` over the ray's bounds answers as every face of the world would, to the
// bit, on random worlds and rays.
TL_TEST(collision_a_long_ray_answers_as_every_face_would) {
    tl::core::RunicRandom rng(20260929);
    size_t hits = 0, rays = 0;
    for (int world_index = 0; world_index < 12; ++world_index) {
        CollisionWorld world;
        auto coord = [&rng]() {
            // A third of coordinates exactly on a cell edge.
            if (rng.integer_between(0, 2) == 0)
                return 4.0f * static_cast<float>(rng.integer_between(-8, 8));
            return rng.between(-32.0f, 32.0f);
        };
        for (int f = 0; f < 150; ++f) {
            const Vec3 a{coord(), rng.between(-2.0f, 6.0f), coord()};
            const Vec3 b{a.x + rng.between(-12.0f, 12.0f), rng.between(-2.0f, 6.0f),
                         a.z + rng.between(-12.0f, 12.0f)};
            const Vec3 c{a.x + rng.between(-12.0f, 12.0f), rng.between(-2.0f, 6.0f),
                         a.z + rng.between(-12.0f, 12.0f)};
            const uint32_t material = rng.integer_between(0, 5) == 0 ? kNoCollideMaterial : 0u;
            if (f % 5 == 0) world.set_group(1 + f % 3);
            world.add_triangle(a, b, c, material);
            world.set_group(0);
        }
        world.build();
        world.set_group_blocks(2, true);
        Vec3 world_low, world_high;
        TL_CHECK(world.bounds(world_low, world_high));
        for (int r = 0; r < 400; ++r) {
            Vec3 from{coord(), rng.between(-3.0f, 7.0f), coord()};
            Vec3 to{coord(), rng.between(-3.0f, 7.0f), coord()};
            if (r % 7 == 0) to.z = from.z;   // along x
            if (r % 11 == 0) to.x = from.x;  // along z
            const bool objects = (r % 2) == 0;
            tl::core::CollisionHit walked, every;
            const bool hit_walked = world.ray_collision(from, to, objects, walked);
            world.sort_for_collision({world_low.x - 1, world_low.y - 1, world_low.z - 1},
                                     {world_high.x + 1, world_high.y + 1, world_high.z + 1});
            const bool hit_every = world.pre_sorted_ray_collision(from, to, objects, every);
            TL_CHECK(same_hit(hit_walked, walked, hit_every, every));
            hits += hit_walked ? 1 : 0;
            ++rays;
        }
    }
    // Enough of them meet something for the comparison to mean anything.
    TL_CHECK(hits > rays / 4);
}

// `CQuadtreeCollisionNode::sort` and `sortForCollisionByPoints`:
// exactly the faces whose bounds meet the box on x and z.
TL_TEST(collision_the_quadtree_sorts_exactly_the_faces_that_meet_the_box) {
    tl::core::RunicRandom rng(20261013);
    for (int world_index = 0; world_index < 8; ++world_index) {
        CollisionWorld world;
        struct Box {
            Vec3 low, high;
            bool level;
        };
        std::vector<Box> boxes;
        const float spread = world_index % 2 == 0 ? 40.0f : 1500.0f;  // past the 1000 diagonal too
        for (int f = 0; f < 300; ++f) {
            const Vec3 a{rng.between(-spread, spread), rng.between(-2.0f, 6.0f),
                         rng.between(-spread, spread)};
            const float size = f % 9 == 0 ? spread : 6.0f;
            const Vec3 b{a.x + rng.between(-size, size), rng.between(-2.0f, 6.0f),
                         a.z + rng.between(-size, size)};
            const Vec3 c{a.x + rng.between(-size, size), rng.between(-2.0f, 6.0f),
                         a.z + rng.between(-size, size)};
            const bool level = f % 7 != 0;
            if (!level) world.set_group(77);
            world.add_triangle(a, b, c, 0);
            world.set_group(0);
            Box box{a, a, level};
            for (const Vec3& p : {b, c}) {
                box.low = {std::fmin(box.low.x, p.x), std::fmin(box.low.y, p.y),
                           std::fmin(box.low.z, p.z)};
                box.high = {std::fmax(box.high.x, p.x), std::fmax(box.high.y, p.y),
                            std::fmax(box.high.z, p.z)};
            }
            boxes.push_back(box);
        }
        world.build();
        for (int q = 0; q < 200; ++q) {
            Vec3 low{rng.between(-spread, spread), -10.0f, rng.between(-spread, spread)};
            const float reach = q % 5 == 0 ? spread : 3.0f;
            Vec3 high{low.x + rng.between(0.0f, reach), 10.0f, low.z + rng.between(0.0f, reach)};
            world.sort_for_collision(low, high);
            std::vector<uint32_t> got = world.sorted_faces();
            // No face twice.
            std::vector<uint32_t> ordered = got;
            std::sort(ordered.begin(), ordered.end());
            TL_CHECK(std::adjacent_find(ordered.begin(), ordered.end()) == ordered.end());
            std::vector<uint32_t> want;
            for (uint32_t i = 0; i < boxes.size(); ++i) {
                const Box& b = boxes[i];
                if (!b.level) continue;
                if (b.high.x < low.x || b.high.z < low.z || high.x < b.low.x || high.z < b.low.z) {
                    continue;
                }
                want.push_back(i);
            }
            TL_CHECK(ordered == want);

            // The same box by its two corners and a radius.
            const float radius = 0.75f;
            const Vec3 a{low.x + radius, 0.0f, high.z - radius};
            const Vec3 b{high.x - radius, 1.0f, low.z + radius};
            if (a.x <= b.x && b.z <= a.z) {
                Vec3 grown_low{a.x - radius, 0.0f - radius, b.z - radius};
                Vec3 grown_high{b.x + radius, 1.0f + radius, a.z + radius};
                world.sort_for_collision(grown_low, grown_high);
                const std::vector<uint32_t> by_box = world.sorted_faces();
                world.sort_for_collision_by_points(a, b, radius);
                TL_CHECK(world.sorted_faces() == by_box);
            }
        }
    }
}

// `CQuadtreeCollisionNode::addData` and `sort`: own faces first, then the
// children low-x/high-z, high-x/high-z, low-x/low-z, high-x/low-z.
TL_TEST(collision_the_quadtree_walks_its_children_in_the_consoles_order) {
    CollisionWorld world;
    auto tiny = [&world](float x, float z) {
        world.add_triangle({x, 0.0f, z}, {x + 0.5f, 0.0f, z}, {x, 0.0f, z + 0.5f}, 0);
    };
    // Face 0 and 1 pin the bounds to (0,0)-(16,16) and straddle the middle, so
    // they stay with the root.
    world.add_triangle({0.0f, 0.0f, 0.0f}, {16.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 16.0f}, 0);
    world.add_triangle({16.0f, 0.0f, 16.0f}, {16.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 16.0f}, 0);
    tiny(12.0f, 2.0f);   // 2: high x, low z  - the fourth child
    tiny(2.0f, 2.0f);    // 3: low x, low z   - the third
    tiny(12.0f, 12.0f);  // 4: high x, high z - the second
    tiny(2.0f, 12.0f);   // 5: low x, high z  - the first
    world.build();
    world.sort_for_collision({-1.0f, -1.0f, -1.0f}, {17.0f, 1.0f, 17.0f});
    const std::vector<uint32_t> want{0, 1, 5, 4, 3, 2};
    TL_CHECK(world.sorted_faces() == want);
}

// `CLevel::objectRayCollision` asks only `BLOCK` units; `objectSphereCollision` every
// collideable one.
TL_TEST(collision_a_ray_passes_a_chest_and_stops_at_a_gate) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.set_group(1);  // the chest
    add_wall_z(world, -1.0f, 1.0f, 2.0f, 0.0f, 3.0f, 0);
    world.set_group(2);  // the gate
    add_wall_z(world, -1.0f, 1.0f, 5.0f, 0.0f, 3.0f, 0);
    world.set_group(0);
    world.build();
    world.set_group_blocks(2, true);

    const Vec3 from{0.0f, 1.0f, 0.0f};
    const Vec3 to{0.0f, 1.0f, 8.0f};
    CollisionHit ray;
    TL_CHECK(world.ray_collision(from, to, true, ray));
    TL_CHECK_EQ(ray.group, int64_t{2});
    TL_CHECK(near(ray.point.z, 5.0f));
    TL_CHECK_EQ(ray.material, kNoCollideMaterial);
    CollisionHit none;
    TL_CHECK(!world.ray_collision(from, to, false, none));

    world.sort_for_collision_by_points(from, to, 0.3f);
    CollisionHit sphere;
    TL_CHECK(world.pre_sorted_sphere_collision(from, to, 0.3f, true, sphere));
    TL_CHECK_EQ(sphere.group, int64_t{1});
    TL_CHECK(near(sphere.point.z, 2.0f));

    // A gate whose collision is off is no longer met by either cast, but the
    // map grid's probe still finds it (`CBaseUnit::rayCollision` forced).
    world.set_group_enabled(2, false);
    TL_CHECK(!world.ray_collision(from, to, true, ray));
    CollisionHit probe;
    TL_CHECK(world.ray_collision_group(2, from, to, probe));

    // A hit on an object under the feet is never ground (material 100).
    world.set_group(3);
    add_floor(world, 9.0f, 9.0f, 11.0f, 11.0f, 2.0f);
    world.set_group(0);
    world.build();
    float y = 0.0f;
    TL_CHECK(world.ground_under(10.0f, 10.0f, 5.0f, y));
    TL_CHECK(near(y, 0.0f));  // not `BLOCK`: the ray goes through to the floor
    world.set_group_blocks(3, true);
    TL_CHECK(!world.ground_under(10.0f, 10.0f, 5.0f, y));
}
