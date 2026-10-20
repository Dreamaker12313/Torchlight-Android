// The item roll: `CEquipment::enchant` and the affix pick, requirements, prices, sockets and
// loot. A step is `state = 0x29777b41 * (state & 0xffffffff) + (state >> 32)`; `randomBetween`
// takes two steps, `randomIntegerBetween` one (none when `hi <= lo`).
#include "core/item_roll.h"
#include "core/character.h"
#include "core/item_drop.h"
#include "core/random.h"
#include "core/spawn_class.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using tl::core::Affix;
using tl::core::AffixBook;
using tl::core::ItemRollContext;
using tl::core::ItemRollGlobals;
using tl::core::ItemStats;
using tl::core::RunicRandom;
using tl::core::StatBook;
using tl::core::StatGraph;
using tl::core::UnitTypeHierarchy;

tl::formats::AdmSection type(const std::string& name, uint32_t id,
                             const std::vector<uint32_t>& ancestors) {
    tl::formats::AdmSection section;
    section.name = name;
    tl::formats::AdmRecord key;
    key.key = "ID";
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        tl::formats::AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

// The gear and rarity unit types by id, and concrete item types over them; 200 stands for
// NORMAL.
UnitTypeHierarchy hierarchy() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("any", 0, {}));
    types.children.push_back(type("Weapon", 8, {0}));
    types.children.push_back(type("Armor", 13, {0}));
    types.children.push_back(type("Ring", 17, {0}));
    types.children.push_back(type("Necklace", 24, {0}));
    types.children.push_back(type("Potion", 33, {0}));
    types.children.push_back(type("UNIQUE", 54, {0}));
    types.children.push_back(type("MAGIC", 55, {0}));
    types.children.push_back(type("NORMAL", 200, {0}));
    types.children.push_back(type("NORMAL SWORD", 300, {8, 0xa2, 200, 0}));
    types.children.push_back(type("MAGIC SWORD", 301, {8, 0xa2, 55, 0}));
    types.children.push_back(type("UNIQUE SWORD", 302, {8, 0xa2, 54, 0}));
    types.children.push_back(type("NORMAL HELMET", 310, {13, 200, 0}));
    types.children.push_back(type("MAGIC HELMET", 311, {13, 55, 0}));
    types.children.push_back(type("NORMAL RING", 320, {17, 200, 0}));
    types.children.push_back(type("SOCKETABLE", 120, {0}));
    types.children.push_back(type("UNIQUE RING", 321, {17, 54, 0}));
    types.children.push_back(type("UNIQUE EMBER", 331, {120, 54, 0}));
    types.children.push_back(type("Takeable", 31, {0}));
    types.children.push_back(type("Gold", 34, {31, 2, 0}));
    types.children.push_back(type("TAKEABLE SWORD", 340, {8, 31, 0}));
    root.children.push_back(types);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

Affix affix(const std::string& name, int weight, int min, int max, uint32_t unit_type,
            int slots = 1) {
    Affix a;
    a.name = name;
    a.weight = weight;
    a.min_spawn_range = min;
    a.max_spawn_range = max;
    a.slots_occupy = slots;
    a.unit_types.push_back(unit_type);
    a.listed_unit_types.push_back(unit_type);
    return a;
}

// The roll walks the book in the order its files were added: HEAVY 0,
// OF_FIRE 1, OF_ICE 2.
AffixBook pool(bool with_passive_effects = false) {
    AffixBook p;
    Affix fire = affix("Of_Fire", 10, 0, 50, 8);
    Affix ice = affix("Of_Ice", 30, 0, 50, 8);
    if (with_passive_effects) {
        tl::core::Effect passive;
        passive.activation = tl::core::EffectActivation::Passive;
        passive.type = 5;
        fire.effects.push_back(passive);
        ice.effects.push_back(passive);
    }
    // Rolls from level 4 only, and costs two slots.
    p.add(affix("Heavy", 40, 4, 50, 8, 2));
    p.add(fire);
    p.add(ice);
    p.add(affix("Sturdy", 20, 0, 50, 13));
    p.add(affix("Zero", 0, 0, 50, 8));
    return p;
}

ItemStats item(const std::string& unittype, int level) {
    ItemStats it;
    it.name = unittype;
    it.unittype = unittype;
    it.level = level;
    return it;
}

