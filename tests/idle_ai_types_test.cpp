// `CMonster::idleAI` by `AITYPE`: `idleAINormal`, `idleAIRangedDefender`,
// `idleAIRangedCaster`, `idleAICircler`, none for DUMMY;
// and `getRangeAI`.
#include "core/collision.h"
#include "core/navigation.h"
#include "core/passability.h"
#include "core/quest.h"
#include "core/script.h"
#include "core/skills.h"
#include "core/sound_bank.h"
#include "core/stats.h"
#include "test_seeds.h"
#include "test_support.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace tl::core;

StatGraph health_curve() {
    tl::formats::AdmSection line;
    line.name = "LINE";
    tl::formats::AdmRecord id;
    id.key = "NAME";
    id.type = tl::formats::AdmType::String;
    id.text = "Health_Minion_byLevel";
    line.records.push_back(id);
    for (int x = 1; x <= 2; ++x) {
        tl::formats::AdmSection point;
        point.name = "POINT";
        tl::formats::AdmRecord px;
        px.key = "X";
        px.type = tl::formats::AdmType::Float;
        px.number = static_cast<float>(x);
        tl::formats::AdmRecord py;
        py.key = "Y";
        py.type = tl::formats::AdmType::Float;
        py.number = 100.0f;
        point.records.push_back(px);
        point.records.push_back(py);
        line.children.push_back(point);
    }
    return StatGraph::from(line);
}

UnitStats monster(int ai_type, float speed) {
    UnitStats stats;
    stats.radius = 0.4f;
    stats.movement.walking_speed = speed;
    stats.movement.running_speed = speed;
    stats.sight_radius = 14.0f;
    stats.motion_radius = 14.0f;
    stats.view_angle = 360.0f;
    stats.follow_radius = 100.0f;
    stats.alignment = UnitStats::Alignment::Hostile;
    stats.health_graph = "Health_Minion_byLevel";
    stats.ai_type = ai_type;
    // A model: `selectAttack` chooses nothing for a unit without one, and
    // `inAttackRange` is then false.
    stats.mesh = "media/models/monster/monster";
    return stats;
}

/// A flat level with one monster (-1) at the origin and the player where each
/// frame puts it.
struct Scene {
    CollisionWorld world;
    PassabilityMap grid;
    NavGrid nav;
    UnitBook units;
    StatBook graphs;
    LevelScript level;
    QuestState quests;
    std::vector<ScriptEvent> events;
    /// Every AI state the monster was seen in after a frame, and how often it
    /// roared (`UnitSample` with `ROAR_SOUND`'s slot) or took a target.
    std::set<int32_t> states;
    int roars = 0;
    int targets = 0;

    Scene(int ai_type, float speed) {
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {-80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, 80.0f});
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, -80.0f});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
        units.add("monster", monster(ai_type, speed));
        graphs.add("HEALTH_MINION_BYLEVEL", health_curve());
        level.set_units(&units);
        level.set_stat_graphs(&graphs);
        level.set_navigation(&nav);
        level.set_collision(&world);
        level.set_passability(&grid);
        level.set_unit_active_range(100.0f);
        level.set_game_playing(true);
        tl::test::seed_generators();
        LevelScript::reset_near_list_timer();
        level.build(tl::formats::CmpScene{}, {});
        level.add_spawned_unit(-1, "monster", Vec3{0.0f, 0.0f, 0.0f}, false);
    }

    void run(const Vec3& player, float seconds) {
        const int frames = static_cast<int>(seconds * 60.0f + 0.5f);
        for (int i = 0; i < frames; ++i) {
            events.clear();
            level.update(1.0f / 60.0f, player, quests, events);
            states.insert(level.unit_ai_state(-1));
            for (const ScriptEvent& e : events) {
                if (e.node_id != -1) continue;
                if (e.action == ScriptAction::UnitSample &&
                    e.input == static_cast<uint32_t>(SoundSlot::Roar)) {
                    ++roars;
                }
                if (e.action == ScriptAction::Hunt && e.on) ++targets;
            }
        }
    }
};

const Vec3 kBeside{1.0f, 0.0f, 0.0f};
const Vec3 kAcross{10.0f, 0.0f, 0.0f};

}  // namespace

/// In reach, every handler hunts and roars once.
TL_TEST(every_ai_type_roars_as_it_turns_on_a_unit_in_reach) {
    for (const int type : {0, 1, 2, 3, 4, 5}) {
        Scene at(type, 0.0f);
        at.run(kBeside, 3.0f);
        TL_CHECK_EQ(at.roars, 1);
        TL_CHECK(at.states.count(ai_state::kHunt) != 0 || at.states.count(ai_state::kAttack) != 0);
    }
}

