// A body flung by the blow that killed it: the `DEATH_FLIGHT` branch of
// `CCharacter::die` and the movement
// `CCharacter::dyingAI` gives it.
// `applyDamage` hands
// `die` the blow's direction and force; with a force above 0 and a
// `DEATH_FLIGHT` animation, the body flies a three-point `DROP` path, pulled back
// by what it would hit and dropped onto ground no more than 4 below where it
// stood. With no such ground (an abyss) it ends 5 below and falls, fading out
// four times as fast.
//
// No OGRE and no file I/O.
#pragma once

#include "core/collision.h"
#include "core/random.h"
#include "core/route.h"
#include "core/runic_math.h"

namespace tl::core {

/// `CCharacter::rollAttack`'s direction for
/// `applyDamage`: from the attacker to the target on the ground plane,
/// normalised (`Ogre::Vector3::normalise`), turned by `jitter` degrees (its
/// `randomBetweenVolatile(-10, 10)`).
Vec3 blow_direction(const Vec3& from, const Vec3& to, float jitter);

/// What `applyDamage` hands `die` beside the
/// attacker: the blow's direction and force, and whether
/// it was a critical - which, or a blow of at least 1.75 times the maximum,
/// bursts the body.
struct KillingBlow {
    bool has_direction = false;
    Vec3 direction;
    float force = 0.0f;
    bool critical = false;
    /// `applyDamage`'s 7th argument: "Glancing Blow!" is shown instead of the
    /// damage being added to the number.
    bool glancing = false;
};

/// The path `die` lays and the speed it moves along it at (`+0x670`, `+0x67c`).
struct DeathFlight {
    Route path;          ///< start, midpoint, end
    bool falls = false;  ///< `+0x681`: no ground at the end
    float speed = 0.0f;  ///< `+0x67c`, units of the path a second
};

/// `die`'s flight: `centre` is where the body stands (`getPosition(true)`,
/// the sphere's centre), `radius` its `+0x194`, `direction` and `force` what
/// `applyDamage` hands on. Draws `randomBetweenVolatile(2, 2.8)` and then
/// `randomBetweenVolatile(0, 2)`.
DeathFlight plan_death_flight(const CollisionWorld& world, const Vec3& centre, float radius,
                              const Vec3& direction, float force, RunicRandom& rolls);

/// One step of `dyingAI` along the flight: the distance flown so far
/// (`+0x678`) grows by `((p1.y - p0.y) - max(y - p0.y, 0)) * 0.65 + 0.75` times
/// `delta * speed`, held to the path's length. Returns the body's new centre;
/// `landed` is set on the step that reaches the end (`completeDeath`).
Vec3 step_death_flight(const DeathFlight& flight, float& distance, float centre_y, float delta,
                       bool& landed);

}  // namespace tl::core
