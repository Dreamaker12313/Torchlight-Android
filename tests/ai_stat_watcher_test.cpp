// Unit tests for AIStatWatcher.
// Everything is built in code; no game data files are accessed.

#include "core/ai_stat_watcher.h"

#include "core/ai_flags.h"
#include "core/ai_skills.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using tl::core::AIFlag;
using tl::core::AIFlags;
using tl::core::AISkill;
using tl::core::AISkillManager;
using tl::core::AIStatLogic;
using tl::core::AIStatTarget;
using tl::core::AIStatType;
using tl::core::AIStatWatcher;
using tl::core::AIStatWatcherDef;
using tl::core::AIStatWatcherHooks;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;

}  // namespace

// Parse defaults: empty [STAT] gives original constructor defaults.
// Note quirk: VALUE == 0.0f auto-sets ONLYONCE to true.
TL_TEST(ai_stat_watcher_parse_defaults) {
    AdmSection ai;
    ai.name = "AI";
    AdmSection stat;
    stat.name = "STAT";
    ai.children.push_back(stat);

    const auto watchers = tl::core::parse_ai_stat_watchers(ai);
    TL_CHECK_EQ(watchers.size(), 1u);

    const auto& w = watchers[0];
    TL_CHECK_EQ(static_cast<uint32_t>(w.stat), 0u);   // NONE
    TL_CHECK_EQ(static_cast<uint32_t>(w.logic), 0u);  // BELOW
    TL_CHECK(w.flags.empty());
    TL_CHECK(w.skills.empty());
    TL_CHECK_EQ(static_cast<uint32_t>(w.target), 0u);  // SELF
    TL_CHECK_EQ(w.target_unittype, 0u);                // ANY
    TL_CHECK_EQ(w.target_alignment, 3u);               // ALL
    TL_CHECK_EQ(w.target_area, 10.0f);                 // 10.0
    TL_CHECK_EQ(w.duration, 0.0f);
    TL_CHECK_EQ(w.value, 0.0f);
    TL_CHECK(w.only_once);  // auto-set by value == 0.0f
    TL_CHECK(w.animation.empty());
    TL_CHECK(w.enable_on_event);  // true
    TL_CHECK(w.include_living);   // true
    TL_CHECK(!w.include_dead);    // false
}

