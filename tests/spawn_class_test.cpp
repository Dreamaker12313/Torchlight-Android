#include "core/spawn_class.h"
#include "core/stats.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

tl::core::SpawnUnitData make_unit(std::string name, int32_t level = 1, std::string unittype = "",
                                  int32_t rarity = 1, int32_t min_level = 0,
                                  int32_t max_level = 0) {
    tl::core::SpawnUnitData u;
    u.name = std::move(name);
    u.level = level;
    u.unittype = std::move(unittype);
    u.rarity = rarity;
    u.min_level = min_level;
    u.max_level = max_level;
    // An item, as every base item file authors it: the branch of
    // `fillRandomizer` with the level window and the quality split.
    u.create_as = "EQUIPMENT";
    return u;
}

// Scenario 1: Weighted pick over 3 entries
// Fixed seed, hand-calculated index matching CRandomizer arithmetic.
TL_TEST(spawn_class_weighted_pick_over_three_entries) {
    tl::core::SpawnClass sc("TestWeighted");
    tl::core::SpawnContext ctx;

    // Entry 0: "UnitA", weight 100
    tl::core::SpawningData sd0;
    sd0.weight = 100;
    auto data0 = std::make_shared<tl::core::SpawnClassData>("DataA");
    data0->add_unit_data_object(make_unit("UnitA", 1));
    sc.add_spawn_class_data(sd0, data0);

    // Entry 1: "UnitB", weight 200
    tl::core::SpawningData sd1;
    sd1.weight = 200;
    auto data1 = std::make_shared<tl::core::SpawnClassData>("DataB");
    data1->add_unit_data_object(make_unit("UnitB", 1));
    sc.add_spawn_class_data(sd1, data1);

    // Entry 2: "UnitC", weight 300
    tl::core::SpawningData sd2;
    sd2.weight = 300;
    auto data2 = std::make_shared<tl::core::SpawnClassData>("DataC");
    data2->add_unit_data_object(make_unit("UnitC", 1));
    sc.add_spawn_class_data(sd2, data2);

    const uint32_t seed = 12345;
    tl::core::RunicRandom rng(seed);
    tl::core::RunicRandom control_rng(seed);

    // In roll_spawn_class, when seed != 0xffffffff, CRandomizer::setRandomSeed(seed) is called.
    control_rng.set_seed(static_cast<int32_t>(seed));
    const float roll = control_rng.between(0.0f, 1.0f);
    int expected_entry = 0;
    if (roll < 100.0f / 600.0f) {
        expected_entry = 0;
    } else if (roll < 300.0f / 600.0f) {
        expected_entry = 1;
    } else {
        expected_entry = 2;
    }

    std::vector<std::string> out;
    sc.roll_spawn_class(out, nullptr, nullptr, nullptr, 1, seed, 0, -1, 0, 0x10, rng, ctx);
    TL_CHECK_EQ(out.size(), 1u);
    if (expected_entry == 0) {
        TL_CHECK_EQ(out[0], "UnitA");
    } else if (expected_entry == 1) {
        TL_CHECK_EQ(out[0], "UnitB");
    } else {
        TL_CHECK_EQ(out[0], "UnitC");
    }
}

