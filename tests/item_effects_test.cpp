// An item copy's own effect values. With no graph an effect's value is its authored number
// (`CEffect::calculateBaseValue` with both ends equal); a query sums one list's matches.
#include "core/item_effects.h"
#include "core/character.h"
#include "core/character_effects.h"
#include "core/effect_manager.h"
#include "core/inventory.h"
#include "core/item_roll.h"
#include "core/item_sets.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

using tl::core::Affix;
using tl::core::DamageType;
using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::EffectTypeTable;
using tl::core::ItemStats;
using tl::core::kDamageTypeAny;
using tl::core::kDurationAlways;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

constexpr int kStrength = 0x45;  // `STRENGTH BONUS`
constexpr int kArmor = 0x08;     // `ARMOR BONUS`

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

/// 0x91 rows, none of them a per-tick type.
EffectTypeTable types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    for (int index = 0; index < 0x91; ++index) {
        AdmSection row;
        row.name = "EFFECT";
        row.records.push_back(str("NAME", "ROW " + std::to_string(index)));
        row.records.push_back(str("TYPE", "Value"));
        file.root.children.push_back(row);
    }
    EffectTypeTable table;
    table.load(file);
    return table;
}

Effect effect(const std::string& name, int type, float value,
              EffectActivation activation = EffectActivation::Passive,
              DamageType damage = DamageType::Physical) {
    Effect e;
    e.name = name;
    e.type = type;
    e.values[0] = value;
    e.values[1] = value;
    e.duration = kDurationAlways;
    e.activation = activation;
    e.damage_type = damage;
    return e;
}

Affix affix(const std::string& name, const std::vector<Effect>& effects) {
    Affix a;
    a.name = name;
    a.min_spawn_range = 0;
    a.max_spawn_range = 100;
    a.effects = effects;
    return a;
}

ItemStats::RolledAffix rolled(const Affix& a, uint32_t level = 5) {
    return ItemStats::RolledAffix{a, level};
}

EffectManager::Context item_context(const EffectTypeTable& table,
                                    const tl::core::StatBook& graphs) {
    EffectManager::Context ctx;
    ctx.types = &table;
    ctx.graphs = &graphs;
    ctx.owner.present = false;  // an item is not a character
    return ctx;
}

bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

TL_TEST(item_effect_value_sums_its_affixes_on_the_passive_list) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    ItemStats ring;
    ring.affixes.push_back(rolled(affix("STR_A", {effect("A", kStrength, 10.0f)})));
    ring.affixes.push_back(rolled(affix("STR_B", {effect("B", kStrength, 5.0f)})));
    const EffectManager::Context ctx = item_context(table, graphs);
    // 10 + 5 on the passive list; damage type 7 is no filter.
    TL_CHECK(near(tl::core::item_effect_value(ring, ctx, EffectActivation::Passive, kStrength,
                                              kDamageTypeAny),
                  15.0f));
    // Both effects are physical (0): the filtered form finds the same 15.
    TL_CHECK(near(tl::core::item_effect_value(ring, ctx, EffectActivation::Passive, kStrength, 0),
                  15.0f));
    // A type nothing on the item authors is 0.
    TL_CHECK(near(
        tl::core::item_effect_value(ring, ctx, EffectActivation::Passive, kArmor, kDamageTypeAny),
        0.0f));
    // An item that never held an effect answers 0, as a null `+0x1b8` does.
    const ItemStats plain;
    TL_CHECK(near(tl::core::item_effect_value(plain, ctx, EffectActivation::Passive, kStrength,
                                              kDamageTypeAny),
                  0.0f));
}

TL_TEST(item_effect_value_reads_one_activation_list_only) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    ItemStats sword;
    sword.affixes.push_back(
        rolled(affix("MIXED", {effect("P", kStrength, 10.0f),
                               effect("D", kStrength, 4.0f, EffectActivation::Dynamic)})));
    const EffectManager::Context ctx = item_context(table, graphs);
    // Activation 0 (what `CInventory` passes) sees the 10 and not the 4.
    TL_CHECK(near(tl::core::item_effect_value(sword, ctx, EffectActivation::Passive, kStrength,
                                              kDamageTypeAny),
                  10.0f));
    TL_CHECK(near(tl::core::item_effect_value(sword, ctx, EffectActivation::Dynamic, kStrength,
                                              kDamageTypeAny),
                  4.0f));
    // The by-name form walks Passive and Dynamic both.
    TL_CHECK(near(tl::core::item_effect_value(sword, ctx, kStrength, std::string("D")), 4.0f));
}