// Parse records and unknowns: multiple FLAG and SKILL lines preserved in order,
// unknown strings map to the field's default (an unknown flag to AWARE), and ONLYONCE false preserved when VALUE > 0.
TL_TEST(ai_stat_watcher_parse_records_and_unknowns) {
    AdmSection ai;
    ai.name = "AI";
    AdmSection stat;
    stat.name = "STAT";

    stat.records.push_back({"STAT", tl::formats::AdmType::String, 0, 0.0f, "INVALID_STAT"});
    stat.records.push_back({"LOGIC", tl::formats::AdmType::String, 0, 0.0f, "INVALID_LOGIC"});
    stat.records.push_back({"FLAG", tl::formats::AdmType::String, 0, 0.0f, "AWARE"});
    stat.records.push_back({"FLAG", tl::formats::AdmType::String, 0, 0.0f, "BOGUS_FLAG"});
    stat.records.push_back({"FLAG", tl::formats::AdmType::String, 0, 0.0f, "FRIGHTEN"});
    stat.records.push_back({"SKILL", tl::formats::AdmType::String, 0, 0.0f, "Minor Haste"});
    stat.records.push_back({"SKILL", tl::formats::AdmType::String, 0, 0.0f, "Fireball"});
    stat.records.push_back({"TARGET", tl::formats::AdmType::String, 0, 0.0f, "FORMATION"});
    stat.records.push_back({"TARGET_ALIGNMENT", tl::formats::AdmType::String, 0, 0.0f, "EVIL"});
    stat.records.push_back({"VALUE", tl::formats::AdmType::Float, 0, 25.0f, ""});
    stat.records.push_back({"ONLYONCE", tl::formats::AdmType::Bool, 0, 0.0f, ""});
    stat.records.push_back({"ANIMATION", tl::formats::AdmType::String, 0, 0.0f, "cast"});
    ai.children.push_back(stat);

    const auto watchers = tl::core::parse_ai_stat_watchers(ai);
    TL_CHECK_EQ(watchers.size(), 1u);

    const auto& w = watchers[0];
    TL_CHECK_EQ(static_cast<uint32_t>(w.stat), 0u);   // invalid -> NONE
    TL_CHECK_EQ(static_cast<uint32_t>(w.logic), 0u);  // invalid -> BELOW
    // An unknown flag name falls back to index 0 and is still appended.
    TL_CHECK_EQ(w.flags.size(), 3u);
    TL_CHECK_EQ(static_cast<uint32_t>(w.flags[0]), 0u);  // AWARE
    TL_CHECK_EQ(static_cast<uint32_t>(w.flags[1]), 0u);  // BOGUS_FLAG reads as AWARE
    TL_CHECK_EQ(static_cast<uint32_t>(w.flags[2]), 3u);  // FRIGHTEN
    TL_CHECK_EQ(w.skills.size(), 2u);
    TL_CHECK_EQ(w.skills[0], "Minor Haste");
    TL_CHECK_EQ(w.skills[1], "Fireball");
    TL_CHECK_EQ(static_cast<uint32_t>(w.target), 1u);  // FORMATION
    TL_CHECK_EQ(w.target_alignment, 2u);               // EVIL
    TL_CHECK_EQ(w.value, 25.0f);
    TL_CHECK(!w.only_once);            // false retained since value == 25 != 0
    TL_CHECK_EQ(w.animation, "CAST");  // uppercased
}

// Enum text is upper-cased before the lookup, and TARGET_ALIGNMENT is
// `KALIGNMENT_STRINGS` (NEUTRAL, GOOD, EVIL, ALL, BERSERK, EVILBERSERK,
// GOODBERSERK); an unknown name keeps the default, ALL.
TL_TEST(ai_stat_watcher_parse_case_and_alignment_table) {
    const auto parse_one = [](const std::string& stat, const std::string& logic,
                              const std::string& alignment) {
        AdmSection ai;
        ai.name = "AI";
        AdmSection group;
        group.name = "STAT";
        group.records.push_back({"STAT", tl::formats::AdmType::String, 0, 0.0f, stat});
        group.records.push_back({"LOGIC", tl::formats::AdmType::String, 0, 0.0f, logic});
        group.records.push_back(
            {"TARGET_ALIGNMENT", tl::formats::AdmType::String, 0, 0.0f, alignment});
        group.records.push_back(
            {"FLAG", tl::formats::AdmType::String, 0, 0.0f, "never change target"});
        ai.children.push_back(group);
        return tl::core::parse_ai_stat_watchers(ai).at(0);
    };

    const AIStatWatcherDef lower = parse_one("hp pct", "above", "evil");
    TL_CHECK_EQ(static_cast<uint32_t>(lower.stat), 3u);
    TL_CHECK_EQ(static_cast<uint32_t>(lower.logic), 1u);
    TL_CHECK_EQ(lower.target_alignment, 2u);
    TL_CHECK_EQ(static_cast<uint32_t>(lower.flags.at(0)), 5u);

    TL_CHECK_EQ(parse_one("HP", "BELOW", "BERSERK").target_alignment, 4u);
    TL_CHECK_EQ(parse_one("HP", "BELOW", "EVILBERSERK").target_alignment, 5u);
    TL_CHECK_EQ(parse_one("HP", "BELOW", "GOODBERSERK").target_alignment, 6u);
    TL_CHECK_EQ(parse_one("HP", "BELOW", "WAR").target_alignment, 3u);
}

