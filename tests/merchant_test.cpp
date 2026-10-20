// A merchant's stock roll and when it restocks. Two generators seeded with 1 log into one
// trace, so the order of draws across the streams is checked (0x29777b41 multiplier).
#include "core/merchant.h"
#include "core/item_roll.h"
#include "core/random.h"
#include "core/spawn_class.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <memory>
#include <string>
#include <vector>

namespace {

using tl::core::Inventory;
using tl::core::InventoryPane;
using tl::core::ItemStats;
using tl::core::MerchantCharacter;
using tl::core::MerchantStockContext;
using tl::core::MerchantStockOutcome;
using tl::core::RunicRandom;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

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

UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Weapon", 8, {0}));
    all.children.push_back(unit_type("Armor", 13, {0}));
    all.children.push_back(unit_type("Ring", 17, {0}));
    all.children.push_back(unit_type("Takeable", 31, {0}));
    all.children.push_back(unit_type("Potion", 33, {31, 0}));
    all.children.push_back(unit_type("UNIQUE", 54, {0}));
    all.children.push_back(unit_type("MAGIC", 55, {0}));
    all.children.push_back(unit_type("NORMAL", 200, {0}));
    all.children.push_back(unit_type("NORMAL SWORD", 300, {8, 200, 31, 0}));
    all.children.push_back(unit_type("NORMAL HELMET", 310, {13, 200, 31, 0}));
    all.children.push_back(unit_type("UNIQUE RING", 321, {17, 54, 31, 0}));
    all.children.push_back(unit_type("QUESTITEM", 103, {31, 0}));
    root.children.push_back(all);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

tl::core::StatGraph line(float y_at_100) {
    tl::core::StatGraph g(tl::core::GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(100.0f, y_at_100);
    g.process_points();
    return g;
}

/// Straight price lines through 0: at level 10 each is a tenth of its 100.
tl::core::StatBook prices() {
    tl::core::StatBook book;
    book.add("PRICE_PLAYERBUY_NORMAL", line(1000.0f));    // 100
    book.add("PRICE_PLAYERSELL_NORMAL", line(250.0f));    // 25
    book.add("PRICE_PLAYERBUY_MAGIC", line(3000.0f));     // 300
    book.add("PRICE_PLAYERSELL_MAGIC", line(750.0f));     // 75
    book.add("PRICE_PLAYERGAMBLE_MAGIC", line(2000.0f));  // 200
    return book;
}

/// One spawn class that always yields `unit`, `1..2` copies a roll.
std::shared_ptr<tl::core::SpawnClass> one_unit_class(const std::string& unit) {
    auto spawn_class = std::make_shared<tl::core::SpawnClass>("MERCHANT_STOCK");
    tl::core::SpawningData always;
    always.weight = -1;
    always.min_count = 1;
    always.max_count = 2;
    auto data = std::make_shared<tl::core::SpawnClassData>("STOCK");
    tl::core::SpawnUnitData data_unit;
    data_unit.name = unit;
    data->add_unit_data_object(data_unit);
    spawn_class->add_spawn_class_data(always, data);
    return spawn_class;
}

/// A log both generators write to, each line tagged with its stream.
struct DrawLog {
    std::vector<std::string> lines;
    struct Tap {
        DrawLog* log;
        const char* tag;
    };
    Tap vol_tap{this, "V"};
    Tap game_tap{this, "G"};
    tl::core::RandomTrace vol_trace{&DrawLog::write, &vol_tap};
    tl::core::RandomTrace game_trace{&DrawLog::write, &game_tap};
    static void write(void* user, const char* line) {
        Tap* tap = static_cast<Tap*>(user);
        tap->log->lines.push_back(std::string(tap->tag) + " " + line);
    }
};

struct Shop {
    UnitTypeHierarchy types = hierarchy();
    tl::core::StatBook book = prices();
    tl::core::ItemRollGlobals globals;
    tl::core::AffixBook affixes;
    tl::core::SpawnContext spawn;
    RunicRandom vol{1};
    RunicRandom game{1};
    uint32_t player_level = 7;
    Inventory inventory = tl::core::make_merchant_inventory();
    std::shared_ptr<tl::core::SpawnClass> stock;
    tl::core::ItemRollContext roll;

    explicit Shop(const std::string& unit) : stock(one_unit_class(unit)) {
        globals.min_random_enchant_slots = 1;
        globals.max_random_enchant_slots = 1;
        tl::core::Affix sharp;
        sharp.name = "SHARP";
        sharp.min_spawn_range = 0;
        sharp.max_spawn_range = 100;
        sharp.unit_types = {8};
        affixes.add(sharp);
        roll.globals = &globals;
        roll.affixes = &affixes;
        roll.unit_types = &types;
        roll.graphs = &book;
        roll.volatile_rng = &vol;
        roll.game_rng = &game;
    }

    MerchantCharacter merchant(bool gambler = false) {
        MerchantCharacter m;
        m.inventory = &inventory;
        m.is_merchant = !gambler;
        m.is_gambler = gambler;
        m.treasure.spawn_class = stock.get();
        m.treasure.min = 2;
        m.treasure.max = 3;
        m.spawn.is_merchant = true;
        return m;
    }

    MerchantStockContext context() {
        MerchantStockContext ctx;
        ctx.player_level = &player_level;
        ctx.spawn_context = &spawn;
        ctx.volatile_rng = &vol;
        ctx.game_rng = &game;
        ctx.roll = &roll;
        ctx.create_unit = [](const std::string& unit, int32_t, bool, ItemStats& out) {
            out = ItemStats{};
            out.name = unit;
            out.level = 10;
            if (unit == "Health Potion") {
                out.unittype = "Potion";
                out.max_stack = 20;
            } else if (unit == "Plain Sword") {
                out.unittype = "NORMAL SWORD";
            } else if (unit == "Magic Sword") {
                out.unittype = "NORMAL SWORD";
                tl::core::Affix rolled;
                rolled.name = "ROLLED";
                out.affixes.push_back(ItemStats::RolledAffix{rolled, 10});
            } else {
                out.unittype = "UNIQUE RING";
            }
            out.identified = false;
            out.random_enchant_allowed = true;  // `createItem` stores its third argument
            return true;
        };
        return ctx;
    }
};

TL_TEST(merchant_stock_draws_the_count_on_volatile_then_rolls_on_g_rand) {
    Shop shop("Health Potion");
    DrawLog log;
    shop.vol.set_trace(&log.vol_trace);
    shop.game.set_trace(&log.game_trace);
    MerchantCharacter merchant = shop.merchant();
    const MerchantStockOutcome out =
        tl::core::generate_merchant_inventory(merchant, shop.context());

    // The count: `integer_between(2, 3)` on volatile, 2 + 695696193 % 2 = 3.
    TL_CHECK_EQ(out.count, 3u);
    // Then three spawn-class rolls on `g_Rand`, `integer_between(1, 2)` copies
    // each: 1 + 1, 1 + 1, 1 + 1 (steps 1-3 above) = 6 potions.
    TL_CHECK_EQ(log.lines.size(), 4u);
    TL_CHECK(log.lines[0].rfind("V I 2 3", 0) == 0);
    TL_CHECK(log.lines[1].rfind("G I 1 2", 0) == 0);
    TL_CHECK(log.lines[2].rfind("G I 1 2", 0) == 0);
    TL_CHECK(log.lines[3].rfind("G I 1 2", 0) == 0);
    TL_CHECK_EQ(shop.game.state(), 0x1a871235b2759effull);
    // Stacking is off: six identical potions take six backpack slots, each
    // identified.
    TL_CHECK(!shop.inventory.stacking());
    TL_CHECK_EQ(out.slots.size(), 6u);
    TL_CHECK_EQ(shop.inventory.items_in_pane(InventoryPane::Backpack), 6);
    TL_CHECK_EQ(shop.inventory.slots()[19].count, 1);
    TL_CHECK_EQ(shop.inventory.slots()[20].count, 1);
    TL_CHECK(shop.inventory.slots()[19].item.identified);
}

TL_TEST(merchant_level_comes_from_the_player_and_the_factor) {
    Shop shop("Health Potion");
    MerchantCharacter merchant = shop.merchant();
    merchant.treasure.spawn_class = nullptr;  // the level and flags only
    MerchantStockContext ctx = shop.context();
    // Factor 0 (the default): -1.
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).level, -1);
    // (int)ceilf(7 * 1.5) = (int)ceilf(10.5) = 11.
    merchant.data.player_level_factor = 1.5f;
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).level, 11);
    // No player in the level: -1 whatever the factor.
    ctx.player_level = nullptr;
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).level, -1);
    // Outside a level: nothing at all.
    merchant.in_level = false;
    TL_CHECK(!tl::core::generate_merchant_inventory(merchant, ctx).rolled);
}

