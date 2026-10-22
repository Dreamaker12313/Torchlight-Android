// The gameplay half of render/renderer_pet_items.cpp (core/world.h).
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

void World::refresh_pet_effects() {
    ensure_effect_data();
    load_stat_graphs();
    load_unit_themes();
    // The tables `CEffect::calculateBaseValue` reads, and the pet as the owner
    // whose level `OWNERLEVEL` scales by. A pet has no attributes of its own
    // (`+0x42c`.. are never set by a pet's `unitInit`), so the four stats are 0.
    tl::core::EffectManager::Context context;
    context.types = &effect_types;
    context.graphs = &stat_graphs;
    context.owner.level = static_cast<uint32_t>(std::max(1, player_character.level()));
    context.owner.present = true;
    context.theme_book = &unit_themes;
    pet.effects().set_context(context);
    // The spells' effective levels read the pet's own totals
    // (`calculateEffectiveSkillLevel`); the pet has
    // no pet.
    pet.skills().set_effect_managers(pet_total(), nullptr);
}

tl::core::ItemUseTarget World::pet_use_target() {
    tl::core::ItemUseTarget target;
    target.hp = pet.health();
    target.max_hp = pet.max_health();
    // `mana()` and `maxMana()`:
    // the pool of the level script's character, which pays for its casts and
    // takes `MANA RECHARGE` from the manager below (`updateMana`, in
    // `character_update`).
    if (pet_node_id != 0) script.unit_mana(pet_node_id, target.mana, target.max_mana);
    target.damageable = true;
    target.unittype = pet.stats().unittype.empty() ? "PET" : pet.stats().unittype;
    // The pet's own level (`+0x100`).
    target.level = pet.level();
    target.effects = &pet.effects();
    target.inventory = &pet.bag();
    target.level_allows_portals = tl::core::town_portal_allowed(portal_level());
    target.level_allows_map_portals = tl::core::map_portal_allowed(portal_level());
    target.has_master = true;
    target.master_is_player = true;
    // `LEARN SKILL`: the pet's own book; a master (`+0x640`) makes it a pet to
    // `canLearnSpell` and `canUseOnTarget`.
    target.skills = &pet.skills();
    target.pet = true;
    target.character = nullptr;
    target.skill_holds = pet_node_id != 0 && script.unit_skill_holds(pet_node_id);
    ensure_effect_data();
    target.effect_types = &effect_types;
    target.skill_def = [this](const std::string& name) -> const tl::core::SkillDef* {
        ensure_player_skill_files();
        const auto it = player_skill_files.find(tl::core::upper_ascii(name));
        if (it != player_skill_files.end()) return &it->second->def;
        // `learnSpell`'s "Skill not found : <name>".
        TL_LOG_WARN("Skill not found : %s", name.c_str());
        return nullptr;
    };
    return target;
}

// What the trays' row fill asks of the player.
tl::core::TrayRowContext World::tray_row_context() {
    ensure_effect_data();
    load_unit_types();
    tl::core::TrayRowContext context;
    context.unit_types = &unit_types;
    context.effects = &item_effect_context();
    context.colours = &item_label_colours;
    // The item's vtable `+0x15c` with the player and true:
    // `CEquipment::canEquip(CCharacter*, bool)`.
    //
    context.player_can_equip = [this](const tl::core::ItemStats& item) {
        return player_inventory.item_can_equip(item, unit_types, &player_character,
                                               &player_effects);
    };
    const tl::core::CharacterEffects total = player_total();
    context.reductions = tl::core::requirement_reductions(total);
    context.level = player_character.level();
    context.strength = player_character.strength(total);
    context.dexterity = player_character.dexterity(total);
    context.magic = player_character.magic(total);
    context.defense = player_character.defense(total);
    context.worn = tl::core::tray_worn_figures(player_inventory, unit_types, item_effect_context());
    return context;
}