// Scenario 2: WEIGHT:0 / absent WEIGHT
// Stored as -1 (always spawns), drawn before weighted choices in roll_spawn_class.
TL_TEST(spawn_class_always_spawn_weight_zero_and_absent) {
    tl::core::SpawnContext ctx;

    tl::formats::AdmSection root;
    root.name = "SPAWNCLASS";

    // Child 0: absent WEIGHT -> stored as -1
    tl::formats::AdmSection obj0;
    obj0.name = "OBJECT";
    tl::formats::AdmRecord rec_unit0;
    rec_unit0.key = "UNIT";
    rec_unit0.type = tl::formats::AdmType::String;
    rec_unit0.text = "AlwaysUnit1";
    obj0.records.push_back(rec_unit0);
    root.children.push_back(obj0);

    // Child 1: WEIGHT: 0 -> stored as -1
    tl::formats::AdmSection obj1;
    obj1.name = "OBJECT";
    tl::formats::AdmRecord rec_unit1;
    rec_unit1.key = "UNIT";
    rec_unit1.type = tl::formats::AdmType::String;
    rec_unit1.text = "AlwaysUnit2";
    obj1.records.push_back(rec_unit1);
    tl::formats::AdmRecord rec_w1;
    rec_w1.key = "WEIGHT";
    rec_w1.type = tl::formats::AdmType::Integer;
    rec_w1.integer = 0;
    obj1.records.push_back(rec_w1);
    root.children.push_back(obj1);

    // Child 2: WEIGHT: 100 -> weighted
    tl::formats::AdmSection obj2;
    obj2.name = "OBJECT";
    tl::formats::AdmRecord rec_unit2;
    rec_unit2.key = "UNIT";
    rec_unit2.type = tl::formats::AdmType::String;
    rec_unit2.text = "WeightedUnit";
    obj2.records.push_back(rec_unit2);
    tl::formats::AdmRecord rec_w2;
    rec_w2.key = "WEIGHT";
    rec_w2.type = tl::formats::AdmType::Integer;
    rec_w2.integer = 100;
    obj2.records.push_back(rec_w2);
    root.children.push_back(obj2);

    tl::core::SpawnClass sc = tl::core::parse_spawn_class(root, ctx);
    TL_CHECK_EQ(sc.entries().size(), 3u);
    TL_CHECK_EQ(sc.entries()[0].spawning_data.weight, -1);
    TL_CHECK_EQ(sc.entries()[1].spawning_data.weight, -1);
    TL_CHECK_EQ(sc.entries()[2].spawning_data.weight, 100);

    tl::core::RunicRandom rng(42);
    std::vector<std::string> out;
    sc.roll_spawn_class(out, nullptr, nullptr, nullptr, 1, 42, 0, -1, 0, 0x10, rng, ctx);

    // Exactly 3 units: AlwaysUnit1, AlwaysUnit2, and WeightedUnit (in that order)
    TL_CHECK_EQ(out.size(), 3u);
    TL_CHECK_EQ(out[0], "AlwaysUnit1");
    TL_CHECK_EQ(out[1], "AlwaysUnit2");
    TL_CHECK_EQ(out[2], "WeightedUnit");
}

// Scenario 3: MINCOUNT:2 MAXCOUNT:4
// Count drawn from seeded RunicRandom with integer_between, matching exact step count.
TL_TEST(spawn_class_mincount_maxcount_step_count) {
    tl::core::SpawnClass sc("CountTest");
    tl::core::SpawnContext ctx;

    tl::core::SpawningData sd;
    sd.weight = -1;
    sd.min_count = 2;
    sd.max_count = 4;

    auto data = std::make_shared<tl::core::SpawnClassData>("Data");
    data->add_unit_data_object(make_unit("Spider", 1));
    sc.add_spawn_class_data(sd, data);

    const uint32_t seed = 777;
    tl::core::RunicRandom rng(seed);
    tl::core::RunicRandom control_rng(seed);

    const int expected_count = control_rng.integer_between(2, 4);

    std::vector<std::string> out;
    sc.roll_spawn_class(out, nullptr, nullptr, nullptr, 1, seed, 0, -1, 0, 0x10, rng, ctx);

    TL_CHECK_EQ(out.size(), static_cast<size_t>(expected_count));
    TL_CHECK_EQ(rng.state(), control_rng.state());
    for (const auto& u : out) {
        TL_CHECK_EQ(u, "Spider");
    }
}

// Scenario 4: SPAWNCLASS recursion
// Sub-class recurses with seed 0xffffffff (no reseed).
TL_TEST(spawn_class_recursion_no_reseed) {
    tl::core::SpawnContext ctx;

    tl::core::SpawnClass child("ChildClass");
    tl::core::SpawningData child_sd;
    child_sd.weight = 100;
    auto child_data = std::make_shared<tl::core::SpawnClassData>("ChildData");
    child_data->add_unit_data_object(make_unit("ChildUnit", 1));
    child.add_spawn_class_data(child_sd, child_data);

    tl::core::SpawnClass parent("ParentClass");
    tl::core::SpawningData parent_sd;
    parent_sd.weight = 100;
    auto parent_data = std::make_shared<tl::core::SpawnClassData>("ParentRef");
    parent_data->add_spawn_class(&child);
    parent.add_spawn_class_data(parent_sd, parent_data);

    ctx.get_spawn_class = [&](const std::string& name) -> const tl::core::SpawnClass* {
        if (name == "ChildClass") return &child;
        return nullptr;
    };

    tl::core::RunicRandom rng(999);
    std::vector<std::string> out;
    parent.roll_spawn_class(out, nullptr, nullptr, nullptr, 1, 999, 0, -1, 0, 0x10, rng, ctx);

    TL_CHECK_EQ(out.size(), 1u);
    TL_CHECK_EQ(out[0], "ChildUnit");
}