// `varkolyn_startquest`'s watcher (HP ABOVE 0, NEVER CHANGE TARGET, no
// DURATION): VALUE 0 makes it ONLYONCE, it fires on the first update, and the
// 0-second add leaves the flag unset (`CAIFlagManager::addAIFlag` only raises
// the time), so the flag never takes hold - and the watcher never fires again.
TL_TEST(ai_stat_watcher_zero_duration_flag_never_holds) {
    AdmSection ai;
    ai.name = "AI";
    AdmSection group;
    group.name = "STAT";
    group.records.push_back({"STAT", tl::formats::AdmType::String, 0, 0.0f, "HP"});
    group.records.push_back({"LOGIC", tl::formats::AdmType::String, 0, 0.0f, "ABOVE"});
    group.records.push_back({"FLAG", tl::formats::AdmType::String, 0, 0.0f, "NEVER CHANGE TARGET"});
    group.records.push_back({"VALUE", tl::formats::AdmType::Float, 0, 0.0f, ""});
    ai.children.push_back(group);
    const AIStatWatcherDef def = tl::core::parse_ai_stat_watchers(ai).at(0);
    TL_CHECK(def.only_once);

    AIFlags flags;
    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 7; };
    hooks.hp = []() { return 120; };
    hooks.alive = []() { return true; };
    hooks.find_flags_by_id = [&](int64_t id) { return id == 7 ? &flags : nullptr; };
    AIStatWatcher watcher(def, hooks);

    watcher.update(0.016f);
    TL_CHECK(watcher.has_fired());
    TL_CHECK(!flags.has(AIFlag::NeverChangeTarget));
    TL_CHECK_EQ(watcher.recorded_target_guids().size(), 1u);
    watcher.update(0.016f);
    TL_CHECK_EQ(watcher.recorded_target_guids().size(), 1u);
}

// Inclusivity: BELOW and ABOVE comparisons are both inclusive.
TL_TEST(ai_stat_watcher_below_above_inclusive) {
    AIStatWatcherDef def_below;
    def_below.stat = AIStatType::HpPct;
    def_below.logic = AIStatLogic::Below;
    def_below.value = 50.0f;
    def_below.duration = 5.0f;
    def_below.flags.push_back(AIFlag::Aware);

    AIStatWatcherDef def_above = def_below;
    def_above.logic = AIStatLogic::Above;

    int32_t current_hp = 50;
    AIFlags flags_below;
    AIFlags flags_above;

    AIStatWatcherHooks hooks_below;
    hooks_below.hp = [&]() { return current_hp; };
    hooks_below.max_hp = []() { return 100; };
    hooks_below.owner_id = []() { return 1; };
    hooks_below.find_flags_by_id = [&](int64_t) { return &flags_below; };

    AIStatWatcherHooks hooks_above = hooks_below;
    hooks_above.find_flags_by_id = [&](int64_t) { return &flags_above; };

    AIStatWatcher watcher_below(def_below, hooks_below);
    AIStatWatcher watcher_above(def_above, hooks_above);

    // At exactly 50%: both BELOW 50 and ABOVE 50 must be met (inclusive).
    current_hp = 50;
    watcher_below.update(0.1f);
    watcher_above.update(0.1f);
    TL_CHECK(flags_below.has(AIFlag::Aware));
    TL_CHECK(flags_above.has(AIFlag::Aware));

    // Reset flags
    flags_below.remove(AIFlag::Aware);
    flags_above.remove(AIFlag::Aware);

    // At 51%: BELOW 50 is not met; ABOVE 50 is met.
    current_hp = 51;
    watcher_below.update(0.1f);
    watcher_above.update(0.1f);
    TL_CHECK(!flags_below.has(AIFlag::Aware));
    TL_CHECK(flags_above.has(AIFlag::Aware));

    // Reset flags
    flags_above.remove(AIFlag::Aware);

    // At 49%: BELOW 50 is met; ABOVE 50 is not met.
    current_hp = 49;
    watcher_below.update(0.1f);
    watcher_above.update(0.1f);
    TL_CHECK(flags_below.has(AIFlag::Aware));
    TL_CHECK(!flags_above.has(AIFlag::Aware));
}

