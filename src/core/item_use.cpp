#include "core/item_use.h"

#include "core/character.h"
#include "core/skill_cast.h"

namespace tl::core {

bool item_is_useable(const ItemStats& item, const UnitTypeHierarchy& unit_types) {
    // `CItem::isUseable`.
    return unit_types.isa(item.unittype, unit_type::kConsumable) ||
           unit_types.isa(item.unittype, unit_type::kInteractable);
}

namespace {

/// `isEffectValidForUnit`'s health and mana branches (the mana one):
/// only a `DONT_USE_ON_FULL` potion is ever refused, at a full meter (the two
/// compared as integers) or while an effect of the same `NAME` is still on the
/// unit.
bool meter_effect_valid(const ItemUseTarget& target, const ItemStats* item, const Effect& effect,
                        float value, float current, float maximum,
                        const UnitTypeHierarchy& unit_types) {
    if (value < 0.0f) return true;
    if (item == nullptr) return true;
    if (!unit_types.isa(item->unittype, unit_type::kPotion)) return true;
    if (!item->dont_use_on_full) return true;
    if (static_cast<int>(current) == static_cast<int>(maximum)) return false;
    if (target.effects == nullptr) return true;
    return !target.effects->has_effect_named(effect.name);
}

}  // namespace

bool effect_valid_for_unit(const ItemUseTarget& target, const ItemStats* item, const Effect& effect,
                           float value, const UnitTypeHierarchy& unit_types) {
    // `CCharacter::isEffectValidForUnit`. An
    // undamageable unit takes no damage and no healing.
    const int type = effect.type;
    if (!target.damageable && (type == kEffectTypeDamage || type == kEffectTypeHpRecharge ||
                               type == kEffectTypeHpRechargePlayer)) {
        return false;
    }
    if (type == kEffectTypeHpRecharge || type == kEffectTypeHpRechargePlayer) {
        return meter_effect_valid(target, item, effect, value, target.hp, target.max_hp,
                                  unit_types);
    }
    if (type == kEffectTypeManaRecharge || type == kEffectTypeManaRechargePlayer) {
        return meter_effect_valid(target, item, effect, value, target.mana, target.max_mana,
                                  unit_types);
    }
    if (type == kEffectTypeKill) return true;
    // `DAMAGE` (0x34) is refused only to a unit in state 5 or 6, dying or dead.
    if (type == kEffectTypeDamage) return !target.dying_or_dead;
    // `OPEN TOWN PORTAL`: the character's level has a template that allows portals, and
    // nothing else.
    if (type == kEffectTypeOpenTownPortal) return target.level_allows_portals;
    // `OPEN PORTAL`: a template that allows portals, or a town's (`+0x8a`).
    if (type == kEffectTypeOpenPortal) return target.level_allows_map_portals;
    // `LEARN SKILL`: `canLearnSpell(effect name, (int)value)`, with the empty slot
    // asked for (opens the replace dialog exactly when this refuses).
    if (type == kEffectTypeLearnSkill) {
        return target.skills != nullptr &&
               target.skills->can_learn_spell(effect.name, static_cast<int32_t>(value), target.pet);
    }
    // `STUN`: not a unit that already wears the `STUNNED` theme.
    if (type == kEffectTypeStun) return !target.stunned;
    // `TURN ALIGNMENT`: a living unit of alignment 2 that is no player and
    // whose master, when it has one, is no player either.
    if (type == kEffectTypeTurnAlignment) {
        if (target.dying_or_dead) return false;
        if (target.alignment != 2) return false;
        if (unit_types.isa(target.unittype, unit_type::kPlayer)) return false;
        if (!target.has_master) return true;
        return !target.master_is_player;
    }

    // every other type: a `DONT_USE_ON_FULL` potion is kept from a
    // unit that is not the player outright, and from the player while an
    // effect of the same name is on it.
    if (item != nullptr && unit_types.isa(item->unittype, unit_type::kPotion) &&
        item->dont_use_on_full &&
        (!unit_types.isa(target.unittype, unit_type::kPlayer) ||
         (target.effects != nullptr && target.effects->has_effect_named(effect.name)))) {
        return false;
    }
    const int activation = static_cast<int>(effect.activation);
    return activation == 0 || activation == 1 || activation == 2;
}

bool can_use_on_target(const ItemStats& item, const ItemUseTarget& target,
                       const UnitTypeHierarchy& unit_types, const StatBook& graphs) {
    // `CEquipment::canUseOnTarget`, user and
    // target the same character. No uses left falls off the end of the
    // function.
    if (item.uses == 0) return false;
    // `getLevelRequirement` against the
    // target's `+0x100`.
    if (item.min_level_required > target.level) return false;
    // `+0x1b8`: at least one effect the target will take.
    if (!item.effects.empty()) {
        bool any = false;
        for (const Effect& effect : item.effects) {
            const float value =
                effect_base_value(effect, EffectCalculate::Roll, graphs, EffectOwner{});
            if (effect_valid_for_unit(target, &item, effect, value, unit_types)) any = true;
        }
        if (!any) return false;
    }
    // The item's own skills: `CSkill::canAffixesAndEffectsBeAppliedToUnit`
    // on the skill's level property,
    // refused without one; the user is the character itself.
    if (!item.skills.empty() && target.skill_def && target.effect_types != nullptr) {
        if (target.skill_holds) return false;
        for (const ItemStats::Skill& skill : item.skills) {
            const SkillDef* def = target.skill_def(skill.name);
            // A name `addSkillByName` does not find makes no skill to test.
            if (def == nullptr) continue;
            KnownSkill known;
            known.def = def;
            known.effective = skill.level;
            const SkillProperty* property = known.property(skill.level);
            if (property == nullptr) return false;
            if (property->section == nullptr) continue;
            SkillApplyFacts facts;
            facts.isa = [&](int32_t type) {
                return unit_types.isa(target.unittype, static_cast<uint32_t>(type));
            };
            facts.unit_is_character = true;
            facts.has_user = true;
            facts.unit_is_user = true;
            facts.same_alignment = true;
            facts.master_is_user = false;
            // DIVERGENCE: passes an unset value (only its sign is read); the
            // effect's own base value stands in for it.
            facts.effect_valid = [&](const Effect& effect) {
                const float value =
                    effect_base_value(effect, EffectCalculate::Roll, graphs, EffectOwner{});
                return effect_valid_for_unit(target, nullptr, effect, value, unit_types);
            };
            // The blocks were made with the skill (`kSkillBlockTargetDefault`).
            const SkillTargetFilter unauthored = kSkillBlockTargetDefault;
            if (!skill_property_can_apply(*property->section, *target.effect_types, unauthored,
                                          facts, property->events, &unit_types)) {
                return false;
            }
        }
    }
    // The pet tests at the end, on the target's master (`+0x640`): a `NOPETS`
    // item is refused to a pet, a `PETONLY` one to anyone else.
    if (unit_types.isa(item.unittype, unit_type::kNoPets) && target.pet) return false;
    if (unit_types.isa(item.unittype, unit_type::kPetOnly)) return target.pet;
    return true;
}

bool spell_learnable(const ItemStats& item, int uses, const ItemUseTarget& target,
                     const UnitTypeHierarchy& unit_types, const StatBook& graphs) {
    //
    if (!unit_types.isa(item.unittype, unit_type::kSpell)) return false;
    // No use left refuses; -9999 or any other count goes on to the level requirement.
    //
    if (uses == 0) return false;
    if (item.min_level_required > target.level) return false;
    if (target.skills == nullptr) return false;
    // (effects, 0x41)`: the first effect of the type.
    for (const Effect& effect : item.effects) {
        if (effect.type != kEffectTypeLearnSkill) continue;
        const float value = effect_base_value(effect, EffectCalculate::Roll, graphs, EffectOwner{});
        // (name, level, false)`: the console's `canLearnSpell`
        // without the empty-slot test, so a full book still reads "Learn".
        return target.skills->can_learn_spell(effect.name, static_cast<int32_t>(value), target.pet,
                                              /*need_empty_slot=*/false);
    }
    return false;
}

ItemUseResult use_on_target(const ItemStats& item, int stack, int& uses, ItemUseTarget& target,
                            const UnitTypeHierarchy& unit_types, const StatBook& graphs) {
    // `CEquipment::useOnTarget`.
    ItemUseResult result;
    const bool potion = unit_types.isa(item.unittype, unit_type::kPotion);
    for (const Effect& effect : item.effects) {
        const float value = effect_base_value(effect, EffectCalculate::Roll, graphs, EffectOwner{});
        if (!effect_valid_for_unit(target, &item, effect, value, unit_types)) continue;
        if (target.effects == nullptr) continue;

        // `CCharacter::applyEffectOnUnit`:
        // the validity test again, then the kinds with a behaviour of their
        // own, and everything else copied onto the unit
        // (`CBaseUnit::copyEffect` ->
        // `CEffectManager::cloneEffect`).
        if (!effect_valid_for_unit(target, &item, effect, value, unit_types)) continue;
        // `INSTANT` (-900) goes to `applyInstantEffect` and
        // is not copied; its portal, transform and `RESPEC` cases
        // (`CPlayer::resetSkills`) are the caller's. `LEARN SKILL` asks again.
        // INVENTED: every other instant type, and `0x2f`, `0x38`, `0x3f`, `0x40`
        // and `0x4f`, are not modelled and apply nothing.
        const int type = effect.type;
        if (effect.duration == kDurationInstant) {
            // `applyEffectOnUnit` hands it to `applyInstantEffect` and answers
            // true.
            result.used = true;
            const bool player = unit_types.isa(target.unittype, unit_type::kPlayer);
            CharacterProgression* sheet = player ? target.character : nullptr;
            if (sheet != nullptr) {
                switch (type) {
                    case kEffectTypePermanentStrength:
                        sheet->add_permanent_attribute(StatType::Strength, value);
                        ++result.applied;
                        break;
                    case kEffectTypePermanentDexterity:
                        sheet->add_permanent_attribute(StatType::Dexterity, value);
                        ++result.applied;
                        break;
                    case kEffectTypePermanentMagic:
                        sheet->add_permanent_attribute(StatType::Magic, value);
                        ++result.applied;
                        break;
                    case kEffectTypePermanentDefense:
                        sheet->add_permanent_attribute(StatType::Defense, value);
                        ++result.applied;
                        break;
                    case kEffectTypePermanentHealth:
                        sheet->add_permanent_hp(value);
                        result.maxima_changed = true;
                        ++result.applied;
                        break;
                    case kEffectTypePermanentMana:
                        sheet->add_permanent_mp(value);
                        result.maxima_changed = true;
                        ++result.applied;
                        break;
                    case kEffectTypeAwardStatpoint:
                        sheet->award_stat_points(value);
                        ++result.applied;
                        break;
                    default: break;
                }
            }
            if (type == kEffectTypeRespec && player) {
                result.respec = true;
                ++result.applied;
            }
            if (type == kEffectTypeOpenTownPortal) {
                result.open_town_portal = true;
                ++result.applied;
            }
            // `OPEN PORTAL` ('s case 0x8d):
            // `openMapPortal(NAME, level)`.
            if (type == kEffectTypeOpenPortal) {
                result.open_map_portals.push_back(effect.name);
                ++result.applied;
            }
            // `TRANSFORM` and `TRANSFORM PERMANENT`:
            // `transformCharacter(NAME, value)`, -999 for the permanent one.
            if (type == kEffectTypeTransform || type == kEffectTypeTransformPermanent) {
                result.transforms.push_back(
                    {effect.name, type == kEffectTypeTransform ? value : -999.0f});
                ++result.applied;
            }
            if (type == kEffectTypeLearnSkill && target.skills != nullptr) {
                const int32_t level = static_cast<int32_t>(value);
                if (target.skills->can_learn_spell(effect.name, level, target.pet)) {
                    target.skills->learn_spell(
                        effect.name, level,
                        target.skill_def ? target.skill_def(effect.name) : nullptr);
                    result.learned_spell = effect.name;
                    ++result.applied;
                }
            }
            continue;
        }
        if (type == 0x2f || type == 0x38 || type == 0x3f || type == 0x40 || type == 0x4f) {
            continue;
        }
        const int activation = static_cast<int>(effect.activation);
        if (type != kEffectTypeHpRecharge && type != kEffectTypeHpRechargePlayer &&
            activation != 0 && activation != 1 && activation != 2) {
            continue;
        }
        LiveEffect& held = target.effects->add_effect(effect);
        result.used = true;

        // After the copy, a potion's copy is
        // scaled by `POTION EFFICIENCY`: each of the four numbers, then the value a second
        // time - the original's own double application, kept.
        // INVENTED: the four recalculations draw nothing from `g_RandVolatile` (the console's
        // `ROLL` draws once each); the last stands for all four.
        if (potion) {
            const float efficiency = CharacterEffects(target.effects, target.inventory)
                                         .effect_value(kEffectTypePotionEfficiency) *
                                     0.01f;
            if (efficiency != 0.0f) {
                const float factor = efficiency + 1.0f;
                for (size_t i = 0; i < 4; ++i)
                    held.effect.values[i] = held.effect.values[i] * factor;
                held.value = effect_base_value(held.effect, EffectCalculate::Roll, graphs,
                                               target.effects->context().owner);
                held.value = held.value * factor;
            }
        }

        ++result.applied;
        // `incrementJournalStatistic(0xc, 1)`, once per effect a potion puts on.
        if (potion) result.potion_counted = true;
    }

    // the item's own skills (`+0x184`), the first of them cast by
    // the user (`performUnknownSkill`); the item counts as used.
    if (!item.skills.empty()) {
        result.skill = item.skills.front().name;
        result.skill_level = item.skills.front().level;
        result.used = true;
    }
    // nothing taken and no skill, nothing spent and no sound.
    // `USE_SOUND` on the item's bank.
    result.play_use_sound = result.used;
    // A use is spent only off the last of a stack, and never an unlimited one;
    // a larger stack loses one instead, `incrementStackBy(-1)` (vtable
    // `+0x180`).
    if (result.used && stack < 2 && uses != kUnlimitedUses) --uses;
    const int left = stack - 1;
    // `performItemUse`, after the call.
    result.remove_item = left < 2 && uses < 1 && uses != kUnlimitedUses;
    return result;
}

bool effect_valid_for_item(const ItemStats& target, const Effect& effect) {
    // `CEquipment::isEffectValidForUnit`.
    if (effect.type != kEffectTypeIdentify) return true;
    return !target.identified;
}

ItemUseResult use_on_item(const ItemStats& item, int stack, int& uses, ItemStats& target) {
    //
    ItemUseResult result;
    if (!(stack > 1 || uses > 0 || uses == kUnlimitedUses)) return result;
    for (const Effect& effect : item.effects) {
        if (!effect_valid_for_item(target, effect)) continue;
        // `CEquipment::applyEffectOnUnit`:
        // `IDENTIFY` marks the copy and is the one effect that answers true.
        // INVENTED: any other effect is copied onto the item there
        // (`CBaseUnit::copyEffect`); no item aimed at an item carries one, and
        // nothing is copied here.
        if (effect.type != kEffectTypeIdentify) continue;
        target.identified = true;
        ++result.applied;
    }
    if (result.applied == 0) return result;
    result.play_use_sound = true;
    if (stack < 2) {
        if (uses != kUnlimitedUses) --uses;
    }
    // on what is left: a stack, a use or an unlimited copy stays.
    const int left = stack < 2 ? stack : stack - 1;
    result.remove_item = !(left > 1 || uses > 0 || uses == kUnlimitedUses) || left < 1;
    return result;
}

int equipment_of_guid(const Inventory& inventory, int64_t guid) {
    // `CInventory::getEquipmentOfGuid`: the matches in the bag's
    // list, keeping the one with the smallest stack (`+0x238`), the first on a
    // tie; an unstackable match (`+0x23c` below 1) ends the walk as it stands.
    // INVENTED: the original walks its item list in the order the items went
    // in; the port has only the slots, and walks them in slot order.
    if (guid == -1) return -1;
    int best = -1;
    const std::vector<InventorySlot>& slots = inventory.slots();
    for (size_t i = 0; i < slots.size(); ++i) {
        const InventorySlot& slot = slots[i];
        if (slot.count <= 0 || slot.item.unit_guid != guid) continue;
        if (best < 0) best = static_cast<int>(i);
        if (slot.item.max_stack < 1) return best;
        if (slot.count < slots[static_cast<size_t>(best)].count) best = static_cast<int>(i);
    }
    return best;
}

float effect_values_of_damage(const CharacterEffects& effects) {
    // `CCharacter::getEffectValuesOfDamage`.
    float damage = effects.effect_value(kEffectTypeDamage);
    const float hp = effects.effect_value(kEffectTypeHpRecharge);
    const float player = effects.effect_value(kEffectTypeHpRechargePlayer);
    if (0.0f < damage) damage = -damage;
    return player + hp + damage;
}

float hp_change(float max_hp, const RechargeRates& rates, const CharacterEffects& effects, float dt,
                bool invincible_pet) {
    // `CCharacter::updateHP`: an `INVINCIBLE` unit with a
    // master regenerates at `PET_HP_RECHARGE_RATE`, untested; any other at the base rate only
    // when it is not 0. Then the effects: separate `modifyHP` calls in the original, summed
    // here, which clamps the same while the unit is alive.
    float change = 0.0f;
    if (invincible_pet) {
        change += ((static_cast<float>(static_cast<int>(max_hp)) * rates.pet_hp) * dt) * 0.01f;
    } else if (rates.hp != 0.0f) {
        // ((max * rate) * dt) * 0.01f.
        change += ((static_cast<float>(static_cast<int>(max_hp)) * rates.hp) * dt) * 0.01f;
    }
    change += effect_values_of_damage(effects) * dt;
    return change;
}

float mana_change(float max_mana, const RechargeRates& rates, const CharacterEffects& effects,
                  float dt) {
    // `CCharacter::updateMana`: the base
    // rate, then `MANA RECHARGE` and `MANA RECHARGE PLAYER`.
    // ((max * rate) * dt) * 0.01f.
    float change = ((static_cast<float>(static_cast<int>(max_mana)) * rates.mana) * dt) * 0.01f;
    const float recharge = effects.effect_value(kEffectTypeManaRecharge);
    const float player = effects.effect_value(kEffectTypeManaRechargePlayer);
    change += (player + recharge) * dt;
    return change;
}

}  // namespace tl::core