bool World::use_pet_item(size_t slot) {
    if (slot >= pet.bag().slots().size() || !pet.bag().occupied(slot)) return false;
    const tl::core::ItemStats item = pet.bag().slots()[slot].item;
    const int stack = pet.bag().slots()[slot].count;
    // the owner as user and target.
    tl::core::ItemUseResult used;
    if (!use_item_on_pet(item, stack, /*by_player=*/false, used)) return false;
    // the copy is destroyed with its last use; otherwise one off
    // the stack. (A copy's own `+0x248` is not kept between uses, as for the
    // player's: every shipped item authors `USES:1`.)
    if (used.remove_item) {
        pet.bag().remove_item(slot, stack);
    } else if (stack > 1) {
        pet.bag().remove_item(slot, 1);
    }
    return true;
}

bool World::feed_pet_from(tl::core::Inventory& holder, size_t slot) {
    // the ISA 0x7c branch.
    if (slot >= holder.slots().size() || !holder.occupied(slot)) return false;
    const tl::core::ItemStats item = holder.slots()[slot].item;
    const int stack = holder.slots()[slot].count;
    tl::core::ItemUseResult used;
    if (!use_item_on_pet(item, stack, /*by_player=*/true, used)) return false;
    // as for any use.
    if (used.remove_item) {
        holder.remove_item(slot, stack);
    } else if (stack > 1) {
        holder.remove_item(slot, 1);
    }
    return true;
}

void World::learn_spell_from_tray(bool pet_owner, size_t slot) {
    // how many of the four slots name a skill the owner holds
    // (`getKnownSpell`); full is more than one for an owner that
    // ISA 0x57, more than three for any other.
    tl::core::Inventory& bag = pet_owner ? pet.bag() : player_inventory;
    if (slot >= bag.slots().size() || !bag.occupied(slot)) return;
    load_unit_types();
    ensure_effect_data();
    if (pet_owner) refresh_pet_effects();
    tl::core::SkillBook& book = pet_owner ? pet.skills() : player_skills;
    int known = 0;
    for (size_t i = 0; i < book.spells.size(); ++i) {
        if (book.known_spell(i) != nullptr) ++known;
    }
    const bool full = (pet_owner && known > 1) || known > 3;
    const tl::core::ItemStats item = bag.slots()[slot].item;
    const tl::core::ItemUseTarget target = pet_owner ? pet_use_target() : player_use_target();
    // `canUse(item, owner, owner)`, then `useOnTarget` and
    //
    if (tl::core::can_use_on_target(item, target, unit_types, stat_graphs)) {
        if (pet_owner) {
            use_pet_item(slot);
        } else {
            use_player_item(slot);
        }
        return;
    }
    if (full) {
        open_spell_remove(pet_owner, slot);
        return;
    }
    TL_LOG_INFO("%s tray: '%s' is not learned", pet_owner ? "pet" : "bag",
                item.display_name.c_str());
}

void World::relearn_pet_spells(const std::array<std::string, 4>& names,
                               const std::array<int32_t, 4>& levels) {
    // `CCharacter::applySaveState`: each
    // saved spell learned again. The book is rebuilt from nothing: the one
    // carried here pointed into the skill files of the renderer it came from.
    pet.skills() = tl::core::SkillBook{};
    ensure_player_skill_files();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i].empty()) continue;
        const auto file = player_skill_files.find(tl::core::upper_ascii(names[i]));
        pet.skills().learn_spell(names[i], levels[i],
                                 file != player_skill_files.end() ? &file->second->def : nullptr);
    }
    refresh_pet_effects();
    give_pet_spells_to_script();
    if (!names[0].empty() || !names[1].empty()) {
        TL_LOG_INFO("pet: its spells learned again: '%s' (%d), '%s' (%d)",
                    pet.skills().spells[0].c_str(), pet.skills().spell_levels[0],
                    pet.skills().spells[1].c_str(), pet.skills().spell_levels[1]);
    }
}

}  // namespace tl::core
