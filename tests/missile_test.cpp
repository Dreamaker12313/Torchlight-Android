#include "core/missile.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "test_support.h"

using namespace tl::core;

// `ParticleUniverse::DynamicAttributeCurved::getValue`, linear: the
// first point's value before it, the last's after it, a line between.
TL_TEST(curved_attribute_linear) {
    DynamicAttribute a;
    a.kind = DynamicAttribute::Kind::Curved;
    a.points = {{0.25f, 10.0f}, {0.5f, 20.0f}, {1.0f, 0.0f}};
    TL_CHECK_EQ(a.at(0.0f), 10.0f);
    TL_CHECK_EQ(a.at(0.25f), 10.0f);
    TL_CHECK(std::fabs(a.at(0.375f) - 15.0f) < 1e-5f);
    TL_CHECK_EQ(a.at(0.5f), 20.0f);
    TL_CHECK(std::fabs(a.at(0.75f) - 10.0f) < 1e-5f);
    TL_CHECK_EQ(a.at(1.0f), 0.0f);
    TL_CHECK_EQ(a.at(2.0f), 0.0f);
    DynamicAttribute none;
    none.kind = DynamicAttribute::Kind::Curved;
    TL_CHECK_EQ(none.at(0.5f), 0.0f);
}

// The spline kind (3): `Ogre::SimpleSpline::interpolate(x / range)` over the points by index.
TL_TEST(curved_attribute_spline) {
    DynamicAttribute a;
    a.kind = DynamicAttribute::Kind::Curved;
    a.spline = true;
    a.points = {{0.0f, 0.0f}, {0.5f, 10.0f}, {1.0f, 0.0f}};
    TL_CHECK_EQ(a.at(0.0f), 0.0f);
    TL_CHECK_EQ(a.at(0.5f), 10.0f);
    TL_CHECK_EQ(a.at(1.0f), 0.0f);
    TL_CHECK_EQ(a.at(3.0f), 0.0f);
    // A quarter of the way: half of the first segment, 5.625 where a straight line gives 5.
    TL_CHECK(std::fabs(a.at(0.25f) - 5.625f) < 1e-4f);
    // The position is `x / range`, not the distance from the first point: with
    // the points from 1 to 3 (range 2), x = 1 is half way along.
    DynamicAttribute late;
    late.kind = DynamicAttribute::Kind::Curved;
    late.spline = true;
    late.points = {{1.0f, 0.0f}, {2.0f, 10.0f}, {3.0f, 0.0f}};
    TL_CHECK_EQ(late.at(1.0f), 10.0f);
    TL_CHECK_EQ(late.at(2.0f), 0.0f);
}

// The random-curved kinds (4, 5): `Math::RangeRandom` between a lower and an upper curve
// (`DynamicAttributeRandomCurved::getValue`).
TL_TEST(random_curved_attribute_draws_between_its_two_curves) {
    DynamicAttribute a;
    a.kind = DynamicAttribute::Kind::RandomCurved;
    a.points = {{0.0f, 10.0f}, {1.0f, 20.0f}};
    a.high_points = {{0.0f, 30.0f}, {1.0f, 60.0f}};
    TL_CHECK(a.draws());
    float low = 0.0f;
    float high = 0.0f;
    a.range(0.5f, low, high);
    TL_CHECK(std::fabs(low - 15.0f) < 1e-5f);
    TL_CHECK(std::fabs(high - 45.0f) < 1e-5f);
    TL_CHECK(std::fabs(a.at(0.5f, 0.0f) - 15.0f) < 1e-5f);
    TL_CHECK(std::fabs(a.at(0.5f, 1.0f) - 45.0f) < 1e-5f);
    TL_CHECK(std::fabs(a.at(0.5f, 0.5f) - 30.0f) < 1e-5f);
    DynamicAttribute fixed;
    fixed.value = 4.0f;
    TL_CHECK(!fixed.draws());
    TL_CHECK_EQ(fixed.at(0.5f), 4.0f);
}

TL_TEST(area_damage_off_without_a_radius) {
    for (float radius : {0.0f, -1.0f, -5.0f}) {
        Missile m;
        m.spec.aoe_radius = radius;
        m.spec.aoe_damage_scale = 0.5f;
        m.position = {0.0f, 0.0f, 0.0f};
        m.owner = 0;
        m.velocity = {10.0f, 0.0f, 0.0f};
        m.fired = true;

        int harm_calls = 0;
        MissileWorld world;
        world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                         Vec3& centre, Vec3& normal, int64_t& unit) {
            centre = {1.0f, 0.0f, 0.0f};
            normal = {-1.0f, 0.0f, 0.0f};
            unit = 0;  // Wall
            return true;
        };
        world.get_active_units_at_position = [](const Vec3&, float, std::vector<int64_t>*) {
            return std::vector<int64_t>{1, 2};
        };
        world.harm = [&](int64_t, const Vec3&, float, float) {
            ++harm_calls;
            return true;
        };

        update_missile(m, 0.2f, world);
        TL_CHECK_EQ(harm_calls, 0);
    }
}