TL_TEST(merchant_flags_take_no_uniques_and_no_magic_bits) {
    Shop shop("Health Potion");
    MerchantCharacter merchant = shop.merchant();
    merchant.treasure.spawn_class = nullptr;
    const MerchantStockContext ctx = shop.context();
    // Defaults: uniques not allowed (2), magic allowed.
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).flags, 2);
    merchant.data.allows_magic = false;
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).flags, 6);
    merchant.data.allows_uniques = true;
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).flags, 4);
    merchant.data.allows_magic = true;
    TL_CHECK_EQ(tl::core::generate_merchant_inventory(merchant, ctx).flags, 0);
}

TL_TEST(merchant_throws_a_unique_away_unless_allowed) {
    {
        Shop shop("Unique Band");
        MerchantCharacter merchant = shop.merchant();
        const MerchantStockOutcome out =
            tl::core::generate_merchant_inventory(merchant, shop.context());
        TL_CHECK_EQ(out.discarded, 6u);
        TL_CHECK(out.slots.empty());
    }
    {
        Shop shop("Unique Band");
        MerchantCharacter merchant = shop.merchant();
        merchant.data.allows_uniques = true;
        const MerchantStockOutcome out =
            tl::core::generate_merchant_inventory(merchant, shop.context());
        TL_CHECK_EQ(out.discarded, 0u);
        TL_CHECK_EQ(out.slots.size(), 6u);
    }
}

