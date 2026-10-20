// Which animation a skill plays, the keys on it, and the `EVENT_TRIGGER` group they fire.
#include "core/skill_animation.h"

#include <cmath>

#include "core/effect_manager.h"
#include "core/random.h"
#include "test_support.h"

namespace {

using tl::core::ActiveSkill;
using tl::core::AnimationCatalog;
using tl::core::AnimationTag;
using tl::core::FiredKey;
using tl::core::KnownSkill;
using tl::core::SkillAnimationCue;
using tl::core::SkillAnimations;
using tl::core::SkillCastContext;
using tl::core::SkillCaster;
using tl::core::SkillCastResult;
using tl::core::SkillDef;
using tl::core::SkillProperty;
using tl::core::SkillRunner;
using tl::core::SkillRunState;
using tl::core::SkillStop;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord num(const std::string& key, int64_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Integer;
    r.integer = static_cast<uint32_t>(value);
    r.number = static_cast<float>(value);
    return r;
}

/// The Alchemist's first few animations in manifest order, with made-up lengths.
AnimationCatalog alchemist_model() {
    return {{"alchemist", 0.0f},        {"Attack1", 0.8f},  {"attack2", 0.9f},
            {"special_gen_buff", 1.2f}, {"rslash_1", 0.7f}, {"special_loop", 2.0f},
            {"special_loop_end", 0.5f}};
}

UnitTypeHierarchy weapon_types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "HIERARCHY";
    AdmSection types;
    types.name = "UNITTYPES";
    const auto add = [&types](const char* name, int id, const std::vector<int>& ancestors) {
        AdmSection group;
        group.name = name;
        group.records.push_back(num("ID", id));
        int n = 0;
        for (int a : ancestors) group.records.push_back(num("child" + std::to_string(n++), a));
        types.children.push_back(group);
    };
    // Every shipped type lists `[any]`, id 0, among its ancestors - which is
    // what makes an unauthored requirement (0) pass `ISA` for any item.
    add("any", 0, {});
    add("item", 2, {0});
    add("Weapon", 8, {2, 0});
    add("Sword", 50, {8, 2, 0});
    add("Shield", 21, {2, 0});
    file.root.children.push_back(types);
    UnitTypeHierarchy hierarchy;
    hierarchy.parse(file.root);
    return hierarchy;
}

bool near(float a, float b) { return std::fabs(a - b) < 0.0005f; }

// -- keyframe types --------------------------------------------------------

TL_TEST(keyframe_types_are_indexed_as_the_table_builds_them) {
    TL_CHECK_EQ(tl::core::keyframe_type_of("HIT"), 0);
    TL_CHECK_EQ(tl::core::keyframe_type_of("ATTACKEND"), 15);
    TL_CHECK_EQ(tl::core::keyframe_type_of("HITTWO"), 28);
    TL_CHECK_EQ(tl::core::keyframe_type_of("hittwo"), 28);  // upper-cased first
    TL_CHECK_EQ(tl::core::keyframe_type_of("FOOTSTEP"), 6);
}

TL_TEST(an_unknown_keyframe_name_is_a_hit) {
    // `getStringIndex` returns its default argument, 0, for a miss.
    TL_CHECK_EQ(tl::core::keyframe_type_of("NOT A KEY"), tl::core::kKeyframeHit);
    TL_CHECK_EQ(tl::core::keyframe_type_of(""), tl::core::kKeyframeHit);
}

// -- the model's lookups ---------------------------------------------------

TL_TEST(animation_index_ignores_case_and_misses_with_minus_one) {
    const AnimationCatalog model = alchemist_model();
    TL_CHECK_EQ(tl::core::model_animation_index(model, "SPECIAL_GEN_BUFF"), 3);
    TL_CHECK_EQ(tl::core::model_animation_index(model, "attack1"), 1);
    TL_CHECK_EQ(tl::core::model_animation_index(model, "special"), -1);  // whole name only
}