// Scenario 5: UNITTYPE:NONE
// Yields no units and produces no errors or logs.
TL_TEST(spawn_class_unittype_none_yields_no_units) {
    tl::core::SpawnContext ctx;
    tl::core::UnitTypeHierarchy hier;

    tl::formats::AdmSection hier_sec;
    hier_sec.name = "UNITTYPES";
    tl::formats::AdmSection none_sec;
    none_sec.name = "NONE";
    tl::formats::AdmRecord none_id;
    none_id.key = "ID";
    none_id.type = tl::formats::AdmType::Integer;
    none_id.integer = 22;
    none_sec.records.push_back(none_id);
    hier_sec.children.push_back(none_sec);
    hier.parse(hier_sec);
    ctx.unit_types = &hier;

    tl::formats::AdmSection root;
    root.name = "SPAWNCLASS";
    tl::formats::AdmRecord rec_name;
    rec_name.key = "NAME";
    rec_name.type = tl::formats::AdmType::String;
    rec_name.text = "NONE";
    root.records.push_back(rec_name);

    tl::formats::AdmSection obj;
    obj.name = "OBJECT";
    tl::formats::AdmRecord rec_type;
    rec_type.key = "UNITTYPE";
    rec_type.type = tl::formats::AdmType::String;
    rec_type.text = "NONE";
    obj.records.push_back(rec_type);
    root.children.push_back(obj);

    tl::core::SpawnClass sc = tl::core::parse_spawn_class(root, ctx);
    // Entry exists for NONE, but has 0 units (type 22 skips unit loading)
    TL_CHECK_EQ(sc.entries().size(), 1u);
    TL_CHECK_EQ(sc.entries()[0].data->get_unit_object_count(), 0u);

    tl::core::RunicRandom rng(123);
    std::vector<std::string> out;
    uint32_t count =
        sc.roll_spawn_class(out, nullptr, nullptr, nullptr, 1, 123, 0, -1, 0, 0, rng, ctx);
    TL_CHECK_EQ(count, 0u);
    TL_CHECK(out.empty());
}

// Drawing `NONE` ends the roll with nothing (`rollSpawnClass`'s loop test); it is
// not passed over for the next entry.
TL_TEST(spawn_class_drawing_none_ends_the_roll_with_nothing) {
    tl::core::SpawnContext ctx;
    tl::core::UnitTypeHierarchy hier;
    tl::formats::AdmSection hier_sec;
    hier_sec.name = "UNITTYPES";
    tl::formats::AdmSection none_sec;
    none_sec.name = "NONE";
    tl::formats::AdmRecord none_id;
    none_id.key = "ID";
    none_id.type = tl::formats::AdmType::Integer;
    none_id.integer = 22;
    none_sec.records.push_back(none_id);
    hier_sec.children.push_back(none_sec);
    hier.parse(hier_sec);
    ctx.unit_types = &hier;

    auto text = [](const char* key, const char* value) {
        tl::formats::AdmRecord r;
        r.key = key;
        r.type = tl::formats::AdmType::String;
        r.text = value;
        return r;
    };
    auto number = [](const char* key, int64_t value) {
        tl::formats::AdmRecord r;
        r.key = key;
        r.type = tl::formats::AdmType::Integer;
        r.integer = value;
        return r;
    };
    tl::formats::AdmSection root;
    root.name = "SPAWNCLASS";
    root.records.push_back(text("NAME", "GOBLIN_TREASURE"));
    tl::formats::AdmSection none_obj;
    none_obj.name = "OBJECT";
    none_obj.records.push_back(text("UNITTYPE", "NONE"));
    none_obj.records.push_back(number("WEIGHT", 92));
    root.children.push_back(none_obj);
    tl::formats::AdmSection item_obj;
    item_obj.name = "OBJECT";
    item_obj.records.push_back(text("UNIT", "Helmet"));
    item_obj.records.push_back(number("WEIGHT", 8));
    root.children.push_back(item_obj);

    const tl::core::SpawnClass sc = tl::core::parse_spawn_class(root, ctx);
    TL_CHECK_EQ(sc.entries().size(), 2u);
    TL_CHECK(sc.entries()[0].data->is_none());

    tl::core::RunicRandom rng(7);
    int dropped = 0;
    for (int i = 0; i < 1000; ++i) {
        std::vector<std::string> out;
        dropped += sc.roll_spawn_class(out, nullptr, nullptr, nullptr, 0, 0xffffffffu, 0, -1, 0, 0,
                                       rng, ctx) != 0;
    }
    // 8% expected; anything near 1000 is the old re-pick.
    TL_CHECK(dropped > 40);
    TL_CHECK(dropped < 130);
}

