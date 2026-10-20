// The player inventory: panes, slots, stacking, equipping and the class default items.
#include "core/inventory.h"
#include "core/character.h"
#include "core/item_use.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

namespace {

tl::core::ItemBook make_test_item_index() {
    tl::core::ItemBook index;

    tl::core::ItemStats sword;
    sword.name = "Rusty Blade";
    sword.unittype = "SWORD";
    sword.icon = "rustyblade.png";
    sword.min_damage = 5.0f;
    sword.max_damage = 10.0f;
    sword.value = 25;
    sword.max_stack = 1;
    index.add(sword);

    tl::core::ItemStats bow;
    bow.name = "Loose Shortbow";
    bow.unittype = "BOW";
    bow.icon = "shortbow.png";
    bow.min_damage = 4.0f;
    bow.max_damage = 8.0f;
    bow.value = 20;
    bow.max_stack = 1;
    index.add(bow);

    tl::core::ItemStats staff;
    staff.name = "Moldy Staff";
    staff.unittype = "STAFF";
    staff.icon = "moldystaff.png";
    staff.min_damage = 6.0f;
    staff.max_damage = 9.0f;
    staff.value = 30;
    staff.max_stack = 1;
    index.add(staff);

    tl::core::ItemStats hp;
    hp.name = "Health Potion";
    hp.unittype = "POTION";
    hp.icon = "healthpotion.png";
    hp.value = 10;
    hp.max_stack = 20;
    index.add(hp);

    tl::core::ItemStats mp;
    mp.name = "Mana Potion";
    mp.unittype = "POTION";
    mp.icon = "manapotion.png";
    mp.value = 10;
    mp.max_stack = 20;
    index.add(mp);

    tl::core::ItemStats tp;
    tp.name = "Town Portal Scroll";
    tp.unittype = "SCROLL";
    tp.icon = "portalscroll.png";
    tp.value = 50;
    tp.max_stack = 20;
    index.add(tp);

    tl::core::ItemStats id;
    id.name = "Identify Scroll";
    id.unittype = "SCROLL";
    id.icon = "identifyscroll.png";
    id.value = 50;
    id.max_stack = 20;
    index.add(id);

    return index;
}

}  // namespace

TL_TEST(inventory_starts_with_the_original_slot_layout) {
    // `CCharacter::unitInit`: `CInventory(this, 0x32)`: 19 equip locations,
    // then one 50-slot Backpack, its capacity.
    tl::core::Inventory inv;
    TL_CHECK(inv.empty());
    TL_CHECK(inv.size() == 0);
    TL_CHECK(inv.capacity() == 19 + 50);
    TL_CHECK(inv.item_capacity() == 50u);
    TL_CHECK(inv.gold() == 0);

    TL_CHECK(inv.pane_start(tl::core::InventoryPane::Backpack) == 19);
    TL_CHECK(inv.pane_size(tl::core::InventoryPane::Backpack) == 50);
    // A spell or a fish goes into the one pane there is.
    tl::core::ItemStats fish;
    fish.name = "Fish";
    fish.unittype = "FISH";
    TL_CHECK(inv.required_pane(fish, tl::core::UnitTypeHierarchy{}) ==
             tl::core::InventoryPane::Backpack);

    for (size_t i = 0; i < tl::core::Inventory::kFixedSlotCount; ++i) {
        TL_CHECK(tl::core::Inventory::is_equip_location(i));
    }
    TL_CHECK(!tl::core::Inventory::is_equip_location(19));
}

TL_TEST(inventory_item_stacking) {
    tl::core::Inventory inv;
    tl::core::UnitTypeHierarchy unit_types;
    tl::core::ItemStats potion;
    potion.name = "Health Potion";
    potion.unittype = "POTION";
    potion.max_stack = 20;

    const int slot = inv.pickup(potion, 5, unit_types);
    TL_CHECK(slot >= 0);
    TL_CHECK(inv.size() == 1);
    TL_CHECK(inv.count_by_name("Health Potion") == 5);

    // A second pickup merges onto the same stack rather than opening a new one.
    TL_CHECK(inv.pickup(potion, 10, unit_types) == slot);
    TL_CHECK(inv.size() == 1);
    TL_CHECK(inv.count_by_name("Health Potion") == 15);

    // Remove 7
    TL_CHECK(inv.remove_item(static_cast<size_t>(slot), 7));
    TL_CHECK(inv.size() == 1);
    TL_CHECK(inv.count_by_name("Health Potion") == 8);

    // Remove the rest: the slot empties but keeps its position.
    TL_CHECK(inv.remove_item(static_cast<size_t>(slot), 8));
    TL_CHECK(inv.empty());
    TL_CHECK(inv.count_by_name("Health Potion") == 0);
    TL_CHECK(inv.capacity() == 19 + 50);
}