TL_TEST(item_effect_manager_holds_the_block_chance_and_drops_the_identify_type) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    ItemStats shield;
    shield.block_chance = 12;
    shield.affixes.push_back(
        rolled(affix("ODD", {effect("ID", tl::core::kEffectTypeIdentifiesItem, 1.0f),
                             effect("S", kStrength, 3.0f)})));
    const EffectManager manager =
        tl::core::item_effect_manager(shield, item_context(table, graphs));
    // `BLOCK_CHANCE: 12` is a passive 0x3e of 12, `INSTANT` (never expires).
    TL_CHECK(near(manager.effect_value(EffectActivation::Passive,
                                       tl::core::item_effect::kBlockChance, kDamageTypeAny),
                  12.0f));
    TL_CHECK_EQ(manager.effects(EffectActivation::Passive).size(), 2u);  // 0x3e and the 3
    TL_CHECK(near(manager.effect_value(tl::core::kEffectTypeIdentifiesItem), 0.0f));
    TL_CHECK(near(manager.effect_value(kStrength), 3.0f));
}

tl::formats::AdmSection unit_type(const std::string& name, uint32_t id,
                                  const std::vector<uint32_t>& ancestors) {
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

TL_TEST(add_container_item_stops_at_the_socket_count_and_filters_a_gems_affixes) {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Weapon", 8, {0}));
    all.children.push_back(unit_type("Armor", 13, {0}));
    all.children.push_back(unit_type("SOCKETABLE", 120, {0}));
    all.children.push_back(unit_type("NORMAL SWORD", 300, {8, 0}));
    all.children.push_back(unit_type("EMBER", 330, {120, 0}));
    root.children.push_back(all);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);

    ItemStats sword;
    sword.unittype = "NORMAL SWORD";
    sword.sockets = 1;
    ItemStats ember;
    ember.unittype = "EMBER";
    Affix on_weapons = affix("ON_WEAPONS", {effect("W", kStrength, 2.0f)});
    on_weapons.unit_types = {8};
    Affix on_armor = affix("ON_ARMOR", {effect("R", kArmor, 2.0f)});
    on_armor.unit_types = {13};
    ember.affixes.push_back(rolled(on_weapons));
    ember.affixes.push_back(rolled(on_armor));

    TL_CHECK(tl::core::add_container_item(sword, ember, hierarchy));
    TL_CHECK_EQ(sword.socketed.size(), 1u);
    // A socketable in a sword keeps only what a Weapon (8) can take.
    TL_CHECK_EQ(sword.socketed[0].affixes.size(), 1u);
    TL_CHECK_EQ(sword.socketed[0].affixes[0].affix.name, std::string("ON_WEAPONS"));
    // One socket: the second goes nowhere (`+0x3f0 < +0x3e0` fails).
    TL_CHECK(!tl::core::add_container_item(sword, ember, hierarchy));
    TL_CHECK_EQ(sword.socketed.size(), 1u);
}

// --------------------------------------------------------------------------
// The inventory's effect half (`CInventory`) and the character's total.

/// The gear unit types by their ids, and a concrete type over each.
tl::core::UnitTypeHierarchy gear_types() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Weapon", 8, {0}));
    all.children.push_back(unit_type("Helmet", 12, {0}));
    all.children.push_back(unit_type("Armor", 13, {0}));
    all.children.push_back(unit_type("Ring", 17, {0}));
    all.children.push_back(unit_type("Gloves", 23, {0}));
    all.children.push_back(unit_type("Necklace", 24, {0}));
    all.children.push_back(unit_type("Takeable", 31, {0}));
    all.children.push_back(unit_type("Trinket", 39, {0}));
    all.children.push_back(unit_type("RING", 400, {17, 39, 31, 0}));
    all.children.push_back(unit_type("NECKLACE", 401, {24, 39, 31, 0}));
    all.children.push_back(unit_type("SWORD", 402, {8, 31, 0}));
    all.children.push_back(unit_type("HELMET", 403, {12, 13, 31, 0}));
    all.children.push_back(unit_type("GLOVES", 404, {23, 13, 31, 0}));
    root.children.push_back(all);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

/// A piece of gear carrying one affix with one passive strength effect.
ItemStats gear(const std::string& name, const std::string& type, float strength,
               const std::string& set = std::string()) {
    ItemStats item;
    item.name = name;
    item.unittype = type;
    item.set = set;
    if (strength != 0.0f) {
        item.affixes.push_back(
            rolled(affix("AFFIX_" + name, {effect("E_" + name, kStrength, strength)})));
    }
    return item;
}

/// Everything an `Inventory`'s effect half is handed, kept alive together.
struct World {
    EffectTypeTable table = types();
    tl::core::StatBook graphs;
    tl::core::AffixBook affixes;
    tl::core::ItemSetBook sets;
    tl::core::InventoryEffectContext context() const {
        return tl::core::InventoryEffectContext{&table, &graphs, &sets, &affixes};
    }
};