TL_TEST(random_animation_compares_the_prefix_as_passed) {
    const AnimationCatalog model = alchemist_model();
    // The model's names are upper-cased, the prefix is not.
    TL_CHECK_EQ(tl::core::model_find_random_animation(model, "rslash"), -1);
    TL_CHECK_EQ(tl::core::model_find_random_animation(model, "RSLASH"), 4);
}

TL_TEST(random_animation_draws_only_for_a_second_match) {
    const AnimationCatalog model = alchemist_model();
    tl::core::volatile_random().set_seed(4242);
    const uint64_t before = tl::core::volatile_random().state();
    // One match: no draw.
    TL_CHECK_EQ(tl::core::model_find_random_animation(model, "RSLASH"), 4);
    TL_CHECK_EQ(tl::core::volatile_random().state(), before);
    // Two matches: one draw, and either may win.
    const int index = tl::core::model_find_random_animation(model, "ATTACK");
    TL_CHECK(index == 1 || index == 2);
    TL_CHECK(tl::core::volatile_random().state() != before);
}

// -- assignSkillAnimations -------------------------------------------------

TL_TEST(no_model_gives_no_animations_and_a_second_long_run) {
    SkillProperty root;
    root.animation = "special_gen_buff";
    SkillAnimations a;
    a.loop_end = 6;
    SkillCaster caster;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.animation, -1);
    TL_CHECK_EQ(a.animation_dw, -1);
    TL_CHECK_EQ(a.loop_into, -1);
    TL_CHECK_EQ(a.dw_loop_into, -1);
    TL_CHECK_EQ(a.loop_end, 6);  // untouched
    TL_CHECK(near(a.length, 1.0f));
}

TL_TEST(the_named_animation_and_its_length) {
    const AnimationCatalog model = alchemist_model();
    SkillProperty root;
    root.animation = "special_gen_buff";
    SkillCaster caster;
    caster.model = &model;
    SkillAnimations a;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.animation, 3);
    TL_CHECK_EQ(a.animation_dw, 3);
    TL_CHECK_EQ(a.loop_into, -1);
    TL_CHECK(near(a.length, 1.2f));
}

TL_TEST(a_missing_animation_falls_back_to_rslash_when_no_attack_matches) {
    // Only `RSLASH*` is left once the model has no `ATTACK*`.
    AnimationCatalog model = {{"idle", 1.0f}, {"rslash_1", 0.7f}};
    SkillProperty root;
    root.animation = "not_on_this_rig";
    SkillCaster caster;
    caster.model = &model;
    SkillAnimations a;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.animation, 1);
    TL_CHECK(near(a.length, 0.7f));
}

TL_TEST(nothing_found_keeps_the_length_already_held) {
    AnimationCatalog model = {{"idle", 1.0f}};
    SkillProperty root;
    root.animation = "not_on_this_rig";
    SkillCaster caster;
    caster.model = &model;
    SkillAnimations a;
    a.length = 3.0f;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.animation, -1);
    TL_CHECK(near(a.length, 3.0f));
}

TL_TEST(an_unauthored_animation_leaves_the_indices_alone) {
    const AnimationCatalog model = alchemist_model();
    SkillProperty root;
    SkillCaster caster;
    caster.model = &model;
    SkillAnimations a;
    a.animation = 2;
    a.length = 0.9f;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.animation, 2);
    TL_CHECK(near(a.length, 0.9f));
}

TL_TEST(the_loop_and_its_end_are_looked_up_separately) {
    const AnimationCatalog model = alchemist_model();
    SkillProperty root;
    root.animation = "special_gen_buff";
    root.animation_loop = "special_loop";
    root.animation_loop_end = "special_loop_end";
    SkillCaster caster;
    caster.model = &model;
    SkillAnimations a;
    tl::core::assign_skill_animations(root, caster, a);
    TL_CHECK_EQ(a.loop_into, 5);
    TL_CHECK_EQ(a.dw_loop_into, 5);
    TL_CHECK_EQ(a.loop_end, 6);
}

