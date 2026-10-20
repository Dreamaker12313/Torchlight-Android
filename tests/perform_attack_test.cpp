// CCharacter::performAttack's wall tests and a ranged shot's end, on made-up geometry.

#include "core/collision.h"
#include "core/combat.h"

#include <cmath>
#include "test_support.h"

namespace {

using tl::core::CollisionWorld;
using tl::core::RangedShot;
using tl::core::Vec3;

bool near_to(float a, float b) { return std::fabs(a - b) < 1e-4f; }

/// A wall across x = `x`, over z in [z0, z1] and y in [0, top], both faces.
void add_wall(CollisionWorld& world, float x, float z0, float z1, float top) {
    const Vec3 a{x, 0.0f, z0}, b{x, 0.0f, z1}, c{x, top, z1}, d{x, top, z0};
    world.add_triangle(a, b, c);
    world.add_triangle(a, c, d);
    world.add_triangle(a, c, b);
    world.add_triangle(a, d, c);
    world.build();
}

// Facing +X: `(sin f, 0, cos f)` is (1, 0, 0).
const float kFacingX = 1.5707964f;

}  // namespace

// a wall that all three rays meet, 5 above the feet.
TL_TEST(melee_wall_blocks_when_all_three_rays_meet_it) {
    CollisionWorld world;
    add_wall(world, 2.0f, -5.0f, 5.0f, 10.0f);
    TL_CHECK(tl::core::melee_attack_walled(world, Vec3{0, 0, 0}, kFacingX, Vec3{4, 0, 0}));
}

// The second ray starts 1.25 back and 0.75 along `getRight` (here -Z): a post
// the first ray meets is passed by the second.
TL_TEST(melee_wall_a_post_does_not_block) {
    CollisionWorld world;
    add_wall(world, 2.0f, -0.2f, 0.2f, 10.0f);
    TL_CHECK(!tl::core::melee_attack_walled(world, Vec3{0, 0, 0}, kFacingX, Vec3{4, 0, 0}));
    // ... and a wall below the rays' height does not count at all.
    CollisionWorld low;
    add_wall(low, 2.0f, -5.0f, 5.0f, 3.0f);
    TL_CHECK(!tl::core::melee_attack_walled(low, Vec3{0, 0, 0}, kFacingX, Vec3{4, 0, 0}));
}

// at a target, its x and z; the rise is clamped to a fifth of |(end.x, start.y, end.z)|.
TL_TEST(ranged_shot_rise_is_held_by_the_end_place) {
    const Vec3 start{0.0f, 1.5f, 0.0f};
    const Vec3 target{10.0f, 3.0f, 0.0f};
    const RangedShot shot = tl::core::ranged_shot(nullptr, start, Vec3{0, 0, 0}, 0.5f, &target,
                                                  0.5f, Vec3{1, 0, 0}, 8.0f);
    TL_CHECK(!shot.walled);
    TL_CHECK_EQ(shot.end.x, 10.0f);
    TL_CHECK_EQ(shot.end.z, 0.0f);
    const float limit = std::sqrt(100.0f + 1.5f * 1.5f) / 5.0f;
    TL_CHECK(near_to(shot.end.y, limit + 1.5f));

    // A rise inside the limit is kept.
    const Vec3 level{10.0f, 0.5f, 0.0f};
    const RangedShot low = tl::core::ranged_shot(nullptr, start, Vec3{0, 0, 0}, 0.5f, &level, 0.5f,
                                                 Vec3{1, 0, 0}, 8.0f);
    TL_CHECK(near_to(low.end.y, 2.0f));
}

// no target, `reach` along the facing: a wall the ray 0.8
// above both ends meets is the end, at the start's height.
TL_TEST(ranged_shot_without_a_target_and_into_a_wall) {
    const Vec3 start{0.0f, 1.5f, 0.0f};
    const RangedShot open = tl::core::ranged_shot(nullptr, start, Vec3{0, 0, 0}, 0.5f, nullptr,
                                                  0.0f, Vec3{1, 0, 0}, 8.0f);
    TL_CHECK(!open.walled);
    TL_CHECK_EQ(open.end.x, 8.0f);
    TL_CHECK_EQ(open.end.y, 1.5f);

    CollisionWorld world;
    add_wall(world, 5.0f, -5.0f, 5.0f, 10.0f);
    const RangedShot stopped = tl::core::ranged_shot(&world, start, Vec3{0, 0, 0}, 0.5f, nullptr,
                                                     0.0f, Vec3{1, 0, 0}, 8.0f);
    TL_CHECK(stopped.walled);
    TL_CHECK(near_to(stopped.end.x, 5.0f));
    TL_CHECK_EQ(stopped.end.y, 1.5f);
}