TL_TEST(inventory_sums_each_equipped_item_ceiled) {
    World world;
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Ring", "RING", 10.0f), 1, tl::core::equip_location::kRing1);
    inv.pickup_into_slot(gear("Amulet", "NECKLACE", 5.0f), 1, tl::core::equip_location::kNecklace);
    // ceilf(10) + ceilf(5) + ceilf(own 0) = 15, cached under 7 and under 0
    // (both effects are physical); under fire (2) nothing matches.
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 15.0f));
    TL_CHECK(near(inv.effect_value(kStrength, 0), 15.0f));
    TL_CHECK(near(inv.effect_value(kStrength, 2), 0.0f));
    // The cache holds the one non-zero total and nothing else.
    TL_CHECK_EQ(inv.effect_cache(kDamageTypeAny).size(), 1u);
    TL_CHECK_EQ(inv.effect_cache(2).size(), 0u);
    // The by-name walk is not cached: the ring's effect is `E_Ring`.
    TL_CHECK(near(inv.effect_value(kStrength, std::string("E_Ring")), 10.0f));
}

TL_TEST(inventory_ceils_each_item_before_adding) {
    World world;
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Ring", "RING", 2.4f), 1, tl::core::equip_location::kRing1);
    inv.pickup_into_slot(gear("Ring2", "RING", 2.4f), 1, tl::core::equip_location::kRing2);
    // ceilf(2.4) + ceilf(2.4) = 3 + 3 = 6, not ceilf(4.8) = 5.
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 6.0f));
}

TL_TEST(inventory_leaves_out_the_second_weapon_set) {
    World world;
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Sword", "SWORD", 7.0f), 1, tl::core::equip_location::kRightHand2);
    // Locations 0-11 only: the +7 at 12 counts nothing ...
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 0.0f));
    // and the slot query refuses 12 outright.
    TL_CHECK(near(inv.effect_value_from_equipment_slot(kStrength, 12, kDamageTypeAny), 0.0f));
    // Swapping the sets puts it at 0, where it counts.
    inv.swap_weapon_set();
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 7.0f));
}

TL_TEST(inventory_applies_a_set_once_and_only_the_bonuses_worn) {
    World world;
    world.affixes.add(affix("SET_TWO", {effect("S2", kStrength, 3.0f)}));
    world.affixes.add(affix("SET_THREE", {effect("S3", kStrength, 100.0f)}));
    tl::core::ItemSet mercenary;
    mercenary.name = "Mercenary";
    tl::core::ItemSetBonus two;
    two.affix_name = "SET_TWO";
    two.count = 2;
    tl::core::ItemSetBonus three;
    three.affix_name = "SET_THREE";
    three.count = 3;
    mercenary.bonuses = {two, three};
    world.sets.add(mercenary);

    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Helm", "HELMET", 0.0f, "Mercenary"), 1,
                         tl::core::equip_location::kHelmet);
    inv.pickup_into_slot(gear("Gloves", "GLOVES", 0.0f, "Mercenary"), 1,
                         tl::core::equip_location::kGloves);
    TL_CHECK_EQ(inv.set_count(mercenary), 2);
    // Two pieces: the 2-piece bonus (COUNT 2 <= 2), not the 3-piece one, and
    // once for the set however many pieces carry it: 3, not 6.
    TL_CHECK_EQ(inv.effects().affixes().size(), 1u);
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 3.0f));
    // A third piece: both bonuses, 3 + 100.
    inv.pickup_into_slot(gear("Band", "RING", 0.0f, "Mercenary"), 1,
                         tl::core::equip_location::kRing1);
    TL_CHECK_EQ(inv.effects().affixes().size(), 2u);
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 103.0f));
    // A piece in the second weapon set is not worn (`getSetCount` stops at 11).
    tl::core::Inventory spare;
    spare.set_effect_context(world.context());
    spare.pickup_into_slot(gear("Helm", "HELMET", 0.0f, "Mercenary"), 1,
                           tl::core::equip_location::kHelmet);
    spare.pickup_into_slot(gear("Blade", "SWORD", 0.0f, "Mercenary"), 1,
                           tl::core::equip_location::kRightHand2);
    TL_CHECK_EQ(spare.set_count(mercenary), 1);
    TL_CHECK(near(spare.effect_value(kStrength, kDamageTypeAny), 0.0f));
}

