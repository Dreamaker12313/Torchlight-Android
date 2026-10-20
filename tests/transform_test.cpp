// A transformation: `CCharacter::transformCharacter` -> `convertCharacter`,
// run down by `CCharacter::update` to `revertTransformation`;
// `TRANSFORM PERMANENT`'s -999 never runs down.
#include "core/collision.h"
#include "core/navigation.h"
#include "core/passability.h"
#include "core/quest.h"
#include "core/script.h"
#include "core/stats.h"
#include "test_seeds.h"
#include "test_support.h"

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

UnitStats rooted(UnitStats::Alignment side, float radius) {
    UnitStats stats;
    stats.radius = radius;
    stats.movement.walking_speed = 0.0f;
    stats.movement.running_speed = 0.0f;
    stats.follow_radius = 100.0f;
    stats.alignment = side;
    stats.health_graph = "Health_Minion_byLevel";
    return stats;
}

struct Level {
    CollisionWorld world;
    PassabilityMap grid;
    NavGrid nav;
    UnitBook units;
    StatBook graphs;
    LevelScript level;
    QuestState quests;
    std::vector<ScriptEvent> events;
    std::vector<ScriptEvent> transforms;

    Level() {
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {-80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, 80.0f});
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, -80.0f});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
        units.add("Ratlin Foreman", rooted(UnitStats::Alignment::Hostile, 0.4f));
        // The unit it becomes is on the other side: the character keeps its own.
        units.add("RockTroll", rooted(UnitStats::Alignment::Friendly, 1.0f));
        units.add("Azure Gel", rooted(UnitStats::Alignment::Hostile, 0.6f));
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
        level.add_spawned_unit(-1, "Ratlin Foreman", Vec3{0.0f, 0.0f, 0.0f}, false);
    }

    void collect() {
        for (const ScriptEvent& e : events) {
            if (e.action == ScriptAction::Transform) transforms.push_back(e);
        }
        events.clear();
    }

    void run(float seconds) {
        const int frames = static_cast<int>(seconds * 10.0f + 0.5f);
        for (int i = 0; i < frames; ++i) {
            level.update(0.1f, Vec3{30.0f, 0.0f, 0.0f}, quests, events);
            collect();
        }
    }
};

}  // namespace

TL_TEST(a_transformed_character_is_the_new_unit_on_its_own_side_until_its_time_runs_out) {
    Level at;
    at.run(0.1f);
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", 120.0f, at.quests, at.events));
    at.collect();
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("RockTroll"));
    TL_CHECK_EQ(at.level.unit_transformed_from(-1), std::string("Ratlin Foreman"));
    TL_CHECK_EQ(at.transforms.size(), size_t{1});
    TL_CHECK_EQ(at.transforms[0].unit, std::string("RockTroll"));
    TL_CHECK_EQ(at.transforms[0].text, std::string("Ratlin Foreman"));
    // `setAlignment(alignment())` round `unitInit`: still the foreman's side.
    TL_CHECK(at.level.unit_side(-1) == UnitStats::Alignment::Hostile);
    // The new unit's own body: its radius.
    TL_CHECK_EQ(at.level.unit_radius(-1), 1.0f);

    at.run(119.0f);
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("RockTroll"));
    at.run(1.5f);
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("Ratlin Foreman"));
    TL_CHECK(at.level.unit_transformed_from(-1).empty());
    TL_CHECK_EQ(at.transforms.size(), size_t{2});
    TL_CHECK_EQ(at.transforms[1].unit, std::string("Ratlin Foreman"));
}

TL_TEST(a_permanent_transformation_never_runs_down) {
    Level at;
    at.run(0.1f);
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", -999.0f, at.quests, at.events));
    at.run(300.0f);
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("RockTroll"));
}

TL_TEST(a_unit_with_no_record_is_not_transformed) {
    Level at;
    at.run(0.1f);
    TL_CHECK(!at.level.transform_unit(-1, "Nobody", 120.0f, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("Ratlin Foreman"));
}

// The first unit (`+0x690`) is written only while -1, and a permanent transformation never
// clears it.
TL_TEST(a_timed_transformation_after_a_permanent_one_ends_as_the_first_unit) {
    Level at;
    at.run(0.1f);
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", -999.0f, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_transform_left(-1), -999.0f);
    TL_CHECK_EQ(at.level.unit_transformed_from(-1), std::string("Ratlin Foreman"));
    at.run(10.0f);
    TL_CHECK(at.level.transform_unit(-1, "Azure Gel", 30.0f, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_transformed_from(-1), std::string("Ratlin Foreman"));
    at.run(29.0f);
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("Azure Gel"));
    at.run(1.5f);
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("Ratlin Foreman"));
    TL_CHECK_EQ(at.level.unit_transform_left(-1), 0.0f);
}

// `CCharacter::applySaveState` puts `+0x688` and `+0x690` back on
// a character made again as the unit it had become: the time goes on running.
TL_TEST(a_transformation_put_back_runs_out_as_it_would_have) {
    Level at;
    at.level.remove_spawned_unit(-1);
    at.level.add_spawned_unit(-2, "RockTroll", Vec3{0.0f, 0.0f, 0.0f}, false);
    at.run(0.1f);
    TL_CHECK(at.level.restore_unit_transformation(-2, 5.0f, "Ratlin Foreman"));
    TL_CHECK(!at.level.restore_unit_transformation(-7, 5.0f, "Ratlin Foreman"));
    at.run(4.0f);
    TL_CHECK_EQ(at.level.unit_of(-2), std::string("RockTroll"));
    at.run(1.5f);
    TL_CHECK_EQ(at.level.unit_of(-2), std::string("Ratlin Foreman"));
    TL_CHECK(at.level.unit_transformed_from(-2).empty());
}