StatGraph line(float y_at_100) {
    StatGraph g(tl::core::GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(100.0f, y_at_100);
    g.process_points();
    return g;
}

// Every price curve is a straight line through 0, so at level 10 each is a
// tenth of its value at 100.
StatBook graphs() {
    StatBook book;
    book.add("PRICE_PLAYERBUY_NORMAL", line(1000.0f));     // 100 at level 10
    book.add("PRICE_PLAYERSELL_NORMAL", line(250.0f));     // 25
    book.add("PRICE_PLAYERBUY_MAGIC", line(3000.0f));      // 300
    book.add("PRICE_PLAYERSELL_MAGIC", line(750.0f));      // 75
    book.add("PRICE_PLAYERBUY_UNIQUE", line(5000.0f));     // 500
    book.add("PRICE_PLAYERSELL_UNIQUE", line(1250.0f));    // 125
    book.add("PRICE_PLAYERGAMBLE_MAGIC", line(2000.0f));   // 200
    book.add("PRICE_ENCHANT", line(1000.0f));              // 100
    book.add("ITEM_LEVEL_REQUIREMENTS", line(100.0f));     // 10
    book.add("ITEM_STRENGTH_REQUIREMENTS", line(200.0f));  // 20
    book.add("ITEM_DEXTERITY_REQUIREMENTS", line(200.0f));
    book.add("ITEM_MAGIC_REQUIREMENTS", line(200.0f));
    book.add("ITEM_DEFENSE_REQUIREMENTS", line(200.0f));
    return book;
}

/// The state a generator seeded with `seed` has after `steps` raw steps: how
/// the tests count draws.
uint64_t stepped(uint32_t seed, int steps) {
    RunicRandom r(seed);
    for (int i = 0; i < steps; ++i) r.step();
    return r.state();
}

struct Rig {
    UnitTypeHierarchy types = hierarchy();
    StatBook book = graphs();
    ItemRollGlobals globals;
    AffixBook affixes;
    RunicRandom vol{1};
    RunicRandom game{7};
    ItemRollContext context() {
        ItemRollContext c;
        c.globals = &globals;
        c.affixes = &affixes;
        c.unit_types = &types;
        c.graphs = &book;
        c.volatile_rng = &vol;
        c.game_rng = &game;
        return c;
    }
};

}  // namespace

// ---- the pool ---------------------------------------------------------------

TL_TEST(item_roll_affix_pool_walks_in_name_order_and_keeps_the_last_duplicate) {
    AffixBook p = pool();
    // The console's `map[name] = index`: a second file with a
    // name already taken replaces the first.
    TL_CHECK(!p.add(affix("OF_ICE", 99, 0, 50, 8)));
    const std::vector<std::string> names = p.affix_names();
    TL_CHECK_EQ(names.size(), 5u);
    TL_CHECK_EQ(names[0], "HEAVY");
    TL_CHECK_EQ(names[1], "OF_FIRE");
    TL_CHECK_EQ(names[2], "OF_ICE");
    TL_CHECK_EQ(names[3], "STURDY");
    TL_CHECK_EQ(names[4], "ZERO");
    TL_CHECK_EQ(p.get_affix("of_ice")->weight, 99);
    TL_CHECK(p.get_affix("nothing") == nullptr);
}

// ---- (a) a weapon passing the chance roll -----------------------------------

TL_TEST(item_roll_enchant_normal_sword_passing_the_chance_roll) {
    Rig rig;
    rig.affixes = pool();
    rig.globals.random_enchant_chance = 50.0f;  // below 500 of 1000
    rig.globals.min_random_enchant_slots = 1;
    rig.globals.max_random_enchant_slots = 2;
    ItemStats sword = item("NORMAL SWORD", 3);
    sword.random_enchant_allowed = true;

    tl::core::enchant_item(sword, false, rig.context());

    // Volatile seed 1: 2 slots (low32 0xb2759eff); g_Rand seed 7 picks OF_ICE, then OF_FIRE.
    TL_CHECK_EQ(sword.affixes.size(), 2u);
    TL_CHECK_EQ(sword.affixes[0].affix.name, "Of_Ice");
    TL_CHECK_EQ(sword.affixes[1].affix.name, "Of_Fire");
    TL_CHECK_EQ(sword.affixes[0].level, 3u);
    TL_CHECK_EQ(sword.sockets, 0);
    TL_CHECK(!sword.identified);
    TL_CHECK_EQ(rig.vol.state(), 0x1c1304efe84fd003ull);  // 5 volatile steps
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 5));
    TL_CHECK_EQ(rig.game.state(), stepped(7, 4));  // 2 picks, 2 steps each
}

// ---- (b) failing it -----------------------------------------------------------

TL_TEST(item_roll_enchant_normal_sword_failing_the_chance_roll) {
    Rig rig;
    rig.affixes = pool();
    rig.vol = RunicRandom(2);
    rig.globals.random_enchant_chance = 50.0f;
    rig.globals.min_random_enchant_slots = 1;
    rig.globals.max_random_enchant_slots = 2;
    ItemStats sword = item("NORMAL SWORD", 3);
    sword.random_enchant_allowed = true;

    tl::core::enchant_item(sword, false, rig.context());

    // Volatile, seed 2: randomBetween(0, 1000) = 935.18, not below 500 -> no
    // slot roll and no affixes; the socket roll still draws (702.64), 4 steps.
    TL_CHECK(sword.affixes.empty());
    TL_CHECK(sword.identified);
    TL_CHECK_EQ(rig.vol.state(), 0x1058c6de5aa3ebe9ull);
    TL_CHECK_EQ(rig.vol.state(), stepped(2, 4));
    TL_CHECK_EQ(rig.game.state(), stepped(7, 0));
}

TL_TEST(item_roll_enchant_draws_nothing_without_random_enchant_allowed) {
    Rig rig;
    rig.affixes = pool();
    rig.globals.random_enchant_chance = 100.0f;
    ItemStats sword = item("NORMAL SWORD", 3);  // +0x20a clear
    tl::core::enchant_item(sword, true, rig.context());
    // Not MAGIC and +0x20a clear: no roll, and the socket roll needs +0x20a too.
    TL_CHECK(sword.affixes.empty());
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));
}

// ---- (c) the unique's level + 1 pass -----------------------------------------

