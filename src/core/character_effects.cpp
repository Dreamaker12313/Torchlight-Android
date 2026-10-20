// A character's effect totals. See `character_effects.h`.
#include "core/character_effects.h"

#include "core/inventory.h"
#include "core/item_roll.h"

#include <cmath>

namespace tl::core {

float CharacterEffects::effect_value(int type, int damage_type) const {
    // `CCharacter::getEffectValue(EEFFECT_TYPE, EDAMAGE_TYPES)`.
    //
    if (own_ == nullptr) {
        return inventory_ != nullptr ? inventory_->effect_value(type, damage_type) : 0.0f;
    }
    if (inventory_ == nullptr) return own_->effect_value(type, damage_type);
    const float own = own_->effect_value(type, damage_type);
    return inventory_->effect_value(type, damage_type) + own;
}

float CharacterEffects::effect_value(int type, const std::string& name) const {
    // `CCharacter::getEffectValue(EEFFECT_TYPE, float, const std::wstring&)`.
    //
    if (own_ == nullptr) {
        return inventory_ != nullptr ? inventory_->effect_value(type, name) : 0.0f;
    }
    if (inventory_ == nullptr) return own_->effect_value(type, name);
    const float own = own_->effect_value(type, name);
    return own + inventory_->effect_value(type, name);
}

float CharacterEffects::effect_value_minus_location(int type, int location, int damage_type) const {
    // `CCharacter::getEffectValue(EEFFECT_TYPE, EEQUIP_LOCATIONS,
    // EDAMAGE_TYPES)`.
    if (own_ == nullptr) {
        return inventory_ != nullptr
                   ? inventory_->effect_value_minus_equipment_slot(type, location, damage_type)
                   : 0.0f;
    }
    if (inventory_ == nullptr) return own_->effect_value(type, damage_type);
    const float own = own_->effect_value(type, damage_type);
    return inventory_->effect_value_minus_equipment_slot(type, location, damage_type) + own;
}

float CharacterEffects::effect_value_without_inventory(int type, int damage_type) const {
    // `CCharacter::getEffectValueWithoutInventory`.
    return own_ != nullptr ? own_->effect_value(type, damage_type) : 0.0f;
}

// PORT: the six requirement-reduction effect values gathered for `stat_requirement`.
RequirementReductions requirement_reductions(const CharacterEffects& effects) {
    RequirementReductions out;
    out.all = effects.effect_value(item_effect::kRequirementsAll);
    out.martial = effects.effect_value(item_effect::kRequirementsMartial);
    out.ranged = effects.effect_value(item_effect::kRequirementsRanged);
    out.magic = effects.effect_value(item_effect::kRequirementsMagic);
    out.armor = effects.effect_value(item_effect::kRequirementsArmor);
    out.spell = effects.effect_value(item_effect::kRequirementsSpell);
    return out;
}

float character_speed(float base, const CharacterEffects& effects) {
    // `CCharacter::runningSpeed`.:
    // a slowdown (or a NaN) is cut by effect 0x8c's share
    // times 0.01f, clamped to [0, 1]; the speed is `base * fma(percent, 0.01f, 1)`.
    const float percent = effects.effect_value(0x15);
    // The second value is only asked for a slowdown.
    return running_speed(base, percent, !(percent >= 0.0f) ? effects.effect_value(0x8c) : 0.0f);
}

float running_speed(float base, float pct, float kept_pct) {
    float percent = pct;
    if (!(percent >= 0.0f)) {
        float kept = kept_pct * 0.01f;
        kept = kept < 1.0f ? kept : 1.0f;
        kept = kept > 0.0f ? kept : 0.0f;
        percent = (1.0f - kept) * percent;
    }
    const float speed = std::fma(percent, 0.01f, 1.0f) * base;
    return speed >= 0.0f ? speed : 0.0f;
}

}  // namespace tl::core