TL_TEST(area_damage_hits_every_unit_in_reach_in_order) {
    Missile m;
    m.spec.aoe_radius = 5.0f;
    m.spec.aoe_damage_scale = 0.5f;
    m.spec.soak_scale = 0.75f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.owner = 0;
    m.velocity = {10.0f, 0.0f, 0.0f};
    m.fired = true;

    std::vector<ActiveUnitCandidate> candidates = {
        {10, {3.0f, 0.0f, 0.0f}},   // in reach
        {20, {15.0f, 0.0f, 0.0f}},  // out of reach
        {30, {0.0f, 4.0f, 0.0f}}    // in reach
    };

    struct HarmRecord {
        int64_t unit;
        float damage_scale;
        float soak_scale;
    };
    std::vector<HarmRecord> harmed;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {0.0f, 0.0f, 0.0f};
        normal = {-1.0f, 0.0f, 0.0f};
        unit = 0;  // Wall
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                             std::vector<int64_t>* items_out) {
        return get_active_units_at_position(candidates, pos, r, items_out);
    };
    world.unit = [&](int64_t id, Vec3& c, float& r, bool& alive) {
        for (const auto& u : candidates) {
            if (u.id == id) {
                c = u.center;
                r = 1.0f;
                alive = true;
                return true;
            }
        }
        return false;
    };
    world.harm = [&](int64_t unit, const Vec3&, float damage_scale, float soak_scale) {
        harmed.push_back({unit, damage_scale, soak_scale});
        return true;
    };

    update_missile(m, 0.1f, world);

    TL_CHECK_EQ(harmed.size(), 2u);
    TL_CHECK_EQ(harmed[0].unit, 10);
    TL_CHECK(std::abs(harmed[0].damage_scale - 0.5f) < 1e-4f);
    TL_CHECK(std::abs(harmed[0].soak_scale - 0.75f) < 1e-4f);
    TL_CHECK_EQ(harmed[1].unit, 30);
    TL_CHECK(std::abs(harmed[1].damage_scale - 0.5f) < 1e-4f);
    TL_CHECK(std::abs(harmed[1].soak_scale - 0.75f) < 1e-4f);
}

TL_TEST(area_damage_boundary) {
    const Vec3 pos{0.0f, 0.0f, 0.0f};
    const float radius = 5.0f;
    std::vector<ActiveUnitCandidate> candidates = {
        {1, {5.0f, 0.0f, 0.0f}},    // exactly at distance 5.0 (<= inclusive: hit)
        {2, {5.001f, 0.0f, 0.0f}},  // just beyond distance 5.0 (miss)
        {3, {0.0f, 3.0f, 4.0f}}     // sqrt(9 + 16) = 5.0 exactly (hit)
    };

    std::vector<int64_t> result = get_active_units_at_position(candidates, pos, radius);
    TL_CHECK_EQ(result.size(), 2u);
    TL_CHECK_EQ(result[0], 1);
    TL_CHECK_EQ(result[1], 3);
}