TL_TEST(item_roll_enchant_unique_takes_a_second_pass_one_level_up) {
    Rig rig;
    rig.affixes = pool();
    rig.game = RunicRandom(12);
    rig.globals.min_unique_item_slots = 1;
    rig.globals.max_unique_item_slots = 1;
    ItemRollContext context = rig.context();
    context.in_level = false;  // keeps the socket roll out of the count
    ItemStats sword = item("UNIQUE SWORD", 3);
    sword.random_enchant_allowed = true;

    tl::core::enchant_item(sword, true, context);  // forced: no chance roll

    // g_Rand seed 12: OF_ICE, then a second pass at level 4 drops HEAVY (too many slots) and
    // picks OF_FIRE.
    TL_CHECK_EQ(sword.affixes.size(), 2u);
    TL_CHECK_EQ(sword.affixes[0].affix.name, "Of_Ice");
    TL_CHECK_EQ(sword.affixes[0].level, 3u);
    TL_CHECK_EQ(sword.affixes[1].affix.name, "Of_Fire");
    TL_CHECK_EQ(sword.affixes[1].level, 4u);
    TL_CHECK_EQ(rig.game.state(), 0x24e3aea1641f5062ull);
    TL_CHECK_EQ(rig.game.state(), stepped(12, 6));  // three picks
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));
}

TL_TEST(item_roll_enchant_unique_with_passive_effects_skips_the_second_pass) {
    Rig rig;
    rig.affixes = pool(true);
    rig.game = RunicRandom(12);
    rig.globals.min_unique_item_slots = 1;
    rig.globals.max_unique_item_slots = 1;
    ItemRollContext context = rig.context();
    context.in_level = false;
    ItemStats sword = item("UNIQUE SWORD", 3);
    sword.random_enchant_allowed = true;

    tl::core::enchant_item(sword, true, context);

    // Pass 1 as above (OF_ICE, one pick); OF_ICE brings a passive effect, so
    // `+0x1b8`'s `+0x30` is 1 and there is no second pass.
    TL_CHECK_EQ(sword.affixes.size(), 1u);
    TL_CHECK_EQ(sword.affixes[0].affix.name, "Of_Ice");
    TL_CHECK_EQ(rig.game.state(), 0x272263aead424e0dull);
    TL_CHECK_EQ(rig.game.state(), stepped(12, 2));
}

// ---- (d) an affix filtered out by unit type -----------------------------------

TL_TEST(item_roll_enchant_magic_helmet_only_sees_armor_affixes) {
    Rig rig;
    rig.affixes = pool();
    rig.game = RunicRandom(5);
    rig.globals.min_magic_item_slots = 1;
    rig.globals.max_magic_item_slots = 1;
    ItemStats helmet = item("MAGIC HELMET", 3);

    // MAGIC skips the +0x20a test and the chance roll.
    tl::core::enchant_item(helmet, false, rig.context());

    // Only STURDY takes Armor: one candidate, normalised 1.0; roll 0.3380.
    TL_CHECK_EQ(helmet.affixes.size(), 1u);
    TL_CHECK_EQ(helmet.affixes[0].affix.name, "Sturdy");
    TL_CHECK_EQ(rig.game.state(), stepped(5, 2));
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));
}

TL_TEST(item_roll_remove_affixes_that_dont_support_unit_type_swap_removes) {
    const UnitTypeHierarchy types = hierarchy();
    ItemStats helmet = item("NORMAL HELMET", 3);
    helmet.affixes.push_back({affix("Of_Fire", 10, 0, 50, 8), 3});
    helmet.affixes.push_back({affix("Sturdy", 20, 0, 50, 13), 3});
    helmet.affixes.push_back({affix("Armored", 20, 0, 50, 13), 3});
    TL_CHECK(!tl::core::affix_can_be_applied_to_unit_type(helmet.affixes[0].affix, 310, types));
    TL_CHECK(tl::core::affix_can_be_applied_to_unit_type(helmet.affixes[1].affix, 310, types));

    tl::core::remove_affixes_that_dont_support_unit_type(helmet, 310, types);
    // OF_FIRE at 0 goes, and the last entry (ARMORED) moves into its place.
    TL_CHECK_EQ(helmet.affixes.size(), 2u);
    TL_CHECK_EQ(helmet.affixes[0].affix.name, "Armored");
    TL_CHECK_EQ(helmet.affixes[1].affix.name, "Sturdy");
}

// ---- (e) the socket roll -------------------------------------------------------

TL_TEST(item_roll_enchant_socket_roll_one_and_two) {
    for (const auto& [seed, expected] : std::vector<std::pair<uint32_t, int>>{{1, 1}, {43, 2}}) {
        Rig rig;
        rig.affixes = pool();
        rig.vol = RunicRandom(seed);
        rig.globals.random_enchant_chance = 0.0f;  // 0 * 10 <= any roll: no affixes
        rig.globals.random_socket_chance = 50.0f;
        rig.globals.second_socket_chance = 50.0f;
        ItemStats sword = item("NORMAL SWORD", 3);
        sword.random_enchant_allowed = true;
        tl::core::enchant_item(sword, false, rig.context());
        // Seed 1: 1 socket; seed 43: 2 sockets. Six volatile steps either way.
        TL_CHECK(sword.affixes.empty());
        TL_CHECK_EQ(sword.sockets, expected);
        TL_CHECK_EQ(rig.vol.state(), stepped(seed, 6));
    }
}