TL_TEST(pickup_into_slot_replays_a_saved_position) {
    // What a loaded save does: `CInventory::pickupEquipment(CEquipment*, int,
    // bool)` places an item at an exact slot, equip location or not.
    tl::core::Inventory inv;
    tl::core::ItemStats blade;
    blade.name = "Rusty Blade";
    blade.max_stack = 1;

    TL_CHECK(inv.pickup_into_slot(blade, 1, tl::core::equip_location::kRightHand));
    TL_CHECK(inv.occupied(tl::core::equip_location::kRightHand));
    TL_CHECK(tl::core::Inventory::is_equip_location(tl::core::equip_location::kRightHand));
    TL_CHECK(inv.slots()[tl::core::equip_location::kRightHand].item.name == "Rusty Blade");
}

TL_TEST(swap_weapon_set_exchanges_the_two_hand_pairs) {
    // `CInventory::swapWeaponSet`: slot 0<->12, 1<->13.
    tl::core::Inventory inv;
    tl::core::ItemStats blade;
    blade.name = "Rusty Blade";
    tl::core::ItemStats shield;
    shield.name = "Buckler";

    inv.pickup_into_slot(blade, 1, tl::core::equip_location::kRightHand);
    inv.pickup_into_slot(shield, 1, tl::core::equip_location::kLeftHand);
    TL_CHECK(!inv.occupied(tl::core::equip_location::kRightHand2));
    TL_CHECK(!inv.occupied(tl::core::equip_location::kLeftHand2));

    inv.swap_weapon_set();
    TL_CHECK(!inv.occupied(tl::core::equip_location::kRightHand));
    TL_CHECK(!inv.occupied(tl::core::equip_location::kLeftHand));
    TL_CHECK(inv.slots()[tl::core::equip_location::kRightHand2].item.name == "Rusty Blade");
    TL_CHECK(inv.slots()[tl::core::equip_location::kLeftHand2].item.name == "Buckler");

    inv.swap_weapon_set();
    TL_CHECK(inv.slots()[tl::core::equip_location::kRightHand].item.name == "Rusty Blade");
    TL_CHECK(inv.slots()[tl::core::equip_location::kLeftHand].item.name == "Buckler");
}

TL_TEST(unequip_returns_the_item_to_the_backpack) {
    tl::core::Inventory inv;
    tl::core::UnitTypeHierarchy unit_types;
    tl::core::ItemStats blade;
    blade.name = "Rusty Blade";

    inv.pickup_into_slot(blade, 1, tl::core::equip_location::kRightHand);
    TL_CHECK(inv.unequip(tl::core::equip_location::kRightHand, unit_types));
    TL_CHECK(!inv.occupied(tl::core::equip_location::kRightHand));
    TL_CHECK(inv.find_by_name("Rusty Blade") == inv.pane_start(tl::core::InventoryPane::Backpack));
}

namespace {

/// A class file's `[EQUIPMENT]` with one key, as the three shipped classes author theirs.
tl::formats::AdmSection class_with(const std::string& key, const std::string& item) {
    tl::formats::AdmSection unit;
    tl::formats::AdmSection equipment;
    equipment.name = "EQUIPMENT";
    tl::formats::AdmRecord r;
    r.key = key;
    r.type = tl::formats::AdmType::String;
    r.text = item;
    equipment.records.push_back(r);
    unit.children.push_back(equipment);
    return unit;
}

tl::core::UnitTypeHierarchy wear_types();

/// A new character's inventory: its class's equipment, then `firstTimeSetup`.
void start(tl::core::Inventory& inv, const tl::formats::AdmSection& unit,
           const tl::core::ItemBook& items, const tl::core::UnitTypeHierarchy& unit_types,
           bool fish) {
    inv.create_default_equipment(tl::core::read_default_equipment(unit), items, unit_types);
    tl::core::StartingGifts gifts;
    gifts.launch_promo = fish;
    inv.first_time_setup(items, unit_types, gifts);
}

}  // namespace

