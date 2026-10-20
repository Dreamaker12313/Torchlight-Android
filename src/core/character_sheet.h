// The stats screen's damage figures: `CCharacter::maximumDamageForDisplay`
// and `CCharacter::minimumDamageForDisplay`,
// and the rows `CStatsMenu::update`
// builds out of them.
#pragma once

#include "core/character.h"
#include "core/character_effects.h"
#include "core/effect_manager.h"
#include "core/units.h"

#include <vector>

namespace tl::core {

class Inventory;
class UnitTypeHierarchy;

/// One entry of an item copy's damage-type list, in the order
/// `addInherentDamage` first met them.
struct WeaponDamageBreakdown {
    int damage_type = 0;  ///< `+0x350[i]`, `EDAMAGE_TYPES`
    int inherent = 0;     ///< `+0x380[i]`, `ItemStats::inherent_damage`
    int socket = 0;       ///< `+0x368[i]`, `ItemStats::damage_bonus`
};

/// What the display function asks of the item in one hand.
struct WeaponInputs {
    bool equipped = false;
    bool is_weapon = false;           ///< `ISA(8)`, Weapon
    bool is_ranged = false;           ///< `ISA(0x23)`, Ranged Weapon
    bool is_shield = false;           ///< `ISA(0x15)`, Shield
    bool is_martial = false;          ///< `ISA(0xa2)`, ItemCategoryMartial
    bool is_ranged_category = false;  ///< `ISA(0xa3)`, ItemCategoryRanged
    bool is_magic_item = false;       ///< `ISA(0xa4)`, ItemCategoryMagic
    int max_damage = 0;               ///< `CEquipment::maximumDamage()`, `+0x334`
    std::vector<WeaponDamageBreakdown> damage_types;

    /// The item's answers, the unit-type tests through `unit_types`
    /// (`CBaseUnit::ISA`). Null `item`: an empty hand.
    static WeaponInputs from_item(const ItemStats* item, const UnitTypeHierarchy& unit_types,
                                  const StatBook* graphs = nullptr);
};

/// The character's own attack, for the bare-handed path.
struct AttackInputs {
    bool has_attack = true;                  ///< `+0x2c0` set (after `selectAttack(0)`)
    int damage_by_type[kDamageTypeCount]{};  ///< `CAttackDescription +0x30[type]`, maximum
};

/// Everything `maximumDamageForDisplay` reads off the character.
struct DisplayInputs {
    const CharacterProgression* progression = nullptr;
    /// The character's total (`CCharacter::getEffectValue`).
    ///
    CharacterEffects effects;
    WeaponInputs left_hand;   ///< equip location 1
    WeaponInputs right_hand;  ///< equip location 0
    AttackInputs attack;
    /// `+0x3ec`, the weapon in use, is set.
    bool weapon_in_use = false;
    /// `+0x5c8` (the base level), compared with the level (`+0xf0`) on the
    /// no-weapon path: 1 for every character (`make_display_inputs`).
    int base_level = 1;
    /// `+0x5a4` (the master): a pet's master's totals, for its
    /// `PERCENT PET DAMAGE`; null for anyone else.
    const CharacterEffects* master = nullptr;
};

/// The inputs for the player: both hands from `inventory`, `weapon_in_use` as
/// the definition says, and `own_attack` the character's own first attack (the
/// one `setLevel(LEVEL, true)` made at creation, `own_attack` in `core/combat`;
/// null for none). The effects are the character's own manager
/// (`effects.own()`) and `inventory`'s totals.
DisplayInputs make_display_inputs(const CharacterProgression& progression,
                                  const Inventory& inventory, const UnitTypeHierarchy& unit_types,
                                  CharacterEffects effects, const StatBook* graphs,
                                  const AttackDescription* own_attack);

/// `CCharacter::maximumDamageForDisplay(bool, bool, bool)`.
/// `melee` counts the hands whose
/// `ISA(0x23)` differs from it; `skip_base_damage` leaves out `+0x308`, the
/// physical entries and physical `DAMAGE BONUS`; `skip_effects` leaves out
/// every percentage. Every percentage is taken by 0.01 and fused as the
/// console does.
int maximum_damage_for_display(const DisplayInputs& in, bool melee, bool skip_base_damage = false,
                               bool skip_effects = false);

/// `CCharacter::minimumDamageForDisplay`:
/// `ceilf(maximum * 0.5f)`, 0 when the character has no attack at all.
int minimum_damage_for_display(const DisplayInputs& in, bool melee, bool skip_base_damage = false,
                               bool skip_effects = false);

/// The four figures `CStatsMenu::update`
/// prints: melee (1,0,0), ranged (0,0,0), magic the larger of (0,1,0) and
/// (1,1,0), and `AC()`.
CombatSheet combat_sheet(const DisplayInputs& in);

}  // namespace tl::core