TL_TEST(area_damage_skips_owner_and_struck_unit) {
    // Case 1: Player as owner (id 0).
    {
        Missile m;
        m.spec.aoe_radius = 5.0f;
        m.spec.aoe_damage_scale = 0.5f;
        m.spec.soak_scale = 1.0f;
        m.position = {0.0f, 0.0f, 0.0f};
        m.owner = 0;  // player
        m.velocity = {10.0f, 0.0f, 0.0f};
        m.fired = true;

        std::vector<ActiveUnitCandidate> candidates = {
            {0, {1.0f, 0.0f, 0.0f}},  // player / owner
            {1, {2.0f, 0.0f, 0.0f}},  // unit A (struck)
            {2, {3.0f, 0.0f, 0.0f}}   // unit B (in reach)
        };

        std::vector<std::pair<int64_t, float>> blows;

        MissileWorld world;
        world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                         Vec3& centre, Vec3& normal, int64_t& unit) {
            centre = {2.0f, 0.0f, 0.0f};
            normal = {-1.0f, 0.0f, 0.0f};
            unit = 1;  // struck unit A
            return true;
        };
        world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                                 std::vector<int64_t>* items_out) {
            return get_active_units_at_position(candidates, pos, r, items_out);
        };
        world.unit = [&](int64_t id, Vec3& c, float& r, bool& alive) {
            for (const auto& u : candidates) {
                if (u.id == id) {
                    c = u.center;
                    r = 1.0f;
                    alive = true;
                    return true;
                }
            }
            return false;
        };
        world.harm = [&](int64_t unit, const Vec3&, float damage_scale, float) {
            blows.push_back({unit, damage_scale});
            return true;
        };

        update_missile(m, 0.1f, world);

        // A is struck directly with 1.0f.
        // Area damage hits B with 0.5f.
        // A is excepted (not struck twice).
        // Owner 0 (the player) is skipped!
        TL_CHECK_EQ(blows.size(), 2u);
        TL_CHECK_EQ(blows[0].first, 1);
        TL_CHECK(std::abs(blows[0].second - 1.0f) < 1e-4f);
        TL_CHECK_EQ(blows[1].first, 2);
        TL_CHECK(std::abs(blows[1].second - 0.5f) < 1e-4f);
    }

    // Case 2: Monster as owner (id 10). Player (id 0) is in reach!
    {
        Missile m;
        m.spec.aoe_radius = 5.0f;
        m.spec.aoe_damage_scale = 0.5f;
        m.spec.soak_scale = 1.0f;
        m.position = {0.0f, 0.0f, 0.0f};
        m.owner = 10;  // monster
        m.velocity = {10.0f, 0.0f, 0.0f};
        m.fired = true;

        std::vector<ActiveUnitCandidate> candidates = {
            {0, {1.0f, 0.0f, 0.0f}},  // player (in reach)
            {1, {2.0f, 0.0f, 0.0f}},  // struck unit
            {10, {3.0f, 0.0f, 0.0f}}  // monster owner (in reach)
        };

        std::vector<std::pair<int64_t, float>> blows;

        MissileWorld world;
        world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                         Vec3& centre, Vec3& normal, int64_t& unit) {
            centre = {2.0f, 0.0f, 0.0f};
            normal = {-1.0f, 0.0f, 0.0f};
            unit = 1;
            return true;
        };
        world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                                 std::vector<int64_t>* items_out) {
            return get_active_units_at_position(candidates, pos, r, items_out);
        };
        world.unit = [&](int64_t id, Vec3& c, float& r, bool& alive) {
            for (const auto& u : candidates) {
                if (u.id == id) {
                    c = u.center;
                    r = 1.0f;
                    alive = true;
                    return true;
                }
            }
            return false;
        };
        world.harm = [&](int64_t unit, const Vec3&, float damage_scale, float) {
            blows.push_back({unit, damage_scale});
            return true;
        };

        update_missile(m, 0.1f, world);

        // Struck unit 1 hit directly (1.0f).
        // Player (id 0) IS harmed by monster's blast (0.5f)!
        // Monster owner (id 10) is skipped.
        TL_CHECK_EQ(blows.size(), 2u);
        TL_CHECK_EQ(blows[0].first, 1);
        TL_CHECK(std::abs(blows[0].second - 1.0f) < 1e-4f);
        TL_CHECK_EQ(blows[1].first, 0);
        TL_CHECK(std::abs(blows[1].second - 0.5f) < 1e-4f);
    }
}

TL_TEST(area_damage_push_points_from_missile_to_unit) {
    Missile m;
    m.spec.aoe_radius = 10.0f;
    m.spec.aoe_damage_scale = 1.0f;
    m.position = {1.0f, 2.0f, 3.0f};
    m.owner = -2;
    m.velocity = {0.0f, 0.0f, 0.0f};
    m.fired = true;

    // Unit 1 at (4, 6, 3): dx=3, dy=4, dz=0, len=5. push should be (0.6, 0.8, 0.0)
    // Unit 2 at (1, 2, 3): dx=0, dy=0, dz=0, len=0 <= 1e-08. push should be (0, 0, 0)
    std::vector<ActiveUnitCandidate> candidates = {{1, {4.0f, 6.0f, 3.0f}},
                                                   {2, {1.0f, 2.0f, 3.0f}}};

    std::vector<Vec3> pushes;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {1.0f, 2.0f, 3.0f};
        normal = {0.0f, 1.0f, 0.0f};
        unit = 0;
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                             std::vector<int64_t>* items_out) {
        return get_active_units_at_position(candidates, pos, r, items_out);
    };
    world.unit = [&](int64_t id, Vec3& c, float& r, bool& alive) {
        for (const auto& u : candidates) {
            if (u.id == id) {
                c = u.center;
                r = 1.0f;
                alive = true;
                return true;
            }
        }
        return false;
    };
    world.harm = [&](int64_t, const Vec3& push, float, float) {
        pushes.push_back(push);
        return true;
    };

    update_missile(m, 0.1f, world);

    TL_CHECK_EQ(pushes.size(), 2u);
    TL_CHECK(std::abs(pushes[0].x - 0.6f) < 1e-4f);
    TL_CHECK(std::abs(pushes[0].y - 0.8f) < 1e-4f);
    TL_CHECK(std::abs(pushes[0].z - 0.0f) < 1e-4f);

    TL_CHECK_EQ(pushes[1].x, 0.0f);
    TL_CHECK_EQ(pushes[1].y, 0.0f);
    TL_CHECK_EQ(pushes[1].z, 0.0f);
}