// `inStrikeRange` with a ranged weapon: no height test, effect 0x4e added.
TL_TEST(strike_range_with_a_ranged_weapon) {
    tl::core::UnitStats stats;
    stats.reach = 1.0f;
    stats.scale = 1.0f;
    stats.range_multiplier = 1.0f;
    stats.radius = 0.0f;
    tl::core::AttackDescription attack;
    attack.strike_range = 5.0f;
    const Vec3 here{0, 0, 0};
    const Vec3 high{3.0f, 4.0f, 0.0f};
    TL_CHECK(!tl::core::in_strike_range(stats, attack, here, high, 0.0f));
    TL_CHECK(tl::core::in_strike_range(stats, attack, here, high, 0.0f, true, 0.0f));
    const Vec3 far{7.0f, 0.0f, 0.0f};
    TL_CHECK(!tl::core::in_strike_range(stats, attack, here, far, 0.0f, true, 0.0f));
    TL_CHECK(tl::core::in_strike_range(stats, attack, here, far, 0.0f, true, 1.0f));
}

// the player's reach is 0.5 longer.
TL_TEST(strike_range_of_the_console_player) {
    tl::core::UnitStats stats;
    stats.reach = 1.0f;
    stats.scale = 1.0f;
    stats.range_multiplier = 1.0f;
    stats.radius = 0.0f;
    tl::core::AttackDescription attack;
    attack.strike_range = 1.0f;
    const Vec3 here{0, 0, 0};
    const Vec3 at{2.5f, 0.0f, 0.0f};  // the reach is 2.2
    TL_CHECK(!tl::core::in_strike_range(stats, attack, here, at, 0.0f));
    TL_CHECK(tl::core::in_strike_range(stats, attack, here, at, 0.0f, false, 0.0f, true));
    const Vec3 past{2.8f, 0.0f, 0.0f};
    TL_CHECK(!tl::core::in_strike_range(stats, attack, here, past, 0.0f, false, 0.0f, true));
}

// nearer than 1.5 is faced; the heading wraps; the weapon's
// arc widens `ATTACKANGLE`.
TL_TEST(facing_target_of_the_console) {
    const Vec3 here{0, 0, 0};
    // Behind, 1.4 away: faced. 1.6 away: not.
    TL_CHECK(tl::core::facing_target(0.0f, here, {0.0f, 0.0f, -1.4f}, 60.0f, 0.0f));
    TL_CHECK(!tl::core::facing_target(0.0f, here, {0.0f, 0.0f, -1.6f}, 60.0f, 0.0f));
    // 25 and 35 degrees off with `ATTACKANGLE` 60.
    const float d = 3.14159265f / 180.0f;
    const Vec3 at25{4.0f * std::sin(25.0f * d), 0.0f, 4.0f * std::cos(25.0f * d)};
    const Vec3 at35{4.0f * std::sin(35.0f * d), 0.0f, 4.0f * std::cos(35.0f * d)};
    TL_CHECK(tl::core::facing_target(0.0f, here, at25, 60.0f, 0.0f));
    TL_CHECK(!tl::core::facing_target(0.0f, here, at35, 60.0f, 0.0f));
    // A sword's 50 lets 35 by; a mace's 40 too; 45 only the sword.
    TL_CHECK(tl::core::facing_target(0.0f, here, at35, 60.0f, 40.0f));
    const Vec3 at45{4.0f * std::sin(45.0f * d), 0.0f, 4.0f * std::cos(45.0f * d)};
    TL_CHECK(!tl::core::facing_target(0.0f, here, at45, 60.0f, 40.0f));
    TL_CHECK(tl::core::facing_target(0.0f, here, at45, 60.0f, 50.0f));
    // The wrap: facing 170 degrees, the target at -170.
    const Vec3 across{4.0f * std::sin(-170.0f * d), 0.0f, 4.0f * std::cos(-170.0f * d)};
    TL_CHECK(tl::core::facing_target(170.0f * d, here, across, 60.0f, 0.0f));
    // A weapon's arc smaller than half of `ATTACKANGLE` changes nothing.
    TL_CHECK(tl::core::facing_target(0.0f, here, at25, 60.0f, 10.0f));
}

TL_TEST(console_weapon_arc_by_kind) {
    TL_CHECK_EQ(tl::core::console_weapon_arc(true, false, false, false, false), 50.0f);
    TL_CHECK_EQ(tl::core::console_weapon_arc(false, true, false, false, false), 50.0f);
    TL_CHECK_EQ(tl::core::console_weapon_arc(false, false, true, false, false), 40.0f);
    TL_CHECK_EQ(tl::core::console_weapon_arc(false, false, false, true, false), 70.0f);
    TL_CHECK_EQ(tl::core::console_weapon_arc(false, false, false, false, true), 70.0f);
    TL_CHECK_EQ(tl::core::console_weapon_arc(false, false, false, false, false), 0.0f);
}
