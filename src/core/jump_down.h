// A monster jumping down off a ledge at its target: the geometry of
// `CCharacter::attemptJumpDown` and the step
// `CMonster::jumpDownAI` takes along it. Allowed by a
// `Jump Down Area` property node (`CPropertyNode::characterUpdatedInLevel`,
// `setAllowJumpDown`); tried by `huntAI`.
// No OGRE and no file I/O.
#pragma once

#include "core/collision.h"
#include "core/random.h"
#include "core/route.h"

#include <functional>

namespace tl::core {

/// The path `attemptJumpDown` lays and the speed it is flown at, the same fields the death
/// flight uses.
struct JumpDown {
    Route path;          ///< start, midpoint, landing
    float speed = 0.0f;  ///< units of the path a second
};

/// `attemptJumpDown`'s geometry, after its `JUMP_DOWN` test. `random_open` is
/// `CLevel::randomOpenPosition(p, 1.0, false)`, which hands `p` back when it finds
/// nothing. Draws, after `random_open`'s own, two volatile rolls. False, with nothing drawn
/// past the failing test, when the jump is not taken.
bool plan_jump_down(const CollisionWorld& world, const Vec3& centre, float radius,
                    const Vec3& target, float sight,
                    const std::function<Vec3(const Vec3&)>& random_open, float jump_speed,
                    RunicRandom& rolls, JumpDown& out);

}  // namespace tl::core
