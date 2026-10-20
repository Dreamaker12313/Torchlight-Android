// The console housing screen's navigation: `CMasterHousingXenon`, `CInventoryMenuXenon` and
// `CInventoryXenon`.
#include "core/inventory_screen.h"
#include "core/unit_types.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

using tl::core::HousingTab;
using tl::core::InventoryFilter;
using tl::core::InventorySlotAction;

/// Read off console screenshots: Skills - Quest - Inventory - Stats - Skills;
/// `CMasterHousingXenon`'s step wraps at both ends.
TL_TEST(housing_tab_ring_wraps) {
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Quest, -1) == HousingTab::Skills);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Skills, -1) == HousingTab::Stats);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Quest, 1) == HousingTab::Inventory);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Inventory, 1) == HousingTab::Stats);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Stats, 1) == HousingTab::Skills);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Skills, 1) == HousingTab::Quest);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Stats, -1) == HousingTab::Inventory);
    TL_CHECK(tl::core::step_housing_tab(HousingTab::Inventory, -1) == HousingTab::Quest);
}

/// `inventorymenu_xenon.layout`'s six filters, in its own order, wrapping.
TL_TEST(inventory_filter_ring_wraps) {
    TL_CHECK(tl::core::step_inventory_filter(InventoryFilter::Weapons, -1) ==
             InventoryFilter::Fish);
    TL_CHECK(tl::core::step_inventory_filter(InventoryFilter::Fish, 1) == InventoryFilter::Weapons);
    TL_CHECK(tl::core::step_inventory_filter(InventoryFilter::Armor, 1) ==
             InventoryFilter::Jewelry);
}

TL_TEST(selected_index_clamps) {
    TL_CHECK_EQ(tl::core::step_selected_index(0, -1, 5), 0);
    TL_CHECK_EQ(tl::core::step_selected_index(0, 1, 5), 1);
    TL_CHECK_EQ(tl::core::step_selected_index(4, 1, 5), 4);
    TL_CHECK_EQ(tl::core::step_selected_index(2, 1, 0), 2);
}

TL_TEST(selected_quest_clamps_to_log_size) {
    TL_CHECK_EQ(tl::core::step_selected_quest(0, -1, 6), 0);
    TL_CHECK_EQ(tl::core::step_selected_quest(5, 1, 6), 5);
    TL_CHECK_EQ(tl::core::step_selected_quest(2, 1, 6), 3);
    TL_CHECK_EQ(tl::core::step_selected_quest(2, 1, 0), 0);
}

/// `inventorymenu_xenon.layout`'s default legend: "Unequip" for a paperdoll
/// slot, the opposite for a backpack one.
TL_TEST(slot_action_follows_equip_location) {
    TL_CHECK(tl::core::inventory_slot_action(0, true) == InventorySlotAction::Unequip);
    TL_CHECK(tl::core::inventory_slot_action(18, true) == InventorySlotAction::Unequip);
    TL_CHECK(tl::core::inventory_slot_action(19, true) == InventorySlotAction::Equip);
    TL_CHECK(tl::core::inventory_slot_action(19, false) == InventorySlotAction::None);
}

