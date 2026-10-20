// A merchant's stock: `CCharacter::generateMerchantInventory` and what decides
// when it runs (`CLevel::populateNewMerchantsInventory`, `CCharacter::unitInit`).
// The items are made by the caller's `CreateLootUnit`, as loot is. No OGRE and no file I/O.
#pragma once

#include "core/inventory.h"
#include "core/item_roll.h"
#include "core/random.h"
#include "core/spawn_class.h"
#include "core/unit_types.h"
#include "formats/adm.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// The merchant keys of a character's unit data, each with its default.
struct MerchantData {
    /// `PLAYER_LEVEL_FOR_MERCHANT`, a float, default 0.0
    /// (`CCharacter::playerLevelForMerchantInventory`).
    float player_level_factor = 0.0f;
    /// `ALLOWS_MERCH_UNIQUES`, default false; `ALLOWS_MERCH_MAGIC`, default
    /// true (`generateMerchantInventory`).
    bool allows_uniques = false;
    bool allows_magic = true;
    /// `MERCHANTTAB`, default 0 (`CCharacter::getDefaultMerchantTab`):
    /// the tab `CMerchantMenu::setOpen` opens on.
    int32_t default_tab = 0;
};

/// The keys above, read from the character's unit data group.
MerchantData read_merchant_data(const tl::formats::AdmSection& unit);

/// `CCharacter::unitInit`'s `[TREASURE]`
/// read: `SPAWNCLASS` through `CResourceManager::getSpawnClassByName`, `MIN`
/// and `MAX`, each default 1. No `[TREASURE]` child leaves no class.
MonsterTreasure read_treasure(
    const tl::formats::AdmSection& unit,
    const std::function<const SpawnClass*(const std::string& name)>& spawn_class_by_name);

/// A merchant's inventory as `CCharacter::unitInit` makes it:
/// `CInventory(this, 0x32)`, one 50-slot backpack and no other pane (unlike the Linux build's
/// Merchant branch). Stacking stays on; `generate_merchant_inventory` turns it off.
Inventory make_merchant_inventory();

/// A merchant, as the stock roll reads it.
struct MerchantCharacter {
    Inventory* inventory = nullptr;
    /// It has a resource manager and a level; the roll does nothing without both.
    bool in_level = true;
    bool is_merchant = false;  ///< ISA `kMerchant` (0x29)
    bool is_gambler = false;   ///< ISA `kGambler` (0x7f)
    MerchantData data;
    MonsterTreasure treasure;
    /// The merchant as the spawn class sees it, as both the character and the
    /// finder (`is_merchant` there widens the level window).
    SpawnCharacterContext spawn;
};

/// What the stock roll needs from around the merchant.
struct MerchantStockContext {
    /// `CLevel::getPlayer()`'s level; null for no player.
    const uint32_t* player_level = nullptr;
    const SpawnContext* spawn_context = nullptr;
    RunicRandom* volatile_rng = nullptr;  ///< `g_RandVolatile`: the count
    RunicRandom* game_rng = nullptr;      ///< `g_Rand`: the spawn class rolls
    CreateLootUnit create_unit;           ///< `createUnit` for each rolled name
    /// For a gambler's `enchant(true)` and `recalculatePrice`; its
    /// `gambler_owner` is ignored (the item has no owner until it is picked
    /// up, and is priced as the gambler's after).
    const ItemRollContext* roll = nullptr;
};

/// What one stock roll did, for a trace or a test.
struct MerchantStockOutcome {
    bool rolled = false;     ///< the early returns passed
    int32_t level = -1;      ///< the level handed to the spawn class
    int32_t flags = 0;       ///< the spawn flags: 2 no uniques, 4 no magic
    uint32_t count = 0;      ///< the volatile roll between `MIN` and `MAX`
    std::vector<int> slots;  ///< where each kept item landed
    uint32_t discarded = 0;  ///< items destroyed (a refused unique, a full bag)
};

/// `CCharacter::generateMerchantInventory()`. The level is
/// `(int)ceilf((float)player_level * PLAYER_LEVEL_FOR_MERCHANT)` when the factor is above 0 and
/// there is a player, else -1. A gambler enchants a non-magical item and keeps it; otherwise a
/// `UNIQUE` is destroyed unless allowed. Also run by `CCharacter::unitInit`
/// and `CCharacter::applySaveState`.
MerchantStockOutcome generate_merchant_inventory(MerchantCharacter& merchant,
                                                 const MerchantStockContext& context);

