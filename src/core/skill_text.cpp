#include "core/skill_text.h"

#include <cstdio>

namespace tl::core {
namespace {

/// The screen's one colour for everything that is not a stat: `|cFF9180FF`,
/// a lilac.
constexpr const char* kLilac = "|cFF9180FF";

/// `std::basic_ostream << float` as writes it: the stream's
/// defaults, six significant digits with no trailing zeros.
std::string stream_number(float value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
    return buf;
}

/// Which `[LEVELn]` the description reads, as finds it: 0 or -1 is
/// the level the skill runs at; one past the authored levels, or none, is the
/// first level (or the root when there are no levels).
const SkillProperty* level_property(const KnownSkill& skill, int level) {
    if (skill.def == nullptr) return nullptr;
    uint32_t wanted = static_cast<uint32_t>(level);
    if (level == -1 || level == 0) wanted = skill.effective;
    const uint32_t index = wanted - 1;
    const uint32_t count = static_cast<uint32_t>(skill.def->levels.size());
    const uint32_t bound = count == 0 ? 1 : count;
    const SkillProperty* property = count == 0 ? &skill.def->base : &skill.def->levels[0];
    if (index <= bound && index < count) property = &skill.def->levels[index];
    return property;
}

}  // namespace

std::string skill_type_caption(SkillType type) {
    // `CSkill::getSkillTypeDisplayName`,
    // through the string table (`CStringTranslate::getTranslateString`).
    switch (type) {
        case SkillType::Offensive: return "Offensive Spell";
        case SkillType::Defensive: return "Defensive Spell";
        case SkillType::Skill: return "Class Skill";
        case SkillType::Charm: return "Charm Spell";
    }
    return std::string();
}

std::string weapon_string(const UnitTypeHierarchy& types, uint32_t type) {
    // `getWeaponString`. `EUNITTYPES`: 0x3c melee, 0x23
    // ranged, 0xa3 `ITEMCATEGORYRANGED` (tested beside 0x23), 9 one-handed, 10
    // two-handed.
    constexpr uint32_t kMelee = 0x3c;
    constexpr uint32_t kRanged = 0x23;
    constexpr uint32_t kRangedCategory = 0xa3;
    constexpr uint32_t kOneHanded = 9;
    constexpr uint32_t kTwoHanded = 10;
    if (!types.isa(type, kMelee)) {
        if (!types.isa(type, kRanged) && !types.isa(type, kRangedCategory)) return "any";
        if (types.isa(type, kOneHanded)) return "single handed ranged";
        if (!types.isa(type, kTwoHanded)) return "ranged";
        return "two-handed ranged";
    }
    if (types.isa(type, kOneHanded)) return "single handed melee";
    if (!types.isa(type, kTwoHanded)) return "melee";
    return "two-handed melee";
}

std::string skill_requirement_text(const SkillProperty& root, const UnitTypeHierarchy& types) {
    // `CSkill::getDescriptionRequirments`,
    // on the property in use (`+0x5c`): +0x10 `DUELWIELD_REQUIRED`,
    // +8 the right hand's unit type, +0xc the left's.
    const uint32_t none = 0;
    const uint32_t right =
        root.requirement_right.empty() ? none : types.id_of(root.requirement_right);
    const uint32_t left = root.requirement_left.empty() ? none : types.id_of(root.requirement_left);
    const bool dual = root.dual_wield_required;
    std::string out;
    if (!dual && right == none && left == none) return out;
    if (dual) out = std::string("Dual wield only") + "\n";
    if (!dual || right == none || left != right) {
        out += std::string("Right hand requires ") + ": " + weapon_string(types, right) + " " +
               "Weapon";
    } else {
        out += std::string("Right and left hands require ") + ": " + weapon_string(types, right) +
               " " + "Weapons";
    }
    return out;
}

std::string skill_description_text(const KnownSkill& skill, int level,
                                   const UnitTypeHierarchy& types,
                                   const EffectDisplayContext& effects) {
    // `CSkill::getDescription`
    if (skill.def == nullptr) return std::string();
    const SkillProperty* property = level_property(skill, level);
    if (property == nullptr) return std::string();

    std::string text;
    // the level's cooldown in seconds (`+0x6c`; the monster one,
    // `+0x70`, only for an owner that is not a player).
    const float cooldown = property->cooldown;
    if (0.0f < cooldown) {
        char seconds[32];
        std::snprintf(seconds, sizeof(seconds), "%.1f", static_cast<double>(cooldown));
        text = std::string(kLilac) + "Cooldown" + ": " + seconds + " " + "seconds" + "|u \n";
    }
    // `+0x4c`, `MANACOST`. The description (`+0x120`) goes out with it and only
    // with it: a skill whose level costs no mana up front - a passive, or one
    // that drains over time (`MANACOSTOT`) - prints no description here.
    const float mana = property->mana_cost;
    if (0.0f < mana) {
        text = text + property->description + "\n" + kLilac + "Mana Cost" + ": " +
               stream_number(mana) + "|u\n";
    }
    // `getDescriptionRequirments` answers only for an owner that is a
    // character (its `__dynamic_cast`): no owner, no line.
    const SkillProperty* in_use = skill.current_property();
    const std::string requirement = in_use != nullptr && effects.owner.present
                                        ? skill_requirement_text(*in_use, types)
                                        : std::string();
    if (!requirement.empty()) text += std::string(kLilac) + requirement + "|u \n";
    text += skill_stats_text(*property, effects);
    if (text.empty() || text.back() != '\n') text += "\n";
    return text;
}

}  // namespace tl::core
