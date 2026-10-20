#include "core/inventory_screen.h"

#include "core/inventory.h"
#include "core/unit_types.h"

#include <algorithm>

namespace tl::core {

HousingTab step_housing_tab(HousingTab from, int direction) {
    // CMasterHousingXenon's ring step
    constexpr HousingTab kRing[] = {HousingTab::Skills, HousingTab::Quest, HousingTab::Inventory,
                                    HousingTab::Stats};
    constexpr int kCount = 4;
    int at = 0;
    while (at < kCount && kRing[at] != from) ++at;
    if (at == kCount) return from;
    int next = at + direction;
    if (next < 0) {
        next = kCount - 1;
    } else if (next >= kCount) {
        next = 0;
    }
    return kRing[next];
}

int inventory_filter_of(const std::string& unittype, const UnitTypeHierarchy& unit_types,
                        bool owner_is_combiner) {
    // The hierarchy's ids: 0x21 POTION, 0x81 SPELL, 0x2a SCROLL,
    // 0xac MAP, 0x67 QUESTITEM, 8 WEAPON, 0xd ARMOR, 0x11 RING, 0x18 NECKLACE,
    // 0x78 SOCKETABLE, 0x7c FISH.
    if (owner_is_combiner) return 0;
    const uint32_t type = unit_types.id_of(unittype);
    const auto isa = [&](uint32_t want) { return unit_types.isa(type, want); };
    if (isa(0x21) || isa(0x81) || isa(0x2a) || isa(0xac) || isa(0x67)) return 3;
    if (isa(0x8)) return 0;
    if (isa(0xd)) return 1;
    if (isa(0x11) || isa(0x18)) return 2;
    if (isa(0x78)) return 4;
    // `(ISA(0x7c) == 0) + 5`: 5 for a fish, 6 - no tab at all - for the rest.
    return isa(0x7c) ? 5 : kInventoryFilterCount;
}

// INVENTED: the filter ring stepped with a wrap; see the header.
InventoryFilter step_inventory_filter(InventoryFilter from, int direction) {
    int value = static_cast<int>(from);
    value = (value + direction) % kInventoryFilterCount;
    if (value < 0) value += kInventoryFilterCount;
    return static_cast<InventoryFilter>(value);
}

const char* inventory_filter_name(int tab, bool owner_is_combiner) {
    // seven wide strings built once, the
    // first six indexed by the tray's tab (`+0x128`).
    static const char* const kNames[kInventoryFilterCount] = {
        "Weapons", "Armor", "Jewelry", "Miscellaneous", "Socketables", "Fish"};
    if (owner_is_combiner) return "Transmute";
    if (tab < 0 || tab >= kInventoryFilterCount) return kNames[0];
    return kNames[tab];
}

int tray_count_capacity(TrayOwner owner) {
    //
    switch (owner) {
        case TrayOwner::Combiner: return 4;
        case TrayOwner::Player:
        case TrayOwner::Stash:
        case TrayOwner::FirstPet: return 0x32;
        case TrayOwner::Other: break;
    }
    return 0x7e;
}

int step_tray_tab(int& tab, int direction, bool owner_is_combiner) {
    //
    if (owner_is_combiner) return direction > 0 ? 1 : direction < 0 ? -1 : 0;
    int next = tab + (direction > 0 ? 1 : direction < 0 ? -1 : 0);
    int leave = 0;
    if (next < 0) {
        leave = -1;
        next = 0;
    } else if (next == kInventoryFilterCount) {
        next = kInventoryFilterCount - 1;
        leave = 1;
    }
    tab = next;
    return leave;
}

// INVENTED: the cursor stepped and clamped; see the header.
int step_selected_index(int index, int direction, size_t count) {
    if (count == 0) return index;
    const int last = static_cast<int>(count) - 1;
    return std::clamp(index + direction, 0, last);
}

// INVENTED: the journal's cursor stepped and clamped.
int step_selected_quest(int index, int direction, int quest_log_size) {
    if (quest_log_size <= 0) return 0;
    return std::clamp(index + direction, 0, quest_log_size - 1);
}

// PORT: what A does on a slot: equip, unequip, or nothing.
InventorySlotAction inventory_slot_action(size_t slot, bool occupied) {
    if (!occupied) return InventorySlotAction::None;
    return Inventory::is_equip_location(slot) ? InventorySlotAction::Unequip
                                              : InventorySlotAction::Equip;
}

}  // namespace tl::core
