// The recipe book and `CCombineMenu::performInteraction`, on recipes by unit type and by unit.
#include "core/combine.h"
#include "core/inventory.h"
#include "core/random.h"
#include "core/spawn_class.h"
#include "core/tray_moves.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <memory>
#include <string>
#include <vector>

namespace {

using tl::core::CombineContext;
using tl::core::CombineOutcome;
using tl::core::Inventory;
using tl::core::ItemStats;
using tl::core::Recipe;
using tl::core::RecipeBook;
using tl::core::RecipeIngredient;
using tl::core::RecipeResult;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

constexpr int k15 = tl::core::equip_location::kCombine1;
constexpr int k16 = tl::core::equip_location::kCombine2;
constexpr int k17 = tl::core::equip_location::kCombine3;
constexpr int k18 = tl::core::equip_location::kCombine4;

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord integer(const std::string& key, uint32_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Integer;
    r.integer = value;
    return r;
}

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

/// Ring 17, Takeable 31, Potion 33, UNIQUE 54, MAGIC 55, SOCKETABLE 120,
/// RANDOMMAGIC 135; NORMAL has no id the port names, 200 stands for it.
UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Ring", 17, {0}));
    all.children.push_back(unit_type("Takeable", 31, {0}));
    all.children.push_back(unit_type("Potion", 33, {31, 0}));
    all.children.push_back(unit_type("UNIQUE", 54, {0}));
    all.children.push_back(unit_type("MAGIC", 55, {0}));
    all.children.push_back(unit_type("SOCKETABLE", 120, {31, 0}));
    all.children.push_back(unit_type("RANDOMMAGIC", 135, {0}));
    all.children.push_back(unit_type("NORMAL", 200, {0}));
    all.children.push_back(unit_type("NORMAL RING", 400, {17, 200, 31, 0}));
    all.children.push_back(unit_type("MAGIC RING", 401, {17, 55, 31, 0}));
    all.children.push_back(unit_type("UNIQUE RING", 402, {17, 54, 31, 0}));
    all.children.push_back(unit_type("GEM", 410, {120, 31, 0}));
    root.children.push_back(all);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

ItemStats item(const std::string& name, const std::string& type, int64_t guid, int max_stack = 1) {
    ItemStats out;
    out.name = name;
    out.display_name = name;
    out.unittype = type;
    out.unit_guid = guid;
    out.max_stack = max_stack;
    return out;
}

/// A copy with one rolled affix: `isMagical` on anything not `SOCKETABLE`.
ItemStats rolled(ItemStats base) {
    tl::core::Affix affix;
    affix.name = "ROLLED";
    base.affixes.push_back(ItemStats::RolledAffix{affix, 1});
    return base;
}

RecipeIngredient by_type(uint32_t type, int32_t count) {
    RecipeIngredient out;
    out.unit_type = type;
    out.count = count;
    return out;
}

RecipeIngredient by_unit(const std::string& unit, int32_t count) {
    RecipeIngredient out;
    out.unit = unit;
    out.count = count;
    return out;
}

Recipe recipe(const std::vector<RecipeIngredient>& ingredients, const std::string& unit_result,
              const std::string& spawn_class_result = std::string()) {
    Recipe out;
    out.ingredients = ingredients;
    if (!unit_result.empty() || !spawn_class_result.empty()) {
        RecipeResult result;
        result.unit = unit_result;
        result.spawn_class = spawn_class_result;
        out.results.push_back(result);
    }
    return out;
}

/// The world a combine runs in; `made` records every `create_unit` call.
struct Bench {
    UnitTypeHierarchy types = hierarchy();
    tl::core::ItemBook items;
    RecipeBook recipes;
    tl::core::SpawnContext spawn;
    tl::core::RunicRandom game{1};
    std::vector<std::pair<std::string, bool>> made;
    std::vector<std::shared_ptr<tl::core::SpawnClass>> classes;

    Bench() {
        items.add(item("Flawless Citrine", "GEM", 101, 10));
        items.add(item("Perfect Citrine", "GEM", 102, 10));
    }