TL_TEST(area_damage_on_a_wall_hit) {
    Missile m;
    m.spec.aoe_radius = 5.0f;
    m.spec.aoe_damage_scale = 1.0f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.velocity = {10.0f, 0.0f, 0.0f};
    m.fired = true;

    int aoe_calls = 0;
    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {5.0f, 0.0f, 0.0f};
        normal = {-1.0f, 0.0f, 0.0f};
        unit = 0;  // face / wall
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3&, float, std::vector<int64_t>*) {
        ++aoe_calls;
        return std::vector<int64_t>{};
    };

    MissileStep step1 = update_missile(m, 1.0f, world);
    TL_CHECK(step1.killed);
    TL_CHECK_EQ(aoe_calls, 1);

    // Next frame: missile is already dying, must NOT trigger area damage again!
    (void)update_missile(m, 0.1f, world);
    TL_CHECK_EQ(aoe_calls, 1);
}

TL_TEST(area_damage_at_max_distance) {
    Missile m;
    m.spec.max_distance = 10.0f;
    m.spec.aoe_radius = 5.0f;
    m.spec.aoe_damage_scale = 1.0f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.previous = {0.0f, 0.0f, 0.0f};
    m.travelled = 10.0f;  // reached max distance
    m.fired = true;

    int aoe_calls = 0;
    MissileWorld world;
    world.get_active_units_at_position = [&](const Vec3&, float, std::vector<int64_t>*) {
        ++aoe_calls;
        return std::vector<int64_t>{};
    };

    MissileStep step1 = update_missile(m, 0.1f, world);
    TL_CHECK(step1.killed);
    TL_CHECK_EQ(aoe_calls, 1);

    // Next frame: dying missile does not trigger area damage again
    (void)update_missile(m, 0.1f, world);
    TL_CHECK_EQ(aoe_calls, 1);
}

TL_TEST(piercing_missile_and_area_damage) {
    Missile m;
    m.spec.piercing = true;
    m.spec.aoe_radius = 5.0f;
    m.spec.aoe_damage_scale = 0.5f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.velocity = {10.0f, 0.0f, 0.0f};
    m.fired = true;

    std::vector<ActiveUnitCandidate> candidates = {
        {1, {2.0f, 0.0f, 0.0f}},  // struck unit
        {2, {3.0f, 0.0f, 0.0f}}   // area unit
    };

    std::vector<std::pair<int64_t, float>> blows;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>& ignore,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        if (std::find(ignore.begin(), ignore.end(), 1) == ignore.end()) {
            centre = {2.0f, 0.0f, 0.0f};
            normal = {-1.0f, 0.0f, 0.0f};
            unit = 1;
            return true;
        }
        return false;
    };
    world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                             std::vector<int64_t>* items_out) {
        return get_active_units_at_position(candidates, pos, r, items_out);
    };
    world.unit = [&](int64_t id, Vec3& c, float& r, bool& alive) {
        for (const auto& u : candidates) {
            if (u.id == id) {
                c = u.center;
                r = 1.0f;
                alive = true;
                return true;
            }
        }
        return false;
    };
    world.harm = [&](int64_t unit, const Vec3&, float damage_scale, float) {
        blows.push_back({unit, damage_scale});
        return true;
    };

    MissileStep step = update_missile(m, 1.0f, world);

    // Struck unit 1 hit directly (1.0f)
    // Area hit on unit 2 (0.5f), unit 1 excepted
    // Missile is piercing: NOT killed, kept flying
    TL_CHECK(!step.killed);
    TL_CHECK_EQ(blows.size(), 2u);
    TL_CHECK_EQ(blows[0].first, 1);
    TL_CHECK(std::abs(blows[0].second - 1.0f) < 1e-4f);
    TL_CHECK_EQ(blows[1].first, 2);
    TL_CHECK(std::abs(blows[1].second - 0.5f) < 1e-4f);
    TL_CHECK_EQ(m.struck.size(), 1u);
    TL_CHECK_EQ(m.struck[0], 1);
}

TL_TEST(ricochet_reflects_off_a_wall) {
    Missile m;
    m.spec.ricochets = 1;
    m.velocity = {-10.0f, -5.0f, 0.0f};
    m.position = {1.0f, 10.0f, 0.0f};
    m.previous = {1.0f, 10.0f, 0.0f};
    m.forward = {-1.0f, 0.0f, 0.0f};
    m.lift = 0.0f;
    m.spec.radius = 0.1f;
    m.spec.keep = 1.0f;
    m.fired = true;

    MissileWorld world;
    world.sweep = [](const Vec3& from, const Vec3& to, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        if (to.x < 0.0f) {
            float t = from.x / (from.x - to.x);
            centre = {0.0f, from.y + (to.y - from.y) * t, from.z + (to.z - from.z) * t};
            normal = {1.0f, 0.0f, 0.0f};  // +x wall normal
            unit = 0;
            return true;
        }
        return false;
    };

    MissileStep step = update_missile(m, 1.0f, world);

    TL_CHECK_EQ(m.ricocheted, 1u);
    TL_CHECK(step.ricochet);
    TL_CHECK(!step.killed);

    // normal.y == 0 zeroes y, and scales XZ to original speed
    float original_speed = std::sqrt(100.0f + 25.0f);
    TL_CHECK_EQ(m.velocity.y, 0.0f);
    TL_CHECK(std::abs(m.velocity.x - original_speed) < 1e-3f);
    TL_CHECK(std::abs(m.velocity.z - 0.0f) < 1e-3f);
}

