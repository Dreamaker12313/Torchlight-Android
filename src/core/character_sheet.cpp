#include "core/character_sheet.h"

#include "core/inventory.h"
#include "core/unit_types.h"

#include <cmath>

namespace tl::core {

namespace {

// `CCharacter::getEffectValue`: the
// character's total.
float effect_val(const CharacterEffects& effects, int type, int damage_type = kDamageTypeAny) {
    return effects.effect_value(type, damage_type);
}

// PORT: the sheet's attribute, through `CharacterProgression::strength`.
int strength_of(const DisplayInputs& in) {
    return in.progression ? in.progression->strength(in.effects) : 0;
}
// PORT: the sheet's attribute, through `CharacterProgression::dexterity`.
int dexterity_of(const DisplayInputs& in) {
    return in.progression ? in.progression->dexterity(in.effects) : 0;
}
// PORT: the sheet's attribute, through `CharacterProgression::magic`.
int magic_of(const DisplayInputs& in) {
    return in.progression ? in.progression->magic(in.effects) : 0;
}

constexpr float kPercent =
    0.01f;  // by which every percentage is taken (`fmuls`/`fmadds`)

// `ceilf` then `fctiwz`, as each flat and percentage term is taken in.
int ceil_int(float v) { return static_cast<int>(std::ceil(v)); }

// The scale of a damage type's own terms: `PERCENT DAMAGE BONUS` of `type` by
// 0.01, the type-6 one fused on, then for any type but physical `magic()`
// fused.
float type_scale(const DisplayInputs& in, int type) {
    float scale = effect_val(in.effects, kEffectTypePercentDamageBonus, type) * kPercent;
    scale = std::fma(effect_val(in.effects, kEffectTypePercentDamageBonus, 6), kPercent, scale);
    if (type != 0) scale = std::fma(static_cast<float>(magic_of(in)), kPercent, scale);
    return scale;
}

// One hand of `maximumDamageForDisplay`:
// for location 1, for location 0, the same code. Returns
// the hand's total.
int hand_damage(const DisplayInputs& in, const WeaponInputs& weapon, bool skip_base_damage,
                bool skip_effects) {
    // `+0x308` (the maximum damage), 0 with the base skipped.
    int base = skip_base_damage ? 0 : weapon.max_damage;
    float scale = 0.0f;  // f28 / f29, 0 when effects are skipped

    if (!skip_effects) {
        // The attribute and its percentage by the weapon's kind: `PERCENT
        // RANGED DAMAGE` (0x10) by 0.01 and `dexterity()` fused on, or
        // `PERCENT MELEE DAMAGE` (0xf) and `strength()`.
        float s = 0.0f;
        if (weapon.is_ranged) {
            s = effect_val(in.effects, kEffectTypePercentRangedDamage) * kPercent;
            s = std::fma(static_cast<float>(dexterity_of(in)), kPercent, s);
        } else {
            s = effect_val(in.effects, kEffectTypePercentMeleeDamage) * kPercent;
            s = std::fma(static_cast<float>(strength_of(in)), kPercent, s);
        }
        // Both hands held and location 1 no shield (`ISA 0x15`), whichever
        // hand this is: `DUAL WIELDING BONUS` (0x58).
        if (in.left_hand.equipped && in.right_hand.equipped && !in.left_hand.is_shield) {
            s = std::fma(effect_val(in.effects, kEffectTypeDualWieldingBonus), kPercent, s);
        }
        if (weapon.is_ranged) {
            if (weapon.is_ranged_category) {  // ISA 0xa3
                s = std::fma(effect_val(in.effects, kEffectTypePercentRangedItemDamageBonus),
                             kPercent, s);
            }
        } else if (weapon.is_martial) {  // ISA 0xa2
            s = std::fma(effect_val(in.effects, kEffectTypePercentMartialItemDamageBonus), kPercent,
                         s);
        }
        if (weapon.is_magic_item) {  // ISA 0xa4
            s = std::fma(effect_val(in.effects, kEffectTypePercentMagicItemDamageBonus), kPercent,
                         s);
        }
        s = std::fma(effect_val(in.effects, kEffectTypePercentDamageBonus, 0), kPercent, s);
        scale = std::fma(effect_val(in.effects, kEffectTypePercentDamageBonus, 6), kPercent, s);
        const int scaled = ceil_int(static_cast<float>(base) * scale) + base;
        // `RANGEDDAMAGEBONUS` (1) or `MELEEDAMAGEBONUS` (0), flat.
        base = ceil_int(effect_val(in.effects, weapon.is_ranged ? kEffectTypeRangedDamageBonus
                                                                : kEffectTypeMeleeDamageBonus)) +
               scaled;
    }

    // The type list (`+0x320` the types, `+0x340` inherent and `+0x330` from
    // sockets beside it): physical entries passed over with the base.
    int types = 0;
    for (const WeaponDamageBreakdown& entry : weapon.damage_types) {
        if (skip_base_damage && entry.damage_type == 0) continue;
        int amount = entry.inherent + entry.socket;
        if (!skip_effects) {
            // `fadds` of the type's scale and the hand's, then times the amount.
            const float s = type_scale(in, entry.damage_type) + scale;
            amount += ceil_int(s * static_cast<float>(amount));
        }
        types += amount;
    }

    // Physical `DAMAGE BONUS` (0xa) once unconditionally, then every type 0-6
    // in turn, physical again unless the base is skipped: without
    // `skip_base_damage` the physical bonus counts twice.
    int total = ceil_int(effect_val(in.effects, kEffectTypeDamageBonus, 0)) + types + base;
    for (int type = 0; type <= 6; ++type) {
        if (skip_base_damage && type == 0) continue;
        int flat = ceil_int(effect_val(in.effects, kEffectTypeDamageBonus, type));
        if (!skip_effects) flat += ceil_int(static_cast<float>(flat) * type_scale(in, type));
        total += flat;
    }
    return total;
}

}  // namespace

WeaponInputs WeaponInputs::from_item(const ItemStats* item, const UnitTypeHierarchy& unit_types,
                                     const StatBook* graphs) {
    WeaponInputs inputs;
    if (!item) return inputs;
    inputs.equipped = true;
    inputs.is_weapon = unit_types.isa(item->unittype, unit_type::kWeapon);
    inputs.is_ranged = unit_types.isa(item->unittype, unit_type::kRangedWeapon);
    inputs.is_shield = unit_types.isa(item->unittype, unit_type::kShield);
    inputs.is_martial = unit_types.isa(item->unittype, unit_type::kItemCategoryMartial);
    inputs.is_ranged_category = unit_types.isa(item->unittype, unit_type::kItemCategoryRanged);
    inputs.is_magic_item = unit_types.isa(item->unittype, unit_type::kItemCategoryMagic);

    if (item->combat_rolled) {
        inputs.max_damage = item->damage;
        // The copy's damage-type list (`+0x350`) with `+0x380` and `+0x368`
        // beside it, as `maximumDamageForDisplay`
        // walks them.
        for (const int type : item->damage_types) {
            inputs.damage_types.push_back({type, item->inherent_damage[static_cast<size_t>(type)],
                                           item->damage_bonus[static_cast<size_t>(type)]});
        }
    } else if (graphs != nullptr) {
        // INVENTED: a copy the port has not rolled yet (it rolls when first held)
        // shows the top of its authored range, as the screen did before.
        inputs.max_damage = item_max_damage(*item, *graphs);
    }
    return inputs;
}

DisplayInputs make_display_inputs(const CharacterProgression& progression,
                                  const Inventory& inventory, const UnitTypeHierarchy& unit_types,
                                  CharacterEffects effects, const StatBook* graphs,
                                  const AttackDescription* own_attack) {
    DisplayInputs in;
    in.progression = &progression;
    // The character's total is its own manager and this inventory's
    // (`CCharacter +0x3e8`), unless the caller named another.
    in.effects = CharacterEffects(
        effects.own(), effects.inventory() != nullptr ? effects.inventory() : &inventory);
    const auto& slots = inventory.slots();
    auto held = [&](int location) -> const ItemStats* {
        const size_t at = static_cast<size_t>(location);
        return at < slots.size() && slots[at].count > 0 ? &slots[at].item : nullptr;
    };
    in.left_hand = WeaponInputs::from_item(held(equip_location::kLeftHand), unit_types, graphs);
    in.right_hand = WeaponInputs::from_item(held(equip_location::kRightHand), unit_types, graphs);
    // INVENTED: `+0x3ec`, the weapon in use, is not kept; a weapon in either hand
    // stands for it.
    in.weapon_in_use = in.left_hand.is_weapon || in.right_hand.is_weapon;
    // The character's own first attack (`+0x368`, the unarmed attacks): its
    // `+0x30` damage by type. `selectAttack(0)` finds one whenever there is one or
    // a weapon is held.
    in.attack.has_attack = own_attack != nullptr || in.weapon_in_use;
    if (own_attack != nullptr) {
        in.attack.damage_by_type[0] = own_attack->max_damage;
        for (int type = 1; type < kDamageTypeCount; ++type) {
            in.attack.damage_by_type[type] = own_attack->damage_by_type[static_cast<size_t>(type)];
        }
    }
    // `+0x5c8` (the base level): 1, stored by `CCharacter::CCharacter`
    // and written by nothing else.
    in.base_level = 1;
    return in;
}

// CCharacter::maximumDamageForDisplay
int maximum_damage_for_display(const DisplayInputs& in, bool melee, bool skip_base_damage,
                               bool skip_effects) {
    if (!in.attack.has_attack) return 0;  // `+0x2c0` null after `selectAttack(0)`

    int result = 0;
    // Location 1 needs `ISA(Weapon)`; location 0 is taken as a weapon. Either is
    // passed over when its `ISA(Ranged Weapon)` equals `melee`.
    const WeaponInputs& left = in.left_hand;
    if (left.equipped && left.is_ranged != melee && left.is_weapon) {
        const int total = hand_damage(in, left, skip_base_damage, skip_effects);
        if (total > 0) result = total;
    }
    const WeaponInputs& right = in.right_hand;
    if (right.equipped && right.is_ranged != melee) {
        const int total = hand_damage(in, right, skip_base_damage, skip_effects);
        if (result < total) result = total;  // the larger hand
    }

    // Bare hands: with nothing so far, an attack of its own, melee
    // and the base not skipped - the attack's `+0x30[0]`, then each other
    // type's `+0x30[type]` raised by its `PERCENT DAMAGE BONUS` and type 6's.
    if (result == 0 && melee && !skip_base_damage) {
        result = in.attack.damage_by_type[0];
        for (int type = 1; type <= 6; ++type) {
            int amount = in.attack.damage_by_type[type];
            if (!skip_effects) {
                const float s = std::fma(
                    effect_val(in.effects, kEffectTypePercentDamageBonus, 6), kPercent,
                    effect_val(in.effects, kEffectTypePercentDamageBonus, type) * kPercent);
                amount += ceil_int(static_cast<float>(amount) * s);
            }
            result += amount;
        }
    }

    // No weapon in use (`+0x3ec` null), melee only: the figure so
    // far raised by the levels gained over `+0x5c8` and by strength.
    if (in.weapon_in_use || !melee) return result;
    const unsigned level = static_cast<unsigned>(in.progression ? in.progression->level() : 1);
    const unsigned base_level = static_cast<unsigned>(in.base_level);
    if (base_level < level) {
        const float gained = static_cast<float>(level) - static_cast<float>(base_level);
        result = static_cast<int>((gained + 100.0f) * static_cast<float>(result) * kPercent);
    }
    // `+0x394` (the strength field, no effects) when effects are skipped, else
    // `strength()`.
    const int str =
        skip_effects ? (in.progression ? in.progression->strength() : 0) : strength_of(in);
    result =
        static_cast<int>(static_cast<float>(str + 100) * static_cast<float>(result) * kPercent);
    if (skip_effects) return result;
    float s = effect_val(in.effects, kEffectTypePercentDamageBonus, 0) * kPercent;
    s = std::fma(effect_val(in.effects, kEffectTypePercentDamageBonus, 6), kPercent, s);
    s = std::fma(effect_val(in.effects, kEffectTypePercentMeleeDamage), kPercent, s);
    // With a master (`+0x5a4`), the master's `PERCENT PET DAMAGE` (0x5b).
    if (in.master != nullptr) {
        s = std::fma(in.master->effect_value(kEffectTypePercentPetDamage, kDamageTypeAny), kPercent,
                     s);
    }
    result = ceil_int(static_cast<float>(result) * s) + result;
    result = ceil_int(effect_val(in.effects, kEffectTypeMeleeDamageBonus)) + result;
    return ceil_int(effect_val(in.effects, kEffectTypeDamageBonus, 0)) + result;
}

// CCharacter::minimumDamageForDisplay
int minimum_damage_for_display(const DisplayInputs& in, bool melee, bool skip_base_damage,
                               bool skip_effects) {
    if (!in.attack.has_attack) return 0;
    return CharacterProgression::display_minimum(
        maximum_damage_for_display(in, melee, skip_base_damage, skip_effects));
}

CombatSheet combat_sheet(const DisplayInputs& in) {
    // `CStatsMenu::update`'s calls with effects (third flag 0): melee (1,0,0),
    // ranged (0,0,0), magic the larger of (0,1,0) and
    // (1,1,0) (`cmovge`). The (x,y,1) call beside each is
    // the figure without effects.
    CombatSheet sheet;
    sheet.melee_max = maximum_damage_for_display(in, true, false, false);
    sheet.ranged_max = maximum_damage_for_display(in, false, false, false);
    const int magic_ranged = maximum_damage_for_display(in, false, true, false);
    const int magic_melee = maximum_damage_for_display(in, true, true, false);
    sheet.magic_max = magic_melee >= magic_ranged ? magic_melee : magic_ranged;
    // The magic row's low end is its own pair of minimums:
    // the smaller one that is not 0.
    const int low_ranged = minimum_damage_for_display(in, false, true, false);
    const int low_melee = minimum_damage_for_display(in, true, true, false);
    if (low_ranged == 0) sheet.magic_min = low_melee;
    else if (low_melee == 0) sheet.magic_min = low_ranged;
    else sheet.magic_min = low_ranged > low_melee ? low_melee : low_ranged;
    sheet.armour_max = in.progression ? in.progression->armour_class(in.effects) : 0;
    return sheet;
}

}  // namespace tl::core