TL_TEST(gambler_enchants_a_plain_item_and_prices_it_to_gamble) {
    Shop shop("Plain Sword");
    MerchantCharacter gambler = shop.merchant(true);
    const MerchantStockOutcome out = tl::core::generate_merchant_inventory(gambler, shop.context());
    TL_CHECK_EQ(out.slots.size(), 6u);
    // Into the backpack (19..68), a merchant's only pane on the console.
    const ItemStats& sword = shop.inventory.slots()[static_cast<size_t>(out.slots[0])].item;
    TL_CHECK(out.slots[0] >= 19 && out.slots[0] < 69);
    // `enchant(true)`: one slot (MIN = MAX = 1), SHARP, the only sword affix.
    TL_CHECK_EQ(sword.affixes.size(), 1u);
    TL_CHECK(sword.identified);
    // Magical now: sells at the MAGIC curve; buys at the gamble curve once it is the gambler's
    // (not the MAGIC buy price `enchant` left).
    TL_CHECK_EQ(sword.sell_price, 75);
    TL_CHECK_EQ(sword.buy_price, 200);

    // A magical item is not enchanted again, but still priced to gamble.
    Shop other("Magic Sword");
    MerchantCharacter second = other.merchant(true);
    const MerchantStockOutcome kept =
        tl::core::generate_merchant_inventory(second, other.context());
    const ItemStats& magic = other.inventory.slots()[static_cast<size_t>(kept.slots[0])].item;
    TL_CHECK_EQ(magic.affixes.size(), 1u);
    TL_CHECK_EQ(magic.affixes[0].affix.name, std::string("ROLLED"));
    TL_CHECK_EQ(magic.buy_price, 200);

    // A plain merchant keeps a plain sword plain.
    Shop plain("Plain Sword");
    MerchantCharacter merchant = plain.merchant();
    const MerchantStockOutcome unchanged =
        tl::core::generate_merchant_inventory(merchant, plain.context());
    TL_CHECK(plain.inventory.slots()[static_cast<size_t>(unchanged.slots[0])].item.affixes.empty());
}