TL_TEST(ricochet_count_runs_out) {
    // NUM RICOCHETS != 0 && done <= NUM RICOCHETS: N authored allows N + 1 bounces. Kept.
    Missile m;
    m.spec.ricochets = 1;  // 1 authored allows 2 bounces (when done=0 and done=1)
    m.spec.aoe_radius = 5.0f;
    m.spec.keep = 1.0f;
    m.position = {1.0f, 0.0f, 0.0f};
    m.previous = {1.0f, 0.0f, 0.0f};
    m.velocity = {-10.0f, 0.0f, 0.0f};
    m.forward = {-1.0f, 0.0f, 0.0f};
    m.fired = true;

    int aoe_calls = 0;
    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {0.0f, 0.0f, 0.0f};
        normal = {1.0f, 0.0f, 0.0f};
        unit = 0;
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3&, float, std::vector<int64_t>*) {
        ++aoe_calls;
        return std::vector<int64_t>{};
    };

    // Bounce 1: done=0 <= 1 -> ricochets (done becomes 1)
    MissileStep s1 = update_missile(m, 0.1f, world);
    TL_CHECK(!s1.killed);
    TL_CHECK(s1.ricochet);
    TL_CHECK_EQ(m.ricocheted, 1u);
    TL_CHECK_EQ(aoe_calls, 0);

    // Set heading towards wall again
    m.velocity = {-10.0f, 0.0f, 0.0f};
    m.previous = m.position;

    // Bounce 2: done=1 <= 1 -> ricochets (done becomes 2, the N+1 bounce), and
    // the re-test 2 <= 1 fails: the same hit kills and deals the area damage.
    MissileStep s2 = update_missile(m, 0.1f, world);
    TL_CHECK(s2.ricochet);
    TL_CHECK(s2.killed);
    TL_CHECK_EQ(m.ricocheted, 2u);
    TL_CHECK_EQ(aoe_calls, 1);

    // Dying: no third bounce and no second blast.
    m.velocity = {-10.0f, 0.0f, 0.0f};
    m.previous = m.position;
    MissileStep s3 = update_missile(m, 0.1f, world);
    TL_CHECK(!s3.ricochet);
    TL_CHECK_EQ(m.ricocheted, 2u);
    TL_CHECK_EQ(aoe_calls, 1);
}

TL_TEST(struck_list_is_only_kept_by_a_piercing_missile) {
    Missile m;
    m.spec.keep = 1.0f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.velocity = {10.0f, 0.0f, 0.0f};
    m.forward = {1.0f, 0.0f, 0.0f};
    m.owner = 7;
    m.fired = true;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {1.0f, 0.0f, 0.0f};
        normal = {-1.0f, 0.0f, 0.0f};
        unit = 3;
        return true;
    };
    world.unit = [](int64_t, Vec3& c, float& r, bool& alive) {
        c = {1.0f, 0.0f, 0.0f};
        r = 0.5f;
        alive = true;
        return true;
    };
    world.harm = [](int64_t, const Vec3&, float, float) { return true; };

    MissileStep step = update_missile(m, 0.1f, world);
    TL_CHECK(step.killed);
    TL_CHECK(m.struck.empty());
}

TL_TEST(ricochet_clears_the_struck_list) {
    Missile m;
    m.spec.ricochets = 1;
    m.spec.keep = 1.0f;
    m.position = {1.0f, 0.0f, 0.0f};
    m.velocity = {-10.0f, 0.0f, 0.0f};
    m.forward = {-1.0f, 0.0f, 0.0f};
    m.fired = true;
    m.struck = {1, 2, 3};
    m.travelled = 15.0f;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {0.0f, 0.0f, 0.0f};
        normal = {1.0f, 0.0f, 0.0f};
        unit = 0;
        return true;
    };

    MissileStep step = update_missile(m, 0.1f, world);
    TL_CHECK(step.ricochet);
    TL_CHECK(m.struck.empty());
    TL_CHECK_EQ(m.travelled, 0.0f);
}