TL_TEST(inventory_minus_a_location_and_its_zero_total_shortcut) {
    World world;
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Ring", "RING", 10.0f), 1, tl::core::equip_location::kRing1);
    inv.pickup_into_slot(gear("Amulet", "NECKLACE", 5.0f), 1, tl::core::equip_location::kNecklace);
    // t = ceilf(15) = 15; the ring's slot is ceilf(ceilf(10)) = 10: 15 - 10.
    TL_CHECK(near(inv.effect_value_minus_equipment_slot(kStrength, 8, kDamageTypeAny), 5.0f));
    // An empty location takes nothing off: 15 - 0.
    TL_CHECK(near(inv.effect_value_minus_equipment_slot(kStrength, 9, kDamageTypeAny), 15.0f));

    // +5 and -5: ceilf(5) + ceilf(-5) = 0, nothing cached, and the shortcut
    // answers 0 where 0 - 5 would have been -5.
    tl::core::Inventory even;
    even.set_effect_context(world.context());
    even.pickup_into_slot(gear("Ring", "RING", 5.0f), 1, tl::core::equip_location::kRing1);
    even.pickup_into_slot(gear("Curse", "NECKLACE", -5.0f), 1, tl::core::equip_location::kNecklace);
    TL_CHECK_EQ(even.effect_cache(kDamageTypeAny).size(), 0u);
    TL_CHECK(near(even.effect_value_from_equipment_slot(kStrength, 8, kDamageTypeAny), 5.0f));
    TL_CHECK(near(even.effect_value_minus_equipment_slot(kStrength, 8, kDamageTypeAny), 0.0f));
}

TL_TEST(inventory_refreshes_its_totals_as_items_come_and_go) {
    World world;
    const tl::core::UnitTypeHierarchy hierarchy = gear_types();
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    const int bag = inv.pickup(gear("Ring", "RING", 10.0f), 1, hierarchy);
    TL_CHECK(bag >= 19);  // into the backpack: carried, not worn
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 0.0f));
    TL_CHECK(inv.equip_into_first_free_location(static_cast<size_t>(bag), hierarchy, nullptr));
    TL_CHECK(inv.occupied(tl::core::equip_location::kRing1));
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 10.0f));
    TL_CHECK(inv.unequip(tl::core::equip_location::kRing1, hierarchy));
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 0.0f));
    inv.pickup_into_slot(gear("Amulet", "NECKLACE", 5.0f), 1, tl::core::equip_location::kNecklace);
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 5.0f));
    TL_CHECK(inv.remove_item(tl::core::equip_location::kNecklace));
    TL_CHECK(near(inv.effect_value(kStrength, kDamageTypeAny), 0.0f));
}

TL_TEST(inventory_armor_bonus_sums_the_worn_armour_points) {
    World world;
    tl::core::Inventory inv;
    ItemStats helm = gear("Helm", "HELMET", 0.0f);
    helm.graph_ac = 12;
    ItemStats spare = gear("Spare", "HELMET", 0.0f);
    spare.graph_ac = 50;
    inv.pickup_into_slot(helm, 1, tl::core::equip_location::kHelmet);
    inv.pickup_into_slot(spare, 1, tl::core::equip_location::kReserved14);  // past 11: not summed
    TL_CHECK_EQ(inv.armor_bonus(), 12);
}

TL_TEST(character_total_is_its_own_effects_plus_its_inventorys) {
    World world;
    tl::core::Inventory inv;
    inv.set_effect_context(world.context());
    inv.pickup_into_slot(gear("Ring", "RING", 10.0f), 1, tl::core::equip_location::kRing1);
    inv.pickup_into_slot(gear("Amulet", "NECKLACE", 5.0f), 1, tl::core::equip_location::kNecklace);
    EffectManager own;
    own.set_context(item_context(world.table, world.graphs));
    own.add_effect(effect("BUFF", kStrength, 2.0f, EffectActivation::Dynamic));
    own.calculate_effect_values();

    const tl::core::CharacterEffects total(&own, &inv);
    TL_CHECK(near(total.effect_value(kStrength), 17.0f));                   // 2 + 15
    TL_CHECK(near(total.effect_value_without_inventory(kStrength), 2.0f));
    TL_CHECK(near(total.effect_value_minus_location(kStrength, 8, kDamageTypeAny), 7.0f));  // 2 + 5
    TL_CHECK(near(total.effect_value(kStrength, std::string("E_Ring")), 10.0f));
    TL_CHECK(near(tl::core::CharacterEffects(&own).effect_value(kStrength), 2.0f));
    TL_CHECK(near(tl::core::CharacterEffects(nullptr, &inv).effect_value(kStrength), 15.0f));

    // `CCharacter::strength` over the total: base 0, flat 17.
    const tl::core::CharacterProgression character;
    TL_CHECK_EQ(character.strength(total), 17);
    TL_CHECK_EQ(character.strength(&own), 2);
}

}  // namespace