TL_TEST(a_dual_wield_animation_needs_a_weapon_in_the_left_hand) {
    const AnimationCatalog model = alchemist_model();
    const UnitTypeHierarchy types = weapon_types();
    SkillProperty root;
    root.animation = "attack1";
    root.animation_dw = "special_loop";
    SkillCaster caster;
    caster.model = &model;
    caster.unit_types = &types;

    caster.left_hand_type = "Shield";
    SkillAnimations shielded;
    tl::core::assign_skill_animations(root, caster, shielded);
    TL_CHECK_EQ(shielded.animation_dw, 1);  // the main one's, from the first lookup
    TL_CHECK(near(shielded.length, 0.8f));

    caster.left_hand_type = "Sword";
    SkillAnimations armed;
    tl::core::assign_skill_animations(root, caster, armed);
    TL_CHECK_EQ(armed.animation, 1);
    TL_CHECK_EQ(armed.animation_dw, 5);
    TL_CHECK(near(armed.length, 2.0f));  // the longer of the two
}

// -- castSkill's animation half -------------------------------------------

TL_TEST(the_cast_rate_is_speed_floored_at_a_fifth) {
    SkillProperty root;
    TL_CHECK(near(tl::core::skill_cast_speed(nullptr, root), 1.0f));
    root.animation_speed = 1.5f;
    TL_CHECK(near(tl::core::skill_cast_speed(nullptr, root), 1.5f));
    root.animation_speed = 0.1f;
    TL_CHECK(near(tl::core::skill_cast_speed(nullptr, root), 0.2f));
}

TL_TEST(a_one_shot_plays_then_idles_and_holds_for_its_length) {
    const AnimationCatalog model = alchemist_model();
    SkillProperty root;
    root.animation = "special_gen_buff";
    root.animation_speed = 2.0f;
    SkillCaster caster;
    caster.model = &model;
    ActiveSkill active;
    active.attack_ended = true;
    active.struck = true;
    SkillRunState state;
    const SkillAnimationCue cue =
        tl::core::begin_skill_animation(active, "Ember Shield", root, caster, state);
    TL_CHECK_EQ(cue.animation, 3);
    TL_CHECK(cue.then_idle);
    TL_CHECK_EQ(cue.then_loop, -1);
    TL_CHECK(near(cue.speed, 2.0f));
    TL_CHECK(near(cue.duration, 0.6f));
    TL_CHECK(near(active.animation_left, 0.6f));  // 1.2 s at twice the rate
    TL_CHECK(near(state.speed, 2.0f));
    TL_CHECK(active.casting);
    TL_CHECK(!active.attack_ended);
    TL_CHECK(!active.struck);
    TL_CHECK_EQ(active.skill, std::string("Ember Shield"));
}

TL_TEST(a_loop_alone_loops_and_nothing_at_all_casts_nothing) {
    const AnimationCatalog model = {{"idle", 1.0f}, {"special_loop", 2.0f}};
    SkillProperty root;
    root.animation_loop = "special_loop";
    SkillCaster caster;
    caster.model = &model;
    ActiveSkill active;
    SkillRunState state;
    SkillAnimationCue cue = tl::core::begin_skill_animation(active, "Beam", root, caster, state);
    TL_CHECK_EQ(cue.animation, 1);
    TL_CHECK(near(cue.duration, -1.0f));
    TL_CHECK(active.casting);

    SkillProperty bare;
    ActiveSkill idle;
    SkillRunState fresh;
    cue = tl::core::begin_skill_animation(idle, "Nothing", bare, caster, fresh);
    TL_CHECK_EQ(cue.animation, -1);
    TL_CHECK(!idle.casting);
}

// -- updateActiveSkill -----------------------------------------------------

