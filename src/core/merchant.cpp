// A merchant's stock. See `merchant.h`.
#include "core/merchant.h"

#include <cmath>

namespace tl::core {

MerchantData read_merchant_data(const tl::formats::AdmSection& unit) {
    MerchantData data;
    // `CCharacter::playerLevelForMerchantInventory`.
    data.player_level_factor = unit.float_or("PLAYER_LEVEL_FOR_MERCHANT", 0.0f);
    // `CCharacter::generateMerchantInventory`.
    //
    data.allows_uniques = unit.bool_or("ALLOWS_MERCH_UNIQUES", false);
    data.allows_magic = unit.bool_or("ALLOWS_MERCH_MAGIC", true);
    // `CCharacter::getDefaultMerchantTab`.
    data.default_tab = static_cast<int32_t>(unit.integer_or("MERCHANTTAB", 0));
    return data;
}

MonsterTreasure read_treasure(
    const tl::formats::AdmSection& unit,
    const std::function<const SpawnClass*(const std::string& name)>& spawn_class_by_name) {
    // `CCharacter::unitInit`:
    // `GetDataGroupByName(L"TREASURE")`.
    MonsterTreasure treasure;
    for (const tl::formats::AdmSection& group : unit.children) {
        if (group.name != "TREASURE") continue;
        const std::string name = group.string_or("SPAWNCLASS", "");
        treasure.spawn_class = spawn_class_by_name ? spawn_class_by_name(name) : nullptr;
        treasure.min = static_cast<int32_t>(group.integer_or("MIN", 1));
        treasure.max = static_cast<int32_t>(group.integer_or("MAX", 1));
        break;
    }
    return treasure;
}

Inventory make_merchant_inventory() {
    // `CCharacter::unitInit`:
    // `CInventory(this, 0x32)` for every character that is not ISA 0xaa.
    return Inventory(Inventory::kDefaultBackpackCapacity, true);
}

MerchantStockOutcome generate_merchant_inventory(MerchantCharacter& merchant,
                                                 const MerchantStockContext& context) {
    // `CCharacter::generateMerchantInventory()`.
    //
    MerchantStockOutcome outcome;
    if (merchant.inventory == nullptr || !merchant.in_level) return outcome;
    outcome.rolled = true;

    const float factor = merchant.data.player_level_factor;
    outcome.level = -1;
    if (0.0f < factor && context.player_level != nullptr) {
        // The player's level read as unsigned and converted.
        outcome.level =
            static_cast<int32_t>(std::ceil(static_cast<float>(*context.player_level) * factor));
    }
    merchant.inventory->set_stacking(false);
    outcome.flags = merchant.data.allows_uniques ? 0 : 2;
    if (!merchant.data.allows_magic) outcome.flags |= 4;

    if (merchant.treasure.spawn_class == nullptr) return outcome;
    if (context.volatile_rng == nullptr || context.game_rng == nullptr ||
        context.spawn_context == nullptr || context.roll == nullptr ||
        context.roll->unit_types == nullptr) {
        return outcome;
    }
    const UnitTypeHierarchy& types = *context.roll->unit_types;
    outcome.count = static_cast<uint32_t>(
        context.volatile_rng->integer_between(merchant.treasure.min, merchant.treasure.max));
    std::vector<ItemStats> units =
        create_units_by_spawn_class(*merchant.treasure.spawn_class, outcome.count, &merchant.spawn,
                                    &merchant.spawn, outcome.level, outcome.flags,
                                    *context.game_rng, *context.spawn_context, context.create_unit);

    // Not yet the gambler's: the item has no inventory until it is picked up.
    ItemRollContext unowned = *context.roll;
    unowned.gambler_owner = false;
    for (ItemStats& item : units) {
        if (!merchant.is_gambler || item_is_magical(item, types)) {
            if (!merchant.data.allows_uniques && item_isa(item, unit_type::kUnique, types)) {
                ++outcome.discarded;  // the item's destructor
                destroy_item_rolls(item, item_effect_rolls(*context.roll));
                continue;
            }
        } else {
            enchant_item(item, true, unowned);
        }
        item.identified = true;  // `+0x348`, then `CItem::destroyItemText`
        const int slot = merchant.inventory->pickup(item, 1, types);
        if (slot < 0) {
            ++outcome.discarded;
            destroy_item_rolls(item, item_effect_rolls(*context.roll));
            continue;
        }
        outcome.slots.push_back(slot);
        if (merchant.is_gambler && context.roll->graphs != nullptr) {
            // `CEquipment::recalculatePrice`
            // on the picked-up copy, whose owner is now the gambler.
            ItemStats* held = merchant.inventory->item_in_slot(static_cast<size_t>(slot));
            if (held != nullptr) recalculate_item_price(*held, *context.roll->graphs, types, true);
        }
    }
    return outcome;
}

int populate_new_merchants_inventory(const std::vector<MerchantCharacter*>& characters,
                                     const MerchantStockContext& context) {
    // `CLevel::populateNewMerchantsInventory()`,
    // over the level's character list in order.
    int restocked = 0;
    for (MerchantCharacter* character : characters) {
        if (character == nullptr) continue;
        if (!character->is_merchant && !character->is_gambler) continue;
        if (character->inventory == nullptr) continue;
        const Inventory& inventory = *character->inventory;
        if (inventory.items_in_pane(InventoryPane::Armor) != 0) continue;
        if (inventory.items_in_pane(InventoryPane::Weapon) != 0) continue;
        if (inventory.items_in_pane(InventoryPane::Spell) != 0) continue;
        generate_merchant_inventory(*character, context);
        ++restocked;
    }
    return restocked;
}

bool merchant_restocks_on_restore(float clock, int32_t merchant_respawn_time) {
    // `CCharacter::applySaveState`: the saved
    // items are restored while `+0x388 <= (float)CGameGlobals +0x2c8`.
    return !(clock <= static_cast<float>(merchant_respawn_time));
}

namespace {

// `CEquipment::addedToInventory`: an item whose icon has
// been made (`createIcon`) is priced again for the inventory it
// joins; anything traded from a tray has one.
void price_for_new_owner(ItemStats& item, bool gambler_owner, const TradeContext& context) {
    if (context.graphs == nullptr || context.unit_types == nullptr) return;
    recalculate_item_price(item, *context.graphs, *context.unit_types, gambler_owner);
}

}  // namespace

bool tray_can_take(const Inventory& inventory, const ItemStats& item,
                   const UnitTypeHierarchy& unit_types) {
    // the item's own gate and `ISA(item, 2)`, as `canPickup`.
    if (!inventory.can_pickup(item, unit_types, true)) return false;
    // A stack limit below 2, or no copy of it held, and the bag full: no.
    const bool stacks_onto_one = item.max_stack >= 2 && inventory.find_by_name(item.name) >= 0;
    if (!stacks_onto_one && !inventory.can_pickup(item, unit_types, false)) return false;
    return true;
}

TradeOutcome buy_from_merchant(Inventory& merchant, bool merchant_is_gambler, size_t slot,
                               Inventory& buyer, const TradeContext& context) {
    //
    TradeOutcome outcome;
    if (!merchant.occupied(slot) || context.unit_types == nullptr) return outcome;
    const UnitTypeHierarchy& types = *context.unit_types;
    const InventorySlot held = merchant.slots()[slot];
    const int price = item_buy_price_of(held.item, held.count, context.player_price_bonus);
    if (buyer.gold() < price || item_isa(held.item, unit_type::kQuestItem, types)) {
        outcome.result = TradeResult::Refused;
        return outcome;
    }
    if (!tray_can_take(buyer, held.item, types)) {
        // `queueGlobalSample(player bank +0x224, 0x2f, 0.0, 0.1)`.
        outcome.result = TradeResult::InventoryFull;
        return outcome;
    }
    outcome.result = TradeResult::Bought;
    outcome.price = price;
    outcome.count = held.count;
    ++outcome.gold_buy_sounds;  // `playSample(0x17)` at the player
    buyer.give_gold(-price);    // `CCharacter::giveGold`

    ItemStats item = held.item;
    if (held.count == 1 && held.item.merchant_infinite) {
        // `createUnit(data, 0, 0)`: the merchant keeps its own.
        if (context.create_copy) item = context.create_copy(held.item);
        outcome.copied = true;
    } else {
        // `CInventory::removeEquipment`.
        merchant.remove_item(slot, held.count);
    }
    if (merchant_is_gambler) {
        // Identified, `destroyItemText`, journal statistic 0xf; a
        // unique's achievement 5 (Xbox Live) is not carried.
        item.identified = true;
        outcome.gambled = true;
    }
    // `CInventory::pickupEquipment(item, true)`.
    const int landed = buyer.pickup(item, held.count, types);
    if (landed < 0) {
        // `CLevel::addItem(level, item, the merchant's position, true)`.
        outcome.dropped = true;
        outcome.dropped_item = item;
    } else {
        outcome.slot = landed;
        if (ItemStats* now = buyer.item_in_slot(static_cast<size_t>(landed))) {
            price_for_new_owner(*now, false, context);
        }
    }
    ++outcome.gold_buy_sounds;  // `playSample(0x17)` again
    return outcome;
}

TradeOutcome sell_to_merchant(Inventory& seller, size_t slot, Inventory& merchant,
                              bool merchant_is_gambler, Inventory& gold_to,
                              const TradeContext& context) {
    //
    TradeOutcome outcome;
    if (!seller.occupied(slot) || context.unit_types == nullptr) return outcome;
    const UnitTypeHierarchy& types = *context.unit_types;
    const InventorySlot held = seller.slots()[slot];
    if (item_isa(held.item, unit_type::kQuestItem, types)) {
        outcome.result = TradeResult::Refused;
        return outcome;
    }
    outcome.result = TradeResult::Sold;
    outcome.count = held.count;
    ++outcome.gold_buy_sounds;  // `playSample(0x17)` at the player
    outcome.price = item_sell_price_of(held.item, held.count, context.player_price_bonus);
    gold_to.give_gold(outcome.price);  // `giveGold`; `CPlayer::soldItem` is Steam's
    seller.remove_item(slot, held.count);
    const int landed = merchant.pickup(held.item, held.count, types);
    if (landed < 0) {
        outcome.destroyed = true;  // the item's deleting destructor
        return outcome;
    }
    outcome.slot = landed;
    if (ItemStats* now = merchant.item_in_slot(static_cast<size_t>(landed))) {
        price_for_new_owner(*now, merchant_is_gambler, context);
    }
    return outcome;
}

}  // namespace tl::core