TL_TEST(point_blank_wall_hit) {
    Missile m;
    m.spec.aoe_radius = 5.0f;
    m.owner = 1;
    m.owner_center = {0.0f, 0.0f, 0.0f};
    m.owner_radius = 1.0f;

    int aoe_calls = 0;
    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {0.5f, 0.0f, 0.0f};
        normal = {-1.0f, 0.0f, 0.0f};
        unit = 0;  // Wall
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3&, float, std::vector<int64_t>*) {
        ++aoe_calls;
        return std::vector<int64_t>{};
    };

    // Launch from (1, 0, 0), distance from owner is 1.0 (< 4.0).
    // Hits wall at point blank. Must deal area damage EXACTLY ONCE.
    fire_missile(m, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, world);

    TL_CHECK_EQ(aoe_calls, 1);
}

// -- The arch (`ARCH HEIGHT`): `calculateLaunchOrientation` lays the path,
// `updatePositionByVelocity` carries the missile along it.

namespace {

Missile arched_missile(float arch_height) {
    Missile m;
    m.spec.arch_height = arch_height;
    m.spec.max_distance = 100.0f;
    m.position = {0.0f, 1.0f, 0.0f};
    m.previous = m.position;
    m.fired = true;
    return m;
}

void launch(Missile& m, const Vec3& to, float speed, const MissileWorld& world) {
    missile_launch_orientation(m, m.position, to, world);
    m.velocity = m.forward * speed;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

bool near(const Vec3& a, const Vec3& b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

Route route_of(const Vec3& a, const Vec3& b, const Vec3& c) {
    Route r;
    r.add_point(a);
    r.add_point(b);
    r.add_point(c);
    return r;
}

}  // namespace

TL_TEST(no_arch_without_height) {
    for (float height : {0.0f, -2.0f, std::nanf("")}) {
        Missile m = arched_missile(height);
        MissileWorld world;
        launch(m, {6.0f, 1.0f, 8.0f}, 5.0f, world);
        TL_CHECK(!m.arch_built);
        const Vec3 start = m.position;
        const Vec3 v = m.velocity;
        (void)update_missile(m, 0.1f, world);
        TL_CHECK(
            near(m.position, {start.x + 0.1f * v.x, start.y + 0.1f * v.y, start.z + 0.1f * v.z}));
    }
}

TL_TEST(arch_path_points) {
    Missile m = arched_missile(3.0f);
    Vec3 ray_from, ray_to;
    MissileWorld world;
    world.ray = [&](const Vec3& from, const Vec3& to, Vec3& point) {
        ray_from = from;
        ray_to = to;
        point = {10.0f, -0.5f, 4.0f};
        return true;
    };
    launch(m, {10.0f, 2.0f, 4.0f}, 5.0f, world);
    TL_CHECK(m.arch_built);
    TL_CHECK(near(ray_from, {10.0f, 102.0f, 4.0f}));
    TL_CHECK(near(ray_to, {10.0f, -98.0f, 4.0f}));
    TL_CHECK(near(m.target_point, {10.0f, 2.0f, 4.0f}));  // the ray's hit is not kept here
    TL_CHECK_EQ(m.arch.size(), static_cast<size_t>(3));
    TL_CHECK(near(m.arch.point(0), {0.0f, 1.0f, 0.0f}));
    TL_CHECK(near(m.arch.point(1), {5.0f, 3.25f, 2.0f}));  // (1 + -0.5) / 2 + 3
    TL_CHECK(near(m.arch.point(2), {10.0f, -0.5f, 4.0f}));
    const Vec3 a = m.arch.point(1) - m.arch.point(0);
    const Vec3 b = m.arch.point(2) - m.arch.point(1);
    const float expected =
        std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z) + std::sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
    TL_CHECK(near(m.arch.length(), expected));
    TL_CHECK_EQ(m.arch_fraction, 0.0f);
}

TL_TEST(arch_path_without_a_ray_hit) {
    for (int with_hook = 0; with_hook < 2; ++with_hook) {
        Missile m = arched_missile(1.0f);
        MissileWorld world;
        if (with_hook) world.ray = [](const Vec3&, const Vec3&, Vec3&) { return false; };
        launch(m, {4.0f, 3.0f, -2.0f}, 5.0f, world);
        TL_CHECK(near(m.arch.point(1), {2.0f, 3.0f, -1.0f}));
        TL_CHECK(near(m.arch.point(2), {4.0f, 3.0f, -2.0f}));
    }
}

TL_TEST(arch_step_follows_the_spline) {
    Missile m = arched_missile(2.0f);
    MissileWorld world;
    launch(m, {6.0f, 1.0f, 8.0f}, 4.0f, world);
    const Route expected = route_of({0.0f, 1.0f, 0.0f}, {3.0f, 3.0f, 4.0f}, {6.0f, 1.0f, 8.0f});
    const float speed = std::sqrt(m.velocity.x * m.velocity.x + m.velocity.y * m.velocity.y +
                                  m.velocity.z * m.velocity.z);
    (void)update_missile(m, 0.5f, world);
    TL_CHECK(near(m.arch_travelled, speed * 0.5f));
    const Vec3 at = expected.spline_position_at_distance(speed * 0.5f);
    TL_CHECK(near(m.position, at));
    TL_CHECK(!near(m.position.z, 0.0f));
    (void)update_missile(m, 0.5f, world);
    TL_CHECK(near(m.position, expected.spline_position_at_distance(speed)));
}

TL_TEST(arch_step_clamps_and_ends) {
    Missile m = arched_missile(2.0f);
    MissileWorld world;
    launch(m, {6.0f, 1.0f, 8.0f}, 50.0f, world);
    const MissileStep first = update_missile(m, 1.0f, world);
    TL_CHECK(!first.killed);
    TL_CHECK_EQ(m.arch_travelled, m.arch.length());
    TL_CHECK(near(m.position, {6.0f, 1.0f, 8.0f}));
    TL_CHECK_EQ(m.travelled, m.spec.max_distance);
    TL_CHECK_EQ(m.travelled_total, m.target_distance);
    const MissileStep second = update_missile(m, 0.1f, world);
    TL_CHECK(second.killed);
}

TL_TEST(arch_tangent) {
    Missile m = arched_missile(2.0f);
    MissileWorld world;
    launch(m, {6.0f, 1.0f, 8.0f}, 4.0f, world);
    (void)update_missile(m, 0.5f, world);
    const float d = m.arch_travelled;
    const Vec3 ahead = m.arch.spline_position_at_distance(d + 0.01f);
    const Vec3 behind = m.arch.spline_position_at_distance(d + 0.01f - 0.02f);
    Vec3 t = ahead - behind;
    const float len = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
    t = t * (1.0f / len);
    TL_CHECK(near(m.node_facing, t));
    // Past the midpoint's rise the tangent climbs; it is not the missile's heading.
    TL_CHECK(0.0f < m.node_facing.y);
}

TL_TEST(arch_draws_the_same_rolls) {
    RunicRandom straight_rolls(1234);
    RunicRandom arched_rolls(1234);
    for (int arched = 0; arched < 2; ++arched) {
        Missile m = arched_missile(arched ? 2.0f : 0.0f);
        m.spec.turn_randomization = 30.0f;
        m.spec.max_turn_rate = 90.0f;
        m.spec.randomization_rate = 0.05f;
        MissileWorld world;
        world.volatile_rolls = arched ? &arched_rolls : &straight_rolls;
        launch(m, {6.0f, 1.0f, 8.0f}, 4.0f, world);
        for (int i = 0; i < 6; ++i) (void)update_missile(m, 0.1f, world);
    }
    TL_CHECK_EQ(straight_rolls.between(0.0f, 1000.0f), arched_rolls.between(0.0f, 1000.0f));
}

TL_TEST(refire_resets_arch_distance) {
    Missile m = arched_missile(2.0f);
    MissileWorld world;
    launch(m, {6.0f, 1.0f, 8.0f}, 4.0f, world);
    (void)update_missile(m, 0.5f, world);
    TL_CHECK(0.0f < m.arch_travelled);
    (void)fire_missile(m, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, world);
    TL_CHECK_EQ(m.arch_travelled, 0.0f);
    // +0x210 is never released: the path laid by the last launch stays.
    TL_CHECK(m.arch_built);
}

/// `doAOEDamage`'s items loop: every item the blast reaches is
/// interacted with by nobody - a breakable breaks - but for the owner.
TL_TEST(area_damage_interacts_with_the_items_it_reaches) {
    Missile m;
    m.spec.aoe_radius = 5.0f;
    m.spec.aoe_damage_scale = 1.0f;
    m.position = {0.0f, 0.0f, 0.0f};
    m.owner = 7;
    m.velocity = {0.0f, 0.0f, 0.0f};
    m.fired = true;

    const std::vector<ActiveUnitCandidate> characters;
    const std::vector<ActiveUnitCandidate> items = {
        {5, {3.0f, 0.0f, 0.0f}},  // in the blast
        {6, {9.0f, 0.0f, 0.0f}},  // out of it
        {7, {1.0f, 0.0f, 0.0f}},  // the owner
    };
    std::vector<int64_t> touched;

    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {0.0f, 0.0f, 0.0f};
        normal = {0.0f, 1.0f, 0.0f};
        unit = 0;
        return true;
    };
    world.get_active_units_at_position = [&](const Vec3& pos, float r,
                                             std::vector<int64_t>* items_out) {
        return get_active_units_at_position(characters, pos, r, items, items_out);
    };
    world.interact_item = [&](int64_t item) {
        touched.push_back(item);
        return true;
    };

