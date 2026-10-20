// Retirement and the heirloom. See `retirement.h`.
#include "core/retirement.h"

#include "core/combat.h"
#include "core/enchanting.h"
#include "core/item_effects.h"
#include "core/item_name.h"
#include "formats/float_convert.h"

#include <cstdint>

namespace tl::core {

bool can_retire(int player_level, int retirement_age) {
    // `mov 0x48(%rax),%ebp; test %ebp,%ebp; js` -> open; then
    // `cmp 0x100(%rdi),%ebp; jbe` -> open.
    if (retirement_age < 0) return true;
    return static_cast<uint32_t>(retirement_age) <= static_cast<uint32_t>(player_level);
}

// The modal `CGameUI` opens: "You cannot retire until you
// reach level " and the age. PORT: the PC's English text.
std::string cannot_retire_text(int retirement_age) {
    return "You cannot retire until you reach level " + std::to_string(retirement_age);
}

bool can_become_heirloom(const ItemStats& item, const UnitTypeHierarchy& unit_types) {
    // `ISA(8)`, `ISA(0xd)`, `ISA(0x27)`.
    return item_isa(item, unit_type::kWeapon, unit_types) ||
           item_isa(item, unit_type::kArmor, unit_types) ||
           item_isa(item, unit_type::kTrinket, unit_types);
}

namespace {

/// The loop body of `improveHeirloom`: both numbers become
/// `value(i) * 1.1f` (each through `calculateBaseValue`), then the kept applied value times
/// 1.1f over what they left. A roll draws on `g_RandVolatile` when the two numbers differ.
void improve_heirloom_effect(LiveEffect& live, const StatBook& graphs, RunicRandom& rng) {
    const float applied = live.value;
    const EffectOwner none;
    live.effect.values[0] = effect_column_value(live.effect, 0, graphs) * 1.1f;
    roll_effect_value(live.effect, graphs, none, rng);
    live.effect.values[1] = effect_column_value(live.effect, 1, graphs) * 1.1f;
    roll_effect_value(live.effect, graphs, none, rng);
    live.value = applied * 1.1f;
    live.effect.rolled_value = live.value;
}

}  // namespace

void improve_heirloom(ItemStats& item, const ItemRollContext& rolls,
                      const EffectManager::Context& effects) {
    // `CEquipment::improveHeirloom()`.
    const UnitTypeHierarchy& types = *rolls.unit_types;
    // A loaded copy rolls as a fresh one: `calculateCombatStats(true)` reads `MINDAMAGE`,
    // `MAXDAMAGE` and `ARMOR` off the unit's group.
    calculate_combat_stats(item, item_isa(item, unit_type::kWeapon, types),
                           item_isa(item, unit_type::kArmor, types), /*from_save=*/true,
                           *rolls.graphs, *rolls.volatile_rng, item.heirloom_level);
    // `cmpl $0x9, +0x28c; jg`: the manager `+0x1b8`.
    if (item.heirloom_level <= 9 && item_has_effect_manager(item)) {
        EffectManager manager = item_effect_manager(item, effects);
        // The name parts `getFullItemName` caches, while the affixes that word
        // them are still on the copy (`fillSaveState` does the same).
        item_full_name(item, true, types);
        // DIVERGENCE (see the header): the lists become the copy's saved ones.
        item.saved_effects.clear();
        for (int list = 0; list < 3; ++list) {
            for (const LiveEffect& held : manager.effects(static_cast<EffectActivation>(list))) {
                LiveEffect live = held;
                improve_heirloom_effect(live, *rolls.graphs, *rolls.volatile_rng);
                ItemStats::SavedEffect saved;
                saved.effect = live.effect;
                saved.value = live.value;
                saved.elapsed = live.elapsed;
                item.saved_effects.push_back(saved);
            }
        }
        item.from_save = true;
        item.effects.clear();
        item.combat_effects.clear();
        item.affixes.clear();
        // The saved lists are in the manager's order already.
        item.passive_order.clear();
        item.affixes_before_combat = 0;
        // `clearOutDescriptions` empties the
        // description cache; the port keeps none.
    }
    // the tail call.
    set_item_requirements(item, *rolls.graphs, types);
}

void make_heirloom(ItemStats& item, const std::string& player_name, const ItemRollContext& rolls,
                   const EffectManager::Context& effects) {
    // `CEnchantMenu::performInteraction` case 0x1b.
    if (item.heirloom_level == 0) {
        // `+0x210` = the player's name (`+0x4c0`) + "'s " +
        // `getItemName()` (vtable +0x2a0).
        item.display_name = player_name + "'s " + item_name(item);
    }
    item.heirloom_level += 1;
    improve_heirloom(item, rolls, effects);
}

int heirloom_fame(int generation, const StatBook& graphs) {
    // `CGameClient::loadLevel`: `min(generation, 5)`, the
    // `cmovle` taking the generation when it is at most 5.
    int level = generation <= 5 ? generation : 5;
    // `CMasterResourceManager::fameGate(int)`.
    //
    if (level == 0) return 0;
    const StatGraph* graph = graphs.find(kFameGateGraph);
    const int points = graph != nullptr ? static_cast<int>(graph->size()) : 0;
    if (points < level) level = points;
    return tl::formats::x86_float_to_int32(
        graphs.value(kFameGateGraph, static_cast<float>(level), 0.0f));
}

}  // namespace tl::core