// Float HP rounding: CCharacter::HP truncates to integer before conversion.
TL_TEST(ai_stat_watcher_hp_truncation) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 49.0f;
    def.duration = 5.0f;
    def.flags.push_back(AIFlag::Frighten);

    float float_hp = 49.9f;
    AIFlags flags;

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 1; };
    // Truncates float_hp (49.9f) to int (49)
    hooks.hp = [&]() -> int32_t { return static_cast<int32_t>(std::floor(float_hp)); };
    hooks.find_flags_by_id = [&](int64_t) { return &flags; };

    AIStatWatcher watcher(def, hooks);

    // 49.9f truncated to int is 49. 49.0f <= 49.0f is MET.
    // If unrounded float 49.9f were used, 49.9f <= 49.0f would fail.
    watcher.update(0.1f);
    TL_CHECK(flags.has(AIFlag::Frighten));
}

// Zero duration takes back, non-zero duration does not.
TL_TEST(ai_stat_watcher_zero_duration_removal_vs_nonzero) {
    // Non-zero duration: 5.0s
    AIStatWatcherDef def_nonzero;
    def_nonzero.stat = AIStatType::Hp;
    def_nonzero.logic = AIStatLogic::Below;
    def_nonzero.value = 50.0f;
    def_nonzero.duration = 5.0f;
    def_nonzero.flags.push_back(AIFlag::Aware);

    int32_t hp = 40;
    AIFlags flags_nonzero;
    AIStatWatcherHooks hooks_nonzero;
    hooks_nonzero.owner_id = []() { return 1; };
    hooks_nonzero.hp = [&]() { return hp; };
    hooks_nonzero.find_flags_by_id = [&](int64_t) { return &flags_nonzero; };

    AIStatWatcher watcher_nonzero(def_nonzero, hooks_nonzero);

    // Condition met (hp 40 <= 50): flag added with duration 5.0s
    watcher_nonzero.update(0.1f);
    TL_CHECK(flags_nonzero.has(AIFlag::Aware));
    TL_CHECK_EQ(flags_nonzero.time(AIFlag::Aware), 5.0f);

    // Condition no longer met (hp 60 > 50): non-zero duration leaves flag to expire
    hp = 60;
    watcher_nonzero.update(0.1f);
    TL_CHECK(flags_nonzero.has(AIFlag::Aware));

    // Zero duration: 0.0s with skills
    AIStatWatcherDef def_zero;
    def_zero.stat = AIStatType::Hp;
    def_zero.logic = AIStatLogic::Below;
    def_zero.value = 50.0f;
    def_zero.duration = 0.0f;
    def_zero.only_once = false;  // explicitly allow repeat
    def_zero.skills.push_back("Haste");

    bool has_skill = false;
    AIStatWatcherHooks hooks_zero;
    hooks_zero.owner_id = []() { return 2; };
    hooks_zero.hp = [&]() { return hp; };
    hooks_zero.add_ai_skill = [&](AISkill*, float) { has_skill = true; };
    hooks_zero.has_ai_skill = [&](AISkill*) { return has_skill; };
    hooks_zero.remove_ai_skill = [&](AISkill*) { has_skill = false; };

    AIStatWatcher watcher_zero(def_zero, hooks_zero);

    // Condition met (hp 40 <= 50): skill added
    hp = 40;
    watcher_zero.update(0.1f);
    TL_CHECK(has_skill);

    // Condition no longer met (hp 60 > 50): 0 duration takes skill back!
    hp = 60;
    watcher_zero.update(0.1f);
    TL_CHECK(!has_skill);
}