    update_missile(m, 0.1f, world);

    TL_CHECK_EQ(touched.size(), 1u);
    TL_CHECK_EQ(touched[0], 5);
}

/// `handleMissileHitUnit`: an item that `interact`s ends the missile; one that
/// does not is flown through.
TL_TEST(a_missile_breaks_the_item_it_meets) {
    for (int scenario = 0; scenario < 3; ++scenario) {
        Missile m;
        m.position = {0.0f, 0.0f, 0.0f};
        m.velocity = {0.0f, 0.0f, 10.0f};
        m.forward = {0.0f, 0.0f, 1.0f};
        m.spec.max_distance = 100.0f;
        m.fired = true;
        int asked = 0;

        MissileWorld world;
        world.sweep = [](const Vec3& from, const Vec3&, float, bool, const std::vector<int64_t>&,
                         Vec3& centre, Vec3& normal, int64_t& unit) {
            centre = from;
            normal = {0.0f, 0.0f, -1.0f};
            unit = 9;
            return true;
        };
        world.unit_kind = [scenario](int64_t) {
            return scenario == 2 ? MissileUnitKind::Interactable : MissileUnitKind::Item;
        };
        world.interact_item = [&asked, scenario](int64_t item) {
            TL_CHECK_EQ(item, 9);
            ++asked;
            return scenario == 0;
        };
        world.harm = [](int64_t, const Vec3&, float, float) {
            TL_CHECK(false);  // an item is never dealt a character's blow
            return false;
        };

        const MissileStep step = update_missile(m, 0.1f, world);
        if (scenario == 0) {
            TL_CHECK_EQ(asked, 1);
            TL_CHECK(m.dying || step.finished || step.killed);
        } else if (scenario == 1) {
            TL_CHECK_EQ(asked, 1);
            TL_CHECK(!m.dying);
        } else {
            TL_CHECK_EQ(asked, 0);
            TL_CHECK(m.dying || step.finished || step.killed);
        }
    }
}