TL_TEST(populate_restocks_only_an_empty_merchant) {
    Shop shop("Health Potion");
    Inventory stocked = tl::core::make_merchant_inventory();
    ItemStats helm;
    helm.name = "Helm";
    helm.unittype = "NORMAL HELMET";
    TL_CHECK(stocked.pickup_into_slot(helm, 1, 20));  // the backpack: a merchant has no other pane
    Inventory backpack_only = tl::core::make_merchant_inventory();
    ItemStats potion;
    potion.name = "Health Potion";
    potion.unittype = "Potion";
    TL_CHECK(backpack_only.pickup_into_slot(potion, 1, 19));

    MerchantCharacter a = shop.merchant();
    a.inventory = &stocked;
    // No Spell pane on a merchant: `getPaneIndex` answers for the backpack,
    // so a potion in the backpack also keeps the old stock.
    MerchantCharacter b = shop.merchant();
    b.inventory = &backpack_only;
    MerchantCharacter c = shop.merchant();  // the empty one
    MerchantCharacter npc = shop.merchant();
    npc.is_merchant = false;
    Inventory npc_bag = tl::core::make_merchant_inventory();
    npc.inventory = &npc_bag;

    const int restocked =
        tl::core::populate_new_merchants_inventory({&a, &b, &npc, &c}, shop.context());
    TL_CHECK_EQ(restocked, 1);
    TL_CHECK_EQ(stocked.items_in_pane(InventoryPane::Backpack), 1);
    TL_CHECK_EQ(backpack_only.items_in_pane(InventoryPane::Backpack), 1);
    TL_CHECK_EQ(npc_bag.items_in_pane(InventoryPane::Backpack), 0);
    TL_CHECK_EQ(shop.inventory.items_in_pane(InventoryPane::Backpack), 6);
}

TL_TEST(merchant_data_and_treasure_read_with_their_defaults) {
    AdmSection unit;
    unit.name = "UNIT";
    tl::core::MerchantData data = tl::core::read_merchant_data(unit);
    TL_CHECK(data.player_level_factor == 0.0f);
    TL_CHECK(!data.allows_uniques);
    TL_CHECK(data.allows_magic);
    TL_CHECK_EQ(data.default_tab, 0);

    AdmSection treasure;
    treasure.name = "TREASURE";
    AdmRecord name;
    name.key = "SPAWNCLASS";
    name.type = AdmType::String;
    name.text = "MERCHANT_STOCK";
    treasure.records.push_back(name);
    unit.children.push_back(treasure);
    const auto stock = one_unit_class("Health Potion");
    const tl::core::MonsterTreasure read = tl::core::read_treasure(
        unit, [&](const std::string& n) { return n == "MERCHANT_STOCK" ? stock.get() : nullptr; });
    TL_CHECK(read.spawn_class == stock.get());
    TL_CHECK_EQ(read.min, 1);  // `MIN`/`MAX` default 1
    TL_CHECK_EQ(read.max, 1);
}

// ---- Buying and selling ----------------------

ItemStats priced(const std::string& name, const std::string& unittype,
                 const tl::core::StatBook& book, const UnitTypeHierarchy& types) {
    ItemStats item;
    item.name = name;
    item.unittype = unittype;
    item.level = 10;
    tl::core::recalculate_item_price(item, book, types, false);
    return item;
}

struct Counter {
    UnitTypeHierarchy types = hierarchy();
    tl::core::StatBook book = prices();
    Inventory merchant = tl::core::make_merchant_inventory();
    Inventory bag;
    int copies = 0;
    tl::core::TradeContext context() {
        tl::core::TradeContext ctx;
        ctx.unit_types = &types;
        ctx.graphs = &book;
        ctx.create_copy = [this](const ItemStats& data) {
            ++copies;
            ItemStats fresh = data;
            fresh.display_name = "fresh";
            return fresh;
        };
        return ctx;
    }
    Counter() { merchant.set_stacking(false); }
};

TL_TEST(buying_takes_the_price_and_the_stack_leaves_the_merchant) {
    Counter c;
    const int at =
        c.merchant.pickup(priced("Plain Sword", "NORMAL SWORD", c.book, c.types), 1, c.types);
    c.bag.set_gold(150);
    const tl::core::TradeOutcome out =
        tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(at), c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Bought);
    TL_CHECK_EQ(out.price, 100);  // PRICE_PLAYERBUY_NORMAL(10) x VALUE 100%
    TL_CHECK_EQ(c.bag.gold(), 50);
    TL_CHECK(!c.merchant.occupied(static_cast<size_t>(at)));
    TL_CHECK_EQ(c.bag.count_by_name("Plain Sword"), 1);
    TL_CHECK_EQ(out.gold_buy_sounds, 2);  // 0x17 before the gold and after the pickup
    TL_CHECK(!out.copied && !out.gambled && !out.dropped);
}