TL_TEST(a_one_shot_ends_its_swing_when_the_time_runs_out) {
    ActiveSkill active;
    active.skill = "Ember Shield";
    active.casting = true;
    active.animation_left = 0.5f;
    SkillAnimations a;
    a.animation = 3;
    tl::core::update_active_skill(active, a, true, 0, 100.0f, 0.3f);
    TL_CHECK(active.casting);
    TL_CHECK(!active.attack_ended);
    tl::core::update_active_skill(active, a, true, 0, 100.0f, 0.3f);
    TL_CHECK(!active.casting);
    TL_CHECK(active.attack_ended);
    TL_CHECK(near(active.animation_left, 0.0f));
}

TL_TEST(the_upkeep_is_an_integer_per_second) {
    ActiveSkill active;
    active.skill = "Beam";
    active.casting = true;
    active.animation_left = 5.0f;
    SkillAnimations a;
    a.animation = 1;
    // `MANACOSTOT` 2.5 reaches here as 2.
    const auto out = tl::core::update_active_skill(active, a, true, 2, 100.0f, 0.5f);
    TL_CHECK(near(out.mana_spent, 1.0f));
    TL_CHECK(!out.stop_all);
}

TL_TEST(running_out_of_upkeep_stops_everything) {
    ActiveSkill active;
    active.skill = "Beam";
    active.casting = true;
    active.attack_ended = true;
    active.animation_left = 5.0f;
    SkillAnimations a;
    a.loop_into = 5;
    a.loop_end = 6;
    const auto out = tl::core::update_active_skill(active, a, true, 10, 1.0f, 0.5f);
    TL_CHECK(out.stop_all);
    TL_CHECK_EQ(out.loop_end, 6);
    TL_CHECK(near(out.mana_spent, 0.0f));
    TL_CHECK(!active.casting);
}

// -- the keys ---------------------------------------------------------------

TL_TEST(keys_fire_between_two_times_and_across_a_wrap) {
    AnimationTag hit;
    hit.name = "HIT";
    hit.frame = 12.0f;  // 0.4 s
    AnimationTag end;
    end.name = "ATTACKEND";
    end.frame = 30.0f;  // 1.0 s
    const std::vector<AnimationTag> tags = {hit, end};

    TL_CHECK(tl::core::keys_crossed(tags, 3, 0.0f, 0.3f).empty());
    std::vector<FiredKey> keys = tl::core::keys_crossed(tags, 3, 0.3f, 0.45f);
    TL_CHECK_EQ(keys.size(), size_t{1});
    TL_CHECK_EQ(keys[0].animation, 3);
    TL_CHECK_EQ(keys[0].type, 0);

    // Wrapped from 1.1 back to 0.5: both the late key and the early one.
    keys = tl::core::keys_crossed(tags, 3, 1.1f, 0.5f);
    TL_CHECK_EQ(keys.size(), size_t{1});  // 1.0 < 1.1, so only the 0.4 one
    keys = tl::core::keys_crossed(tags, 3, 0.9f, 0.5f);
    TL_CHECK_EQ(keys.size(), size_t{2});

    // A clock standing on a key fires it. Frame 12 is 0.40000002 s on the
    // console (12 * 0.033333335f), so a clock standing at 0.4 is short of it.
    const float twelve = 12.0f * 0.033333335f;
    TL_CHECK_EQ(tl::core::keys_crossed(tags, 3, twelve, twelve).size(), size_t{1});
    TL_CHECK(tl::core::keys_crossed(tags, 3, 0.4f, 0.4f).empty());
}