    CombineContext context() {
        CombineContext ctx;
        ctx.recipes = &recipes;
        ctx.unit_types = &types;
        ctx.items = &items;
        ctx.spawn_class = [this](const std::string& name) -> const tl::core::SpawnClass* {
            for (const auto& c : classes) {
                if (c->name() == name) return c.get();
            }
            return nullptr;
        };
        ctx.create_unit = [this](const std::string& unit, int32_t, bool force, ItemStats& out) {
            made.emplace_back(unit, force);
            out = item(unit, "GEM", 900, 10);
            return true;
        };
        ctx.player.level = 7;
        ctx.player_level = 7;
        ctx.spawn_context = &spawn;
        ctx.game_rng = &game;
        return ctx;
    }
};

TL_TEST(recipe_load_reads_both_shapes_with_their_defaults) {
    const UnitTypeHierarchy types = hierarchy();
    AdmFile by_type_file;
    by_type_file.root.name = "RECIPE";
    by_type_file.root.records.push_back(str("NAME", "3rings_magic"));
    AdmSection ring;
    ring.name = "INGREDIENT";
    ring.records.push_back(str("UNITTYPE", "RANDOMMAGIC"));
    ring.records.push_back(integer("COUNT", 3));
    AdmSection spawn;
    spawn.name = "RESULT";
    spawn.records.push_back(str("SPAWNCLASS", "RECIPE_3RINGS_RANDOMMAGIC"));
    by_type_file.root.children = {ring, spawn};

    AdmFile by_unit_file;
    by_unit_file.root.name = "RECIPE";
    AdmSection citrine;
    citrine.name = "INGREDIENT";
    citrine.records.push_back(str("UNIT", "FLAWLESS CITRINE"));  // COUNT left out: 1
    AdmSection perfect;
    perfect.name = "RESULT";
    perfect.records.push_back(str("UNIT", "PERFECT CITRINE"));
    AdmSection odd;
    odd.name = "INGREDIENT";
    odd.records.push_back(str("UNITTYPE", "NO SUCH TYPE"));
    by_unit_file.root.children = {citrine, perfect, odd};

    RecipeBook book;
    book.reload({&by_type_file, &by_unit_file}, types);
    TL_CHECK_EQ(book.size(), 2u);
    const Recipe& first = book.recipes()[0];
    TL_CHECK_EQ(first.ingredients.size(), 1u);
    TL_CHECK_EQ(first.ingredients[0].unit_type, tl::core::unit_type::kRandomMagic);
    TL_CHECK_EQ(first.ingredients[0].count, 3);
    TL_CHECK_EQ(first.results.size(), 1u);
    TL_CHECK_EQ(first.results[0].spawn_class, std::string("RECIPE_3RINGS_RANDOMMAGIC"));
    TL_CHECK(first.results[0].unit.empty());

    const Recipe& second = book.recipes()[1];
    // Ingredients first, in file order, whatever sits between them.
    TL_CHECK_EQ(second.ingredients.size(), 2u);
    TL_CHECK_EQ(second.ingredients[0].unit_type, tl::core::unit_type::kNone);  // 0x16: by unit
    TL_CHECK_EQ(second.ingredients[0].unit, std::string("FLAWLESS CITRINE"));
    TL_CHECK_EQ(second.ingredients[0].count, 1);
    TL_CHECK_EQ(second.ingredients[1].unit_type, UnitTypeHierarchy::kUnknown);
    TL_CHECK(second.results[0].spawn_class.empty());
    TL_CHECK_EQ(second.results[0].unit, std::string("PERFECT CITRINE"));
}

TL_TEST(combine_by_unit_consumes_the_stack_and_makes_the_unit_in_slot_15) {
    Bench bench;
    bench.recipes.add(recipe({by_unit("Flawless Citrine", 2)}, "Perfect Citrine"));
    Inventory inv;
    inv.pickup_into_slot(item("Flawless Citrine", "GEM", 101, 10), 2, k16);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(out.sample, tl::core::kCombineSuccessSample);
    TL_CHECK(out.owner_sample);
    // A stack of 2 against 2 owed: `min(2, 2)` taken, empty, gone.
    TL_CHECK(!inv.occupied(k16));
    TL_CHECK_EQ(out.consumed.size(), 1u);
    TL_CHECK_EQ(bench.made.size(), 1u);
    TL_CHECK_EQ(bench.made[0].first, std::string("Perfect Citrine"));
    TL_CHECK(!bench.made[0].second);  // `createEquipment` does not force an enchant
    TL_CHECK(inv.occupied(k15));
    TL_CHECK(out.journal_counted);
}

TL_TEST(combine_by_type_accepts_a_subtype) {
    Bench bench;
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 1)}, "Perfect Citrine"));
    Inventory inv;
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k18);  // 400 ISA 17
    TL_CHECK(tl::core::perform_combine(inv, bench.context()).matched);
    TL_CHECK(!inv.occupied(k18));
}