TL_TEST(buying_is_refused_short_of_gold_or_for_a_quest_item) {
    Counter c;
    const int sword =
        c.merchant.pickup(priced("Plain Sword", "NORMAL SWORD", c.book, c.types), 1, c.types);
    const int token = c.merchant.pickup(priced("Token", "QUESTITEM", c.book, c.types), 1, c.types);
    c.bag.set_gold(99);
    tl::core::TradeOutcome out = tl::core::buy_from_merchant(
        c.merchant, false, static_cast<size_t>(sword), c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Refused);
    TL_CHECK_EQ(out.gold_buy_sounds, 0);
    TL_CHECK_EQ(c.bag.gold(), 99);
    TL_CHECK(c.merchant.occupied(static_cast<size_t>(sword)));
    c.bag.set_gold(100000);
    out = tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(token), c.bag,
                                      c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Refused);
    TL_CHECK_EQ(c.bag.gold(), 100000);
    // Exactly the price is enough: the test is `gold < price`.
    c.bag.set_gold(100);
    out = tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(sword), c.bag,
                                      c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Bought);
    TL_CHECK_EQ(c.bag.gold(), 0);
}

TL_TEST(an_infinite_single_copy_stays_with_the_merchant) {
    Counter c;
    ItemStats potion = priced("Health Potion", "Potion", c.book, c.types);
    potion.max_stack = 20;
    potion.merchant_infinite = true;
    const int at = c.merchant.pickup(potion, 1, c.types);
    c.bag.set_gold(1000);
    tl::core::TradeOutcome out =
        tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(at), c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Bought);
    TL_CHECK(out.copied);
    TL_CHECK_EQ(c.copies, 1);
    TL_CHECK(c.merchant.occupied(static_cast<size_t>(at)));
    TL_CHECK_EQ(c.bag.gold(), 900);
    out =
        tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(at), c.bag, c.context());
    TL_CHECK_EQ(c.bag.count_by_name("Health Potion"), 2);  // the two copies stack in the bag
    // A stack of two is not a single copy: it leaves whole.
    const int pair = c.merchant.pickup(potion, 2, c.types);
    TL_CHECK(pair != at);  // the merchant's stacking is off
    TL_CHECK_EQ(c.merchant.slots()[static_cast<size_t>(pair)].count, 2);
    out = tl::core::buy_from_merchant(c.merchant, false, static_cast<size_t>(pair), c.bag,
                                      c.context());
    TL_CHECK(!out.copied);
    TL_CHECK_EQ(out.price, 200);
    TL_CHECK(!c.merchant.occupied(static_cast<size_t>(pair)));
    TL_CHECK_EQ(c.bag.count_by_name("Health Potion"), 4);
}

TL_TEST(a_full_bag_takes_only_what_stacks_onto_something_held) {
    Counter c;
    for (size_t i = 0; i < Inventory::kDefaultBackpackCapacity; ++i) {
        c.bag.pickup(priced("Junk " + std::to_string(i), "NORMAL SWORD", c.book, c.types), 1,
                     c.types);
    }
    const int sword =
        c.merchant.pickup(priced("Plain Sword", "NORMAL SWORD", c.book, c.types), 1, c.types);
    c.bag.set_gold(1000);
    const tl::core::TradeOutcome out = tl::core::buy_from_merchant(
        c.merchant, false, static_cast<size_t>(sword), c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::InventoryFull);
    TL_CHECK_EQ(out.gold_buy_sounds, 0);
    TL_CHECK_EQ(c.bag.gold(), 1000);

    ItemStats potion = priced("Health Potion", "Potion", c.book, c.types);
    potion.max_stack = 20;
    TL_CHECK(!tl::core::tray_can_take(c.bag, potion, c.types));
    c.bag.remove_item(static_cast<size_t>(c.bag.find_by_name("Junk 0")), 1);
    c.bag.pickup(potion, 1, c.types);
    TL_CHECK(tl::core::tray_can_take(c.bag, potion, c.types));
    ItemStats single = potion;
    single.max_stack = 1;
    TL_CHECK(!tl::core::tray_can_take(c.bag, single, c.types));
}