TL_TEST(a_fired_key_names_its_tag_and_spawnparticle_is_type_4) {
    // The Alchemist's `special_fireball`: WARMUP on the root at frame 4.1, FIRE on
    // the left hand at 11.6, the missiles' HIT at 12.0.
    AnimationTag warmup;
    warmup.name = "SPAWNPARTICLE_STOP_ON_DEATH";
    warmup.frame = 4.098f;
    AnimationTag fire;
    fire.name = "SPAWNPARTICLE";
    fire.frame = 11.636f;
    AnimationTag hit;
    hit.name = "HIT";
    hit.frame = 12.013f;
    const std::vector<AnimationTag> tags = {hit, warmup, fire};

    std::vector<FiredKey> keys = tl::core::keys_crossed(tags, 7, 0.10f, 0.20f);
    TL_CHECK_EQ(keys.size(), size_t{1});
    TL_CHECK_EQ(keys[0].type, 5);
    TL_CHECK_EQ(keys[0].tag, size_t{1});

    keys = tl::core::keys_crossed(tags, 7, 0.38f, 0.41f);
    TL_CHECK_EQ(keys.size(), size_t{2});
    TL_CHECK_EQ(keys[0].tag, size_t{0});  // HIT, in the tags' order
    TL_CHECK_EQ(keys[1].type, 4);
    TL_CHECK_EQ(keys[1].tag, size_t{2});
}

TL_TEST(a_hit_on_the_skills_own_animation_fires_the_trigger_group) {
    ActiveSkill active;
    active.skill = "Ember Shield";
    active.casting = true;
    SkillAnimations a;
    a.animation = 3;
    const std::vector<int> groups =
        tl::core::update_skill_keys(active, a, true, true, {{3, 0}, {3, 28}});
    TL_CHECK_EQ(groups.size(), size_t{2});
    TL_CHECK_EQ(groups[0], tl::core::kSkillEventTrigger);
    TL_CHECK_EQ(groups[1], tl::core::kSkillEventTriggerTwo);
    TL_CHECK(active.struck);
}

TL_TEST(a_hit_on_another_animation_does_nothing_but_attackend_counts_anywhere) {
    ActiveSkill active;
    active.skill = "Ember Shield";
    active.casting = true;
    SkillAnimations a;
    a.animation = 3;
    const std::vector<int> groups =
        tl::core::update_skill_keys(active, a, true, true, {{1, 0}, {1, 15}});
    TL_CHECK(groups.empty());
    TL_CHECK(!active.struck);
    TL_CHECK(active.attack_ended);
}

TL_TEST(no_keys_for_the_dying_or_a_finished_cast) {
    ActiveSkill active;
    active.skill = "Ember Shield";
    active.casting = true;
    SkillAnimations a;
    a.animation = 3;
    TL_CHECK(tl::core::update_skill_keys(active, a, true, false, {{3, 0}}).empty());
    active.casting = false;
    active.animation_left = 0.0f;
    TL_CHECK(tl::core::update_skill_keys(active, a, true, true, {{3, 0}}).empty());
}

// -- attemptStopOfActiveSkill ---------------------------------------------

TL_TEST(letting_go_stops_a_loop_past_its_minimum_and_a_finished_swing) {
    ActiveSkill none;
    none.casting = true;
    TL_CHECK(tl::core::attempt_stop_of_active_skill(none, nullptr, true) == SkillStop::Released);
    TL_CHECK(!none.casting);

    ActiveSkill looping;
    looping.skill = "Beam";
    looping.casting = true;
    SkillAnimations loop;
    loop.loop_into = 5;
    TL_CHECK(tl::core::attempt_stop_of_active_skill(looping, &loop, false) == SkillStop::None);
    TL_CHECK(tl::core::attempt_stop_of_active_skill(looping, &loop, true) == SkillStop::StopByName);

    ActiveSkill swing;
    swing.skill = "Ember Shield";
    swing.attack_ended = true;
    SkillAnimations once;
    once.animation = 3;
    TL_CHECK(tl::core::attempt_stop_of_active_skill(swing, &once, true) == SkillStop::StopAll);
}

// -- the whole chain ---------------------------------------------------------