// `CLevel::removeCharacter(pet, true)` at a warp sets AI state 2 unless
// the pet is in town.
TL_TEST(a_pet_running_to_town_arrives_idle_after_a_warp) {
    Level at;
    at.run(0.1f);
    TL_CHECK(at.level.make_player_pet(-1, at.quests, at.events));
    TL_CHECK(
        !at.level.restore_unit_town_trip(-1, ai_state::kGoingToTown, 60.0f, at.quests, at.events));
    TL_CHECK(at.level.unit_ai_state(-1) != ai_state::kGoingToTown);
    TL_CHECK(at.level.restore_unit_town_trip(-1, ai_state::kInTown, 60.0f, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_ai_state(-1), ai_state::kInTown);
}

// A pet away in town keeps that state and its town clock through a transformation.
TL_TEST(a_pet_in_town_keeps_its_town_clock_through_a_transformation) {
    Level at;
    at.run(0.1f);
    TL_CHECK(at.level.make_player_pet(-1, at.quests, at.events));
    TL_CHECK(at.level.restore_unit_town_trip(-1, ai_state::kInTown, 60.0f, at.quests, at.events));
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", 120.0f, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_of(-1), std::string("RockTroll"));
    TL_CHECK_EQ(at.level.unit_ai_state(-1), ai_state::kInTown);
    TL_CHECK_EQ(at.level.unit_town_timer(-1), 60.0f);
    // Away, nothing runs but the town clock: the transformation waits.
    at.run(10.0f);
    TL_CHECK_EQ(at.level.unit_transform_left(-1), 120.0f);
}

// `CCharacter::interrupt(true)`: a `HIT` with `IDLE` behind it, held in
// `interruptAI` until it ends.
TL_TEST(an_interrupt_plays_the_hit_reaction_and_interrupt_ai_waits_it_out) {
    Level at;
    at.level.set_animation_catalog([](const std::string& unit) {
        std::vector<std::pair<std::string, float>> list;
        list.emplace_back("Idle", 2.0f);
        if (unit == "Ratlin Foreman") list.emplace_back("Hit", 0.5f);
        return list;
    });
    at.run(0.1f);
    std::vector<ScriptEvent> out;
    at.level.interrupt_unit(-1, at.quests, out);
    TL_CHECK_EQ(at.level.unit_ai_state(-1), 0x1d);
    bool reacted = false;
    for (const ScriptEvent& e : out) {
        if (e.action == ScriptAction::Animation && e.text == "Hit") {
            reacted = !e.loops && e.queue.size() == 1 && e.queue[0] == "IDLE";
        }
    }
    TL_CHECK(reacted);
    at.run(1.5f);
    TL_CHECK(at.level.unit_ai_state(-1) != 0x1d);

    // Without a `HIT`, `interrupt(true)` returns before it touches anything.
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", 120.0f, at.quests, at.events));
    at.run(0.1f);
    const int32_t was = at.level.unit_ai_state(-1);
    out.clear();
    at.level.interrupt_unit(-1, at.quests, out);
    TL_CHECK_EQ(at.level.unit_ai_state(-1), was);
    TL_CHECK(out.empty());
}

// The velocity scale: `DAMPVELOCITY`/`UNDAMPVELOCITY` keys, and a stun's 0 and 1.
TL_TEST(a_damped_or_stunned_unit_has_no_velocity_scale) {
    Level at;
    at.run(0.1f);
    TL_CHECK_EQ(at.level.unit_velocity_scale(-1), 1.0f);
    at.level.damp_unit_velocity(-1, true);
    TL_CHECK_EQ(at.level.unit_velocity_scale(-1), 0.0f);
    at.level.damp_unit_velocity(-1, false);
    TL_CHECK_EQ(at.level.unit_velocity_scale(-1), 1.0f);

    TL_CHECK(at.level.set_unit_stunned(-1, true, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_ai_state(-1), 0x28);
    TL_CHECK_EQ(at.level.unit_velocity_scale(-1), 0.0f);
    at.run(2.0f);
    TL_CHECK_EQ(at.level.unit_ai_state(-1), 0x28);
    TL_CHECK(at.level.set_unit_stunned(-1, false, at.quests, at.events));
    TL_CHECK_EQ(at.level.unit_velocity_scale(-1), 1.0f);
    TL_CHECK(at.level.unit_ai_state(-1) != 0x28);
}

// `CCharacter::convertCharacter` passes `from_save` for a character with a
// record, so it gets none of its new unit's `RIGHTHAND`.
TL_TEST(a_converted_character_is_not_given_its_new_units_weapons) {
    Level at;
    UnitStats armed = rooted(UnitStats::Alignment::Hostile, 0.5f);
    armed.right_hand = "Rusty Sword";
    at.units.add("Armed Ratlin", armed);
    at.run(0.1f);
    TL_CHECK(at.level.transform_unit(-1, "Armed Ratlin", 120.0f, at.quests, at.events));
    std::string right, left;
    TL_CHECK(at.level.unit_hands(-1, right, left));
    TL_CHECK(right.empty());

    at.level.set_has_saved_state(true);
    at.level.set_stored_unit_exists([](int64_t) { return false; });
    TL_CHECK(at.level.transform_unit(-1, "RockTroll", 120.0f, at.quests, at.events));
    TL_CHECK(at.level.transform_unit(-1, "Armed Ratlin", 120.0f, at.quests, at.events));
    TL_CHECK(at.level.unit_hands(-1, right, left));
    TL_CHECK_EQ(right, std::string("Rusty Sword"));
}
