// The console housing screen's own state: which tab is open, which of the six
// item-category filters, and which row, stat or quest tray is selected. Pure state
// stepping - no OGRE, no drawing; `core/inventory.h` owns the item rules
// (`Inventory::equip_into_first_free_location`, `unequip`, `use_item`...).
//
// `CMasterHousingXenon` builds its
// windows inline and has no command table to transcribe; its tab step
// wraps at both ends.
#pragma once

#include <cstddef>
#include <string>

namespace tl::core {

class UnitTypeHierarchy;

/// Which housing screen is open, stepped by LT/RT (`InputState::menu_prev`/`menu_next`
/// also take the shoulder buttons).
enum class HousingTab {
    Quest = 0,
    Inventory = 1,
    Stats = 2,
    Skills = 3,
};

/// One step of the ring `Skills -> Quest -> Inventory -> Stats`, `direction`
/// +1 (RT) or -1 (LT). `CMasterHousingXenon`'s step finds the open
/// screen in its list, adds the direction, and wraps: below zero to the last,
/// past the last to the first.
HousingTab step_housing_tab(HousingTab from, int direction);

/// The six category filters `inventorymenu_xenon.layout` authors
/// (`inventory_filter_weapons/armor/jewelry/potions/spells/fish`), in that
/// order. Jewelry and Potions are display slices of the Backpack pane, not panes.
enum class InventoryFilter {
    Weapons = 0,
    Armor = 1,
    Jewelry = 2,
    Potions = 3,
    Spells = 4,
    Fish = 5,
};
inline constexpr int kInventoryFilterCount = 6;

/// Which filter tab an item is listed under, by its unit type, first
/// match wins: `POTION`, `SPELL`, `SCROLL`, `MAP` or `QUESTITEM` -> Potions; `WEAPON`;
/// `ARMOR`; `RING` or `NECKLACE` -> Jewelry; `SOCKETABLE` -> the fifth tab; `FISH`;
/// anything else -> `kInventoryFilterCount`, listed nowhere. `owner_is_combiner` (the
/// menu's owner `ISA(COMBINER)`) puts everything on the first tab.
int inventory_filter_of(const std::string& unittype, const UnitTypeHierarchy& unit_types,
                        bool owner_is_combiner = false);

/// One step of the filter ring, `direction` +1 or -1, wrapping.
/// INVENTED: no address pins the wrap.
InventoryFilter step_inventory_filter(InventoryFilter from, int direction);

/// A tray's filter caption (`filtername`): `Weapons`,
/// `Armor`, `Jewelry`, `Miscellaneous`, `Socketables`, `Fish` by tab; a
/// combiner's tray says `Transmute` whatever its tab.
const char* inventory_filter_name(int tab, bool owner_is_combiner = false);

/// One step of a vendor screen's tray tab (and the handler's
/// own left step case 0x13): the tab moves by `direction` and
/// stays in 0..5; stepping past either end answers that end's direction (-1 or
/// +1), which moves the focus to the next tray (with its tab reset
/// to the edge it came in from). A combiner's tray has nothing to step: every
/// step leaves it.
int step_tray_tab(int& tab, int direction, bool owner_is_combiner = false);

/// A plain clamped step over a row count: the backpack list's up/down, scrolling the
/// `std::vector` of rows rather than paging.
/// INVENTED: `CInventoryXenon`'s own scroll-by-page rule is not read.
int step_selected_index(int index, int direction, size_t count);

/// Whose tray a count label is for, as tells them apart.
enum class TrayOwner {
    Combiner,  ///< the owner ISA 0x86
    Player,    ///< ISA 0x1c
    Stash,     ///< a stash tray (`+0x18c`) on page 7 or 8
    FirstPet,  ///< the first of the player's pets
    Other,     ///< anyone else: a merchant, a later pet
};

/// The count label's second number (`inventorysize`, "held/N"):
/// 4 for a combiner; 50 (0x32) for the player, a stash tray or
/// the player's first pet; 126 (0x7e) for anyone else. The first number is
/// the inventory's item count (`+0x24`).
int tray_count_capacity(TrayOwner owner);

/// Whether a tray row says "Equipped": asks the inventory
/// whether the item sits in one of its first 12 slots (`< 0xc`), so the
/// combine's slots 15-18 and the other fixed locations past 11 do not.
inline bool tray_shows_equipped(size_t slot) { return slot < 12; }

/// One of the up-to-six journal-visible quest trays `questmenu_xenon.layout`
/// authors (`ItemTray0`-`ItemTray5`, `kQuestLogSize`), clamped.
int step_selected_quest(int index, int direction, int quest_log_size);

/// What `A` does to a focused inventory slot: unequip a worn one, equip a stored one
/// (`Inventory::is_equip_location()` decides).
enum class InventorySlotAction {
    None,
    Unequip,
    Equip,
};

InventorySlotAction inventory_slot_action(size_t slot, bool occupied);

}  // namespace tl::core
