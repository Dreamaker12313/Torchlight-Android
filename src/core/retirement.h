// Retirement: the retirement NPC (`ISA 0xaf`), the heirloom it makes, and the heirloom
// handed to the next character. Chain: AI state 0x1b opens the enchant menu in retirement
// mode (`CGameUI::updateIngameUI`); A on an item retires into it
// (`CEnchantMenu::performInteraction` case 0x1b); New Game in retirement
// mode (`CMenuManager::setActiveMenu` case 2); the new
// character (`CGameClient::loadLevel`) takes generation, fame and heirloom.
#pragma once

#include "core/effect_manager.h"
#include "core/item_roll.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"

#include <optional>
#include <string>

namespace tl::core {

/// `CGameUI::updateIngameUI`: opens when `RETIREMENT_AGE` is negative
/// or, compared unsigned, at most the player's level.
bool can_retire(int player_level, int retirement_age);

/// The modal opened otherwise: `kRetireTitle`, "You cannot retire
/// until you reach level " and the age (`STRINGS::GetValueAsWString`). The
/// player's AI state goes to 2.
std::string cannot_retire_text(int retirement_age);
inline constexpr const char* kRetireTitle = "Retire";  ///<

/// A on the retirement page: only a Weapon (ISA 8), Armor (0xd) or
/// Trinket (0x27) goes on. DIVERGENCE: the PC retires into the menu's slot
/// 0xe, which a pad cannot fill; the port is the console's.
bool can_become_heirloom(const ItemStats& item, const UnitTypeHierarchy& unit_types);

/// `CEquipment::improveHeirloom()`:
/// `calculateCombatStats(true)`, then while the heirloom level is at most 9, every effect's
/// first two numbers become `value(i) * 1.1` and its applied value is scaled by 1.1; then
/// `setRequirements`.
/// DIVERGENCE: the port's item rebuilds its effects on every query, so they are first
/// written into the saved lists as `fillSaveState` would,
/// and the item becomes its loaded copy (`applySaveState`).
/// Same totals; the affixes themselves are gone one load early.
void improve_heirloom(ItemStats& item, const ItemRollContext& rolls,
                      const EffectManager::Context& effects);

/// The item half of `CEnchantMenu::performInteraction` case 0x1b:
/// an item never handed on before is renamed
/// `<player>'s <getItemName()>`; heirloom level + 1; `improveHeirloom`.
void make_heirloom(ItemStats& item, const std::string& player_name, const ItemRollContext& rolls,
                   const EffectManager::Context& effects);

/// What `CGameClient` holds from the retirement to the next new character: the item and
/// the generation (retiring player's plus one). `setGameState(3, ...)` deletes
/// both, state 4 keeps them, `loadLevel` spends them.
struct HeirloomHandOff {
    std::optional<ItemStats> item;
    int generation = 0;
};

/// `loadLevel`: `fameGate(min(generation, 5))`, awarded once the level
/// is loaded. `fameGate` is 0 for 0.
int heirloom_fame(int generation, const StatBook& graphs);

}  // namespace tl::core
