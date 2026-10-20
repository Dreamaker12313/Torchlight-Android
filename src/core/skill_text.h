// What the console's skill screen prints about a skill: its description boxes, type
// caption and stat-bonus row, from the Xbox 360 builder, which differs from
// the PC tooltip (`CSkillTooltip::showTooltip`). The text carries `|cAARRGGBB ... |u`
// colour runs. No OGRE and no file I/O.
#pragma once

#include "core/effects.h"
#include "core/skills.h"
#include "core/unit_types.h"

#include <array>
#include <string>

namespace tl::core {

/// The type caption: `Class Skill`, `Offensive Spell`,
/// `Defensive Spell`, `Charm Spell`.
std::string skill_type_caption(SkillType type);

/// `getWeaponString(EUNITTYPES)`: what a
/// weapon requirement's unit type is called.
std::string weapon_string(const UnitTypeHierarchy& types, uint32_t type);

/// The weapon requirement line (`CSkill::getDescriptionRequirments`) for
/// the property in use (`KnownSkill::current_property`), not the described level's.
/// Empty when it requires nothing.
std::string skill_requirement_text(const SkillProperty& property, const UnitTypeHierarchy& types);

/// The description of `skill` at `level`: 0 or -1 for the
/// level it runs at now, else that level. `effects` carries what the stat lines
/// need - the effect-type table, the curves, the owner's stats and the affix
/// lookup; with an empty one the stat lines are simply absent.
std::string skill_description_text(const KnownSkill& skill, int level,
                                   const UnitTypeHierarchy& types,
                                   const EffectDisplayContext& effects);

/// `CSkillProperty::getSkillStats`: the
/// lines a level's events, affixes and effects print.
std::string skill_stats_text(const SkillProperty& property, const EffectDisplayContext& effects);

/// `CSkill::fillOutStatBonuses` through `CSkillProperty::fillOutStatBonuses`,
/// the four the screen shows in its order: strength,
/// dexterity, magic, defense (indices 0, 1, 3, 2).
std::array<float, kSkillStatBonusCount> skill_stat_bonuses(const KnownSkill& skill,
                                                           const EffectDisplayContext& effects);

/// The same, unreduced: the `float[6]` the original fills, indexed by
/// `EffectStat`.
std::array<float, kEffectStatCount> skill_stat_bonuses_all(const KnownSkill& skill,
                                                           const EffectDisplayContext& effects);

}  // namespace tl::core