TL_TEST(item_roll_enchant_unique_draws_the_socket_roll_but_gets_none) {
    Rig rig;
    rig.affixes = pool(true);
    rig.game = RunicRandom(12);
    rig.globals.random_socket_chance = 100.0f;
    rig.globals.min_unique_item_slots = 1;
    rig.globals.max_unique_item_slots = 1;
    ItemStats sword = item("UNIQUE SWORD", 3);
    sword.random_enchant_allowed = true;
    tl::core::enchant_item(sword, true, rig.context());
    // The first socket draw is taken before the type test; a unique fails it,
    // so there is no second draw: 2 volatile steps.
    TL_CHECK_EQ(sword.sockets, 0);
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 2));
}

// ---- (f) ALWAYS_IDENTIFIED -----------------------------------------------------

TL_TEST(item_roll_enchant_always_identified_undoes_the_unidentify) {
    for (bool always : {false, true}) {
        Rig rig;
        rig.affixes = pool();
        rig.game = RunicRandom(9);
        rig.globals.min_magic_item_slots = 1;
        rig.globals.max_magic_item_slots = 1;
        ItemStats sword = item("MAGIC SWORD", 3);
        sword.always_identified = always;
        tl::core::enchant_item(sword, false, rig.context());
        // g_Rand seed 9: roll 0.2083 < 0.25 -> OF_FIRE; the copy is
        // unidentified, then `ALWAYS_IDENTIFIED` identifies it again.
        TL_CHECK_EQ(sword.affixes.size(), 1u);
        TL_CHECK_EQ(sword.affixes[0].affix.name, "Of_Fire");
        TL_CHECK_EQ(sword.identified, always);
    }
}

TL_TEST(item_roll_is_magical) {
    const UnitTypeHierarchy types = hierarchy();
    ItemStats plain = item("NORMAL SWORD", 3);
    TL_CHECK(!tl::core::item_is_magical(plain, types));
    TL_CHECK(tl::core::item_is_magical(item("MAGIC HELMET", 3), types));
    // The block chance's effect (0x3e) does not count; another passive does.
    tl::core::Effect block;
    block.activation = tl::core::EffectActivation::Passive;
    block.type = 0x3e;
    plain.effects.push_back(block);
    TL_CHECK(!tl::core::item_is_magical(plain, types));
    plain.affixes.push_back({affix("Of_Fire", 10, 0, 50, 8), 3});
    TL_CHECK(tl::core::item_is_magical(plain, types));
}

// ---- globals ----------------------------------------------------------------

TL_TEST(item_roll_globals_read_over_zero_defaults) {
    tl::formats::AdmFile file;
    file.root.name = "GLOBALS";
    tl::formats::AdmRecord chance;
    chance.key = "RANDOM_ENCHANT_CHANCE";
    chance.type = tl::formats::AdmType::Float;
    chance.number = 7.5f;
    file.root.records.push_back(chance);
    tl::formats::AdmRecord slots;
    slots.key = "MAX_UNIQUE_ITEM_SLOTS";
    slots.integer = 4;
    file.root.records.push_back(slots);
    const ItemRollGlobals g = tl::core::read_item_roll_globals(file);
    TL_CHECK_EQ(g.random_enchant_chance, 7.5f);
    TL_CHECK_EQ(g.max_unique_item_slots, 4);
    TL_CHECK_EQ(g.min_unique_item_slots, 0);
    TL_CHECK_EQ(g.second_socket_chance, 0.0f);
}

// ---- Part B: requirements -----------------------------------------------------

TL_TEST(item_roll_set_requirements_from_the_curves) {
    const UnitTypeHierarchy types = hierarchy();
    const StatBook book = graphs();
    ItemStats sword = item("NORMAL SWORD", 10);
    sword.authored_requirements.strength = 50;
    sword.authored_requirements.magic = 5;
    tl::core::set_item_requirements(sword, book, types);
    // Level: ITEM_LEVEL_REQUIREMENTS(10) less min(heirloom, 5); a stat of 1 or less is 0.
    TL_CHECK_EQ(sword.min_level_required, 10);
    TL_CHECK_EQ(sword.strength_required, 10);
    TL_CHECK_EQ(sword.magic_required, 0);
    TL_CHECK_EQ(sword.dexterity_required, 0);

    sword.heirloom_level = 7;
    tl::core::set_item_requirements(sword, book, types);
    // Level: 10 - min(7, 5) = 5. Strength: 10 - min(7, 10) = 3.
    TL_CHECK_EQ(sword.min_level_required, 5);
    TL_CHECK_EQ(sword.strength_required, 3);

    // A ring is not a Weapon, Armor or Trinket: its authored figures stand.
    ItemStats ring = item("NORMAL RING", 10);
    ring.authored_requirements.strength = 50;
    tl::core::set_item_requirements(ring, book, types);
    TL_CHECK_EQ(ring.min_level_required, 0);
    TL_CHECK_EQ(ring.strength_required, 50);
}