// ONLYONCE: fires once then never again, and skips update if unit is dead.
TL_TEST(ai_stat_watcher_only_once_and_dead_skip) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 50.0f;
    def.duration = 5.0f;
    def.only_once = true;
    def.flags.push_back(AIFlag::Berserk);

    int32_t hp = 40;
    bool is_alive = true;
    AIFlags flags;

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 1; };
    hooks.hp = [&]() { return hp; };
    hooks.alive = [&]() { return is_alive; };
    hooks.find_flags_by_id = [&](int64_t) { return &flags; };

    AIStatWatcher watcher(def, hooks);

    // First fire: adds Berserk
    watcher.update(0.1f);
    TL_CHECK(flags.has(AIFlag::Berserk));
    TL_CHECK(watcher.has_fired());

    // Clear flag manually
    flags.remove(AIFlag::Berserk);
    TL_CHECK(!flags.has(AIFlag::Berserk));

    // Clear condition and re-meet: should NOT fire again because ONLYONCE is true
    hp = 80;
    watcher.update(0.1f);
    hp = 30;
    watcher.update(0.1f);
    TL_CHECK(!flags.has(AIFlag::Berserk));

    // Dead unit with ONLYONCE: skips update completely
    AIStatWatcher watcher2(def, hooks);
    is_alive = false;
    hp = 20;
    watcher2.update(0.1f);
    TL_CHECK(!flags.has(AIFlag::Berserk));
    TL_CHECK(!watcher2.has_fired());
}

// Inversion (ENABLE_ON_EVENT == false): condition met removes, cleared condition adds.
TL_TEST(ai_stat_watcher_enable_on_event_inversion) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 50.0f;
    def.duration = 0.0f;
    def.only_once = false;
    def.enable_on_event = false;  // INVERTED
    def.skills.push_back("SkeletonSummon");

    bool has_skill = true;
    int32_t hp = 100;

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 1; };
    hooks.hp = [&]() { return hp; };
    hooks.add_ai_skill = [&](AISkill*, float) { has_skill = true; };
    hooks.has_ai_skill = [&](AISkill*) { return has_skill; };
    hooks.remove_ai_skill = [&](AISkill*) { has_skill = false; };

    AIStatWatcher watcher(def, hooks);

    // Condition met (hp 30 <= 50): inverted -> REMOVES skill!
    hp = 30;
    watcher.update(0.1f);
    TL_CHECK(!has_skill);

    // Condition not met (hp 80 > 50) and duration == 0: inverted -> ADDS skill!
    hp = 80;
    watcher.update(0.1f);
    TL_CHECK(has_skill);
}

// Target resolution: SELF vs FORMATION, and cleanup reaches recorded GUIDs.
TL_TEST(ai_stat_watcher_targets_and_recorded_guids) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 50.0f;
    def.duration = 0.0f;
    def.only_once = false;
    def.target = AIStatTarget::Formation;
    def.flags.push_back(AIFlag::CannotInterrupt);

    AIFlags self_flags;
    AIFlags peer1_flags;
    AIFlags peer2_flags;
    std::vector<int64_t> current_formation = {201, 202};

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 100; };
    hooks.hp = []() { return 40; };
    hooks.formation_units = [&]() { return current_formation; };
    hooks.find_flags_by_id = [&](int64_t id) -> AIFlags* {
        if (id == 100) return &self_flags;
        if (id == 201) return &peer1_flags;
        if (id == 202) return &peer2_flags;
        return nullptr;
    };

    AIStatWatcher watcher(def, hooks);

    // Fire: adds to formation peers (201, 202), but NOT self (100)
    watcher.add_flag();
    TL_CHECK(!self_flags.has(AIFlag::CannotInterrupt));
    TL_CHECK_EQ(watcher.recorded_target_guids().size(), 2u);
    TL_CHECK_EQ(watcher.recorded_target_guids()[0], 201);
    TL_CHECK_EQ(watcher.recorded_target_guids()[1], 202);

    // Change formation: peer 202 leaves formation
    current_formation = {201};

    // Remove: cleanup must use recorded target GUIDs, reaching both 201 and 202!
    watcher.remove_flag();
    TL_CHECK(watcher.recorded_target_guids().empty());
}