// `CMissile::handleMissileHitUnit`: a reflected missile turns round and
// carries its share of the damage.
TL_TEST(a_reflected_missile_turns_round_and_carries_the_share) {
    Missile m;
    m.position = {0.0f, 0.0f, 0.0f};
    m.owner = 0;
    m.target = 7;
    m.velocity = {10.0f, 0.0f, 0.0f};
    m.forward = {1.0f, 0.0f, 0.0f};
    m.travelled = 3.0f;
    m.fired = true;

    int harm_calls = 0;
    float scale_seen = 0.0f;
    MissileWorld world;
    world.sweep = [](const Vec3&, const Vec3&, float, bool, const std::vector<int64_t>&,
                     Vec3& centre, Vec3& normal, int64_t& unit) {
        centre = {1.0f, 0.0f, 0.0f};
        normal = {-1.0f, 0.0f, 0.0f};
        unit = 7;
        return true;
    };
    world.harm = [&](int64_t, const Vec3&, float damage_scale, float) {
        ++harm_calls;
        scale_seen = damage_scale;
        return true;
    };
    float share = 0.4f;
    world.reflect = [&](int64_t unit) { return unit == 7 ? share : 0.0f; };
    update_missile(m, 0.05f, world);
    TL_CHECK_EQ(harm_calls, 0);
    TL_CHECK(m.reflected);
    TL_CHECK(m.damage_scale == 0.4f);
    TL_CHECK(m.velocity.x == -10.0f);
    TL_CHECK(m.forward.x == -1.0f);
    TL_CHECK_EQ(m.target, 0);

    // A second unit that does not reflect takes it at the share.
    share = 0.0f;
    update_missile(m, 0.05f, world);
    TL_CHECK_EQ(harm_calls, 1);
    TL_CHECK(scale_seen == 0.4f);
}

TL_TEST(a_missile_copied_from_its_template_lifts_by_the_templates_height) {
    // `CMissile::CMissile(CResourceManager*, const CMissile*)`:
    // the template's position y becomes the lift.
    MissileSpec spec;
    spec.name = "ARROW";
    spec.height = 1.25f;
    spec.max_distance = 40.0f;
    const Missile m = missile_from_template(spec);
    TL_CHECK_EQ(m.lift, 1.25f);
    TL_CHECK_EQ(m.lifted, 0.0f);
    TL_CHECK_EQ(m.turn_rate, 0.0f);
    TL_CHECK_EQ(m.spec.max_distance, 40.0f);
    TL_CHECK(m.spec.name == "ARROW");
    TL_CHECK(!m.hits_owner);
}

TL_TEST(an_unauthored_shape_keeps_the_constructors_values) {
    // `CShape::CShape`: a fixed angle of
    // release of 360, max radius 2, min radius and offset 0, a 10 x 1 x 10 box.
    const SpawnerShape s;
    TL_CHECK(s.angle.kind == DynamicAttribute::Kind::Fixed);
    TL_CHECK_EQ(s.angle.value, 360.0f);
    TL_CHECK_EQ(s.max_radius.value, 2.0f);
    TL_CHECK_EQ(s.min_radius.value, 0.0f);
    TL_CHECK_EQ(s.angle_offset.value, 0.0f);
    TL_CHECK_EQ(s.box_size.y, 1.0f);
    TL_CHECK(!s.use_max_only);
}