/// DUMMY (6) is in none of the switch's cases: it never looks.
TL_TEST(a_dummy_never_takes_a_target) {
    Scene at(6, 0.0f);
    at.run(kBeside, 3.0f);
    TL_CHECK_EQ(at.targets, 0);
    TL_CHECK_EQ(at.roars, 0);
    TL_CHECK(at.states.count(ai_state::kHunt) == 0);
}

/// Out of reach with no skill, the ranged types take the target and only draw for the flee
/// (10 in 101 a think): no hunt, no roar.
TL_TEST(a_ranged_type_out_of_reach_draws_for_the_flee_and_does_not_hunt) {
    for (const int type : {1, 3}) {
        Scene at(type, 0.0f);
        at.run(kAcross, 20.0f);
        TL_CHECK(at.targets != 0);
        TL_CHECK(at.states.count(ai_state::kFlee) != 0);
        TL_CHECK(at.states.count(ai_state::kHunt) == 0);
        TL_CHECK_EQ(at.roars, 0);
    }
    Scene normal(0, 3.0f);
    normal.run(kAcross, 3.0f);
    TL_CHECK(normal.states.count(ai_state::kHunt) != 0);
    TL_CHECK(normal.states.count(ai_state::kFlee) == 0);
}

/// A circler out of reach hunts whatever its draw, and side-steps (20 in 101);
/// it never flees.
TL_TEST(a_circler_hunts_and_side_steps) {
    Scene at(4, 3.0f);
    at.run(kAcross, 3.0f);
    TL_CHECK(at.states.count(ai_state::kHunt) != 0 || at.states.count(0xb) != 0);
    TL_CHECK_EQ(at.roars, 0);
    at.run(kAcross, 30.0f);
    TL_CHECK(at.states.count(0xb) != 0);
    TL_CHECK(at.states.count(ai_state::kFlee) == 0);
}

/// The number over a unit: `applyDamage` adds (not a glancing blow),
/// `update` shows, a killing blow at once (`modifyHP`).
TL_TEST(a_units_damage_is_shown_over_it) {
    Scene at(6, 0.0f);
    at.run(kAcross, 0.1f);
    const auto numbers = [&]() {
        std::vector<float> seen;
        for (const ScriptEvent& e : at.events) {
            if (e.action == ScriptAction::DamageText && e.node_id == -1 && !e.on) {
                seen.push_back(e.amount);
            }
        }
        return seen;
    };
    at.events.clear();
    at.level.damage_unit(-1, 12.4f, 0, at.quests, at.events);
    TL_CHECK(numbers().empty());
    at.run(kAcross, 1.0f / 60.0f);
    TL_CHECK_EQ(numbers().size(), 1u);
    if (!numbers().empty()) TL_CHECK_EQ(numbers()[0], 13.0f);

    // Glancing: nothing to show a second later.
    KillingBlow glancing;
    glancing.glancing = true;
    at.events.clear();
    at.level.damage_unit(-1, 1.0f, 0, at.quests, at.events, nullptr, &glancing);
    bool any = false;
    for (int i = 0; i < 60; ++i) {
        at.run(kAcross, 1.0f / 60.0f);
        any = any || !numbers().empty();
    }
    TL_CHECK(!any);

    // The killing blow, well past the health: its number in the same call.
    at.events.clear();
    at.level.damage_unit(-1, 500.0f, 0, at.quests, at.events);
    TL_CHECK(at.level.unit_is_dead(-1));
    TL_CHECK_EQ(numbers().size(), 1u);
    if (!numbers().empty()) TL_CHECK_EQ(numbers()[0], 500.0f);
}