// Scenario 6: Rarity randomizer choice & magic find influence
TL_TEST(spawn_class_rarity_randomizer_and_magic_find) {
    tl::core::SpawnContext ctx;
    ctx.normal_item_weight = 5000;
    ctx.magic_item_weight = 120;
    ctx.unique_item_weight = 18;
    ctx.magicfind_magic_influence = 1.5f;  // 150 in the file, times 0.01f by `reload`
    ctx.magicfind_unique_influence = 2.0f;

    tl::core::StatGraph min_graph(tl::core::GraphType::Linear, "ITEM_SPAWN_RANGE_MINIMUM");
    min_graph.add_point(1.0f, 1.0f);
    min_graph.add_point(100.0f, 95.0f);

    tl::core::StatGraph max_graph(tl::core::GraphType::Linear, "ITEM_SPAWN_RANGE_MAXIMUM");
    max_graph.add_point(1.0f, 1.0f);
    max_graph.add_point(100.0f, 105.0f);

    tl::core::StatBook stats;
    stats.add("ITEM_SPAWN_RANGE_MINIMUM", min_graph);
    stats.add("ITEM_SPAWN_RANGE_MAXIMUM", max_graph);
    ctx.stats = &stats;

    // UnitType hierarchy with UNIQUE (54) and MAGIC (55)
    tl::formats::AdmSection hier_sec;
    hier_sec.name = "UNITTYPES";

    tl::formats::AdmSection m_sec;
    m_sec.name = "MAGIC_ITEM";
    tl::formats::AdmRecord m_id;
    m_id.key = "ID";
    m_id.type = tl::formats::AdmType::Integer;
    m_id.integer = 55;
    m_sec.records.push_back(m_id);
    hier_sec.children.push_back(m_sec);

    tl::formats::AdmSection u_sec;
    u_sec.name = "UNIQUE_ITEM";
    tl::formats::AdmRecord u_id;
    u_id.key = "ID";
    u_id.type = tl::formats::AdmType::Integer;
    u_id.integer = 54;
    u_sec.records.push_back(u_id);
    hier_sec.children.push_back(u_sec);

    tl::formats::AdmSection n_sec;
    n_sec.name = "WEAPON";
    tl::formats::AdmRecord n_id;
    n_id.key = "ID";
    n_id.type = tl::formats::AdmType::Integer;
    n_id.integer = 8;
    n_sec.records.push_back(n_id);
    hier_sec.children.push_back(n_sec);

    tl::core::UnitTypeHierarchy hier;
    hier.parse(hier_sec);
    ctx.unit_types = &hier;

    tl::core::SpawnClassData data("TestRarity");
    data.add_unit_data_object(make_unit("SwordNormal", 1, "WEAPON", 100));
    data.add_unit_data_object(make_unit("SwordMagic", 1, "MAGIC_ITEM", 100));
    data.add_unit_data_object(make_unit("SwordUnique", 1, "UNIQUE_ITEM", 100));

    // Flags = 0 (no filters)
    data.calculate_choices(nullptr, 1, 0, -1, 0, ctx);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.magic_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 1u);

    // Flags & 2 (NO_UNIQUE_ITEMS)
    data.calculate_choices(nullptr, 1, 0, -1, 2, ctx);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.magic_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 0u);

    // Flags & 4 (NO_MAGIC_ITEMS)
    data.calculate_choices(nullptr, 1, 0, -1, 4, ctx);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.magic_randomizer().choice_count(), 0u);
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 1u);
}