TL_TEST(a_gamble_is_identified_counted_and_priced_as_the_buyers) {
    Counter c;
    ItemStats sword = priced("Plain Sword", "NORMAL SWORD", c.book, c.types);
    tl::core::recalculate_item_price(sword, c.book, c.types, true);  // the gambler's
    TL_CHECK_EQ(sword.buy_price, 200);
    // The stock roll identifies what it keeps (`+0x348`); an unidentified copy
    // would trade at the normal price.
    const int at = c.merchant.pickup(sword, 1, c.types);
    c.bag.set_gold(1000);
    const tl::core::TradeOutcome out =
        tl::core::buy_from_merchant(c.merchant, true, static_cast<size_t>(at), c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Bought);
    TL_CHECK(out.gambled);
    TL_CHECK_EQ(out.price, 200);
    TL_CHECK_EQ(c.bag.gold(), 800);
    const ItemStats& mine = c.bag.slots()[static_cast<size_t>(out.slot)].item;
    TL_CHECK(mine.identified);
    TL_CHECK_EQ(mine.buy_price, 100);  // `addedToInventory` priced it for the player
}

TL_TEST(selling_pays_the_player_and_the_merchant_prices_it_as_its_own) {
    Counter c;
    const int at = c.bag.pickup(priced("Plain Sword", "NORMAL SWORD", c.book, c.types), 1, c.types);
    tl::core::TradeOutcome out = tl::core::sell_to_merchant(c.bag, static_cast<size_t>(at),
                                                            c.merchant, true, c.bag, c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Sold);
    TL_CHECK_EQ(out.price, 25);  // PRICE_PLAYERSELL_NORMAL(10)
    TL_CHECK_EQ(c.bag.gold(), 25);
    TL_CHECK_EQ(out.gold_buy_sounds, 1);
    TL_CHECK(!c.bag.occupied(static_cast<size_t>(at)));
    TL_CHECK_EQ(c.merchant.slots()[static_cast<size_t>(out.slot)].item.buy_price, 200);  // gamble

    // From the pet's bag the gold still goes to the player.
    Inventory pet_bag;
    const int fish = pet_bag.pickup(priced("Fish", "Potion", c.book, c.types), 1, c.types);
    out = tl::core::sell_to_merchant(pet_bag, static_cast<size_t>(fish), c.merchant, false, c.bag,
                                     c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Sold);
    TL_CHECK_EQ(c.bag.gold(), 50);
    TL_CHECK_EQ(pet_bag.gold(), 0);

    const int token = c.bag.pickup(priced("Token", "QUESTITEM", c.book, c.types), 1, c.types);
    out = tl::core::sell_to_merchant(c.bag, static_cast<size_t>(token), c.merchant, false, c.bag,
                                     c.context());
    TL_CHECK(out.result == tl::core::TradeResult::Refused);
    TL_CHECK(c.bag.occupied(static_cast<size_t>(token)));
    TL_CHECK_EQ(c.bag.gold(), 50);
}

TL_TEST(a_merchant_restocks_on_return_only_past_its_respawn_time) {
    TL_CHECK(!tl::core::merchant_restocks_on_restore(0.0f, 60));
    TL_CHECK(!tl::core::merchant_restocks_on_restore(60.0f, 60));
    TL_CHECK(tl::core::merchant_restocks_on_restore(60.5f, 60));
}

TL_TEST(merchant_infinite_is_read_with_its_default) {
    AdmSection section;
    section.name = "UNIT";
    ItemStats item;
    tl::core::read_item_roll_data(item, section);
    TL_CHECK(!item.merchant_infinite);
    AdmRecord key;
    key.key = "MERCHANTINFINITE";
    key.type = AdmType::Bool;
    key.integer = 1;
    section.records.push_back(key);
    tl::core::read_item_roll_data(item, section);
    TL_CHECK(item.merchant_infinite);
}

}  // namespace