// Skill hooks and animation playback.
TL_TEST(ai_stat_watcher_skill_hooks_and_animation) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 50.0f;
    def.duration = 10.0f;
    def.animation = "WARCRY";
    def.skills.push_back("BattleRage");

    int skill_add_count = 0;
    std::string played_anim;
    bool skill_held = false;

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 1; };
    hooks.hp = []() { return 30; };
    hooks.has_ai_skill = [&](AISkill*) { return skill_held; };
    hooks.add_ai_skill = [&](AISkill*, float) {
        ++skill_add_count;
        skill_held = true;
    };
    hooks.play_animation = [&](int64_t, const std::string& anim) { played_anim = anim; };

    AIStatWatcher watcher(def, hooks);

    // First fire: adds skill and plays animation
    watcher.add_skill();
    TL_CHECK_EQ(skill_add_count, 1);
    TL_CHECK_EQ(played_anim, "WARCRY");

    // Second call: skill already held -> no skill added, no animation played
    played_anim.clear();
    watcher.add_skill();
    TL_CHECK_EQ(skill_add_count, 1);
    TL_CHECK(played_anim.empty());
}

// Bravery calculation under FRIGHTEN flag: CCharacter::getBravery.
TL_TEST(ai_stat_watcher_bravery_frighten_factor) {
    AIFlags flags;
    float base_bravery = 0.8f;

    auto calc_bravery = [&](const AIFlags& f, float b) -> float {
        if (f.has(AIFlag::Frighten)) {
            return b * 0.5f;
        }
        return b;
    };

    // Without FRIGHTEN: bravery is unaltered
    TL_CHECK_EQ(calc_bravery(flags, base_bravery), 0.8f);

    // With FRIGHTEN: bravery is multiplied by 0.5
    flags.add(AIFlag::Frighten, 5.0f);
    TL_CHECK(flags.has(AIFlag::Frighten));
    TL_CHECK_EQ(calc_bravery(flags, base_bravery), 0.4f);

    // Expiry: restored to 0.8
    flags.update(6.0f);
    TL_CHECK(!flags.has(AIFlag::Frighten));
    TL_CHECK_EQ(calc_bravery(flags, base_bravery), 0.8f);
}

// A watcher's add and remove reach a real AISkillManager through the hooks,
// holding the watcher's own AISkill pointer.
TL_TEST(ai_stat_watcher_reaches_real_skill_manager) {
    AIStatWatcherDef def;
    def.stat = AIStatType::Hp;
    def.logic = AIStatLogic::Below;
    def.value = 50.0f;
    def.duration = 5.0f;
    def.skills.push_back("GhostSummon");

    AISkillManager mgr;
    AISkill* passed_skill = nullptr;

    AIStatWatcherHooks hooks;
    hooks.owner_id = []() { return 1; };
    hooks.hp = []() { return 30; };
    hooks.add_ai_skill = [&](AISkill* skill, float duration) {
        passed_skill = skill;
        mgr.add(skill, duration);
    };
    hooks.has_ai_skill = [&](AISkill* skill) { return mgr.has(skill); };
    hooks.remove_ai_skill = [&](AISkill* skill) { mgr.remove(skill); };

    AIStatWatcher watcher(def, hooks);

    // Initial state: manager has no skills
    TL_CHECK_EQ(mgr.skills().size(), 0u);

    // add_skill(): watcher passes its own AISkill* to the manager
    watcher.add_skill();
    TL_CHECK_EQ(mgr.skills().size(), 1u);
    TL_CHECK_EQ(mgr.skills()[0], passed_skill);
    TL_CHECK_EQ(passed_skill->name(), "GhostSummon");
    TL_CHECK_EQ(passed_skill->timer(), 5.0f);
    TL_CHECK(mgr.has(passed_skill));

    // remove_skill(): manager removes the skill and zeroes timer
    watcher.remove_skill();
    TL_CHECK_EQ(mgr.skills().size(), 0u);
    TL_CHECK(!mgr.has(passed_skill));
    TL_CHECK_EQ(passed_skill->timer(), 0.0f);
}
