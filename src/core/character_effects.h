// A character's effect totals: `CCharacter::getEffectValue` and its three
// siblings, its own `CEffectManager` plus its inventory's (`core/inventory.h`).
// Converts implicitly from a bare `const EffectManager*`: a character with no
// inventory.
#pragma once

#include "core/effect_manager.h"

#include <cstddef>
#include <string>

namespace tl::core {

class Inventory;
struct RequirementReductions;

class CharacterEffects {
public:
    CharacterEffects() = default;
    CharacterEffects(std::nullptr_t) {}
    CharacterEffects(const EffectManager* own) : own_(own) {}
    CharacterEffects(const EffectManager& own) : own_(&own) {}
    CharacterEffects(const EffectManager* own, const Inventory* inventory)
        : own_(own), inventory_(inventory) {}

    // PORT: an accessor of the port's own bookkeeping.
    const EffectManager* own() const { return own_; }
    // PORT: an accessor of the port's own bookkeeping.
    const Inventory* inventory() const { return inventory_; }
    /// Neither half: every total is 0.
    explicit operator bool() const { return own_ != nullptr || inventory_ != nullptr; }

    /// `CCharacter::getEffectValue(EEFFECT_TYPE, EDAMAGE_TYPES)`:
    /// the own manager's total
    /// (`CEffectManager::getEffectValue`) plus the inventory's cached
    /// one (`CInventory::getEffectValue`), each half
    /// 0 when it is missing.
    float effect_value(int type, int damage_type = kDamageTypeAny) const;
    /// `CCharacter::getEffectValue(EEFFECT_TYPE, float, const std::wstring&)`:
    /// the by-name totals.
    ///
    float effect_value(int type, const std::string& name) const;
    /// `CCharacter::getEffectValue(EEFFECT_TYPE, EEQUIP_LOCATIONS,
    /// EDAMAGE_TYPES)`, which `maxDamage`
    /// asks: the own total plus `CInventory::getEffectValueMinusEquipmentSlot`.
    ///
    float effect_value_minus_location(int type, int location, int damage_type) const;
    /// `CCharacter::getEffectValueWithoutInventory`.
    float effect_value_without_inventory(int type, int damage_type = kDamageTypeAny) const;

private:
    const EffectManager* own_ = nullptr;
    const Inventory* inventory_ = nullptr;
};

/// The six totals the requirement getters subtract
/// (`CEquipment::getLevelRequirement` & co.), each
/// `CCharacter::getEffectValue(type, 7)`.
RequirementReductions requirement_reductions(const CharacterEffects& effects);

/// `CCharacter::walkingSpeed` and
/// `runningSpeed`, one body over `+0x28c` and
/// `+0x290`: the unit's speed with `PERCENT SPEED` (0x15) on it. A slowing is
/// first cut by effect 0x8c, a percentage held between 0 and 100. Never below
/// 0.
float character_speed(float base, const CharacterEffects& effects);
/// Its arithmetic, pure: `pct` effect 0x15, `kept` effect 0x8c, as the effects
/// give them.
float running_speed(float base, float pct, float kept);

}  // namespace tl::core
