// What the console's skill screen prints (`core/skill_text.h`): the type
// caption, the weapon requirement line and the description box, with the
// X360 builder's colour runs and spacing kept exactly.
#include "core/skill_text.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::EffectDisplayContext;
using tl::core::KnownSkill;
using tl::core::SkillDef;
using tl::core::SkillProperty;
using tl::core::SkillType;
using tl::core::UnitTypeHierarchy;

tl::formats::AdmSection type(const std::string& name, uint32_t id,
                             const std::vector<uint32_t>& ancestors) {
    tl::formats::AdmSection section;
    section.name = name;
    tl::formats::AdmRecord key;
    key.key = "ID";
    key.type = tl::formats::AdmType::Integer;
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        tl::formats::AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.type = tl::formats::AdmType::Integer;
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

/// `getWeaponString`'s four ids (0x3c melee, 0x23 ranged, 9 one-handed, 10
/// two-handed) and one weapon of each kind it names.
UnitTypeHierarchy weapon_types() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";
    types.children = {
        type("any", 0, {}),
        type("OneHanded", 9, {0}),
        type("TwoHanded", 10, {0}),
        type("Ranged", 0x23, {0}),
        type("Melee", 0x3c, {0}),
        type("Sword", 100, {0x3c, 9, 0}),
        type("GreatAxe", 101, {0x3c, 10, 0}),
        type("Claw", 102, {0x3c, 0}),
        type("Pistol", 103, {0x23, 9, 0}),
        type("Bow", 104, {0x23, 0}),
        type("Rifle", 105, {0x23, 10, 0}),
        type("Shield", 106, {0}),
    };
    root.children.push_back(types);
    UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

KnownSkill known(const SkillDef& def, uint32_t effective) {
    KnownSkill skill;
    skill.def = &def;
    skill.invested = effective;
    skill.effective = effective;
    return skill;
}

}  // namespace

TL_TEST(skill_type_captions_follow_the_string_table) {
    TL_CHECK_EQ(tl::core::skill_type_caption(SkillType::Skill), std::string("Class Skill"));
    TL_CHECK_EQ(tl::core::skill_type_caption(SkillType::Offensive), std::string("Offensive Spell"));
    TL_CHECK_EQ(tl::core::skill_type_caption(SkillType::Defensive), std::string("Defensive Spell"));
    TL_CHECK_EQ(tl::core::skill_type_caption(SkillType::Charm), std::string("Charm Spell"));
}

// `getWeaponString`: melee before ranged, one-handed before
// two-handed, and "any" for a type that is neither.
TL_TEST(weapon_string_names_each_weapon_class) {
    const UnitTypeHierarchy t = weapon_types();
    TL_CHECK_EQ(tl::core::weapon_string(t, 100), std::string("single handed melee"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 101), std::string("two-handed melee"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 102), std::string("melee"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 103), std::string("single handed ranged"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 104), std::string("ranged"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 105), std::string("two-handed ranged"));
    TL_CHECK_EQ(tl::core::weapon_string(t, 106), std::string("any"));
}

// the label and ": " are two strings joined, hence "requires : ". Both hands
// the same under `DUELWIELD_REQUIRED` is the plural line; otherwise the right hand's alone.
TL_TEST(skill_requirement_text_reads_the_property_it_is_given) {
    const UnitTypeHierarchy t = weapon_types();
    SkillDef def;
    TL_CHECK_EQ(tl::core::skill_requirement_text(def.base, t), std::string());

    def.base.requirement_right = "SWORD";
    TL_CHECK_EQ(tl::core::skill_requirement_text(def.base, t),
                std::string("Right hand requires : single handed melee Weapon"));

    def.base.requirement_left = "sword";
    def.base.dual_wield_required = true;
    TL_CHECK_EQ(
        tl::core::skill_requirement_text(def.base, t),
        std::string("Dual wield only\nRight and left hands require : single handed melee Weapons"));

    def.base.requirement_left = "Claw";
    TL_CHECK_EQ(tl::core::skill_requirement_text(def.base, t),
                std::string("Dual wield only\nRight hand requires : single handed melee Weapon"));
}

// the cooldown to one decimal, then the description and mana cost ("%g"), each
// in the lilac run `|cFF9180FF`; no `[LEVELn]` block and no parsed section, no stat lines.
TL_TEST(skill_description_text_prints_cooldown_then_description_and_mana) {
    const UnitTypeHierarchy t = weapon_types();
    const EffectDisplayContext effects;
    SkillDef def;
    def.base.cooldown = 2.5f;
    def.base.mana_cost = 12.0f;
    def.base.description = "Hurls a bolt.";
    TL_CHECK_EQ(tl::core::skill_description_text(known(def, 1), 0, t, effects),
                std::string("|cFF9180FFCooldown: 2.5 seconds|u \n"
                            "Hurls a bolt.\n|cFF9180FFMana Cost: 12|u\n"));

    // No mana up front, no description: a passive prints its cooldown only.
    def.base.mana_cost = 0.0f;
    def.base.cooldown = 0.34f;  // "%.1f": 0.3
    TL_CHECK_EQ(tl::core::skill_description_text(known(def, 1), 0, t, effects),
                std::string("|cFF9180FFCooldown: 0.3 seconds|u \n"));

    // Nothing at all still ends the box with a newline.
    SkillDef bare;
    TL_CHECK_EQ(tl::core::skill_description_text(known(bare, 1), 0, t, effects), std::string("\n"));
    KnownSkill unknown;
    TL_CHECK_EQ(tl::core::skill_description_text(unknown, 0, t, effects), std::string());
}

// Which `[LEVELn]` the box reads: 0 and -1 mean the level the skill runs at;
// a level past the authored ones falls back to `[LEVEL1]`.
TL_TEST(skill_description_text_picks_the_level_block) {
    const UnitTypeHierarchy t = weapon_types();
    const EffectDisplayContext effects;
    SkillDef def;
    def.levels.resize(2);
    def.levels[0].mana_cost = 5.0f;
    def.levels[0].description = "one";
    def.levels[1].mana_cost = 7.5f;
    def.levels[1].description = "two";
    const KnownSkill skill = known(def, 2);
    const std::string two = "two\n|cFF9180FFMana Cost: 7.5|u\n";
    const std::string one = "one\n|cFF9180FFMana Cost: 5|u\n";
    TL_CHECK_EQ(tl::core::skill_description_text(skill, 0, t, effects), two);
    TL_CHECK_EQ(tl::core::skill_description_text(skill, -1, t, effects), two);
    TL_CHECK_EQ(tl::core::skill_description_text(skill, 1, t, effects), one);
    TL_CHECK_EQ(tl::core::skill_description_text(skill, 2, t, effects), two);
    TL_CHECK_EQ(tl::core::skill_description_text(skill, 5, t, effects), one);
}