// Scenario 7: Cache in calculate_choices
// Clears and refills only when cache key (level, levelBonus, rarityOverride, flags) changes.
TL_TEST(spawn_class_calculate_choices_caching) {
    tl::core::SpawnContext ctx;
    tl::core::SpawnClassData data("CacheTest");
    data.add_unit_data_object(make_unit("Mob1", 5, "", 1, 1, 10));

    // First call: populates cache
    int32_t choices1 = data.calculate_choices(nullptr, 5, 0, -1, 0x10, ctx);
    TL_CHECK_EQ(choices1, 1);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);

    // Second call with identical keys: hits cache
    int32_t choices2 = data.calculate_choices(nullptr, 5, 0, -1, 0x10, ctx);
    TL_CHECK_EQ(choices2, 1);

    // Third call with different level: invalidates cache
    int32_t choices3 = data.calculate_choices(nullptr, 10, 0, -1, 0x10, ctx);
    TL_CHECK_EQ(choices3, 1);

    // Fourth call with different flags: invalidates cache
    int32_t choices4 = data.calculate_choices(nullptr, 10, 0, -1, 0x0, ctx);
    TL_CHECK_EQ(choices4, 1);
}

// Scenario 8: No-level path (character == nullptr && level <= 0)
// Adds directly to normal_randomizer if rarity != 0 (no type split, negative rarity allowed).
TL_TEST(spawn_class_no_level_path_adds_directly_to_normal_randomizer) {
    tl::core::SpawnContext ctx;

    tl::core::StatGraph min_graph(tl::core::GraphType::Linear, "ITEM_SPAWN_RANGE_MINIMUM");
    min_graph.add_point(1.0f, 1.0f);
    min_graph.add_point(100.0f, 95.0f);

    tl::core::StatGraph max_graph(tl::core::GraphType::Linear, "ITEM_SPAWN_RANGE_MAXIMUM");
    max_graph.add_point(1.0f, 1.0f);
    max_graph.add_point(100.0f, 105.0f);

    tl::core::StatBook stats;
    stats.add("ITEM_SPAWN_RANGE_MINIMUM", min_graph);
    stats.add("ITEM_SPAWN_RANGE_MAXIMUM", max_graph);
    ctx.stats = &stats;

    tl::formats::AdmSection hier_sec;
    hier_sec.name = "UNITTYPES";
    tl::formats::AdmSection u_sec;
    u_sec.name = "UNIQUE_ITEM";
    tl::formats::AdmRecord u_id;
    u_id.key = "ID";
    u_id.type = tl::formats::AdmType::Integer;
    u_id.integer = 54;
    u_sec.records.push_back(u_id);
    hier_sec.children.push_back(u_sec);

    tl::core::UnitTypeHierarchy hier;
    hier.parse(hier_sec);
    ctx.unit_types = &hier;

    tl::core::SpawnClassData data("NoLevelTest");
    data.add_unit_data_object(make_unit("NormalSword", 1, "", 10));
    data.add_unit_data_object(make_unit("UniqueSword", 1, "UNIQUE_ITEM", 20));
    data.add_unit_data_object(make_unit("NegativeRarity", 1, "", -1));
    data.add_unit_data_object(make_unit("ZeroRarity", 1, "", 0));

    // No character and level <= 0: every entry with a rarity, negative included,
    // goes to the normal randomizer.
    int32_t choices = data.calculate_choices(nullptr, 0, 0, -1, 0, ctx);
    TL_CHECK_EQ(choices, 3);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 3u);
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 0u);
    TL_CHECK_EQ(data.magic_randomizer().choice_count(), 0u);

    // Conversely, when level > 0 (e.g. level = 1):
    // Type split occurs, and rarity <= 0 is filtered out.
    int32_t choices_lvl1 = data.calculate_choices(nullptr, 1, 0, -1, 0, ctx);
    TL_CHECK_EQ(choices_lvl1, 2);  // NormalSword and UniqueSword
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 1u);
}

