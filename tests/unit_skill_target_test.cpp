// A monster's skill finding its target: `CCharacter::canCastCurrentSkill` and
// `castSkill` through `CLevel::findCharacterWithinView` and
// `getTargetIsValid`.
#include "core/collision.h"
#include "core/navigation.h"
#include "core/passability.h"
#include "core/quest.h"
#include "core/script.h"
#include "core/skills.h"
#include "core/stats.h"
#include "test_seeds.h"
#include "test_support.h"

#include <memory>
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

UnitStats minion(float sight) {
    UnitStats stats;
    stats.radius = 0.4f;
    // Rooted, so only the search decides.
    stats.movement.walking_speed = 0.0f;
    stats.movement.running_speed = 0.0f;
    stats.sight_radius = sight;
    stats.motion_radius = sight;
    stats.view_angle = 360.0f;
    stats.follow_radius = 100.0f;
    stats.alignment = UnitStats::Alignment::Hostile;
    stats.health_graph = "Health_Minion_byLevel";
    return stats;
}

/// A lich's `RES_SKELETONS` skill gates: `GOOD`, `FINDTARGETANGLE` 360, `RANGE` 25, corpses.
SkillDef res_skeletons(bool corpses) {
    SkillDef def;
    def.name = "LichResSkeletons";
    def.type = SkillType::Offensive;
    def.target_alignment = 1;
    def.target_corpses = corpses;
    def.target_specific_units = {"SKELETAL ARCHER", "SKELETAL WARRIOR", "SKELETON",
                                 "SKELETAL KNIGHT"};
    def.base.target_type = 2;
    def.base.range = 25.0f;
    def.base.find_target_angle = 360.0f;
    return def;
}

/// A flat level: the lich (-1) at the origin, the bodies of a skeleton (-2)
/// and a ratlin (-3) beside it, the player at `player`.
struct LichLevel {
    CollisionWorld world;
    PassabilityMap grid;
    NavGrid nav;
    UnitBook units;
    StatBook graphs;
    LevelScript level;
    QuestState quests;
    std::vector<ScriptEvent> events;
    SkillDef def;
    /// Every cast the scene was asked for: the target's node (-1 the player,
    /// 0 none).
    std::vector<int64_t> casts;

    LichLevel(const SkillDef& skill, const Vec3& skeleton_at, const Vec3& ratlin_at) : def(skill) {
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {-80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, 80.0f});
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, -80.0f});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
        units.add("lich", minion(12.0f));
        units.add("skeleton", minion(0.0f));
        units.add("ratlin", minion(0.0f));
        graphs.add("HEALTH_MINION_BYLEVEL", health_curve());
        level.set_units(&units);
        level.set_stat_graphs(&graphs);
        level.set_navigation(&nav);
        level.set_collision(&world);
        level.set_passability(&grid);
        level.set_unit_active_range(100.0f);
        level.set_game_playing(true);

        LevelScript::UnitSkills hooks;
        hooks.load = [this](const std::string& unit) {
            std::vector<LevelScript::UnitSkill> out;
            if (unit != "lich") return out;
            LevelScript::UnitSkill s;
            s.known.entry.name = def.name;
            s.known.def = &def;
            s.known.invested = 1;
            s.known.effective = 1;
            s.cast_chance = 100;
            out.push_back(s);
            return out;
        };
        hooks.caster = [](LevelScript& machine, int64_t node, LevelScript* target_machine,
                          int64_t target_node, bool has_target) {
            SkillCaster caster;
            caster.is_monster = true;
            machine.unit_position(node, caster.position);
            if (has_target) {
                SkillTargetUnit unit;
                if (target_machine != nullptr)
                    target_machine->unit_position(target_node, unit.position);
                caster.target_unit = unit;
            }
            return caster;
        };
        hooks.cast = [this](LevelScript&, int64_t, LevelScript*, int64_t target_node,
                            bool has_target) {
            casts.push_back(has_target ? target_node : 0);
            return true;
        };
        level.set_unit_skills(hooks);

        tl::test::seed_generators();
        LevelScript::reset_near_list_timer();
        level.build(tl::formats::CmpScene{}, {});
        level.add_spawned_unit(-1, "lich", Vec3{0.0f, 0.0f, 0.0f}, false);
        level.add_spawned_unit(-2, "skeleton", skeleton_at, false);
        level.add_spawned_unit(-3, "ratlin", ratlin_at, false);
    }

    void kill(int64_t id) {
        float hp = 0.0f;
        float max = 0.0f;
        level.unit_health(id, hp, max);
        level.damage_unit(id, max + 1000.0f, -1, quests, events);
    }

    void run(const Vec3& player, float seconds) {
        const int frames = static_cast<int>(seconds * 60.0f + 0.5f);
        for (int i = 0; i < frames; ++i) {
            events.clear();
            level.update(1.0f / 60.0f, player, quests, events);
        }
    }
};

}  // namespace

/// The player is no target for `LichResSkeletons`, so the lich looks among the `EVIL` for a
/// body the skill lists.
TL_TEST(a_lich_finds_the_skeletons_body_to_raise) {
    LichLevel at(res_skeletons(true), Vec3{6.0f, 0.0f, 0.0f}, Vec3{3.0f, 0.0f, 0.0f});
    at.kill(-2);
    at.kill(-3);
    TL_CHECK(at.level.unit_is_dead(-2));
    TL_CHECK(at.level.unit_is_dead(-3));
    at.run(Vec3{0.0f, 0.0f, 8.0f}, 1.5f);
    TL_CHECK(!at.casts.empty());
    for (const int64_t target : at.casts) TL_CHECK_EQ(target, -2);
}