/// `CLevel::populateNewMerchantsInventory()`: every Merchant
/// or Gambler of the level's list, in order, with an inventory and nothing in panes 4, 3 and 1
/// (a missing pane answers for the backpack, `getPaneIndex`) restocks. Returns how many.
int populate_new_merchants_inventory(const std::vector<MerchantCharacter*>& characters,
                                     const MerchantStockContext& context);

/// `CCharacter::applySaveState`, a merchant's inventory: the
/// saved items come back while its clock (advanced by `CCharacter::update`) is at most
/// `MERCHANT_RESPAWN_TIME` seconds; past it the clock goes to 0 and the stock is rolled afresh.
bool merchant_restocks_on_restore(float clock, int32_t merchant_respawn_time);

// --------------------------------------------------------------------------
// Buying and selling: the console's X on a vendor's screen (`CMerchantMenuXenon`,
// X). The PC drags instead (`CGameUI::menuItemClick`),
// with other refusal voices.

/// What a buy or a sell did, as far as the caller's sounds go.
enum class TradeResult {
    /// Nothing selected, or nobody to trade with: silence.
    None,
    /// `Error` (interface slot 0x18) and the player's
    /// `VOICEIMPOSSIBLE` (0x31) queued 0.1 s on. Short of gold, or a quest item.
    Refused,
    /// The bag cannot take it: the player's `VOICEINVENTORYFULL` (0x2f) queued
    /// 0.1 s on, and nothing else.
    InventoryFull,
    Bought,
    Sold,
};

/// What trading needs from around it.
struct TradeContext {
    const UnitTypeHierarchy* unit_types = nullptr;
    /// The price curves, for `recalculatePrice` on the item's new owner
    /// (`CEquipment::addedToInventory`). Null leaves
    /// prices alone.
    const StatBook* graphs = nullptr;
    /// The player's effect 0x53, for `buyPrice`/`sellPrice`; null for none.
    const float* player_price_bonus = nullptr;
    /// `CResourceManager::createUnit(data, 0, false, false)`: a fresh copy of
    /// an item the merchant never runs out of. Unset copies the held one.
    std::function<ItemStats(const ItemStats& data)> create_copy;
};

struct TradeOutcome {
    TradeResult result = TradeResult::None;
    int price = 0;         ///< the gold that changed hands
    int count = 0;         ///< how many went (the whole stack)
    int slot = -1;         ///< where the item landed; -1 for dropped or destroyed
    bool copied = false;   ///< `MERCHANTINFINITE`: the merchant kept its own
    bool gambled = false;  ///< journal statistic 0xf (`times_gambled`) went up
    /// `GoldBuy` (interface slot 0x17) plays: twice on a buy, once on a sale.
    int gold_buy_sounds = 0;
    /// Bought but not taken after all: then puts it in the
    /// level at the merchant (`CLevel::addItem`), which is the caller's to do.
    bool dropped = false;
    ItemStats dropped_item;
    /// Sold, and the merchant had no room: the item is destroyed.
    bool destroyed = false;
};

/// the tray's test before a buy (and a move between bags):
/// `canPickup`'s type gate, then a full pane refuses unless the item stacks
/// (`MAXSTACKSIZE` 2 or more) and the bag already holds some of it
/// (by `UNIT_GUID` there, by name here).
bool tray_can_take(const Inventory& inventory, const ItemStats& item,
                   const UnitTypeHierarchy& unit_types);

/// buy the item in `slot` of the merchant's inventory. Refused when the gold
/// is below `buyPrice` or it is a quest item; `InventoryFull` when `tray_can_take` says no. A
/// stack of 1 with `MERCHANTINFINITE` is copied; from a gambler it is identified and counts as
/// a gamble. If the bag cannot take it after all it is `dropped`.
TradeOutcome buy_from_merchant(Inventory& merchant, bool merchant_is_gambler, size_t slot,
                               Inventory& buyer, const TradeContext& context);

/// sell the item in `slot` of `seller` (the bag or the pet's)
/// to the merchant. A quest item is refused. Otherwise `GoldBuy`, the player
/// (`gold_to`) is given `sellPrice`, the whole stack leaves the seller - an
/// equipped item too - and goes into the merchant's inventory, priced as the
/// merchant's (a gambler's gamble price); no room destroys it.
TradeOutcome sell_to_merchant(Inventory& seller, size_t slot, Inventory& merchant,
                              bool merchant_is_gambler, Inventory& gold_to,
                              const TradeContext& context);

}  // namespace tl::core