TL_TEST(ember_shields_affix_lands_on_its_hit_key_not_on_the_cast) {
    // `[EVENT_START]` holds only a layout; the affix is in `[EVENT_TRIGGER]`,
    // which a `HIT` key on `special_gen_buff` fires.
    AdmFile file;
    file.ok = true;
    file.root.name = "SKILL";
    file.root.records.push_back(str("NAME", "Ember Shield"));
    file.root.records.push_back(str("ACTIVATION_TYPE", "NORMAL"));
    file.root.records.push_back(str("TARGET_TYPE", "SELF"));
    file.root.records.push_back(str("ANIMATION", "special_gen_buff"));
    AdmSection level;
    level.name = "LEVEL1";
    level.records.push_back(num("MANACOST", 12));
    AdmSection start;
    start.name = "EVENT_START";
    start.records.push_back(str("FILE", "warmup.layout"));
    level.children.push_back(start);
    AdmSection trigger;
    trigger.name = "EVENT_TRIGGER";
    AdmSection affixes;
    affixes.name = "AFFIXES";
    affixes.records.push_back(num("AFFIXLEVEL", 5));
    affixes.records.push_back(str("TARGET", "SELF"));
    affixes.records.push_back(str("AFFIX", "EMBERSHIELD"));
    trigger.children.push_back(affixes);
    level.children.push_back(trigger);
    file.root.children.push_back(level);
    const SkillDef def = tl::core::parse_skill_def(file);
    KnownSkill skill;
    skill.entry.name = def.name;
    skill.def = &def;
    skill.invested = 1;
    skill.effective = 1;
    const auto lookup = [&skill](const std::string& name) -> const KnownSkill* {
        return name == skill.entry.name ? &skill : nullptr;
    };

    AdmFile list;
    list.ok = true;
    list.root.name = "EFFECTLIST";
    tl::core::EffectTypeTable types;
    types.load(list);
    tl::core::Affix shield;
    shield.name = "EMBERSHIELD";
    tl::core::EffectManager effects;
    SkillCastContext context;
    context.target = &effects;
    context.types = &types;
    context.affix = [&shield](const std::string& name) -> const tl::core::Affix* {
        return name == "EMBERSHIELD" ? &shield : nullptr;
    };

    const AnimationCatalog model = alchemist_model();
    SkillCaster caster;
    caster.mana = 100.0f;
    caster.model = &model;

    SkillRunner runner;
    TL_CHECK_EQ(static_cast<int>(runner.cast(skill, caster, context)),
                static_cast<int>(SkillCastResult::Cast));
    TL_CHECK(effects.affixes().empty());  // nothing from `EVENT_START`
    SkillRunState* state = runner.state("Ember Shield");
    TL_CHECK(state != nullptr);
    TL_CHECK_EQ(state->animations.animation, 3);
    TL_CHECK(near(state->minimum_time_remaining, 1.2f));  // the run waits for the animation

    const SkillProperty* root = &def.base;
    TL_CHECK(root != nullptr);
    ActiveSkill active;
    const SkillAnimationCue cue =
        tl::core::begin_skill_animation(active, "Ember Shield", *root, caster, *state);
    TL_CHECK_EQ(cue.animation, 3);

    AnimationTag hit;
    hit.name = "HIT";
    hit.frame = 12.0f;
    const std::vector<AnimationTag> tags = {hit};
    const auto step = [&](float from, float to) {
        const std::vector<int> groups = tl::core::update_skill_keys(
            active, state->animations, runner.is_running("Ember Shield"), true,
            tl::core::keys_crossed(tags, cue.animation, from, to));
        for (int group : groups) runner.trigger_event(group, context, lookup);
    };
    step(0.0f, 0.3f);
    TL_CHECK(effects.affixes().empty());
    step(0.3f, 0.45f);
    TL_CHECK_EQ(effects.affixes().size(), size_t{1});
    TL_CHECK(active.struck);
}

}  // namespace