TL_TEST(item_roll_requirement_getters_subtract_the_character_reductions) {
    const UnitTypeHierarchy types = hierarchy();
    ItemStats sword = item("NORMAL SWORD", 10);
    sword.min_level_required = 10;
    sword.strength_required = 10;
    tl::core::RequirementReductions character;
    character.all = 2.7f;
    character.martial = 1.6f;
    // A martial weapon: (int)((float)(int)2.7 + 1.6) = (int)3.6 = 3.
    TL_CHECK_EQ(tl::core::item_level_requirement(sword, &character, types), 7);
    TL_CHECK_EQ(tl::core::item_strength_requirement(sword, &character, types), 7);
    // No character: the stored figure.
    TL_CHECK_EQ(tl::core::item_level_requirement(sword, nullptr, types), 10);
    // A ring is in none of the categories: (int)2.7 = 2 alone.
    ItemStats ring = item("NORMAL RING", 10);
    ring.strength_required = 1;
    TL_CHECK_EQ(tl::core::item_strength_requirement(ring, &character, types),
                0);  // 1 - 2, floored at 0
    ring.strength_required = 5;
    TL_CHECK_EQ(tl::core::item_strength_requirement(ring, &character, types), 3);
}

// ---- Part C: prices ---------------------------------------------------------------

TL_TEST(item_roll_prices_plain_magic_socketed_and_stacked) {
    const UnitTypeHierarchy types = hierarchy();
    const StatBook book = graphs();

    // Plain: VALUE 150 at level 10. Buy ceil(100 * 1.5) = 150, sell
    // ceil(25 * 1.5) = ceil(37.5) = 38; the NORMAL pair is the same.
    ItemStats plain = item("NORMAL SWORD", 10);
    plain.value = 150;
    tl::core::recalculate_item_price(plain, book, types);
    TL_CHECK_EQ(plain.buy_price, 150);
    TL_CHECK_EQ(plain.sell_price, 38);
    TL_CHECK_EQ(plain.sell_price_normal, 38);
    TL_CHECK_EQ(tl::core::item_sell_price_of(plain, 1, nullptr), 38);

    // Sockets do not enter the price.
    ItemStats socketed = plain;
    socketed.sockets = 2;
    tl::core::recalculate_item_price(socketed, book, types);
    TL_CHECK_EQ(socketed.sell_price, 38);

    // Two affixes make it magical: buy ceil(300 * 1.5) = 450, sell
    // ceil(75 * 1.5) = ceil(112.5) = 113; unidentified it trades at 38.
    ItemStats magic = plain;
    magic.affixes.push_back({affix("Of_Fire", 10, 0, 50, 8), 10});
    magic.affixes.push_back({affix("Of_Ice", 30, 0, 50, 8), 10});
    tl::core::recalculate_item_price(magic, book, types);
    TL_CHECK_EQ(magic.buy_price, 450);
    TL_CHECK_EQ(magic.sell_price, 113);
    // A stack of 3 with a 10% bonus: 339 + (int)(339 * 0.1 = 33.9) = 372.
    const float bonus = 10.0f;
    TL_CHECK_EQ(tl::core::item_sell_price_of(magic, 3, &bonus), 372);
    // 450 - (int)(450 * (10 * 0.01f)): the cut truncates to 44, so 406.
    //
    TL_CHECK_EQ(tl::core::item_buy_price_of(magic, 1, &bonus), 406);
    magic.identified = false;
    TL_CHECK_EQ(tl::core::item_sell_price_of(magic, 3, &bonus), 114);  // 3 * 38, no bonus

    // A gambler sells at PRICE_PLAYERGAMBLE_MAGIC: ceil(200 * 1.5) = 300.
    tl::core::recalculate_item_price(magic, book, types, true);
    TL_CHECK_EQ(magic.buy_price, 300);

    // VALUE 0: both left at 1. Level 0 prices at level 1.
    ItemStats worthless = item("NORMAL SWORD", 10);
    worthless.value = 0;
    tl::core::recalculate_item_price(worthless, book, types);
    TL_CHECK_EQ(worthless.buy_price, 1);
    TL_CHECK_EQ(worthless.sell_price, 1);
}

TL_TEST(item_roll_enchant_price_has_a_floor_of_300) {
    const StatBook book = graphs();
    ItemRollGlobals globals;
    globals.enchanter_price_per_enchant = 0.5f;
    ItemStats sword = item("NORMAL SWORD", 10);
    sword.enchant_count = 3;
    // p = 100: (int)(0.5 * 100 * 3) + 100 = 250 -> 300.
    TL_CHECK_EQ(tl::core::item_enchant_price(sword, book, globals), 300);
    sword.enchant_count = 10;
    // (int)(0.5 * 100 * 10) + 100 = 600.
    TL_CHECK_EQ(tl::core::item_enchant_price(sword, book, globals), 600);
}

TL_TEST(item_roll_old_sell_price_wrapper_still_answers) {
    const StatBook book = graphs();
    ItemStats plain = item("NORMAL SWORD", 10);
    plain.value = 0;  // the renderer's loader: not authored -> 100
    // ceil(25 * 1.0) = 25, twice.
    TL_CHECK_EQ(tl::core::item_sell_price(plain, book, 2), 50);
}

// ---- Part D: sockets --------------------------------------------------------------

TL_TEST(item_roll_add_sockets) {
    Rig rig;
    rig.globals.second_socket_chance = 50.0f;
    ItemStats sword = item("NORMAL SWORD", 10);
    // Room for two: randomBetween(0, 1000) from seed 1 = 467.59 < 500 -> +2.
    tl::core::add_sockets(sword, rig.context());
    TL_CHECK_EQ(sword.sockets, 2);
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 2));
    // At the maximum (MAX_SOCKETS 2): nothing, no draw.
    tl::core::add_sockets(sword, rig.context());
    TL_CHECK_EQ(sword.sockets, 2);
    // Room for one: +1 without a draw.
    sword.sockets = 1;
    tl::core::add_sockets(sword, rig.context());
    TL_CHECK_EQ(sword.sockets, 2);
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 2));
    // A potion takes none.
    ItemStats potion = item("Potion", 1);
    tl::core::add_sockets(potion, rig.context());
    TL_CHECK_EQ(potion.sockets, 0);
}

