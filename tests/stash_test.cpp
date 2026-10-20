// The stash and the shared stash: `core/stash.*`, the tray move into a stash
// (`core/tray_moves.*`), the count label and the shared stash's file.
#include "core/stash.h"
#include "core/inventory.h"
#include "core/inventory_screen.h"
#include "core/tray_moves.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "formats/save_file.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::Inventory;
using tl::core::ItemStats;
using tl::core::SharedStash;
using tl::core::TrayMove;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;

AdmSection unit_type(const std::string& name, uint32_t id, const std::vector<uint32_t>& ancestors) {
    AdmSection section;
    section.name = name;
    AdmRecord key;
    key.key = "ID";
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

/// Takeable 31 and Potion 33, enough for `canPickup`'s type gate.
UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Takeable", 31, {0}));
    all.children.push_back(unit_type("Potion", 33, {31, 0}));
    root.children.push_back(all);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

ItemStats item(const std::string& name, const std::string& type, int max_stack = 1) {
    ItemStats out;
    out.name = name;
    out.display_name = name;
    out.unittype = type;
    out.max_stack = max_stack;
    return out;
}

TL_TEST(player_stash_is_fifty_slots_after_the_fixed_nineteen) {
    // `CInventory(player, 0x32)` and no other pane.
    const Inventory stash = tl::core::make_player_stash();
    TL_CHECK_EQ(stash.capacity(), Inventory::kFixedSlotCount + 50);
    TL_CHECK_EQ(stash.pane_size(tl::core::InventoryPane::Backpack), 50);
    const SharedStash shared;
    TL_CHECK_EQ(shared.inventory().capacity(), Inventory::kFixedSlotCount + 42);
    TL_CHECK(!shared.hardcore());
    TL_CHECK_EQ(std::string(shared.file_name()), std::string("sharedstash.bin"));
}

TL_TEST(stash_save_slots_start_above_999) {
    // writes `slot + 1000`; takes `999 < slot`.
    TL_CHECK(!tl::core::is_stash_save_slot(999));
    TL_CHECK(tl::core::is_stash_save_slot(1000));
    TL_CHECK(tl::core::is_stash_save_slot(1000 + 68));
    TL_CHECK(!tl::core::is_stash_save_slot(19));
}

TL_TEST(move_to_a_stash_stacks_and_refuses_when_full) {
    const UnitTypeHierarchy types = hierarchy();
    Inventory bag;
    Inventory stash = tl::core::make_player_stash();
    const ItemStats potion = item("Health Potion", "Potion", 20);
    stash.pickup(potion, 3, types);
    const int from = bag.pickup(potion, 4, types);
    TL_CHECK(from >= 0);
    // with slot -1: `pickupEquipment(item, true)`, onto the
    // stack already there.
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(from), stash, false, types) ==
             TrayMove::Moved);
    TL_CHECK(!bag.occupied(static_cast<size_t>(from)));
    TL_CHECK_EQ(stash.count_by_name("Health Potion"), 7);
    TL_CHECK_EQ(stash.size(), size_t{1});
    // Nothing selected, or the item already the target's: no sample at all.
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(from), stash, false, types) ==
             TrayMove::Nothing);
    TL_CHECK(tl::core::move_to_inventory(stash, stash.find_by_name("Health Potion"), stash, false,
                                         types) == TrayMove::Nothing);
    // A full stash refuses what does not stack onto something it holds.
    for (int i = 0; i < 49; ++i)
        stash.pickup(item("Rock " + std::to_string(i), "Takeable"), 1, types);
    const int ring = bag.pickup(item("Band", "Takeable"), 1, types);
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(ring), stash, false, types) ==
             TrayMove::Refused);
    TL_CHECK(bag.occupied(static_cast<size_t>(ring)));
    // ... but a potion still joins its stack.
    const int more = bag.pickup(potion, 2, types);
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(more), stash, false, types) ==
             TrayMove::Moved);
    TL_CHECK_EQ(stash.count_by_name("Health Potion"), 9);
}

TL_TEST(shared_stash_reloads_only_for_the_other_kind_and_only_when_empty) {
    const UnitTypeHierarchy types = hierarchy();
    SharedStash shared;
    // `setPlayer`: the same kind as the stash (softcore, +0x28 0)
    // does nothing.
    TL_CHECK(!shared.set_player(false));
    // `loadSharedStash`: an empty stash is read ...
    TL_CHECK(shared.begin_load());
    shared.inventory().pickup(item("Band", "Takeable"), 1, types);
    // ... one that holds anything is not.
    TL_CHECK(!shared.begin_load());
    TL_CHECK_EQ(shared.inventory().count_by_name("Band"), 1);
    // A hardcore player empties it and takes the other file.
    TL_CHECK(shared.set_player(true));
    TL_CHECK(shared.hardcore());
    TL_CHECK(shared.inventory().empty());
    TL_CHECK_EQ(std::string(shared.file_name()), std::string("sharedstashh.bin"));
    TL_CHECK(!shared.set_player(true));
}

TL_TEST(tray_count_label_is_four_fifty_or_one_twenty_six) {
    // `li r4,4`, `li r4,0x32`, `li r4,0x7e`.
    using tl::core::TrayOwner;
    TL_CHECK_EQ(tl::core::tray_count_capacity(TrayOwner::Combiner), 4);
    TL_CHECK_EQ(tl::core::tray_count_capacity(TrayOwner::Player), 50);
    TL_CHECK_EQ(tl::core::tray_count_capacity(TrayOwner::Stash), 50);
    TL_CHECK_EQ(tl::core::tray_count_capacity(TrayOwner::FirstPet), 50);
    TL_CHECK_EQ(tl::core::tray_count_capacity(TrayOwner::Other), 126);
}

TL_TEST(shared_stash_file_round_trips_and_refuses_another_version) {
    // `saveSharedStash`: version, count, the items.
    std::vector<tl::formats::ItemSaveState> items(2);
    items[0].name = "HEALTH_POTION";
    items[0].slot = 19;
    items[0].value = 5;
    items[1].name = "BAND";
    items[1].slot = 23;
    items[1].value = 1;
    const std::vector<uint8_t> bytes = tl::formats::write_shared_stash_file(items);
    TL_CHECK_EQ(bytes[0], uint8_t{23});
    TL_CHECK_EQ(bytes[4], uint8_t{2});
    std::vector<tl::formats::ItemSaveState> back;
    TL_CHECK(tl::formats::read_shared_stash_file(bytes.data(), bytes.size(), back));
    TL_CHECK_EQ(back.size(), size_t{2});
    TL_CHECK_EQ(back[0].name, std::string("HEALTH_POTION"));
    TL_CHECK_EQ(back[0].slot, 19);
    TL_CHECK_EQ(back[0].value, 5);
    TL_CHECK_EQ(back[1].slot, 23);
    std::vector<uint8_t> wrong = bytes;
    wrong[0] = 22;
    TL_CHECK(!tl::formats::read_shared_stash_file(wrong.data(), wrong.size(), back));
    TL_CHECK(!tl::formats::read_shared_stash_file(bytes.data(), 6, back));
}

}  // namespace
