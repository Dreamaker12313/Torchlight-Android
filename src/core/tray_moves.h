// X on a console item tray (`CMerchantMenuXenon`'s Transmuter page, `CStashMenuXenon`):
// `CInventoryXenon`'s move into another inventory and back to the
// player. The tray dispatch picks one; sounds and
// dropping into the level are the caller's.
#pragma once

#include "core/inventory.h"
#include "core/unit_types.h"

#include <cstddef>

namespace tl::core {

/// What `move_to_inventory` did, which decides the sample.
enum class TrayMove {
    Nothing,  ///< no item selected, or it is already the target's: no sample
    Moved,    ///< slot 0x12 (`ItemTake`)
    Refused,  ///< slot 0x18 (`Error`)
};

/// the whole stack in `slot` of `from` goes into `target`. A combiner
/// (`target_is_combiner`) takes the first free of slots 15-18; elsewhere
/// a normal pickup. `tray_can_take` can
/// refuse; then `removeEquipment`. The combiner-owned-tray branch is not
/// here: sends that tray first.
TrayMove move_to_inventory(Inventory& from, size_t slot, Inventory& target, bool target_is_combiner,
                           const UnitTypeHierarchy& unit_types);

/// Where `return_to_player` put an item.
enum class TrayReturn {
    None,     ///< nothing happened: no item, or the player has no pet
    Bag,      ///< the player's bag; slot 0x12 (`ItemTake`)
    PetBag,   ///< the first pet's; slot 0x12 (`ItemTake`)
    Dropped,  ///< neither would take it: the caller drops it at the player
};

/// only when the player has a pet (quirk kept), otherwise nothing.
/// Bag, else pet's bag, else `dropped` for the caller to put in the level
/// (`CLevel::addItem`, then `drop`).
TrayReturn return_to_player(Inventory& from, size_t slot, Inventory& player_bag, Inventory* pet_bag,
                            const UnitTypeHierarchy& unit_types, InventorySlot& dropped);

}  // namespace tl::core
