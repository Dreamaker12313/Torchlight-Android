// A unit's figures off its merged data group, as `CCharacter::unitInit`, `CMonster::unitInit` and
// `CCharacter::loadModel` read them.
#pragma once

#include "core/units.h"
#include "core/weapons.h"
#include "formats/adm.h"

#include <functional>
#include <optional>
#include <string>

namespace tl::core {

class UnitTypeHierarchy;
class EffectTypeTable;

/// What the reader reaches outside the unit's file for.
struct UnitStatsSources {
    /// `media/UNITTYPES.HIE`, for the AI stat watchers' unit types.
    const UnitTypeHierarchy* unit_types = nullptr;
    /// `media/EFFECTSLIST.DAT`, for the unit's own `[EFFECT]`s.
    const EffectTypeTable* effect_types = nullptr;
    /// A `[SKILL]` by lower-cased name, as the attack it plays; null for one not known.
    std::function<const AttackSkill*(const std::string& lowered_name)> skill;
    /// The `UNITTYPE` of an item by name; none for an item not known.
    std::function<std::optional<std::string>(const std::string& item)> item_unittype;
    /// Whether the unit's model directory ships an attack animation of a class.
    std::function<bool(const std::string& directory, const std::string& animation, bool armed)>
        has_animation;
    /// `CEquipment::calculateCombatStats` for the weapon in a hand.
    std::function<UnitStats::WeaponAttack(const std::string& item, Hand hand)> weapon_attack;
};

/// The unit's figures off `file`, its merged data group (`read_unit_file`); `name` is its `NAME`.
/// A file that did not read gives the defaults (and the `SPAWN` entrance).
UnitStats read_unit_stats(const tl::formats::AdmFile& file, const std::string& name,
                          const UnitStatsSources& sources);

}  // namespace tl::core