namespace {

/// The types the console's categoriser tests, with their ancestors as
/// `media/UNITTYPES.HIE` authors them.
tl::core::UnitTypeHierarchy filter_types() {
    auto type = [](const std::string& name, const std::vector<uint32_t>& ids) {
        tl::formats::AdmSection group;
        group.name = name;
        for (size_t i = 0; i < ids.size(); ++i) {
            tl::formats::AdmRecord r;
            r.key = i == 0 ? "ID" : "child";
            r.type = tl::formats::AdmType::Integer;
            r.integer = ids[i];
            group.records.push_back(r);
        }
        return group;
    };
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("QUESTITEM", {103, 2, 0}));
    types.children.push_back(type("POTION", {33, 1, 31, 2, 0}));
    types.children.push_back(type("SCROLL", {42, 1, 31, 2, 0, 126}));
    types.children.push_back(type("MAP", {172, 1, 31, 2, 0, 126}));
    types.children.push_back(type("SPELL", {129, 1, 31, 2, 0}));
    types.children.push_back(type("SWORD", {11, 9, 8, 31, 2, 0, 60, 162}));
    types.children.push_back(type("CHEST ARMOR", {15, 13, 31, 2, 0}));
    types.children.push_back(type("BELT", {18, 13, 31, 2, 0}));
    types.children.push_back(type("RING", {17, 39, 31, 2, 0, 164}));
    types.children.push_back(type("NECKLACE", {24, 39, 31, 2, 0, 164}));
    types.children.push_back(type("SOCKETABLE", {120, 31, 2, 0}));
    types.children.push_back(type("FISH", {124, 123, 1, 31, 2, 0, 125}));
    types.children.push_back(type("TRINKET", {39, 31, 2, 0}));
    tl::formats::AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

}  // namespace

// the Gleaming Ember is a `QUESTITEM` and sits with the
// potions, not with the weapons.
TL_TEST(inventory_filter_of_sorts_by_unit_type_as_the_console_does) {
    const tl::core::UnitTypeHierarchy types = filter_types();
    TL_CHECK_EQ(tl::core::inventory_filter_of("QUESTITEM", types), 3);
    TL_CHECK_EQ(tl::core::inventory_filter_of("POTION", types), 3);
    TL_CHECK_EQ(tl::core::inventory_filter_of("SCROLL", types), 3);
    TL_CHECK_EQ(tl::core::inventory_filter_of("MAP", types), 3);
    TL_CHECK_EQ(tl::core::inventory_filter_of("SPELL", types), 3);
    TL_CHECK_EQ(tl::core::inventory_filter_of("SWORD", types), 0);
    TL_CHECK_EQ(tl::core::inventory_filter_of("CHEST ARMOR", types), 1);
    TL_CHECK_EQ(tl::core::inventory_filter_of("BELT", types), 1);
    TL_CHECK_EQ(tl::core::inventory_filter_of("RING", types), 2);
    TL_CHECK_EQ(tl::core::inventory_filter_of("NECKLACE", types), 2);
    TL_CHECK_EQ(tl::core::inventory_filter_of("SOCKETABLE", types), 4);
    TL_CHECK_EQ(tl::core::inventory_filter_of("FISH", types), 5);
    // A pet's trinket is none of them, and is listed under no tab.
    TL_CHECK_EQ(tl::core::inventory_filter_of("TRINKET", types), tl::core::kInventoryFilterCount);
    // A combiner's menu puts everything on the first.
    TL_CHECK_EQ(tl::core::inventory_filter_of("QUESTITEM", types, true), 0);
}

TL_TEST(tray_filter_names_are_the_consoles) {
    TL_CHECK(std::string(tl::core::inventory_filter_name(0)) == "Weapons");
    TL_CHECK(std::string(tl::core::inventory_filter_name(3)) == "Miscellaneous");
    TL_CHECK(std::string(tl::core::inventory_filter_name(4)) == "Socketables");
    TL_CHECK(std::string(tl::core::inventory_filter_name(5)) == "Fish");
    TL_CHECK(std::string(tl::core::inventory_filter_name(2, true)) == "Transmute");
}

TL_TEST(a_tray_tab_steps_off_either_end_into_the_next_tray) {
    int tab = 0;
    TL_CHECK_EQ(tl::core::step_tray_tab(tab, -1), -1);
    TL_CHECK_EQ(tab, 0);
    TL_CHECK_EQ(tl::core::step_tray_tab(tab, 1), 0);
    TL_CHECK_EQ(tab, 1);
    tab = 5;
    TL_CHECK_EQ(tl::core::step_tray_tab(tab, 1), 1);
    TL_CHECK_EQ(tab, 5);
    TL_CHECK_EQ(tl::core::step_tray_tab(tab, 1, true), 1);
}