TL_TEST(default_equipment_is_read_in_the_consoles_order) {
    tl::formats::AdmSection unit = class_with("WEAPON", "Rusty Blade");
    tl::formats::AdmRecord r;
    r.type = tl::formats::AdmType::String;
    r.key = "SPAWNCLASS";
    r.text = "Starter";
    unit.children[0].records.push_back(r);
    r.key = "CHEST";
    r.text = "Tunic";
    unit.children[0].records.push_back(r);
    r.key = "HELMET";
    r.text = "";
    unit.children[0].records.push_back(r);
    const std::vector<tl::core::DefaultEquipment> e = tl::core::read_default_equipment(unit);
    TL_CHECK_EQ(e.size(), static_cast<size_t>(3));
    TL_CHECK(e[0].name == "Tunic" && e[0].location == tl::core::equip_location::kChest &&
             !e[0].spawn_class);
    TL_CHECK(e[1].name == "Rusty Blade" && e[1].location == 12 && !e[1].spawn_class);
    TL_CHECK(e[2].name == "Starter" && e[2].location == 12 && e[2].spawn_class);
}

TL_TEST(destroyer_default_inventory_has_rusty_blade_and_potions) {
    tl::core::Inventory inv;
    tl::core::ItemBook items = make_test_item_index();
    const tl::core::UnitTypeHierarchy unit_types = wear_types();

    // Without the Xbox release's extra, which has a test of its own below.
    start(inv, class_with("RIGHTHAND", "Rusty Blade"), items, unit_types, false);

    // 1 equipped Rusty Blade, 2 Health Potions, 2 Mana Potions, 2 Town Portal
    // Scrolls, 2 Identify Scrolls.
    TL_CHECK(inv.size() == 5);
    TL_CHECK(inv.count_by_name("Health Potion") == 2);
    TL_CHECK(inv.count_by_name("Mana Potion") == 2);
    TL_CHECK(inv.count_by_name("Town Portal Scroll") == 2);
    TL_CHECK(inv.count_by_name("Identify Scroll") == 2);

    const int idx = inv.find_by_name("Rusty Blade");
    TL_CHECK(idx == tl::core::equip_location::kRightHand);
    TL_CHECK(tl::core::Inventory::is_equip_location(static_cast<size_t>(idx)));
}

TL_TEST(vanquisher_default_inventory_has_shortbow_and_potions) {
    tl::core::Inventory inv;
    tl::core::ItemBook items = make_test_item_index();
    const tl::core::UnitTypeHierarchy unit_types = wear_types();

    start(inv, class_with("LEFTHAND", "Loose Shortbow"), items, unit_types, false);

    TL_CHECK(inv.size() == 5);
    TL_CHECK(inv.count_by_name("Health Potion") == 2);
    TL_CHECK(inv.count_by_name("Mana Potion") == 2);
    TL_CHECK(inv.count_by_name("Town Portal Scroll") == 2);
    TL_CHECK(inv.count_by_name("Identify Scroll") == 2);

    const int idx = inv.find_by_name("Loose Shortbow");
    TL_CHECK(idx == tl::core::equip_location::kLeftHand);
}

TL_TEST(alchemist_default_inventory_has_moldy_staff_and_potions) {
    tl::core::Inventory inv;
    tl::core::ItemBook items = make_test_item_index();
    const tl::core::UnitTypeHierarchy unit_types = wear_types();

    start(inv, class_with("RIGHTHAND", "Moldy Staff"), items, unit_types, false);

    TL_CHECK(inv.size() == 5);
    TL_CHECK(inv.count_by_name("Health Potion") == 2);
    TL_CHECK(inv.count_by_name("Mana Potion") == 2);
    TL_CHECK(inv.count_by_name("Town Portal Scroll") == 2);
    TL_CHECK(inv.count_by_name("Identify Scroll") == 2);

    const int idx = inv.find_by_name("Moldy Staff");
    TL_CHECK(idx == tl::core::equip_location::kRightHand);
}

/// A key naming no item makes nothing (`createEquipment` returns null).
TL_TEST(default_equipment_naming_no_item_makes_nothing) {
    tl::core::Inventory inv;
    tl::core::ItemBook items = make_test_item_index();
    const tl::core::UnitTypeHierarchy unit_types = wear_types();
    inv.create_default_equipment(
        tl::core::read_default_equipment(class_with("RIGHTHAND", "No Such Blade")), items,
        unit_types);
    TL_CHECK(inv.size() == 0);
}

