// The death flight (`CCharacter::die`, `dyingAI`) over
// hand-made floors: where a flung body lands, when it falls, and that the step
// carries it to the end of its path.
#include "core/death_flight.h"
#include "core/collision.h"
#include "core/random.h"
#include "test_support.h"

#include <cmath>

using tl::core::CollisionWorld;
using tl::core::DeathFlight;
using tl::core::RunicRandom;
using tl::core::Vec3;

namespace {

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

/// An upward-facing floor over [x0,x1] x [z0,z1] at height y.
void add_floor(CollisionWorld& world, float x0, float z0, float x1, float z1, float y) {
    world.add_triangle({x0, y, z0}, {x1, y, z1}, {x1, y, z0});
    world.add_triangle({x0, y, z0}, {x0, y, z1}, {x1, y, z1});
}

}  // namespace

// A body on open ground lands `force` along the blow, a radius above the floor.
TL_TEST(death_flight_on_a_floor_lands_where_the_blow_sends_it) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.build();
    float ground = -1.0f;
    TL_CHECK(world.ground_under(4.0f, 0.0f, 10.0f, ground));
    TL_CHECK(near(ground, 0.0f));

    RunicRandom rolls(1234);
    const DeathFlight flight = tl::core::plan_death_flight(world, {0.0f, 0.5f, 0.0f}, 0.5f,
                                                           {1.0f, 0.0f, 0.0f}, 4.0f, rolls);
    TL_CHECK(!flight.falls);
    TL_CHECK_EQ(flight.path.size(), size_t{3});
    const Vec3 end = flight.path.point(2);
    TL_CHECK(near(end.x, 4.0f));
    TL_CHECK(near(end.y, 0.5f));
    TL_CHECK(near(end.z, 0.0f));
    // The midpoint is halfway along and 2 to 2.8 above the start.
    const Vec3 mid = flight.path.point(1);
    TL_CHECK(near(mid.x, 2.0f));
    TL_CHECK(2.5f <= mid.y && mid.y <= 3.3f);
    // 40 over the length, held to 9..12, plus 0..2.
    TL_CHECK(9.0f <= flight.speed && flight.speed <= 14.0f);
}

// Past a floor's edge the first probe finds nothing: the body ends 5 below the
// end of the blow and falls, half as fast again.
TL_TEST(death_flight_over_an_edge_falls) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 2.0f, 20.0f, 0.0f);
    world.build();

    RunicRandom rolls(1234);
    const DeathFlight flight = tl::core::plan_death_flight(world, {0.0f, 0.5f, 0.0f}, 0.5f,
                                                           {1.0f, 0.0f, 0.0f}, 4.0f, rolls);
    TL_CHECK(flight.falls);
    const Vec3 end = flight.path.point(2);
    TL_CHECK(near(end.x, 4.0f));
    TL_CHECK(near(end.y, -4.5f));
    TL_CHECK(13.5f <= flight.speed && flight.speed <= 20.0f);
}

// Ground more than 4 below where the body stood does not catch it.
TL_TEST(death_flight_onto_ground_far_below_falls) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 2.0f, 20.0f, 0.0f);
    add_floor(world, 2.0f, -20.0f, 20.0f, 20.0f, -6.0f);
    world.build();

    RunicRandom rolls(99);
    const DeathFlight flight = tl::core::plan_death_flight(world, {0.0f, 0.5f, 0.0f}, 0.5f,
                                                           {1.0f, 0.0f, 0.0f}, 4.0f, rolls);
    TL_CHECK(flight.falls);
}

// Ground within 4 below (a step down) does.
TL_TEST(death_flight_onto_a_step_down_lands) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 2.0f, 20.0f, 0.0f);
    add_floor(world, 2.0f, -20.0f, 20.0f, 20.0f, -2.0f);
    world.build();

    RunicRandom rolls(99);
    const DeathFlight flight = tl::core::plan_death_flight(world, {0.0f, 0.5f, 0.0f}, 0.5f,
                                                           {1.0f, 0.0f, 0.0f}, 4.0f, rolls);
    TL_CHECK(!flight.falls);
    TL_CHECK(near(flight.path.point(2).y, -1.5f));
}

// `dyingAI` carries the body to the end of the path and says so once.
TL_TEST(death_flight_step_reaches_the_end) {
    CollisionWorld world;
    add_floor(world, -20.0f, -20.0f, 20.0f, 20.0f, 0.0f);
    world.build();
    RunicRandom rolls(7);
    const DeathFlight flight = tl::core::plan_death_flight(world, {0.0f, 0.5f, 0.0f}, 0.5f,
                                                           {0.0f, 0.0f, 1.0f}, 3.0f, rolls);

    float distance = 0.0f;
    float y = 0.5f;
    bool landed = false;
    int steps = 0;
    Vec3 at;
    while (!landed && steps < 600) {
        at = tl::core::step_death_flight(flight, distance, y, 1.0f / 60.0f, landed);
        y = at.y;
        ++steps;
    }
    TL_CHECK(landed);
    TL_CHECK(near(distance, flight.path.length()));
    const Vec3 end = flight.path.point(2);
    TL_CHECK(near(at.x, end.x, 1e-3f));
    TL_CHECK(near(at.y, end.y, 1e-3f));
    TL_CHECK(near(at.z, end.z, 1e-3f));
    // A short flight: well under a second at 9 or more units a second.
    TL_CHECK(steps < 60);
}

// `MATH::rotateY`: a quarter turn takes +x to -z.
TL_TEST(death_flight_rotate_y_turns_about_the_vertical) {
    const Vec3 v = tl::core::rotate_y({1.0f, 0.0f, 0.0f}, 1.5707964f);
    TL_CHECK(near(v.x, 0.0f));
    TL_CHECK(near(v.z, -1.0f));
    const Vec3 d = tl::core::blow_direction({0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 3.0f}, 0.0f);
    TL_CHECK(near(d.y, 0.0f));
    TL_CHECK(near(d.z, 1.0f));
}