// ---- Part E: loot ------------------------------------------------------------------

TL_TEST(item_roll_roll_money_with_gold_find) {
    RunicRandom vol(20);
    tl::core::LootCharacter killer;
    killer.level_number = 8;
    killer.gold_find = 50.0f;
    // Seed 20: a miss, a pile of 25, a miss: 15 steps.
    const std::vector<int> piles = tl::core::roll_money(&killer, nullptr, vol);
    TL_CHECK_EQ(piles.size(), 1u);
    TL_CHECK_EQ(piles[0], 25);
    TL_CHECK_EQ(vol.state(), stepped(20, 15));
}

TL_TEST(item_roll_roll_money_one_pile_and_dont_drop_gold) {
    RunicRandom vol(3);
    tl::core::LootCharacter dropper;
    dropper.level_number = 8;
    // Seed 3: 0.4028 (miss), 0.0540 (< 0.1: a pile, randomIntegerBetween(6,
    // 36) = 26, no killer so no bonus, then its 8 making steps), a miss: 15.
    const std::vector<int> piles = tl::core::roll_money(nullptr, &dropper, vol);
    TL_CHECK_EQ(piles.size(), 1u);
    TL_CHECK_EQ(piles[0], 26);
    TL_CHECK_EQ(vol.state(), stepped(3, 15));

    dropper.dont_drop_gold = true;
    RunicRandom untouched(3);
    TL_CHECK(tl::core::roll_money(nullptr, &dropper, untouched).empty());
    TL_CHECK_EQ(untouched.state(), stepped(3, 0));
}

namespace {

// One fixed entry (weight -1) naming one unit, one copy.
tl::core::SpawnClass fixed_class(const std::string& unit, uint32_t flags) {
    tl::core::SpawnClass sc("TEST_TREASURE");
    tl::core::SpawningData data;
    data.weight = -1;
    data.flags = flags;
    auto one = std::make_shared<tl::core::SpawnClassData>("one");
    tl::core::SpawnUnitData u;
    u.name = unit;
    one->add_unit_data_object(u);
    sc.add_spawn_class_data(data, one);
    return sc;
}

struct Made {
    std::string unit;
    int32_t level;
    bool force;
};

}  // namespace

TL_TEST(item_roll_roll_treasure_and_death_drop) {
    // `rollSpawnClass` pushes a fixed single unit with `flags & 1` as its
    // enchant flag; MINCOUNT = MAXCOUNT = 1 draws nothing.
    const tl::core::SpawnClass barrel = fixed_class("Sword", 0);
    tl::core::SpawnContext spawn;
    std::vector<Made> made;
    const tl::core::CreateLootUnit create = [&](const std::string& unit, int32_t level, bool force,
                                                ItemStats& out) {
        made.push_back({unit, level, force});
        out = item("NORMAL SWORD", level);
        return true;
    };
    tl::core::LootCharacter killer;
    killer.level_number = 8;
    RunicRandom vol(3);
    RunicRandom game(7);

    // `rollMoney` is never forced here. The barrel's unit is added at the position
    // itself, with no draw.
    std::vector<tl::core::Vec3> added;
    std::vector<int> gold_added;
    tl::core::LootPlacement place;
    place.at = {10.0f, 2.0f, 20.0f};
    place.add_unit = [&](const ItemStats&, const tl::core::Vec3& where) { added.push_back(where); };
    place.add_gold = [&](const tl::core::GoldPile& pile, const tl::core::Vec3&) {
        gold_added.push_back(pile.amount);
    };
    tl::core::LootDrop drop =
        tl::core::roll_treasure(&barrel, 8, &killer, nullptr, vol, game, spawn, create, &place);
    TL_CHECK_EQ(added.size(), 1u);
    TL_CHECK_EQ(added[0].x, 10.0f);
    TL_CHECK_EQ(added[0].z, 20.0f);
    TL_CHECK_EQ(gold_added.size(), 1u);
    TL_CHECK_EQ(gold_added[0], 26);
    TL_CHECK_EQ(drop.items.size(), 1u);
    TL_CHECK_EQ(made.size(), 1u);
    TL_CHECK_EQ(made[0].unit, "Sword");
    TL_CHECK_EQ(made[0].level, 8);
    TL_CHECK(!made[0].force);
    TL_CHECK_EQ(drop.gold.size(), 1u);
    TL_CHECK_EQ(drop.gold[0], 26);
    TL_CHECK_EQ(vol.state(), stepped(3, 15));
    TL_CHECK_EQ(game.state(), stepped(7, 0));

    // MIN = MAX = 2: two rolls. FORCEENCHANT does not reach a lone fixed unit (the death drop
    // passes `flags & 1` = 0).
    made.clear();
    const tl::core::SpawnClass own = fixed_class("Axe", 1);
    tl::core::DyingCharacter monster;
    monster.loot.level_number = 5;
    monster.treasure.spawn_class = &own;
    monster.treasure.min = 2;
    monster.treasure.max = 2;
    RunicRandom vol2(3);
    drop = tl::core::roll_death_drop(monster, &killer, &barrel, vol2, game, spawn, create);
    TL_CHECK_EQ(drop.items.size(), 2u);
    TL_CHECK_EQ(made.size(), 2u);
    TL_CHECK_EQ(made[0].unit, "Axe");
    TL_CHECK_EQ(made[0].level, 5);  // the level's number, as turns -1
    TL_CHECK(!made[0].force);
    TL_CHECK(!made[1].force);
    // MIN = MAX draws nothing; each unit is placed with a `randomOpenPosition` of 0.5. Gold is
    // never forced.
    TL_CHECK(drop.gold.empty());
    TL_CHECK_EQ(vol2.state(), stepped(3, 14));

    // A player's death drops nothing and draws nothing.
    monster.is_player = true;
    made.clear();
    RunicRandom vol3(3);
    drop = tl::core::roll_death_drop(monster, &killer, &barrel, vol3, game, spawn, create);
    TL_CHECK_EQ(vol3.state(), stepped(3, 0));
    TL_CHECK(drop.items.empty());
    TL_CHECK(drop.gold.empty());
}

