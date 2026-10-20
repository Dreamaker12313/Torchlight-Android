// How a character moves through the level's collision: a sphere, pushed along
// by its velocity, stopped and slid by what it meets, and kept on the ground.
// The position is the sphere's centre, one radius above the floor.
// CCharacter::updateMotion, run every frame by
// CCharacter::update after CCharacter::updatePathing.
//
#pragma once

#include "core/collision.h"

#include <functional>

namespace tl::core {

class PassabilityMap;

/// The fields of `CCharacter` the motion reads and writes.
struct MotionState {
    /// `+0x84`: the sphere's centre.
    Vec3 position;
    /// `+0x240`: an outside push (knockback), decayed by what it hits.
    Vec3 push;
    /// `+0x24c`: the velocity `updatePathing` sets for this frame.
    Vec3 velocity;
    /// `+0x258`: a multiplier on `velocity`.
    float velocity_scale = 1.0f;
    /// `+0x194`, `COLLISION_RADIUS`.
    float radius = kDefaultCollisionRadius;
    /// `+0x264`: the unit has somewhere to go.
    bool pathing = false;
    /// `+0x322`: the last frame found no ground to stand on.
    bool off_ground = false;
    /// `+0x1f4`: the last frame ended on ground.
    bool grounded = false;
    /// `+0x6d8`: the normal of that ground.
    Vec3 ground_normal{0.0f, 1.0f, 0.0f};
    /// `+0x324`: how long it has been off the ground.
    float off_ground_time = 0.0f;
    /// `walkingSpeed()` / `runningSpeed()`, read by the stand-still test.
    float walking_speed = 0.0f;
    float running_speed = 0.0f;

    /// Out of `update_motion`: the last slide was against a wall (normal rising under 0.2),
    /// and where; the caller (`LevelScript::character_update`) re-paths the unit.
    bool against_wall = false;
    Vec3 wall_point;
};

/// `CCharacter::updatePathing`, from the
/// direction it computes onwards: lay a horizontal direction along the ground
/// the unit stands on and scale it to `speed`.
/// DIVERGENCE: the caller hands the direction, not the unit's own `CPath`.
void steer(MotionState& state, float dir_x, float dir_z, float speed);

/// `CCharacter::updateEffects`, the end of its
/// knock-back half: the summed push has its height zeroed, and a
/// length above 10 for a player or 30 for any other character cut to that length.
Vec3 bound_outside_push(const Vec3& push, bool player);

/// `CCharacter::updateMotion`, the collision
/// part: gravity, the sphere cast down onto the floor, up to four slides along
/// what the move meets (ten against a wall), the ray that decides whether it
/// stands on ground, and the recovery when it does not. `passability`, when
/// given, stands in for `CLevel::randomOpenItemPosition`.
void update_motion(float delta, const CollisionWorld& world, MotionState& state,
                   const PassabilityMap* passability = nullptr);

/// `CCharacter::dropToGround`, the branch
/// `updateMotion` takes: one ray from +100 to -100 through the centre, and the
/// centre put one radius above what it meets unless that face is material 100.
void drop_to_ground(const CollisionWorld& world, MotionState& state);

/// The same function's other branch, `dropToGround(4.0, false)`, which `CLevel::addCharacter`
/// ends every character with: where the ray meets nothing or
/// a `nocollide` face, `random_open` (`CLevel::randomOpenPosition(at, radius, false)`) and the
/// ray again, the radius growing by 0.1 up to `reach`, thirty tries; none leaves it at 100.
void drop_to_ground_open(const CollisionWorld& world, MotionState& state, float reach,
                         const std::function<Vec3(const Vec3&, float)>& random_open);

}  // namespace tl::core
