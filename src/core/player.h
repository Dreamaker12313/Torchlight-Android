// The player character: what the stick asks for, turned into a position, a
// facing and an animation. The speeds are the unit's `WALKINGSPEED` / `RUNNINGSPEED`.
//
// No OGRE here, and no rendering vocabulary.
#pragma once

#include "core/collision.h"
#include "core/input.h"
#include "core/motion.h"
#include "core/units.h"

#include <cstddef>
#include <vector>

namespace tl::core {

class PassabilityMap;
class NavGrid;

/// A character's size, for what the port does with it outside the motion:
/// separating units and measuring reach.
struct Body {
    /// `COLLISION_RADIUS`: the motion sphere's radius.
    float radius = kDefaultCollisionRadius;
    /// The character mesh's height.
    float height = 2.217f;
};

class Player {
public:
    void place(const Vec3& at, float facing_radians);

    /// Move the character without disturbing anything else about them (unlike `place()`, the
    /// speed and blocked state stay): a correction applied after a step.
    void nudge(const Vec3& at);

    /// The outside push (`+0x240`, X360 `+0x1d8`) `updateEffects` sets each
    /// frame from the player's `KNOCK BACK EFFECT`s, already bounded
    /// (`bound_outside_push`); the motion carries it.
    // PORT: an accessor of the port's player motion.
    void set_push(const Vec3& push) { motion_.push = push; }

    // PORT: an accessor of the port's player motion.
    void set_stats(const MovementStats& stats) { stats_ = stats; }
    void set_body(const Body& body);
    // PORT: an accessor of the port's player motion.
    const Body& body() const { return body_; }
    // PORT: an accessor of the port's player motion.
    const MovementStats& stats() const { return stats_; }

    /// Advance one frame. `camera_yaw` (radians about +Y) turns the screen-relative stick into
    /// the world. The step is `update_motion()`; `passability` is its fallback with no ground.
    /// `nav` gates a step into a `Path Node Occupation` blocker (`NavGrid::blocked()`).
    void update(float delta, const InputState& in, float camera_yaw, const CollisionWorld& world,
                const PassabilityMap* passability = nullptr, const NavGrid* nav = nullptr);

    // PORT: an accessor of the port's player motion.
    const Vec3& position() const { return position_; }

    /// Yaw about +Y, in radians, of the direction the character faces.
    // PORT: an accessor of the port's player motion.
    float facing() const { return facing_; }
    /// Turn where it stands (`CCharacter::turnToward`'s result), leaving its
    /// motion alone.
    void face(float facing_radians) { facing_ = facing_radians; }

    /// Ground speed this frame, in level units per second.
    // PORT: an accessor of the port's player motion.
    float speed() const { return speed_; }

    // PORT: an accessor of the port's player motion.
    bool moving() const { return speed_ > 0.0f; }

    /// The animation the character should be playing: "Run" or "Idle" (file stems beside the
    /// mesh).
    const char* animation() const;

    /// How fast to advance that animation, as a multiple of real time: the speed over
    /// RUNNINGSPEED, which `Run.SKELETON` is authored for.
    float animation_rate() const;

    /// Whether the last update() was stopped by scenery.
    // PORT: an accessor of the port's player motion.
    bool blocked() const { return blocked_; }

    /// Walk a route found by `NavGrid::find_path()`, waypoint by waypoint, through the same
    /// movement the stick drives. Any stick deflection cancels the route.
    void follow(std::vector<Vec3> route);
    void stop_following();
    // PORT: an accessor of the port's player motion.
    bool following() const { return !route_.empty(); }
    // PORT: an accessor of the port's player motion.
    size_t waypoints_left() const { return route_.size(); }

private:
    Vec3 position_;
    float facing_ = 0.0f;
    float speed_ = 0.0f;
    bool blocked_ = false;

    MovementStats stats_;
    Body body_;

    /// The motion's own state; its position is the sphere's centre, one radius
    /// above `position_`.
    MotionState motion_;

    /// The route left to walk, nearest waypoint first.
    std::vector<Vec3> route_;

    /// How many consecutive frames the route has made no headway, and how far there was left
    /// to go when last measured. Distance, not `blocked_`: a push-out happens after `update()`.
    int stalled_ = 0;
    float last_remaining_ = -1.0f;
};

/// How close counts as arrived at a waypoint, in level units.
inline constexpr float kArriveRadius = 0.35f;

/// How long to keep trying a waypoint the world is blocking, in frames.
inline constexpr int kStallFrames = 30;

/// How much closer a frame has to get for the walk to count as progress, in level units.
inline constexpr float kProgressPerFrame = 0.01f;

/// How fast the character turns to face where it is going, in radians/second. Not authored.
inline constexpr float kTurnRate = 12.0f;

}  // namespace tl::core