TL_TEST(item_roll_gold_pile_and_gold_unit) {
    // `CItemGold::CItemGold`: the turn, then the size, on `g_RandVolatile`.
    RunicRandom vol(5);
    const tl::core::GoldPile pile = tl::core::make_gold_pile(26, vol);
    TL_CHECK_EQ(pile.amount, 26);
    TL_CHECK(std::abs(pile.heading - 121.664383f) < 1e-3f);
    TL_CHECK(std::abs(pile.scale - 1.1026361f) < 1e-6f);
    TL_CHECK_EQ(vol.state(), stepped(5, 4));

    // A `GOLD100`: MINVALUE 100, MAXVALUE 150, SCALE 1.25; the curve at the level plus one.
    StatBook book;
    book.add("GOLDDROP", line(750.0f));        // 37.5 at 5
    book.add("GOLDDROP_HARD", line(1000.0f));  // 50 at 5
    ItemStats gold = item("Gold", 1);
    gold.create_as = "GOLD";
    gold.min_value = 100.0f;
    gold.max_value = 150.0f;
    gold.scale = 1.25f;
    TL_CHECK(gold.is_gold());
    ItemStats normal = gold;
    RunicRandom v1(9);
    tl::core::init_gold_copy(normal, false, 4, 1, book, v1);
    TL_CHECK_EQ(normal.gold.amount, 55);  // ceil(37.5 * 1.44728)
    TL_CHECK_EQ(normal.gold.scale, 1.25f);
    TL_CHECK_EQ(v1.state(), stepped(9, 6));
    ItemStats hard = gold;
    RunicRandom v2(9);
    tl::core::init_gold_copy(hard, false, 4, 2, book, v2);
    TL_CHECK_EQ(hard.gold.amount, 73);
    // No difficulty arm: the amount stays the constructor's 1; the roll is
    // still drawn.
    ItemStats odd = gold;
    RunicRandom v3(9);
    tl::core::init_gold_copy(odd, false, 4, 7, book, v3);
    TL_CHECK_EQ(odd.gold.amount, 1);
    TL_CHECK_EQ(v3.state(), stepped(9, 6));
    // From a save: the constructor only.
    ItemStats saved = gold;
    RunicRandom v4(9);
    tl::core::init_gold_copy(saved, true, 4, 1, book, v4);
    TL_CHECK_EQ(saved.gold.amount, 1);
    TL_CHECK_EQ(v4.state(), stepped(9, 4));
}

TL_TEST(item_roll_spawned_unit_position) {
    const UnitTypeHierarchy types = hierarchy();
    const tl::core::Vec3 at{10.0f, 2.0f, 20.0f};
    tl::core::LootPlacement place;
    place.at = at;
    ItemStats gold = item("Gold", 1);
    ItemStats sword = item("TAKEABLE SWORD", 1);

    // A takeable that is not gold stays at the position; the point is still drawn.
    RunicRandom v0(13);
    const tl::core::Vec3 kept = tl::core::spawned_unit_position(sword, 0, at, &place, &types, v0);
    TL_CHECK_EQ(kept.x, 10.0f);
    TL_CHECK_EQ(kept.y, 2.0f);
    TL_CHECK_EQ(kept.z, 20.0f);
    TL_CHECK_EQ(v0.state(), stepped(13, 4));

    // Gold at index 0 (radius 0.5), then index 6 (radius 6 / 3 = 2), at the
    // point's X and Z and half a unit up.
    RunicRandom vol(13);
    const tl::core::Vec3 first = tl::core::spawned_unit_position(gold, 0, at, &place, &types, vol);
    TL_CHECK(std::abs(first.x - 9.9838915f) < 1e-4f);
    TL_CHECK(std::abs(first.z - 19.964106f) < 1e-4f);
    TL_CHECK_EQ(first.y, 2.5f);
    const tl::core::Vec3 sixth = tl::core::spawned_unit_position(gold, 6, at, &place, &types, vol);
    TL_CHECK(std::abs(sixth.x - 8.869217f) < 1e-4f);
    TL_CHECK(std::abs(sixth.z - 18.736782f) < 1e-4f);
    TL_CHECK_EQ(vol.state(), stepped(13, 8));
}

