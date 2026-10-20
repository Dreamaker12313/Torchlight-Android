#include "core/jump_down.h"

#include <cmath>

namespace tl::core {

bool plan_jump_down(const CollisionWorld& world, const Vec3& centre, float radius,
                    const Vec3& target, float sight,
                    const std::function<Vec3(const Vec3&)>& random_open, float jump_speed,
                    RunicRandom& rolls, JumpDown& out) {
    // `CCharacter::attemptJumpDown`, past the
    // `JUMP_DOWN` test. From 2.5 above where the unit stands, towards the
    // target on the ground plane.
    const Vec3 start{centre.x + 0.0f, centre.y + 2.5f, centre.z + 0.0f};
    float dx = target.x - start.x;
    float dy = 0.0f;
    float dz = target.z - start.z;
    const float distance = std::sqrt(dx * dx + 0.0f + dz * dz);
    if (1e-08f < distance) {
        const float inverse = 1.0f / distance;
        dx = dx * inverse;
        dy = inverse * 0.0f;
        dz = dz * inverse;
    }
    // Only at a target the unit could see.
    if (!(distance <= sight)) return false;
    const float again = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (1e-08f < again) {
        const float inverse = 1.0f / again;
        dx = dx * inverse;
        dy = dy * inverse;
        dz = dz * inverse;
    }
    // Three units out at most (`cmpltss` against 3.0).
    const float reach = distance < 3.0f ? distance : 3.0f;
    const Vec3 probe{dx * reach + start.x, dy * reach + start.y, dz * reach + start.z};

    // `CLevel::sphereCollision(start, probe, 0.05)`: a wall
    // in the way at head height and there is no jump.
    {
        constexpr float kSweep = 0.05f;
        const Vec3 low{std::fmin(start.x, probe.x) - kSweep, std::fmin(start.y, probe.y) - kSweep,
                       std::fmin(start.z, probe.z) - kSweep};
        const Vec3 high{std::fmax(start.x, probe.x) + kSweep, std::fmax(start.y, probe.y) + kSweep,
                        std::fmax(start.z, probe.z) + kSweep};
        world.sort_for_collision(low, high);
        CollisionHit wall;
        if (world.pre_sorted_sphere_collision(start, probe, kSweep, false, wall)) return false;
    }

    // An open spot within one unit of the probe: none found (the probe handed
    // back as it was), no jump.
    const Vec3 open = random_open(probe);
    if (open.x == probe.x && open.y == probe.y && open.z == probe.z) return false;

    // The ground under it (`CLevel::rayCollision(..., false)`).
    CollisionHit ground;
    if (!world.ray_collision({open.x, 100.0f, open.z}, {open.x, -100.0f, open.z}, true, ground)) {
        return false;
    }
    const float drop = centre.y - ground.point.y;
    if (!(0.8f < drop && drop < 6.5f) || ground.material == 100 || !(0.8f < ground.normal.y)) {
        return false;
    }
    const float side = radius * 0.0f;
    const Vec3 landing{side + ground.point.x, radius + ground.point.y, ground.point.z + side};

    // `DROP`: where it stands, a point 0.65 of the way over and 1.5 to 2
    // above, and the landing.
    Vec3 mid;
    mid.x = (landing.x - centre.x) * 0.65f + centre.x;
    mid.z = (landing.z - centre.z) * 0.65f + centre.z;
    mid.y = rolls.between(1.5f, 2.0f) + centre.y;
    out = JumpDown{};
    out.path.add_point(centre);
    out.path.add_point(mid);
    out.path.add_point(landing);

    // Ten over the length, times four, held to 9..10, up to 2 more, then
    // `JUMP_SPEED`.
    float speed = (4.0f / out.path.length()) * 10.0f;
    if (!(9.0f <= speed) && speed == speed) {
        speed = 9.0f;
    } else if (10.0f < speed) {
        speed = 10.0f;
    }
    speed = rolls.between(0.0f, 2.0f) + speed;
    out.speed = jump_speed * speed;
    return true;
}

}  // namespace tl::core