// `CCharacter::update`: a one-shot cast holds the character while its animation
// runs, unless `MOVES_TO_TARGET`; `ATTACKEND` lets it go early.
TL_TEST(a_cast_stops_the_character_until_its_animation_lets_go) {
    ActiveSkill active;
    SkillAnimations oneshot;  // loop_into -1: a one-shot
    TL_CHECK(!tl::core::skill_halts_motion(active, oneshot, false, false));
    active.skill = "Ember Bolt";
    active.casting = true;
    active.animation_left = 0.6f;
    TL_CHECK(tl::core::skill_halts_motion(active, oneshot, false, false));
    TL_CHECK(!tl::core::skill_halts_motion(active, oneshot, false, true));  // MOVES_TO_TARGET
    active.casting = false;
    TL_CHECK(tl::core::skill_halts_motion(active, oneshot, false, false));  // time left
    active.attack_ended = true;
    TL_CHECK(!tl::core::skill_halts_motion(active, oneshot, false, false));
    active.attack_ended = false;
    active.animation_left = 0.0f;
    TL_CHECK(!tl::core::skill_halts_motion(active, oneshot, false, false));
    // A looping skill holds only while it runs.
    SkillAnimations looping;
    looping.loop_into = 4;
    active.casting = true;
    TL_CHECK(!tl::core::skill_halts_motion(active, looping, false, false));
    TL_CHECK(tl::core::skill_halts_motion(active, looping, true, false));
}

// X360 `CCharacter::update`: how a player in a skill turns, at its
// aim or its target.
TL_TEST(the_console_turns_a_player_in_a_skill_at_its_target_or_its_aim) {
    using tl::core::player_skill_turn;
    using tl::core::PlayerSkillTurn;
    ActiveSkill active;
    SkillAnimations oneshot;
    TL_CHECK(player_skill_turn(active, oneshot, false, true, false, 30.0f, true) ==
             PlayerSkillTurn::None);
    active.skill = "Ember Lightning";
    active.casting = true;
    active.animation_left = 0.5f;
    // Ember Lightning: `FINDTARGETANGLE` 30, turning allowed.
    TL_CHECK(player_skill_turn(active, oneshot, false, true, false, 30.0f, true) ==
             PlayerSkillTurn::Target);
    TL_CHECK(player_skill_turn(active, oneshot, false, true, false, 30.0f, false) ==
             PlayerSkillTurn::Point);
    TL_CHECK(player_skill_turn(active, oneshot, false, true, false, 0.0f, true) ==
             PlayerSkillTurn::Point);
    TL_CHECK(player_skill_turn(active, oneshot, false, true, true, 30.0f, true) ==
             PlayerSkillTurn::None);
    // `ALLOWS_TURNING:False`: along the aim while 0.1 s or more is left, even
    // for `MOVES_TO_TARGET`; under 0.1 s the target again.
    TL_CHECK(player_skill_turn(active, oneshot, false, false, false, 30.0f, true) ==
             PlayerSkillTurn::Aim);
    TL_CHECK(player_skill_turn(active, oneshot, false, false, true, 30.0f, true) ==
             PlayerSkillTurn::Aim);
    active.animation_left = 0.05f;
    TL_CHECK(player_skill_turn(active, oneshot, false, false, false, 30.0f, true) ==
             PlayerSkillTurn::Target);
    // A looping skill that no longer runs is not held.
    SkillAnimations looping;
    looping.loop_into = 4;
    TL_CHECK(player_skill_turn(active, looping, false, true, false, 30.0f, true) ==
             PlayerSkillTurn::None);
    // Out of a skill, the attack button held (`+0x1fe`): towards the aim point.
    TL_CHECK(player_skill_turn(active, looping, false, true, false, 30.0f, true, true) ==
             PlayerSkillTurn::Point);
    TL_CHECK(player_skill_turn(ActiveSkill{}, oneshot, false, true, false, 0.0f, false, true) ==
             PlayerSkillTurn::Point);
}
