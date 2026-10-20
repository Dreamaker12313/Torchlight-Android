// The jump down (`CCharacter::attemptJumpDown`) over hand-made floors:
// off a ledge toward a target below, and the tests that refuse it.
#include "core/jump_down.h"
#include "core/collision.h"
#include "core/random.h"
#include "test_support.h"

#include <cmath>

using tl::core::CollisionWorld;
using tl::core::JumpDown;
using tl::core::RunicRandom;
using tl::core::Vec3;

namespace {

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

/// An upward-facing floor over [x0,x1] x [z0,z1] at height y.
void add_floor(CollisionWorld& world, float x0, float z0, float x1, float z1, float y) {
    world.add_triangle({x0, y, z0}, {x1, y, z1}, {x1, y, z0});
    world.add_triangle({x0, y, z0}, {x0, y, z1}, {x1, y, z1});
}

/// A ledge 3 up for x below 0, the floor below beyond it.
void ledge(CollisionWorld& world) {
    add_floor(world, -20.0f, -20.0f, 0.0f, 20.0f, 3.0f);
    add_floor(world, 0.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.build();
}

/// `randomOpenPosition(p, 1.0, false)` finding the spot half a unit along x.
Vec3 open_ahead(const Vec3& p) { return {p.x + 0.5f, p.y, p.z}; }

}  // namespace

// From the ledge toward a target 6 away below: three units out, the open spot
// beside it, the floor under that, a radius up.
TL_TEST(jump_down_off_a_ledge_lands_below_toward_the_target) {
    CollisionWorld world;
    ledge(world);
    RunicRandom rolls(1234);
    JumpDown jump;
    TL_CHECK(tl::core::plan_jump_down(world, {-1.0f, 3.5f, 0.0f}, 0.5f, {5.0f, 0.5f, 0.0f}, 10.0f,
                                      open_ahead, 1.0f, rolls, jump));
    TL_CHECK_EQ(jump.path.size(), size_t{3});
    const Vec3 landing = jump.path.point(2);
    TL_CHECK(near(landing.x, 2.5f));
    TL_CHECK(near(landing.y, 0.5f));
    TL_CHECK(near(landing.z, 0.0f));
    // 0.65 of the way over, 1.5 to 2 above where it stood.
    const Vec3 mid = jump.path.point(1);
    TL_CHECK(near(mid.x, 1.275f));
    TL_CHECK(5.0f <= mid.y && mid.y <= 5.5f);
    // 40 over the length held to 9..10, plus 0..2.
    TL_CHECK(9.0f <= jump.speed && jump.speed <= 12.0f);
}

// `JUMP_SPEED` multiplies the speed, the draws being the same.
TL_TEST(jump_down_speed_takes_the_units_jump_speed) {
    CollisionWorld world;
    ledge(world);
    RunicRandom a(77);
    RunicRandom b(77);
    JumpDown plain;
    JumpDown fast;
    TL_CHECK(tl::core::plan_jump_down(world, {-1.0f, 3.5f, 0.0f}, 0.5f, {5.0f, 0.5f, 0.0f}, 10.0f,
                                      open_ahead, 1.0f, a, plain));
    TL_CHECK(tl::core::plan_jump_down(world, {-1.0f, 3.5f, 0.0f}, 0.5f, {5.0f, 0.5f, 0.0f}, 10.0f,
                                      open_ahead, 2.0f, b, fast));
    TL_CHECK(near(fast.speed, plain.speed * 2.0f));
}

// A target past the unit's sight is not jumped at, and nothing is drawn.
TL_TEST(jump_down_needs_the_target_in_sight) {
    CollisionWorld world;
    ledge(world);
    RunicRandom rolls(5);
    RunicRandom fresh(5);
    JumpDown jump;
    TL_CHECK(!tl::core::plan_jump_down(world, {-1.0f, 3.5f, 0.0f}, 0.5f, {15.0f, 0.5f, 0.0f}, 10.0f,
                                       open_ahead, 1.0f, rolls, jump));
    TL_CHECK_EQ(rolls.integer_between(0, 1000000), fresh.integer_between(0, 1000000));
}

// No open spot near the probe (`randomOpenPosition` hands it back): no jump.
TL_TEST(jump_down_needs_an_open_spot) {
    CollisionWorld world;
    ledge(world);
    RunicRandom rolls(5);
    JumpDown jump;
    TL_CHECK(!tl::core::plan_jump_down(
        world, {-1.0f, 3.5f, 0.0f}, 0.5f, {5.0f, 0.5f, 0.0f}, 10.0f,
        [](const Vec3& p) { return p; }, 1.0f, rolls, jump));
}

// Ground under the spot less than 0.8 below the unit's centre: no jump.
TL_TEST(jump_down_needs_a_drop) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 3.0f);
    world.build();
    RunicRandom rolls(5);
    JumpDown jump;
    TL_CHECK(!tl::core::plan_jump_down(world, {-1.0f, 3.5f, 0.0f}, 0.5f, {5.0f, 3.5f, 0.0f}, 10.0f,
                                       open_ahead, 1.0f, rolls, jump));
}