// Scenario 9: IGNORE_RANGE (0x10) flag skips level window check
TL_TEST(spawn_class_ignore_range_flag_skips_level_window) {
    tl::core::SpawnContext ctx;
    // No stats graph provided: max_level == 0 would fail level check without IGNORE_RANGE

    tl::core::SpawnClassData data("IgnoreRangeTest");
    data.add_unit_data_object(make_unit("HighLevelUnit", 50, "", 10));

    // With flags = 0 and no stats graph, level 1 fails level check
    int32_t choices_no_flag = data.calculate_choices(nullptr, 1, 0, -1, 0, ctx);
    TL_CHECK_EQ(choices_no_flag, 0);

    // With flags = 0x10 (IGNORE_RANGE), level window is skipped
    int32_t choices_flag = data.calculate_choices(nullptr, 1, 0, -1, 0x10, ctx);
    TL_CHECK_EQ(choices_flag, 1);
    TL_CHECK_EQ(data.normal_randomizer().choice_count(), 1u);
}

// Scenario 10: only `CREATEAS` `EQUIPMENT` takes the item spawn range; anything
// else is gated by `MINLEVEL`/`MAXLEVEL` (0 and 99999 when absent).
TL_TEST(spawn_class_only_equipment_takes_the_item_spawn_range) {
    tl::core::SpawnContext ctx;

    tl::core::SpawnUnitData monster;
    monster.name = "Ratlin";
    monster.level = 1;
    tl::core::SpawnUnitData deep = monster;
    deep.name = "Dragonkin";
    deep.min_level = 20;
    // Authored the wrong way round: the original orders the pair.
    tl::core::SpawnUnitData swapped = monster;
    swapped.name = "Swapped";
    swapped.min_level = 30;
    swapped.max_level = 25;
    tl::core::SpawnUnitData gold = monster;
    gold.name = "GOLD100";
    gold.create_as = "GOLD";
    // Not split by quality, and a negative rarity still counts (!= 0).
    tl::core::SpawnUnitData odd = monster;
    odd.name = "Odd";
    odd.unittype = "UNIQUE_ITEM";
    odd.rarity = -3;
    tl::core::SpawnUnitData never = monster;
    never.name = "Never";
    never.rarity = 0;

    tl::core::SpawnClassData data("Monsters");
    for (const auto& u : {monster, deep, swapped, gold, odd, never}) data.add_unit_data_object(u);

    // No graph in the context: an item's window would be empty, a monster's is not.
    TL_CHECK_EQ(data.calculate_choices(nullptr, 1, 0, -1, 0, ctx), 3);  // Ratlin, GOLD100, Odd
    TL_CHECK_EQ(data.unique_randomizer().choice_count(), 0u);
    TL_CHECK_EQ(data.calculate_choices(nullptr, 40, 0, -1, 0, ctx), 4);  // + Dragonkin
    TL_CHECK_EQ(data.calculate_choices(nullptr, 27, 0, -1, 0, ctx), 5);  // + Swapped
    // A rarity override lets every one in, at its weight.
    TL_CHECK_EQ(data.calculate_choices(nullptr, 1, 0, 7, 0, ctx), 6);

    // The same unit as equipment is refused without a range.
    tl::core::SpawnClassData items("Items");
    tl::core::SpawnUnitData hauberk = monster;
    hauberk.name = "a Scale Hauberk";
    hauberk.level = 24;
    hauberk.create_as = "EQUIPMENT";
    hauberk.max_level = 0;
    items.add_unit_data_object(hauberk);
    TL_CHECK_EQ(items.calculate_choices(nullptr, 1, 0, -1, 0, ctx), 0);
}

// `MAXLEVEL`'s default follows `CREATEAS`.
TL_TEST(spawn_unit_data_max_level_default_follows_createas) {
    const auto made_as = [](const char* create_as) {
        tl::formats::AdmSection unit;
        if (create_as != nullptr) {
            tl::formats::AdmRecord record;
            record.key = "CREATEAS";
            record.type = tl::formats::AdmType::String;
            record.text = create_as;
            unit.records.push_back(record);
        }
        return tl::core::SpawnUnitData::from_adm(unit);
    };
    TL_CHECK_EQ(made_as("EQUIPMENT").max_level, 0);
    TL_CHECK_EQ(made_as(nullptr).max_level, 99999);
    TL_CHECK_EQ(made_as("GOLD").max_level, 99999);
}

}  // namespace
