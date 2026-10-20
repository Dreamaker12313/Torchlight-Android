// Procs on being struck: CCharacter::applyDamage runs them before
// modifyHP once `+0x708` (set to 1.0 by an own proc) is run down by
// CCharacter::update; CBaseUnit::unitInit: reapplyEffects first.
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

UnitStats ratlin() {
    UnitStats stats;
    stats.radius = 0.4f;
    stats.movement.walking_speed = 0.0f;
    stats.movement.running_speed = 0.0f;
    stats.follow_radius = 100.0f;
    stats.alignment = UnitStats::Alignment::Hostile;
    stats.health_graph = "Health_Minion_byLevel";
    return stats;
}

/// One proc the scene was asked to cast.
struct ProcCall {
    int type = 0;
    bool has_target = false;
    LevelScript* target_machine = nullptr;
    int64_t target_node = 0;
    float hp = 0.0f;  ///< the unit's hit points as it casts
};

/// A flat level with one ratlin (-1); the scene's hooks write down what they
/// are asked, and a proc locks the unit as `executeProcs` does.
struct ChuteLevel {
    CollisionWorld world;
    PassabilityMap grid;
    NavGrid nav;
    UnitBook units;
    StatBook graphs;
    LevelScript level;
    QuestState quests;
    std::vector<ScriptEvent> events;
    std::vector<std::string> order;
    std::vector<ProcCall> procs;

    ChuteLevel() {
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {-80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, 80.0f});
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, -80.0f});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
        units.add("ratlin chute1", ratlin());
        graphs.add("HEALTH_MINION_BYLEVEL", health_curve());
        level.set_units(&units);
        level.set_stat_graphs(&graphs);
        level.set_navigation(&nav);
        level.set_collision(&world);
        level.set_passability(&grid);
        level.set_unit_active_range(100.0f);
        level.set_game_playing(true);

        LevelScript::UnitSkills hooks;
        hooks.load = [this](const std::string&) {
            order.push_back("skill blocks");
            return std::vector<LevelScript::UnitSkill>{};
        };
        hooks.own_effects = [this](LevelScript&, int64_t) { order.push_back("own effects"); };
        hooks.procs = [this](LevelScript& machine, int64_t node, int type,
                             LevelScript* target_machine, int64_t target_node, bool has_target) {
            ProcCall call;
            call.type = type;
            call.has_target = has_target;
            call.target_machine = target_machine;
            call.target_node = target_node;
            float max = 0.0f;
            machine.unit_health(node, call.hp, max);
            procs.push_back(call);
            machine.lock_unit_procs(node);
        };
        level.set_unit_skills(hooks);

        tl::test::seed_generators();
        LevelScript::reset_near_list_timer();
        level.build(tl::formats::CmpScene{}, {});
        level.add_spawned_unit(-1, "ratlin chute1", Vec3{0.0f, 0.0f, 0.0f}, false);
    }

    float hp() {
        float now = 0.0f;
        float max = 0.0f;
        level.unit_health(-1, now, max);
        return now;
    }

    void run(float seconds) {
        const int frames = static_cast<int>(seconds * 60.0f + 0.5f);
        for (int i = 0; i < frames; ++i) {
            events.clear();
            level.update(1.0f / 60.0f, Vec3{30.0f, 0.0f, 0.0f}, quests, events);
        }
    }
};

}  // namespace

TL_TEST(a_struck_unit_casts_its_procs_at_the_attacker_before_its_hit_points_move) {
    ChuteLevel at;
    const float before = at.hp();
    TL_CHECK(!at.level.damage_unit(-1, 10.0f, 0, at.quests, at.events, nullptr, nullptr,
                                   /*struck=*/true));
    TL_CHECK_EQ(at.procs.size(), size_t{1});
    TL_CHECK_EQ(at.procs[0].type, 0x51);
    // The player struck it: a target with no machine.
    TL_CHECK(at.procs[0].has_target);
    TL_CHECK(at.procs[0].target_machine == nullptr);
    TL_CHECK_EQ(at.procs[0].hp, before);
    TL_CHECK_EQ(at.hp(), before - 10.0f);
}

TL_TEST(a_damage_with_no_struck_flag_casts_no_proc) {
    ChuteLevel at;
    // A damage-over-time shape's and a reflection's `applyDamage`.
    at.level.damage_unit(-1, 10.0f, LevelScript::kNoAttacker, at.quests, at.events);
    TL_CHECK(at.procs.empty());
    // A struck flag with no attacker still rolls, at nobody.
    at.level.damage_unit(-1, 10.0f, LevelScript::kNoAttacker, at.quests, at.events, nullptr,
                         nullptr, /*struck=*/true);
    TL_CHECK_EQ(at.procs.size(), size_t{1});
    TL_CHECK(!at.procs[0].has_target);
}

TL_TEST(an_own_proc_holds_the_next_struck_procs_off_for_a_second) {
    ChuteLevel at;
    at.run(0.1f);
    at.level.damage_unit(-1, 1.0f, 0, at.quests, at.events, nullptr, nullptr, true);
    TL_CHECK_EQ(at.procs.size(), size_t{1});
    // `+0x708` is 1.0: the next blow lands without a roll.
    at.level.damage_unit(-1, 1.0f, 0, at.quests, at.events, nullptr, nullptr, true);
    TL_CHECK_EQ(at.procs.size(), size_t{1});
    at.run(0.5f);
    at.level.damage_unit(-1, 1.0f, 0, at.quests, at.events, nullptr, nullptr, true);
    TL_CHECK_EQ(at.procs.size(), size_t{1});
    // `update` has run it down past 0.
    at.run(0.6f);
    at.level.damage_unit(-1, 1.0f, 0, at.quests, at.events, nullptr, nullptr, true);
    TL_CHECK_EQ(at.procs.size(), size_t{2});
}

TL_TEST(a_units_own_effects_go_on_ahead_of_its_skill_blocks) {
    ChuteLevel at;
    at.run(0.1f);
    TL_CHECK_EQ(at.order.size(), size_t{2});
    TL_CHECK_EQ(at.order[0], std::string("own effects"));
    TL_CHECK_EQ(at.order[1], std::string("skill blocks"));
}