TL_TEST(combine_randommagic_takes_a_rolled_item_but_not_magic_or_unique) {
    Bench bench;
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRandomMagic, 1)}, "Perfect Citrine"));
    {
        Inventory inv;
        inv.pickup_into_slot(rolled(item("Band", "NORMAL RING", 5)), 1, k15);
        TL_CHECK(tl::core::perform_combine(inv, bench.context()).matched);
    }
    for (const char* type : {"MAGIC RING", "UNIQUE RING"}) {
        Inventory inv;
        inv.pickup_into_slot(rolled(item("Band", type, 5)), 1, k15);
        const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
        TL_CHECK(!out.matched);
        TL_CHECK_EQ(out.sample, tl::core::kCombineFailSample);
        TL_CHECK(inv.occupied(k15));  // nothing consumed
    }
    {
        Inventory inv;  // an unrolled ring is not magical
        inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
        TL_CHECK(!tl::core::perform_combine(inv, bench.context()).matched);
    }
}

TL_TEST(combine_takes_part_of_a_larger_stack_and_returns_the_rest_before_the_result) {
    Bench bench;
    bench.recipes.add(recipe({by_type(33, 2)}, "Perfect Citrine"));  // two potions
    Inventory inv;
    inv.pickup_into_slot(item("Health Potion", "Potion", 7, 20), 5, k15);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    // 5 held, 2 owed: `incrementStackBy(-2)` leaves 3, which is not consumed ...
    TL_CHECK(out.consumed.empty());
    // ... goes back to the bag before the result is made, so the result can
    // have slot 15.
    const int back = inv.find_by_name("Health Potion");
    TL_CHECK(back >= 19);
    TL_CHECK_EQ(inv.slots()[static_cast<size_t>(back)].count, 3);
    TL_CHECK_EQ(inv.slots()[k15].item.name, std::string("Perfect Citrine"));
}

TL_TEST(combine_picks_by_the_ingredient_count_quirk) {
    Bench bench;
    // The winner is the recipe with more ingredients matched, not the larger COUNT sum.
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 3)}, "A_RESULT"));
    bench.recipes.add(recipe(
        {by_type(tl::core::unit_type::kRing, 1), by_type(tl::core::unit_type::kSocketable, 1)},
        "B_RESULT"));
    Inventory inv;
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5, 5), 3, k15);
    inv.pickup_into_slot(item("Ember", "GEM", 6), 1, k16);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(bench.made.size(), 1u);
    TL_CHECK_EQ(bench.made[0].first, std::string("B_RESULT"));
    // B took one ring of three and the gem; two rings went back to the bag.
    TL_CHECK_EQ(inv.count_by_name("Band"), 2);
    TL_CHECK_EQ(inv.count_by_name("Ember"), 0);
}

TL_TEST(combine_lets_one_slot_count_toward_two_ingredients) {
    Bench bench;
    // Two ingredients of one ring each, and one ring in the slots: the match
    // subtracts the same stack twice and passes; consuming takes it once.
    bench.recipes.add(
        recipe({by_type(tl::core::unit_type::kRing, 1), by_type(tl::core::unit_type::kRing, 1)},
               "Perfect Citrine"));
    Inventory inv;
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k17);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(out.consumed.size(), 1u);
    TL_CHECK(out.journal_counted);
    TL_CHECK(inv.occupied(k15));
}