/// The Xbox release's own starting item, `TrollFish_Giant`, given when `LAUNCHPROMO` is 1.
TL_TEST(a_new_character_carries_the_xbox_starting_fish) {
    tl::core::ItemBook items = make_test_item_index();
    items.add(tl::core::xbox_starting_fish());
    const tl::core::UnitTypeHierarchy unit_types = wear_types();

    tl::core::Inventory with;
    start(with, class_with("RIGHTHAND", "Moldy Staff"), items, unit_types, true);
    TL_CHECK(with.size() == 6);

    const int idx = with.find_by_name("TrollFish_Giant");
    TL_CHECK(idx >= 0);
    const tl::core::InventorySlot& slot = with.slots()[static_cast<size_t>(idx)];
    TL_CHECK(slot.count == 1);
    TL_CHECK(!tl::core::Inventory::is_equip_location(static_cast<size_t>(idx)));

    // The permanent one, not the PC's thirty-second `Troll Fish` of the same name.
    TL_CHECK(slot.item.display_name == "Improbably Large Fish");
    TL_CHECK(slot.item.unittype == "FISH");
    TL_CHECK_EQ(slot.item.effects.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(slot.item.effects[0].type, tl::core::kEffectTypeTransformPermanent);
    TL_CHECK(slot.item.effects[0].name == "Pet Troll");
    TL_CHECK_EQ(slot.item.effects[0].duration, tl::core::kDurationInstant);
    // And it can be used: one use, as the record authors.
    TL_CHECK_EQ(slot.item.uses, 1);

    tl::core::Inventory without;
    start(without, class_with("RIGHTHAND", "Moldy Staff"), items, unit_types, false);
    TL_CHECK(without.size() == 5);
    TL_CHECK(without.find_by_name("TrollFish_Giant") < 0);
}

namespace {

/// `QuestItem` (103: item 2, any 0) and `Potion` (33: 1, takeable 31, item 2,
/// any 0), as `media/UNITTYPES.HIE` authors them.
tl::core::UnitTypeHierarchy quest_item_types() {
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
    types.children.push_back(type("QuestItem", {103, 2, 0}));
    types.children.push_back(type("Potion", {33, 1, 31, 2, 0}));
    tl::formats::AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

tl::core::ItemStats quest_copy(const std::string& name, const std::string& type, int64_t quest) {
    tl::core::ItemStats item;
    item.name = name;
    item.unittype = type;
    item.max_stack = 20;
    item.quest_guid = quest;
    return item;
}

}  // namespace

// `CPlayer::removeQuestItemFromInventoryOrPetsInventoryByGuid`.
TL_TEST(a_quest_takes_back_only_its_own_copies) {
    const tl::core::UnitTypeHierarchy types = quest_item_types();
    tl::core::Inventory inv;
    constexpr int64_t kVasman = -7000;
    inv.pickup_into_slot(quest_copy("GleamingEmber", "QUESTITEM", kVasman), 1, 19);
    inv.pickup_into_slot(quest_copy("GleamingEmber", "QUESTITEM", 42), 1, 20);  // another quest's
    inv.pickup_into_slot(quest_copy("GleamingEmber", "QUESTITEM", -1), 1, 21);  // nobody's

    // `cleanUpForQuestRemoveall`'s call: the unit, whole stacks, `ISA(0)`.
    TL_CHECK(inv.remove_quest_items(kVasman, "gleamingember", true, 0, types));
    TL_CHECK_EQ(inv.count_by_name("GleamingEmber"), 2);
    TL_CHECK_EQ(inv.slots()[19].count, 0);
    TL_CHECK(!inv.remove_quest_items(kVasman, "GleamingEmber", true, 0, types));
}

TL_TEST(a_quest_wide_clean_up_takes_one_from_each_quest_item_stack) {
    const tl::core::UnitTypeHierarchy types = quest_item_types();
    tl::core::Inventory inv;
    constexpr int64_t kQuest = 77;
    inv.pickup_into_slot(quest_copy("Relic", "QUESTITEM", kQuest), 3, 19);
    inv.pickup_into_slot(quest_copy("Shard", "QUESTITEM", kQuest), 1, 20);
    inv.pickup_into_slot(quest_copy("Health Potion", "POTION", kQuest), 2, 21);  // not a QuestItem

    // `cleanUp`'s last call: any unit, one off each stack, `ISA(0x67)`.
    TL_CHECK(inv.remove_quest_items(kQuest, "", false, tl::core::unit_type::kQuestItem, types));
    TL_CHECK_EQ(inv.slots()[19].count, 2);
    TL_CHECK_EQ(inv.slots()[20].count, 0);
    TL_CHECK_EQ(inv.slots()[21].count, 2);
}

// --------------------------------------------------------------------------
// Wearing: `CInventory::canEquip`, `getComparisonItems`,
// `equipEquipmentIntoFirstFreeLocation` and the console's A.
//

namespace {

tl::formats::AdmSection type_group(const char* name, uint32_t id,
                                   std::initializer_list<uint32_t> up) {
    tl::formats::AdmSection group;
    group.name = name;
    tl::formats::AdmRecord record;
    record.key = "ID";
    record.type = tl::formats::AdmType::Integer;
    record.integer = id;
    group.records.push_back(record);
    for (uint32_t ancestor : up) {
        record.key = "child";
        record.integer = ancestor;
        group.records.push_back(record);
    }
    return group;
}

/// The ancestries `unittypes.hie` gives these types.
tl::core::UnitTypeHierarchy wear_types() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(type_group("SWORD", 11, {9, 8, 31, 2, 0}));
    all.children.push_back(type_group("STAFF", 61, {8, 31, 2, 0, 10}));
    all.children.push_back(type_group("BOW", 36, {35, 8, 31, 2, 0, 10, 38, 102, 163}));
    all.children.push_back(type_group("SHIELD", 21, {13, 31, 2, 0, 9}));
    all.children.push_back(type_group("CHEST ARMOR", 15, {13, 31, 2, 0}));
    all.children.push_back(type_group("RING", 17, {39, 31, 2, 0}));
    all.children.push_back(type_group("POTION", 33, {1, 31, 2, 0}));
    all.children.push_back(type_group("QUESTITEM", 103, {31, 2, 0}));
    root.children.push_back(all);
    tl::core::UnitTypeHierarchy types;
    types.parse(root);
    return types;
}

tl::core::ItemStats thing(const char* name, const char* unittype) {
    tl::core::ItemStats item;
    item.name = name;
    item.display_name = name;
    item.unittype = unittype;
    item.max_stack = 1;
    return item;
}

std::string at(const tl::core::Inventory& inv, size_t slot) {
    return inv.occupied(slot) ? inv.slots()[slot].item.name : std::string();
}

/// The backpack filled one to a slot, leaving `leave` slots free at its end.
void fill_backpack(tl::core::Inventory& inv, int leave) {
    const int begin = inv.pane_start(tl::core::InventoryPane::Backpack);
    const int end = begin + inv.pane_size(tl::core::InventoryPane::Backpack) - leave;
    for (int i = begin; i < end; ++i) {
        if (!inv.occupied(static_cast<size_t>(i)))
            inv.pickup_into_slot(thing("Rock", "QUESTITEM"), 1, i);
    }
}

}  // namespace

TL_TEST(can_equip_wants_a_free_location_and_free_hands) {
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;
    tl::core::Inventory inv;
    const tl::core::ItemStats sword = thing("Sword", "SWORD");
    const tl::core::ItemStats staff = thing("Staff", "STAFF");
    const tl::core::ItemStats shield = thing("Shield", "SHIELD");
    const tl::core::ItemStats vest = thing("Vest", "CHEST ARMOR");

    TL_CHECK(inv.can_equip(sword, types, nullptr));
    TL_CHECK(inv.can_equip(staff, types, nullptr));
    TL_CHECK(!inv.can_equip(thing("Potion", "POTION"), types, nullptr));  // no category
    TL_CHECK(!inv.can_equip(thing("Rock", "QUESTITEM"), types, nullptr));

    // A sword in the right hand: a second goes in the left, a staff nowhere.
    inv.pickup_into_slot(sword, 1, kRightHand);
    TL_CHECK(inv.can_equip(sword, types, nullptr));
    TL_CHECK(inv.can_equip(shield, types, nullptr));
    TL_CHECK(!inv.can_equip(staff, types, nullptr));
    // ... unless the staff is the thing in that hand (the pointer test).
    TL_CHECK(inv.can_equip(sword, types, nullptr, nullptr, kRightHand));

    // A staff in the right hand: no weapon and no shield beside it.
    tl::core::Inventory two;
    two.pickup_into_slot(staff, 1, kRightHand);
    TL_CHECK(!two.can_equip(sword, types, nullptr));
    TL_CHECK(!two.can_equip(shield, types, nullptr));
    TL_CHECK(two.can_equip(vest, types, nullptr));
    TL_CHECK(two.can_equip(staff, types, nullptr, nullptr, kRightHand));

    // The one location a chest armour has, taken.
    two.pickup_into_slot(vest, 1, kChest);
    TL_CHECK(!two.can_equip(vest, types, nullptr));

    // An unidentified copy, for a character that answers to requirements.
    tl::core::ItemStats unknown = sword;
    unknown.identified = false;
    tl::core::CharacterProgression character;
    TL_CHECK(inv.item_can_equip(unknown, types, nullptr));
    TL_CHECK(!inv.item_can_equip(unknown, types, &character));
}

TL_TEST(comparison_items_are_the_worn_things_in_the_way) {
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;
    tl::core::Inventory inv;
    inv.pickup_into_slot(thing("Sword", "SWORD"), 1, kRightHand);
    inv.pickup_into_slot(thing("Shield", "SHIELD"), 1, kLeftHand);
    inv.pickup_into_slot(thing("Ring A", "RING"), 1, kRing1);
    inv.pickup_into_slot(thing("Ring B", "RING"), 1, kRing2);

    int first = -1, second = -1;
    inv.comparison_items(thing("Staff", "STAFF"), types, -1, first, second);
    TL_CHECK_EQ(first, kRightHand);  // a weapon meets the worn weapon
    TL_CHECK_EQ(second, kLeftHand);  // and what sits where a weapon may go

    first = second = -1;
    inv.comparison_items(thing("Ring C", "RING"), types, -1, first, second);
    TL_CHECK_EQ(first, kRing1);
    TL_CHECK_EQ(second, kRing2);

    first = second = -1;
    inv.comparison_items(thing("Vest", "CHEST ARMOR"), types, -1, first, second);
    TL_CHECK_EQ(first, -1);
    TL_CHECK_EQ(second, -1);

    // A shield is no weapon: only the hand its type allows.
    first = second = -1;
    inv.comparison_items(thing("Buckler", "SHIELD"), types, -1, first, second);
    TL_CHECK_EQ(first, kLeftHand);
    TL_CHECK_EQ(second, -1);
}

TL_TEST(wearing_an_item_goes_into_the_first_free_location) {
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;
    tl::core::Inventory inv;
    const int vest = inv.pickup(thing("Vest", "CHEST ARMOR"), 1, types);
    const int sword = inv.pickup(thing("Sword", "SWORD"), 1, types);
    const int second = inv.pickup(thing("Dirk", "SWORD"), 1, types);

    tl::core::Inventory::WearOutcome out = inv.wear_item(static_cast<size_t>(vest), types, nullptr);
    TL_CHECK(out.equipped);
    TL_CHECK_EQ(out.location, kChest);
    TL_CHECK_EQ(out.sounded.size(), 1u);
    TL_CHECK(!inv.occupied(static_cast<size_t>(vest)));

    out = inv.wear_item(static_cast<size_t>(sword), types, nullptr);
    TL_CHECK_EQ(out.location, kRightHand);
    // A second one-handed weapon takes the other hand; nothing comes off.
    out = inv.wear_item(static_cast<size_t>(second), types, nullptr);
    TL_CHECK_EQ(out.location, kLeftHand);
    TL_CHECK_EQ(at(inv, kRightHand), std::string("Sword"));
    TL_CHECK_EQ(at(inv, kLeftHand), std::string("Dirk"));

    // Not a thing to wear: nothing moves.
    const int potion = inv.pickup(thing("Potion", "POTION"), 1, types);
    out = inv.wear_item(static_cast<size_t>(potion), types, nullptr);
    TL_CHECK(!out.equipped);
    TL_CHECK(out.sounded.empty());
    TL_CHECK(inv.occupied(static_cast<size_t>(potion)));
}

TL_TEST(wearing_an_item_changes_places_with_what_is_in_its_way) {
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;

    // A sword for a staff: the staff comes off into the bag.
    tl::core::Inventory inv;
    inv.pickup_into_slot(thing("Staff", "STAFF"), 1, kRightHand);
    const int sword = inv.pickup(thing("Sword", "SWORD"), 1, types);
    tl::core::Inventory::WearOutcome out =
        inv.wear_item(static_cast<size_t>(sword), types, nullptr);
    TL_CHECK(out.equipped);
    TL_CHECK_EQ(out.location, kRightHand);
    TL_CHECK_EQ(at(inv, kRightHand), std::string("Sword"));
    TL_CHECK_EQ(inv.count_by_name("Staff"), 1);
    TL_CHECK(out.dropped.empty());
    // The item, what came off, and the item again.
    TL_CHECK_EQ(out.sounded.size(), 3u);
    TL_CHECK_EQ(out.sounded[0].name, std::string("Sword"));
    TL_CHECK_EQ(out.sounded[1].name, std::string("Staff"));
    TL_CHECK_EQ(out.sounded[2].name, std::string("Sword"));
    // It took the slot the sword left: the first free one.
    TL_CHECK_EQ(at(inv, static_cast<size_t>(sword)), std::string("Staff"));

    // A staff for a sword and a shield: both come off.
    tl::core::Inventory both;
    both.pickup_into_slot(thing("Sword", "SWORD"), 1, kRightHand);
    both.pickup_into_slot(thing("Shield", "SHIELD"), 1, kLeftHand);
    const int staff = both.pickup(thing("Staff", "STAFF"), 1, types);
    out = both.wear_item(static_cast<size_t>(staff), types, nullptr);
    TL_CHECK(out.equipped);
    TL_CHECK_EQ(at(both, kRightHand), std::string("Staff"));
    TL_CHECK(!both.occupied(kLeftHand));
    TL_CHECK_EQ(both.count_by_name("Sword"), 1);
    TL_CHECK_EQ(both.count_by_name("Shield"), 1);

    // A third ring takes the first ring's place and leaves the second.
    tl::core::Inventory rings;
    rings.pickup_into_slot(thing("Ring A", "RING"), 1, kRing1);
    rings.pickup_into_slot(thing("Ring B", "RING"), 1, kRing2);
    const int third = rings.pickup(thing("Ring C", "RING"), 1, types);
    out = rings.wear_item(static_cast<size_t>(third), types, nullptr);
    TL_CHECK_EQ(out.location, kRing1);
    TL_CHECK_EQ(at(rings, kRing1), std::string("Ring C"));
    TL_CHECK_EQ(at(rings, kRing2), std::string("Ring B"));
}

TL_TEST(a_displaced_item_goes_back_to_the_bag_even_at_capacity) {
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;

    // A staff for a sword and a shield at capacity, worn ones counted.
    tl::core::Inventory inv;
    inv.pickup_into_slot(thing("Sword", "SWORD"), 1, kRightHand);
    inv.pickup_into_slot(thing("Shield", "SHIELD"), 1, kLeftHand);
    const int staff = inv.pickup(thing("Staff", "STAFF"), 1, types);
    fill_backpack(inv, 2);
    TL_CHECK_EQ(inv.size(), inv.item_capacity());
    tl::core::Inventory::WearOutcome out =
        inv.wear_item(static_cast<size_t>(staff), types, nullptr);
    TL_CHECK(out.equipped);
    TL_CHECK_EQ(at(inv, kRightHand), std::string("Staff"));
    TL_CHECK_EQ(inv.count_by_name("Sword"), 1);
    TL_CHECK_EQ(inv.count_by_name("Shield"), 1);
    TL_CHECK(out.dropped.empty());
    // Not back on beside the staff.
    TL_CHECK(!inv.occupied(kLeftHand));

    // Taking a thing off at capacity: its own going makes the room, and it goes
    // to the bag (`removeEquipment`, then `pickupEquipment`).
    tl::core::Inventory full;
    full.pickup_into_slot(thing("Vest", "CHEST ARMOR"), 1, kChest);
    fill_backpack(full, 1);
    out = full.take_off_item(kChest, types);
    TL_CHECK(out.taken_off);
    TL_CHECK(!full.occupied(kChest));
    TL_CHECK(out.dropped.empty());
    TL_CHECK_EQ(full.count_by_name("Vest"), 1);

    // With room it goes to the bag and sounds.
    tl::core::Inventory roomy;
    roomy.pickup_into_slot(thing("Vest", "CHEST ARMOR"), 1, kChest);
    out = roomy.take_off_item(kChest, types);
    TL_CHECK(out.taken_off && out.dropped.empty());
    TL_CHECK_EQ(out.sounded.size(), 1u);
    TL_CHECK_EQ(roomy.count_by_name("Vest"), 1);
    TL_CHECK(!roomy.occupied(kChest));
}

TL_TEST(a_full_pane_puts_a_picked_up_item_on_unless_the_hands_forbid_it) {
    // `pickupEquipment(CEquipment*, bool)`: at capacity only a stack it
    // joins; a full pane falls to the first equip location that takes it.
    const tl::core::UnitTypeHierarchy types = wear_types();
    using namespace tl::core::equip_location;
    tl::core::Inventory inv;
    fill_backpack(inv, 0);
    TL_CHECK_EQ(inv.pickup(thing("Sword", "SWORD"), 1, types, false), -1);
    TL_CHECK_EQ(inv.pickup(thing("Sword", "SWORD"), 1, types), -1);
    // With room left and the item's own pane full: a merchant-shaped one, its
    // weapons in a pane of one slot (`addSection`), six items at most.
    tl::core::Inventory shop(6);
    shop.add_pane(tl::core::InventoryPane::Weapon, 1);
    TL_CHECK(shop.pickup_into_slot(thing("Old Sword", "SWORD"), 1, 25));
    TL_CHECK(shop.size() < shop.item_capacity());
    TL_CHECK_EQ(shop.pickup(thing("Sword", "SWORD"), 1, types, false), -1);
    TL_CHECK_EQ(shop.pickup(thing("Sword", "SWORD"), 1, types), static_cast<int>(kRightHand));
    // A two-handed item with a hand in use is refused outright.
    TL_CHECK_EQ(shop.pickup(thing("Staff", "STAFF"), 1, types), -1);
}

TL_TEST(an_identify_scroll_marks_an_unidentified_item_and_is_spent) {
    // with `CEquipment::isEffectValidForUnit` and
    // `applyEffectOnUnit`.
    tl::core::ItemStats scroll = thing("Identify Scroll", "SCROLL");
    scroll.max_stack = 10;
    scroll.uses = 1;
    tl::core::Effect identify;
    identify.name = "IDENTIFY";
    identify.type = tl::core::kEffectTypeIdentify;
    scroll.effects.push_back(identify);

    tl::core::ItemStats sword = thing("Sword", "SWORD");
    sword.identified = false;

    // Off a stack of three: one goes, the copy stays.
    int uses = scroll.uses;
    tl::core::ItemUseResult used = tl::core::use_on_item(scroll, 3, uses, sword);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK(sword.identified);
    TL_CHECK(used.play_use_sound);
    TL_CHECK(!used.remove_item);
    TL_CHECK_EQ(uses, 1);

    // An identified item takes nothing, and nothing is spent.
    used = tl::core::use_on_item(scroll, 3, uses, sword);
    TL_CHECK_EQ(used.applied, 0);
    TL_CHECK(!used.play_use_sound);
    TL_CHECK(!used.remove_item);

    // The last of a stack spends its use and goes.
    sword.identified = false;
    used = tl::core::use_on_item(scroll, 1, uses, sword);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK_EQ(uses, 0);
    TL_CHECK(used.remove_item);

    // A spent copy does nothing.
    sword.identified = false;
    used = tl::core::use_on_item(scroll, 1, uses, sword);
    TL_CHECK_EQ(used.applied, 0);
    TL_CHECK(!sword.identified);
}

TL_TEST(an_inventory_tells_its_items_they_came_and_went) {
    // `addedToInventory` on a new slot or a merge; `removedFromInventory` once the
    // item's ref is gone, not for a stack that only shrinks.
    tl::core::Inventory inv;
    tl::core::UnitTypeHierarchy unit_types;
    std::vector<std::string> heard;
    tl::core::InventoryItemEvents events;
    events.added = [&heard](tl::core::ItemStats& item) {
        heard.push_back("+" + item.name);
        item.quest_guid = 77;  // what a counted pickup stamps
    };
    events.removed = [&heard](const tl::core::ItemStats& item) {
        heard.push_back("-" + item.name);
    };
    inv.set_item_events(&events);

    tl::core::ItemStats potion;
    potion.name = "Health Potion";
    potion.unittype = "POTION";
    potion.max_stack = 20;
    const int slot = inv.pickup(potion, 5, unit_types);
    TL_CHECK(slot >= 0);
    TL_CHECK_EQ(heard.size(), 1u);
    // A new slot keeps what the event wrote on the item.
    TL_CHECK(inv.slots()[static_cast<size_t>(slot)].item.quest_guid == 77);

    TL_CHECK(inv.pickup(potion, 3, unit_types) == slot);  // a merge
    TL_CHECK_EQ(heard.size(), 2u);
    TL_CHECK(inv.remove_item(static_cast<size_t>(slot), 2));  // the stack shrinks
    TL_CHECK_EQ(heard.size(), 2u);
    TL_CHECK(inv.remove_item(static_cast<size_t>(slot), 6));  // the item goes
    TL_CHECK_EQ(heard.size(), 3u);
    TL_CHECK_EQ(heard[0], std::string("+Health Potion"));
    TL_CHECK_EQ(heard[1], std::string("+Health Potion"));
    TL_CHECK_EQ(heard[2], std::string("-Health Potion"));
}

TL_TEST(an_inventorys_listener_is_its_places_not_its_contents) {
    // A copy is nobody's bag yet (a save being read, a bag carried across a
    // warp), and a bag assigned over keeps who was listening to it.
    tl::core::UnitTypeHierarchy unit_types;
    int heard = 0;
    tl::core::InventoryItemEvents events;
    events.added = [&heard](tl::core::ItemStats&) { ++heard; };
    tl::core::Inventory bag;
    bag.set_item_events(&events);

    tl::core::ItemStats blade;
    blade.name = "Rusty Blade";
    blade.max_stack = 1;

    tl::core::Inventory copy = bag;
    TL_CHECK(copy.pickup(blade, 1, unit_types) >= 0);
    TL_CHECK_EQ(heard, 0);

    bag = copy;  // the copy's contents, the bag's own listener
    TL_CHECK(bag.count_by_name("Rusty Blade") == 1);
    TL_CHECK(bag.pickup(blade, 1, unit_types) >= 0);
    TL_CHECK_EQ(heard, 1);
}