/// `CPropertyNode::killOrMoveBaseUnits`: a blocker takes the floor of the hit
/// points of what stands in it, so a fraction is left. Kept.
TL_TEST(a_blocker_switched_on_kills_what_stands_in_it) {
    const auto area_at = [](float x, bool circle, float scale) {
        LevelScript::OccupationArea area;
        area.centre = {x, 0.0f, 0.0f};
        area.min = {x - 2.0f, -1000.0f, -2.0f};
        area.max = {x + 2.0f, 1000.0f, 2.0f};
        area.circle = circle;
        area.scale_x = scale;
        return area;
    };
    {
        // Outside the box: untouched.
        Scene at(6, 0.0f);
        at.run(kAcross, 0.1f);
        std::vector<int64_t> killed;
        at.level.kill_or_move_base_units(area_at(5.0f, false, 4.0f), at.quests, at.events, killed);
        TL_CHECK(killed.empty());
        TL_CHECK(!at.level.unit_is_dead(-1));
        // Inside it: dead.
        at.level.kill_or_move_base_units(area_at(1.0f, false, 4.0f), at.quests, at.events, killed);
        TL_CHECK_EQ(killed.size(), 1u);
        TL_CHECK(at.level.unit_is_dead(-1));
    }
    {
        // A circle: in the box, but 1.5 from a node of scale 1 - spared.
        Scene at(6, 0.0f);
        at.run(kAcross, 0.1f);
        std::vector<int64_t> killed;
        at.level.kill_or_move_base_units(area_at(1.5f, true, 1.0f), at.quests, at.events, killed);
        TL_CHECK(killed.empty());
        at.level.kill_or_move_base_units(area_at(1.5f, true, 2.0f), at.quests, at.events, killed);
        TL_CHECK_EQ(killed.size(), 1u);
    }
    {
        // 99.5 of 100 left: `HP()` is 99, and the half point keeps it alive.
        Scene at(6, 0.0f);
        at.run(kAcross, 0.1f);
        at.level.damage_unit(-1, 0.5f, 0, at.quests, at.events);
        std::vector<int64_t> killed;
        at.level.kill_or_move_base_units(area_at(0.0f, false, 4.0f), at.quests, at.events, killed);
        TL_CHECK(killed.empty());
        float hp = 0.0f;
        float max = 0.0f;
        at.level.unit_health(-1, hp, max);
        TL_CHECK_EQ(hp, 0.5f);
    }
}

/// The player's pet is never killed by one: inside the box, or outside it and
/// no nearer its master than the node's scale X, it is put back by its master.
TL_TEST(a_blocker_switched_on_sends_the_players_pet_to_its_master) {
    const Vec3 master{20.0f, 0.0f, 0.0f};
    LevelScript::OccupationArea area;
    area.centre = {0.0f, 0.0f, 0.0f};
    area.min = {-2.0f, -1000.0f, -2.0f};
    area.max = {2.0f, 1000.0f, 2.0f};
    area.scale_x = 4.0f;
    {
        Scene at(6, 0.0f);
        at.run(master, 0.1f);
        TL_CHECK(at.level.make_player_pet(-1, at.quests, at.events));
        std::vector<int64_t> killed;
        at.level.kill_or_move_base_units(area, at.quests, at.events, killed);
        TL_CHECK(killed.empty());
        Vec3 now;
        TL_CHECK(at.level.unit_position(-1, now));
        TL_CHECK(std::fabs(now.x - master.x) < 9.5f);
    }
    {
        // Outside the box and 20 from its master, with a scale of 4: moved too.
        Scene at(6, 0.0f);
        at.run(master, 0.1f);
        TL_CHECK(at.level.make_player_pet(-1, at.quests, at.events));
        area.min.x = 50.0f;
        area.max.x = 54.0f;
        std::vector<int64_t> killed;
        at.level.kill_or_move_base_units(area, at.quests, at.events, killed);
        Vec3 now;
        TL_CHECK(at.level.unit_position(-1, now));
        TL_CHECK(std::fabs(now.x - master.x) < 9.5f);
        // ... and one that is nearer than the scale stays where it is.
        Scene near(6, 0.0f);
        near.run(Vec3{3.0f, 0.0f, 0.0f}, 0.1f);
        TL_CHECK(near.level.make_player_pet(-1, near.quests, near.events));
        near.level.kill_or_move_base_units(area, near.quests, near.events, killed);
        TL_CHECK(near.level.unit_position(-1, now));
        TL_CHECK(std::fabs(now.x) < 0.01f);
    }
}

/// A held skill stays where it is when the unit learns another (`CBaseUnit::activateEffect`):
/// a skill event holds a reference across its effects (else std::bad_alloc).
TL_TEST(a_units_skills_stay_where_they_are_when_it_learns_another) {
    Scene at(0, 0.0f);
    LevelScript::UnitSkill first;
    first.known.entry.name = "FIRST";
    TL_CHECK(at.level.add_unit_skill(-1, first));
    LevelScript::UnitCasting* casting = at.level.unit_casting(-1);
    TL_CHECK(casting != nullptr);
    TL_CHECK_EQ(casting->skills.size(), static_cast<size_t>(1));
    const KnownSkill* const held = &casting->skills.front().known;
    for (int i = 0; i < 200; ++i) {
        LevelScript::UnitSkill more;
        more.known.entry.name = "LEARNED" + std::to_string(i);
        TL_CHECK(at.level.add_unit_skill(-1, more));
    }
    TL_CHECK_EQ(casting->skills.size(), static_cast<size_t>(201));
    TL_CHECK(&casting->skills.front().known == held);
    TL_CHECK(held->entry.name == "FIRST");
}