TL_TEST(combine_drops_a_leftover_the_bag_cannot_take) {
    Bench bench;
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 1)}, "Perfect Citrine"));
    Inventory inv(0, true);  // no backpack room at all
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    inv.pickup_into_slot(item("Health Potion", "Potion", 7, 20), 4, k16);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(out.dropped.size(), 1u);
    TL_CHECK_EQ(out.dropped[0].item.name, std::string("Health Potion"));
    TL_CHECK_EQ(out.dropped[0].count, 4);
    TL_CHECK(!inv.occupied(k16));
}

TL_TEST(combine_spawn_class_result_fills_the_four_slots_from_g_rand) {
    Bench bench;
    auto spawn_class = std::make_shared<tl::core::SpawnClass>("RECIPE_GEMS");
    tl::core::SpawningData always;
    always.weight = -1;  // always spawns, `MINCOUNT..MAXCOUNT` copies
    always.min_count = 2;
    always.max_count = 6;
    auto data = std::make_shared<tl::core::SpawnClassData>("GEMS");
    tl::core::SpawnUnitData gem;
    gem.name = "Chipped Citrine";
    data->add_unit_data_object(gem);
    spawn_class->add_spawn_class_data(always, data);
    bench.classes.push_back(spawn_class);
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 1)}, "", "RECIPE_GEMS"));

    Inventory inv;
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    const CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    // One `g_Rand` step (0x29777b41) for the copies: 5 units rolled. Seed -1: no reseed.
    TL_CHECK_EQ(bench.game.state(), 695696193ull);
    // Only the first four are made, into 15, 16, 17, 18 in turn, none forced.
    TL_CHECK_EQ(bench.made.size(), 4u);
    for (const auto& call : bench.made) {
        TL_CHECK_EQ(call.first, std::string("Chipped Citrine"));
        TL_CHECK(!call.second);
    }
    TL_CHECK_EQ(out.result_slots.size(), 4u);
    TL_CHECK_EQ(out.result_slots[0], k15);
    TL_CHECK_EQ(out.result_slots[3], k18);
    for (int slot : {k15, k16, k17, k18}) TL_CHECK(inv.occupied(static_cast<size_t>(slot)));
    TL_CHECK(out.journal_counted);
}

TL_TEST(combine_with_no_result_counts_nothing_and_an_unknown_class_stops_early) {
    Bench bench;
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 1)}, ""));  // no [RESULT]
    Inventory inv;
    inv.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    inv.pickup_into_slot(item("Health Potion", "Potion", 7, 20), 1, k16);
    CombineOutcome out = tl::core::perform_combine(inv, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(out.sample, tl::core::kCombineSuccessSample);
    TL_CHECK(!inv.occupied(k15));  // consumed all the same
    TL_CHECK(inv.occupied(k16));   // no result: nothing goes back
    TL_CHECK(!out.journal_counted);

    Bench other;
    other.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 1)}, "", "NO_SUCH_CLASS"));
    Inventory bag;
    bag.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    bag.pickup_into_slot(item("Health Potion", "Potion", 7, 20), 1, k16);
    out = tl::core::perform_combine(bag, other.context());
    TL_CHECK(out.unknown_spawn_class);
    TL_CHECK(!out.journal_counted);
    TL_CHECK(!bag.occupied(k16));  // the leftovers had already gone back
    TL_CHECK(bag.find_by_name("Health Potion") >= 19);
}

TL_TEST(transmute_without_a_match_does_nothing_and_plays_nothing) {
    // acts only on the function's count above 0.
    Bench bench;
    bench.recipes.add(recipe({by_type(tl::core::unit_type::kRing, 2)}, "Perfect Citrine"));
    Inventory combiner;
    Inventory bag;
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    const CombineOutcome out = tl::core::transmute(combiner, bag, bench.context());
    TL_CHECK(!out.matched);
    TL_CHECK_EQ(out.sample, tl::core::kCombineNoSample);
    TL_CHECK(!out.owner_sample);
    TL_CHECK(combiner.occupied(k15));
    TL_CHECK(bench.made.empty());
}