/// A living skeleton is no target (`TARGET_CORPSES` wants the dead), and
/// without `TARGET_CORPSES` the bodies are never looked at: nothing is cast.
TL_TEST(a_lich_raises_nothing_without_a_body_or_without_target_corpses) {
    {
        LichLevel at(res_skeletons(true), Vec3{6.0f, 0.0f, 0.0f}, Vec3{3.0f, 0.0f, 0.0f});
        at.run(Vec3{0.0f, 0.0f, 8.0f}, 1.5f);
        TL_CHECK(at.casts.empty());
    }
    {
        LichLevel at(res_skeletons(false), Vec3{6.0f, 0.0f, 0.0f}, Vec3{3.0f, 0.0f, 0.0f});
        at.kill(-2);
        at.kill(-3);
        at.run(Vec3{0.0f, 0.0f, 8.0f}, 1.5f);
        TL_CHECK(at.casts.empty());
    }
}

/// The reach: `(REACH_BONUS * scale + 0.2 + RANGE) * RANGE_MULTIPLIER + 1`.
TL_TEST(a_body_past_the_skills_reach_is_not_found) {
    {
        LichLevel at(res_skeletons(true), Vec3{20.0f, 0.0f, 0.0f}, Vec3{-60.0f, 0.0f, 0.0f});
        at.kill(-2);
        at.kill(-3);
        at.run(Vec3{0.0f, 0.0f, 8.0f}, 1.5f);
        TL_CHECK(!at.casts.empty());
    }
    LichLevel at(res_skeletons(true), Vec3{27.5f, 0.0f, 0.0f}, Vec3{-60.0f, 0.0f, 0.0f});
    at.kill(-2);
    at.kill(-3);
    at.run(Vec3{0.0f, 0.0f, 8.0f}, 1.5f);
    TL_CHECK(at.casts.empty());
}

/// `[TARGET_SPECIFIC_UNITS]`: every string value of the section, upper-cased,
/// whatever its key; and `TARGET_CORPSES` at the root.
TL_TEST(the_skill_file_reads_its_target_units_and_corpses) {
    tl::formats::AdmFile file;
    file.ok = true;
    file.root.name = "SKILL";
    const auto text = [](const char* key, const char* value) {
        tl::formats::AdmRecord r;
        r.key = key;
        r.type = tl::formats::AdmType::String;
        r.text = value;
        return r;
    };
    file.root.records.push_back(text("NAME", "LichResSkeletons"));
    tl::formats::AdmRecord corpses;
    corpses.key = "TARGET_CORPSES";
    corpses.type = tl::formats::AdmType::Bool;
    corpses.integer = 1;
    file.root.records.push_back(corpses);
    tl::formats::AdmSection units;
    units.name = "TARGET_SPECIFIC_UNITS";
    units.records.push_back(text("UNIT", "Skeletal Archer"));
    units.records.push_back(text("OTHER", "Skeleton"));
    tl::formats::AdmRecord number;
    number.key = "UNIT";
    number.type = tl::formats::AdmType::Integer;
    number.integer = 7;
    units.records.push_back(number);
    file.root.children.push_back(units);
    const SkillDef def = parse_skill_def(file);
    TL_CHECK(def.target_corpses);
    TL_CHECK_EQ(def.target_specific_units.size(), static_cast<size_t>(2));
    TL_CHECK(def.target_specific_units[0] == "SKELETAL ARCHER");
    TL_CHECK(def.target_specific_units[1] == "SKELETON");
    tl::formats::AdmFile bare;
    bare.ok = true;
    bare.root.name = "SKILL";
    const SkillDef none = parse_skill_def(bare);
    TL_CHECK(!none.target_corpses);
    TL_CHECK(none.target_specific_units.empty());
}

/// `CCharacter::modifyHP` on a body takes only a whole heal; then
/// `CSkillEvent::applyEffects` gets it up.
TL_TEST(a_body_filled_by_a_whole_heal_gets_up) {
    LichLevel at(res_skeletons(true), Vec3{6.0f, 0.0f, 0.0f}, Vec3{3.0f, 0.0f, 0.0f});
    float hp = 0.0f;
    float max = 0.0f;
    TL_CHECK(at.level.unit_health(-2, hp, max));
    TL_CHECK(max > 1.0f);
    at.kill(-2);
    TL_CHECK(at.level.unit_is_dead(-2));
    // Short of the whole: nothing.
    TL_CHECK(!at.level.modify_unit_hp(-2, max - 1.0f));
    at.level.unit_health(-2, hp, max);
    TL_CHECK_EQ(hp, 0.0f);
    TL_CHECK(!at.level.unit_gets_up(-2, false, at.quests, at.events));
    TL_CHECK(at.level.unit_is_dead(-2));
    // `HEALSKELETONS`' 10000: full, and `+0x454` cleared.
    TL_CHECK(at.level.modify_unit_hp(-2, 10000.0f));
    at.level.unit_health(-2, hp, max);
    TL_CHECK_EQ(hp, max);
    // Alive before the effects: nothing to get up from.
    TL_CHECK(!at.level.unit_gets_up(-2, true, at.quests, at.events));
    TL_CHECK(at.level.unit_gets_up(-2, false, at.quests, at.events));
    TL_CHECK(!at.level.unit_is_dead(-2));
    TL_CHECK_EQ(at.level.unit_ai_state(-2), ai_state::kIdle);
    // A living unit is healed up to its maximum, and clears nothing.
    at.level.set_unit_hp(-3, 10.0f);
    TL_CHECK(!at.level.modify_unit_hp(-3, 5.0f));
    at.level.unit_health(-3, hp, max);
    TL_CHECK_EQ(hp, 15.0f);
}
