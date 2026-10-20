// The console tray's two moves. See `tray_moves.h`.
#include "core/tray_moves.h"

#include "core/combine.h"
#include "core/merchant.h"

namespace tl::core {

TrayMove move_to_inventory(Inventory& from, size_t slot, Inventory& target, bool target_is_combiner,
                           const UnitTypeHierarchy& unit_types) {
    // no item, or its owner already the target, returns
    // before any sample.
    if (!from.occupied(slot) || &from == &target) return TrayMove::Nothing;
    // Into a combiner: the first of slots 15-18 holding nothing. Anywhere
    // else the slot is -1, "wherever it goes".
    int into = -1;
    if (target_is_combiner) {
        for (int candidate : kCombineSlots) {
            if (!target.occupied(static_cast<size_t>(candidate))) {
                into = candidate;
                break;
            }
        }
        if (into < 0) return TrayMove::Refused;
    }
    const InventorySlot held = from.slots()[slot];
    if (!tray_can_take(target, held.item, unit_types)) return TrayMove::Refused;
    // `removeEquipment` from the item's owner, then.
    from.remove_item(slot, held.count);
    if (into >= 0) {
        target.pickup_into_slot(held.item, held.count, into);
    } else {
        target.pickup(held.item, held.count, unit_types);
    }
    return TrayMove::Moved;
}

TrayReturn return_to_player(Inventory& from, size_t slot, Inventory& player_bag, Inventory* pet_bag,
                            const UnitTypeHierarchy& unit_types, InventorySlot& dropped) {
    // nothing without a player whose first pet exists.
    if (!from.occupied(slot) || pet_bag == nullptr) return TrayReturn::None;
    const InventorySlot held = from.slots()[slot];
    from.remove_item(slot, held.count);
    // The tray's take test on the player's bag, then on the
    // pet's; the first that passes gets `pickupEquipment(item, true)`.
    //
    if (tray_can_take(player_bag, held.item, unit_types)) {
        player_bag.pickup(held.item, held.count, unit_types);
        return TrayReturn::Bag;
    }
    if (tray_can_take(*pet_bag, held.item, unit_types)) {
        pet_bag->pickup(held.item, held.count, unit_types);
        return TrayReturn::PetBag;
    }
    dropped = held;
    return TrayReturn::Dropped;
}

}  // namespace tl::core