TL_TEST(transmute_sends_the_leftovers_to_the_players_bag_and_makes_the_result_in_the_transmuter) {
    Bench bench;
    bench.recipes.add(recipe({by_type(33, 2)}, "Perfect Citrine"));  // two potions
    Inventory combiner;
    Inventory bag;
    combiner.pickup_into_slot(item("Health Potion", "Potion", 7, 20), 5, k15);
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k16);
    const CombineOutcome out = tl::core::transmute(combiner, bag, bench.context());
    TL_CHECK(out.matched);
    TL_CHECK_EQ(out.sample, tl::core::kCombineSuccessSample);
    TL_CHECK(out.owner_sample);
    // Two of five potions taken; the other three and the ring go to the
    // player, not back to the Transmuter.
    TL_CHECK_EQ(bag.count_by_name("Health Potion"), 3);
    TL_CHECK_EQ(bag.count_by_name("Band"), 1);
    TL_CHECK_EQ(combiner.count_by_name("Health Potion"), 0);
    TL_CHECK(!combiner.occupied(k16));
    TL_CHECK_EQ(combiner.slots()[k15].item.name, std::string("Perfect Citrine"));
    TL_CHECK(out.journal_counted);
}

TL_TEST(move_to_a_combiner_takes_the_first_free_slot_and_the_whole_stack) {
    const UnitTypeHierarchy types = hierarchy();
    Inventory bag;
    Inventory combiner;
    const int from = bag.pickup(item("Health Potion", "Potion", 7, 20), 4, types);
    TL_CHECK(from >= 0);
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(from), combiner, true, types) ==
             tl::core::TrayMove::Moved);
    TL_CHECK(!bag.occupied(static_cast<size_t>(from)));
    TL_CHECK_EQ(combiner.slots()[k16].count, 4);
    // Fill 17 and 18: a fifth item has nowhere to go and is refused.
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k17);
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k18);
    const int ember = bag.pickup(item("Ember", "GEM", 6), 1, types);
    TL_CHECK(tl::core::move_to_inventory(bag, static_cast<size_t>(ember), combiner, true, types) ==
             tl::core::TrayMove::Refused);
    TL_CHECK(bag.occupied(static_cast<size_t>(ember)));
}

TL_TEST(return_to_player_needs_a_pet_and_tries_the_bag_then_the_pets) {
    const UnitTypeHierarchy types = hierarchy();
    Inventory combiner;
    Inventory full(0, true);  // no backpack room
    Inventory pet_bag;
    tl::core::InventorySlot dropped;
    combiner.pickup_into_slot(item("Band", "NORMAL RING", 5), 1, k15);
    // No pet: does nothing at all.
    TL_CHECK(tl::core::return_to_player(combiner, k15, full, nullptr, types, dropped) ==
             tl::core::TrayReturn::None);
    TL_CHECK(combiner.occupied(k15));
    // The bag is full, so the pet's takes it.
    TL_CHECK(tl::core::return_to_player(combiner, k15, full, &pet_bag, types, dropped) ==
             tl::core::TrayReturn::PetBag);
    TL_CHECK(!combiner.occupied(k15));
    TL_CHECK_EQ(pet_bag.count_by_name("Band"), 1);
    // Neither: it is the caller's to drop at the player.
    Inventory full_pet(0, true);
    combiner.pickup_into_slot(item("Ember", "GEM", 6), 1, k17);
    TL_CHECK(tl::core::return_to_player(combiner, k17, full, &full_pet, types, dropped) ==
             tl::core::TrayReturn::Dropped);
    TL_CHECK_EQ(dropped.item.name, std::string("Ember"));
    TL_CHECK_EQ(dropped.count, 1);
    TL_CHECK(!combiner.occupied(k17));
}

}  // namespace