TL_TEST(item_roll_random_open_position_widens_to_five) {
    // Every point refused: a hundred tries of two draws, then `at` itself.
    // After the sixth refusal the outer radius is max(5, 0.5 + 0.1) = 5.
    RunicRandom vol(11);
    const tl::core::Vec3 at{1.0f, 2.0f, 3.0f};
    const tl::core::Vec3 got = tl::core::random_open_position(
        at, 0.0f, 0.5f, [](const tl::core::Vec3&) { return false; }, vol);
    TL_CHECK_EQ(got.x, 1.0f);
    TL_CHECK_EQ(got.y, 2.0f);
    TL_CHECK_EQ(got.z, 3.0f);
    TL_CHECK_EQ(vol.state(), stepped(11, 400));
    // Open everywhere: the first try, whose distance is inside the radius.
    RunicRandom once(11);
    const tl::core::Vec3 near = tl::core::random_open_position(at, 0.0f, 0.5f, nullptr, once);
    const float dx = near.x - at.x;
    const float dz = near.z - at.z;
    TL_CHECK(std::sqrt(dx * dx + dz * dz) <= 0.5f + 1e-5f);
    TL_CHECK_EQ(once.state(), stepped(11, 4));
}

// ---- Part F: a fresh copy ---------------------------------------------------------

TL_TEST(item_roll_init_item_copy_caps_sockets_and_prices) {
    Rig rig;
    rig.affixes = pool();
    ItemStats ring = item("NORMAL RING", 10);
    ring.authored_sockets = 5;
    ring.value = 100;
    // +0x20a clear: no chance roll and no socket roll; a ring rolls no damage.
    tl::core::init_item_copy(ring, false, false, false, rig.context());
    TL_CHECK_EQ(ring.sockets, 2);  // `SOCKETS` capped at 2
    TL_CHECK(ring.identified);
    TL_CHECK_EQ(ring.sell_price, 25);  // ceil(25 * 1.0)
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));
}

// `CBaseUnit::reapplyAffixes(false)` first puts a unique's authored `[AFFIXES]` on, so
// `enchant` rolls no second pass; a save's copy gets none unless `SOCKETABLE`.
TL_TEST(item_roll_init_item_copy_puts_the_authored_affixes_on_first) {
    Rig rig;
    rig.affixes = pool(/*with_passive_effects=*/true);
    rig.globals.min_unique_item_slots = 1;
    rig.globals.max_unique_item_slots = 3;
    ItemStats token = item("UNIQUE RING", 10);
    token.authored_affixes = {"of_fire", "NO_SUCH_AFFIX", "HEAVY"};
    tl::core::init_item_copy(token, false, false, false, rig.context());
    TL_CHECK_EQ(token.affixes.size(), 2u);
    TL_CHECK_EQ(token.affixes[0].affix.name, std::string("Of_Fire"));
    TL_CHECK_EQ(token.affixes[0].level, 10u);
    TL_CHECK_EQ(token.affixes[1].affix.name, std::string("Heavy"));
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));

    ItemStats loaded = item("UNIQUE RING", 10);
    loaded.authored_affixes = {"OF_FIRE"};
    tl::core::init_item_copy(loaded, true, false, false, rig.context());
    TL_CHECK(loaded.affixes.empty());
    ItemStats gem = item("UNIQUE EMBER", 10);
    gem.authored_affixes = {"OF_FIRE"};
    tl::core::init_item_copy(gem, true, false, false, rig.context());
    TL_CHECK_EQ(gem.affixes.size(), 1u);

    // Reapplying passes over a name already held.
    tl::core::reapply_affixes(token, rig.affixes, true);
    TL_CHECK_EQ(token.affixes.size(), 2u);
    tl::core::reapply_affixes(token, rig.affixes, false);
    TL_CHECK_EQ(token.affixes.size(), 4u);
}

TL_TEST(item_roll_init_item_copy_identification_and_block_chance) {
    // +0x20a clear: no first pass and no socket roll, so the only draw left is
    // the unique second pass's slot count, randomIntegerBetween(1, 3): one step.
    Rig rig;
    rig.affixes = pool();
    rig.globals.min_unique_item_slots = 1;
    rig.globals.max_unique_item_slots = 3;

    // A unique with `BLOCK_CHANCE` takes no second pass; `unitInit` unidentifies a `UNIQUE`
    // before the roll.
    ItemStats blocking = item("UNIQUE RING", 10);
    blocking.block_chance = 5;
    tl::core::init_item_copy(blocking, false, false, false, rig.context());
    TL_CHECK(!blocking.identified);
    TL_CHECK(blocking.affixes.empty());
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 0));

    // Without it the second pass rolls (no ring affix in the pool: none added).
    ItemStats bare = item("UNIQUE RING", 10);
    tl::core::init_item_copy(bare, false, false, false, rig.context());
    TL_CHECK(!bare.identified);
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 1));

    // A `SOCKETABLE` is identified after the roll, unique or not.
    ItemStats ember = item("UNIQUE EMBER", 10);
    ember.block_chance = 5;
    tl::core::init_item_copy(ember, false, false, false, rig.context());
    TL_CHECK(ember.identified);
    TL_CHECK(!tl::core::item_is_magical(ember, rig.types));

    // From a save: no roll and no identification change.
    ItemStats saved = item("UNIQUE RING", 10);
    tl::core::init_item_copy(saved, true, false, false, rig.context());
    TL_CHECK(saved.identified);
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 1));
}
