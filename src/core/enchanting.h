// The enchanter and the enchant shrine: `CEnchantMenu::performInteraction`
// (cases 0x15 and 0x16); on the console `attemptToEnchantAtMerchant`
// for an NPC and for the shrine, both reached from
// `CEnchantMenuXenon`'s A. The result line is the
// console's (`resultofenchant`).
//
// Sounds, the journal, the pane and the effect recalculation are the caller's
// (`EnchantOutcome`). No OGRE and no file I/O.
#pragma once

#include "core/item_roll.h"
#include "core/units.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tl::core {

/// Which of the two services. The console's `CEnchantMenuXenon +0x12c` is 0
/// for an enchanter and 1 for a shrine (set by the game UI from the player's
/// AI state); 2 and 3 are the gem services and 4
/// the retirement, not here.
enum class EnchantService {
    Enchanter,  ///< AI state 0x15, `ENCHANTER_*` odds, paid, the item's level
    Shrine,     ///< AI state 0x16, `ENCHANTSHRINE_*` odds, free, the player's level
};

enum class EnchantResult {
    /// No service: the enchanter's price above the player's gold. The original
    /// plays `Error`.
    Refused,
    /// "... but nothing happens": the magic roll missed. The count is unchanged.
    NothingHappens,
    /// "... and creates new sockets": `CEquipment::addSockets`.
    ///
    Sockets,
    /// "... and adds new enchantments": `CEquipment::addEnchant`.
    ///
    Enchanted,
    /// "... and accidentally removes all enchantments": the disenchant roll.
    Disenchanted,
};

struct EnchantOutcome {
    EnchantResult result = EnchantResult::Refused;
    /// Gold taken (the enchanter's `enchantPrice` before the roll), else 0.
    int price = 0;
    /// The result line, as the console composes it.
    /// Empty when refused.
    std::string message;

    /// The call's own answer (`!bVar3`): false when refused or disenchanted.
    /// The console tints the line by it, `FFFFFF00` for true and `FFFF0000`
    /// for false, and plays `REVEAL` + `VOICEFORTUNEGOOD` for a
    /// socket or an enchant, `LOWMANA` + `VOICEFORTUNEBAD` otherwise.
    bool succeeded() const {
        return result != EnchantResult::Refused && result != EnchantResult::Disenchanted;
    }
    /// Whether the item was worked on at all: every outcome but a refusal
    /// marks it identified, counts journal statistic 0x11 and makes the
    /// owner's inventory recalculate.
    bool served() const { return result != EnchantResult::Refused; }
};

/// The chance, in percent, that the next enchant removes every enchantment:
/// `(float)count * INCREASE + CHANCE`, at most `MAX` (for the
/// enchanter, for the shrine; the console's pane shows the
/// enchanter's).
float disenchant_chance(uint32_t enchant_count, float chance, float increase, float max_chance);

/// The enchanter's `disenchant_chance` for the item.
float enchanter_disenchant_chance(const ItemStats& item, const ItemRollGlobals& globals);

/// Whether the item's effect manager exists (`CBaseUnit +0x1b8` non-null). An
/// item without one is never disenchanted: the roll's test is `chance * 10 <
/// roll || +0x1b8 == 0`.
bool item_has_effect_manager(const ItemStats& item);

/// `CEnchantMenuXenon::attemptToEnchantAtMerchant` or
/// `attemptToEnchantAtShrine` - on the PC
/// `CEnchantMenu::performInteraction`, case 0x15 or 0x16 - on
/// an item already found enchantable (`canEnchant`). In order, every
/// draw on `g_RandVolatile`:
///
/// 1. `randomIntegerBetweenVolatile(0, 1)`, whatever happens next: the
///    enchant's slots are this plus 1 to plus 2.
/// 2. The enchanter only: refused when `enchantPrice`
///    is above `gold`; otherwise it is taken.
/// 3. `randomBetweenVolatile(0, 1000)` against `disenchant_chance * 10`: at or
///    below it, with an effect manager, every affix and every effect but
///    `BLOCK_CHANCE` go (`clearOutAffixEffects`,
///    `clearEffects(true)`) and the count goes to 0.
/// 4. Otherwise, below `getMaxSockets`: a
///    draw under `SOCKET_CHANCE * 10` adds sockets and counts one.
/// 5. Otherwise a draw at or below `MAGIC_CHANCE * 10` does `add_enchant(level,
///    r + 1, r + 2)` - the item's level for the enchanter, the player's for the
///    shrine, at least 4 - and counts one.
///
/// Every outcome but a refusal leaves the item identified. `name` is the item's
/// name as the line prints it; `player_level` is read by the shrine only.
EnchantOutcome perform_enchant(EnchantService service, ItemStats& item, int& gold,
                               uint32_t player_level, const std::string& name,
                               const ItemRollContext& context);

// ---------------------------------------------------------------------------
// The gem services: `CEnchantMenuXenon` modes 2 and 3, called from the tray's A
// with no draw (the PC's cases 0x1a and 0x19 draw the slot
// roll first; the console is followed). `CCharacter::approachAI`:
// a `SocketableDestroyer` (ISA 0x85, AI state 0x1a,
// mode 2) destroys the item and gives the gems back; a `SocketableRecoverer`
// (0x84, state 0x19, mode 3) keeps the item and destroys the gems.

/// Whether the item holds a gem (the socket list's count
/// non-zero). It gates the services, the pane's A button and the legend.
bool item_has_gems(const ItemStats& item);

/// Mode 3 (Linux `performInteraction` case 0x19): every gem
/// destroyed, the item kept. Returns the line for
/// `getsocketresult`; the caller plays `REVEAL` and recalculates the owner.
/// Only for an item with gems.
std::string destroy_socketed_gems(ItemStats& item, const std::string& name);

/// Mode 2 (Linux `performInteraction` case 0x1a): the gems come
/// out one at a time, each from the front with the last moved into its place,
/// so a, b, c comes out a, c, b. A gem that is not a `RANDOMMAGIC
/// SOCKETABLE` (ISA 0xa0) gets back what the socket took (`reapplyEffects(true)`,
/// `reapplyAffixes(true)`, rolling on `g_RandVolatile`), and one with an
/// heirloom level is `improveHeirloom`ed. Returned in that order for the caller
/// to pick up before the item itself is removed. The item is left with no gems.
std::vector<ItemStats> take_out_socketed_gems(ItemStats& item, const ItemRollContext& context);

/// Mode 2's line for `destroysocketresult`: "The item is
/// destroyed, recovering the gems within" + "!".
std::string gems_recovered_line();

/// with the item-select dialog's tray
/// (`CMessageItemSelectXenon::vfunc_4`): the items a gem can be set
/// into - `ISA(item, 2)`, identified, with sockets other than gems. The PC asks
/// the same (`CGameUI::menuItemClick`).
bool item_takes_gem(const ItemStats& item, const UnitTypeHierarchy& unit_types);

}  // namespace tl::core
