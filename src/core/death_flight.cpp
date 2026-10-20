#include "core/death_flight.h"

#include <cmath>

namespace tl::core {

namespace {

// `die(CCharacter*, Vector3 const*, float, bool)::KDropVectors`, built on first
// use: the end and four points a quarter of a unit round it.
constexpr Vec3 kDropVectors[5] = {{0.0f, 0.0f, 0.0f},
                                  {0.25f, 0.0f, 0.0f},
                                  {-0.25f, 0.0f, 0.0f},
                                  {0.0f, 0.0f, 0.25f},
                                  {0.0f, 0.0f, -0.25f}};

// `MATH::expandBounds` then the radius twice over on every side, and
// `CLevel::sortForCollision`.
void sort_round(const CollisionWorld& world, const Vec3& a, const Vec3& b, float radius) {
    Vec3 low{std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z)};
    Vec3 high{std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z)};
    const float out = radius * -2.0f;
    const float in = radius + radius;
    low = {low.x + out, low.y + out, low.z + out};
    high = {high.x + in, high.y + in, high.z + in};
    world.sort_for_collision(low, high);
}

}  // namespace

// The blow's direction across the ground, normalised as `Ogre::Vector3::normalise`
// inlines it and turned by the jitter in degrees (`MATH::rotateY`).
Vec3 blow_direction(const Vec3& from, const Vec3& to, float jitter) {
    Vec3 d{to.x - from.x, 0.0f, to.z - from.z};
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (1e-08f < length) {
        const float inverse = 1.0f / length;
        d = {d.x * inverse, d.y * inverse, d.z * inverse};
    }
    return rotate_y(d, jitter * 0.017453292f);
}

DeathFlight plan_death_flight(const CollisionWorld& world, const Vec3& centre, float radius,
                              const Vec3& direction, float force, RunicRandom& rolls) {
    // `CCharacter::die`, the `DEATH_FLIGHT`
    // branch (0x8396xx).
    DeathFlight flight;
    const Vec3 start = centre;
    Vec3 end{force * direction.x + start.x, force * direction.y + start.y,
             force * direction.z + start.z};
    sort_round(world, end, start, radius);

    // A 0.4 sphere 2.5 up, from where it stands to where the blow sends it:
    // what it meets stops it there (x and z of the sphere's centre).
    CollisionHit swept;
    const bool walled = world.pre_sorted_sphere_collision(
        {start.x + 0.0f, start.y + 2.5f, start.z + 0.0f},
        {end.x + 0.0f, end.y + 2.5f, end.z + 0.0f}, 0.4f, false, swept);
    if (walled) {
        end.x = swept.centre.x;
        end.z = swept.centre.z;
    }

    // Five rays straight down from 100 to -100 round the end: the ground the
    // body lands on is the lowest one found no more than 4 below where it
    // stood. One ray that finds nothing, and it falls.
    flight.falls = true;
    const float probe_x = end.x;
    const float probe_z = end.z;
    float best = -4.0f;
    bool missed = false;
    for (const Vec3& v : kDropVectors) {
        CollisionHit ground;
        const Vec3 from{probe_x + v.x, 100.0f, probe_z + v.z};
        const Vec3 to{probe_x + v.x, -100.0f, probe_z + v.z};
        if (!world.ray_collision(from, to, true, ground)) {
            flight.falls = true;
            missed = true;
            break;
        }
        const float drop = start.y - ground.point.y;
        if (drop < 4.0f && best < drop) {
            const float side = radius * 0.0f;
            end = {side + ground.point.x, ground.point.y + radius, ground.point.z + side};
            flight.falls = false;
            best = drop;
        }
    }
    if (missed) {
        end.y = end.y - 5.0f;
    } else if (flight.falls && !walled) {
        // No ground close enough round the end: five units on along the blow.
        const Vec3 on{direction.x * 5.0f + start.x, direction.y * 5.0f + start.y,
                      direction.z * 5.0f + start.z};
        sort_round(world, on, start, radius);
        CollisionHit ground;
        const bool found =
            world.ray_collision({on.x, 100.0f, on.z}, {on.x, -100.0f, on.z}, true, ground);
        if (!found || 4.0f <= start.y - ground.point.y) {
            end = {on.x, on.y - 5.0f, on.z};
        } else {
            flight.falls = false;
        }
    }

    // `DROP`: the start, a midpoint 2 to 2.8 above where it stood, the end.
    Vec3 mid;
    mid.x = (end.x - start.x) * 0.5f + start.x;
    mid.z = (end.z - start.z) * 0.5f + start.z;
    mid.y = rolls.between(2.0f, 2.8f) + start.y;
    flight.path.add_point(start);
    flight.path.add_point(mid);
    flight.path.add_point(end);

    // Ten over the length, times four, held to 9..12; half as fast again for a
    // fall; and up to 2 more.
    float speed = (4.0f / flight.path.length()) * 10.0f;
    if (!(9.0f <= speed) && speed == speed) {
        speed = 9.0f;
    } else if (12.0f < speed) {
        speed = 12.0f;
    }
    if (flight.falls) speed = speed * 1.5f;
    flight.speed = rolls.between(0.0f, 2.0f) + speed;
    return flight;
}

Vec3 step_death_flight(const DeathFlight& flight, float& distance, float centre_y, float delta,
                       bool& landed) {
    // `CCharacter::dyingAI`: faster where the
    // path climbs than where the body already stands above its start.
    const Vec3 p1 = flight.path.point(1);
    const Vec3 p0 = flight.path.point(0);
    float above = centre_y - p0.y;
    if (!(0.0f < above)) above = 0.0f;
    const float pace = delta * flight.speed;
    distance = (((p1.y - p0.y) - above) * 0.65f + 0.75f) * pace + distance;
    landed = false;
    const float length = flight.path.length();
    if (length <= distance) {
        distance = length;
        landed = true;
    }
    return flight.path.spline_position_at_distance(distance);
}

}  // namespace tl::core
