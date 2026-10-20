// Whether a skill's blocks may go on a unit:
// CSkillEffectAndAffixes::canAffixesAndEffectsBeAppliedToUnit and
// CSkillProperty's level walk, as CEquipment::canUseOnTarget asks.
#include "core/skill_cast.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::Effect;
using tl::core::EffectTypeTable;
using tl::core::SkillApplyFacts;
using tl::core::SkillEffectAndAffixes;
using tl::core::SkillTargetFilter;
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

/// A unit of any type, with a user that is not it and no other relation.
SkillApplyFacts any_unit() {
    SkillApplyFacts f;
    f.isa = [](int32_t) { return true; };
    f.effect_valid = [](const Effect&) { return true; };
    return f;
}

SkillEffectAndAffixes one_group(SkillTargetFilter target) {
    SkillEffectAndAffixes block;
    SkillEffectAndAffixes::EffectGroup group;
    group.target_type = 0;
    group.target = target;
    group.effects.push_back(Effect{});
    block.effect_groups.push_back(group);
    return block;
}

SkillEffectAndAffixes one_affix(SkillTargetFilter target) {
    SkillEffectAndAffixes block;
    SkillEffectAndAffixes::AffixSpec spec;
    spec.name = "A";
    spec.target_type = 0;
    spec.target = target;
    block.affix_specs.push_back(spec);
    return block;
}

}  // namespace

// `ENEMY` wants a user, and one of another alignment when the unit is a
// character; `EVERYBODY` and `FRIEND` are not tested at all.
TL_TEST(skill_block_enemy_and_untested_targets) {
    SkillApplyFacts f = any_unit();
    f.same_alignment = true;
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Enemy), f));
    f.same_alignment = false;
    TL_CHECK(tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Enemy), f));
    f.has_user = false;
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Enemy), f));
    f.same_alignment = true;
    TL_CHECK(tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Everybody), f));
    TL_CHECK(tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Friend), f));
}

// `SELF`: a character other than the user is refused; with no user, an effect
// group lets a non-character by where an affix refuses it.
TL_TEST(skill_block_self_differs_between_effects_and_affixes) {
    SkillApplyFacts f = any_unit();
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Self), f));
    f.unit_is_user = true;
    TL_CHECK(tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Self), f));
    TL_CHECK(tl::core::skill_block_can_apply(one_affix(SkillTargetFilter::Self), f));
    f.unit_is_user = false;
    f.unit_is_character = false;
    f.has_user = false;
    TL_CHECK(tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Self), f));
    TL_CHECK(!tl::core::skill_block_can_apply(one_affix(SkillTargetFilter::Self), f));
}

// `PET`: the user's own pet only.
TL_TEST(skill_block_pet_wants_the_users_pet) {
    SkillApplyFacts f = any_unit();
    TL_CHECK(!tl::core::skill_block_can_apply(one_affix(SkillTargetFilter::Pet), f));
    f.master_is_user = true;
    TL_CHECK(tl::core::skill_block_can_apply(one_affix(SkillTargetFilter::Pet), f));
    f.unit_is_character = false;
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Pet), f));
}

// A type the unit is not, a unit with no hierarchy, and an effect the unit
// does not take each refuse.
TL_TEST(skill_block_type_hierarchy_and_effect) {
    SkillApplyFacts f = any_unit();
    f.isa = [](int32_t type) { return type == 0; };
    SkillEffectAndAffixes block = one_affix(SkillTargetFilter::Everybody);
    TL_CHECK(tl::core::skill_block_can_apply(block, f));
    block.affix_specs[0].target_type = 7;
    TL_CHECK(!tl::core::skill_block_can_apply(block, f));
    f.isa = nullptr;
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Everybody), f));
    f = any_unit();
    f.effect_valid = [](const Effect&) { return false; };
    TL_CHECK(!tl::core::skill_block_can_apply(one_group(SkillTargetFilter::Everybody), f));
    // The affixes' effects are not asked about.
    TL_CHECK(tl::core::skill_block_can_apply(one_affix(SkillTargetFilter::Everybody), f));
}

// A level with no blocks anywhere passes; a level's own block answers alone,
// before its events; otherwise every event's block is asked.
TL_TEST(skill_property_walks_its_events) {
    const EffectTypeTable types;
    SkillApplyFacts f = any_unit();
    f.effect_valid = [](const Effect&) { return false; };

    AdmSection level;
    level.name = "LEVEL1";
    AdmSection start;
    start.name = "EVENT_START";
    start.records.push_back(str("FILE", "media/skills/x.layout"));
    level.children.push_back(start);
    TL_CHECK(tl::core::skill_property_can_apply(level, types, SkillTargetFilter::Enemy, f));

    // An event with an effect the unit refuses.
    AdmSection effects;
    effects.name = "EFFECTS";
    effects.records.push_back(str("TARGET", "EVERYBODY"));
    AdmSection effect;
    effect.name = "EFFECT";
    effect.records.push_back(str("NAME", "Strange Land"));
    effect.records.push_back(str("TYPE", "WARP"));
    effects.children.push_back(effect);
    AdmSection trigger;
    trigger.name = "EVENT_TRIGGER";
    trigger.children.push_back(effects);
    level.children.push_back(trigger);
    TL_CHECK(!tl::core::skill_property_can_apply(level, types, SkillTargetFilter::Enemy, f));
    f.effect_valid = [](const Effect&) { return true; };
    TL_CHECK(tl::core::skill_property_can_apply(level, types, SkillTargetFilter::Enemy, f));

    // The level's own `[AFFIXES]` answers instead: a `PET` one refuses a unit
    // that is nobody's pet, whatever the events say.
    AdmSection affixes;
    affixes.name = "AFFIXES";
    affixes.records.push_back(str("AFFIX", "A"));
    affixes.records.push_back(str("TARGET", "PET"));
    level.children.push_back(affixes);
    TL_CHECK(!tl::core::skill_property_can_apply(level, types, SkillTargetFilter::Enemy, f));
}
