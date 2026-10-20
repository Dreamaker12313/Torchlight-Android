// The enchanter and the enchant shrine: the order of the volatile draws, the four outcomes,
// the price and the lines printed. Draw counts: an integer draw is one step, a float two.
#include "core/enchanting.h"
#include "core/affix_book.h"
#include "core/effect_manager.h"
#include "core/item_roll.h"
#include "core/random.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::AffixBook;
using tl::core::EnchantResult;
using tl::core::EnchantService;
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

UnitTypeHierarchy hierarchy() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("any", 0, {}));
    types.children.push_back(type("Weapon", 8, {0}));
    types.children.push_back(type("NORMAL", 200, {0}));
    types.children.push_back(type("NORMAL SWORD", 300, {8, 0xa2, 200, 0}));
    types.children.push_back(type("item", 2, {0}));
    types.children.push_back(type("MAGIC AXE", 301, {8, 2, 0}));
    types.children.push_back(type("SOCKETABLE", 120, {2, 0}));
    types.children.push_back(type("RANDOMMAGIC SOCKETABLE", 160, {120, 2, 0}));
    root.children.push_back(types);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

tl::core::Affix affix(const std::string& name, int weight, int min, int max) {
    tl::core::Affix a;
    a.name = name;
    a.weight = weight;
    a.min_spawn_range = min;
    a.max_spawn_range = max;
    a.slots_occupy = 1;
    a.unit_types.push_back(8);
    return a;
}

StatGraph line(float y_at_100) {
    StatGraph g(tl::core::GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(100.0f, y_at_100);
    g.process_points();
    return g;
}

uint64_t stepped(uint32_t seed, int steps) {
    RunicRandom r(seed);
    for (int i = 0; i < steps; ++i) r.step();
    return r.state();
}

struct Rig {
    UnitTypeHierarchy types = hierarchy();
    StatBook book;
    ItemRollGlobals globals;
    AffixBook affixes;
    RunicRandom vol{1};
    RunicRandom game{7};
    Rig() {
        book.add("PRICE_ENCHANT", line(1000.0f));  // 100 at level 10
        book.add("PRICE_PLAYERBUY_NORMAL", line(1000.0f));
        book.add("PRICE_PLAYERSELL_NORMAL", line(250.0f));
        book.add("PRICE_PLAYERBUY_MAGIC", line(3000.0f));
        book.add("PRICE_PLAYERSELL_MAGIC", line(750.0f));
        // Only from level 4: an enchant at the item's level 3 would find nothing.
        affixes.add(affix("Of_Fire", 10, 4, 50));
        globals.enchanter_price_per_enchant = 0.25f;
    }
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

ItemStats sword(int level) {
    ItemStats it;
    it.name = "NORMAL SWORD";
    it.unittype = "NORMAL SWORD";
    it.level = level;
    return it;
}

}  // namespace

TL_TEST(enchanting_disenchant_chance_grows_and_stops_at_the_maximum) {
    // TorchED's `ENCHANTER_*`: 2, +2 a time, at most 80.
    TL_CHECK_EQ(tl::core::disenchant_chance(0, 2.0f, 2.0f, 80.0f), 2.0f);
    TL_CHECK_EQ(tl::core::disenchant_chance(5, 2.0f, 2.0f, 80.0f), 12.0f);
    TL_CHECK_EQ(tl::core::disenchant_chance(100, 2.0f, 2.0f, 80.0f), 80.0f);
}

TL_TEST(enchanting_enchanter_refuses_without_the_gold_after_the_slot_draw) {
    Rig rig;
    ItemStats item = sword(10);
    item.identified = false;
    int gold = 299;  // the price is 300, the floor
    const tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Enchanter, item, gold, 1, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Refused);
    TL_CHECK(!out.served());
    TL_CHECK(!out.succeeded());
    TL_CHECK(out.message.empty());
    TL_CHECK_EQ(gold, 299);
    TL_CHECK(!item.identified);
    // `randomIntegerBetweenVolatile(0, 1)` is drawn before the price is asked.
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 1));
}

TL_TEST(enchanting_enchanter_strips_every_affix_and_effect_but_block_chance) {
    Rig rig;
    rig.globals.enchanter_disenchant_chance = 100.0f;  // 1000: every roll strips
    rig.globals.enchanter_max_disenchant_chance = 100.0f;
    ItemStats item = sword(10);
    item.affixes.push_back({affix("Of_Fire", 10, 4, 50), 10});
    tl::core::Effect block;
    block.type = tl::core::kEffectTypePercentBlockChanceBase;
    tl::core::Effect other;
    other.type = 5;
    item.effects = {other, block};
    item.enchant_count = 4;
    int gold = 1000;
    const tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Enchanter, item, gold, 1, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Disenchanted);
    TL_CHECK(out.served());
    TL_CHECK(!out.succeeded());
    // p = 100, (int)(0.25 * 100 * 4) + 100 = 200 -> the floor, 300.
    TL_CHECK_EQ(out.price, 300);
    TL_CHECK_EQ(gold, 700);
    TL_CHECK(item.affixes.empty());
    TL_CHECK_EQ(item.effects.size(), 1u);
    TL_CHECK_EQ(item.effects[0].type, tl::core::kEffectTypePercentBlockChanceBase);
    TL_CHECK_EQ(item.enchant_count, 0u);
    TL_CHECK(item.identified);
    TL_CHECK_EQ(out.message, std::string("The enchanter performs the ancient rites on the Sword"
                                         " ... and accidentally removes all enchantments!"));
    // The slot draw and the disenchant roll only.
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 3));
    // Stripped, the manager stays: the next strip is still possible.
    TL_CHECK(tl::core::item_has_effect_manager(item));
}

TL_TEST(enchanting_an_item_without_a_manager_is_never_stripped_and_enchants_at_level_4) {
    Rig rig;
    rig.globals.enchanter_disenchant_chance = 100.0f;
    rig.globals.enchanter_max_disenchant_chance = 100.0f;
    rig.globals.enchanter_magic_chance = 100.0f;  // every roll enchants
    ItemStats item = sword(3);
    item.sockets = 2;  // at `MAX_SOCKETS`: no socket roll
    int gold = 1000;
    const tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Enchanter, item, gold, 1, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Enchanted);
    TL_CHECK(out.succeeded());
    TL_CHECK_EQ(item.enchant_count, 1u);
    // Level 3 is raised to 4, where `Of_Fire` first rolls.
    TL_CHECK(!item.affixes.empty());
    for (const ItemStats::RolledAffix& rolled : item.affixes) TL_CHECK_EQ(rolled.level, 4u);
    TL_CHECK_EQ(out.message, std::string("The enchanter performs the ancient rites on the Sword"
                                         " ... and adds new enchantments!"));
}

TL_TEST(enchanting_sockets_come_before_the_magic_roll) {
    Rig rig;
    rig.globals.enchanter_socket_chance = 100.0f;
    rig.globals.enchanter_magic_chance = 100.0f;
    rig.globals.second_socket_chance = 0.0f;
    ItemStats item = sword(10);
    int gold = 1000;
    const tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Enchanter, item, gold, 1, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Sockets);
    TL_CHECK_EQ(item.sockets, 1);
    TL_CHECK(item.affixes.empty());
    TL_CHECK_EQ(item.enchant_count, 1u);
    // Slot 1, disenchant 2, socket 2, `addSockets`' second-socket roll 2.
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 7));
    TL_CHECK_EQ(out.message, std::string("The enchanter performs the ancient rites on the Sword"
                                         " ... and creates new sockets!"));
}

TL_TEST(enchanting_nothing_happens_keeps_the_count_and_takes_the_gold) {
    Rig rig;  // every chance 0
    ItemStats item = sword(10);
    item.enchant_count = 2;
    item.identified = false;
    int gold = 1000;
    const tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Enchanter, item, gold, 1, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::NothingHappens);
    TL_CHECK(out.succeeded());
    TL_CHECK_EQ(item.enchant_count, 2u);
    TL_CHECK_EQ(gold, 700);
    TL_CHECK(item.identified);
    // Slot 1, disenchant 2, socket 2, magic 2.
    TL_CHECK_EQ(rig.vol.state(), stepped(1, 7));
    TL_CHECK_EQ(out.message, std::string("The enchanter performs the ancient rites on the Sword"
                                         " ... but nothing happens!"));
}

TL_TEST(enchanting_the_shrine_is_free_reads_the_player_level_and_has_its_own_words) {
    Rig rig;
    rig.globals.shrine_magic_chance = 100.0f;
    ItemStats item = sword(3);
    item.sockets = 2;
    int gold = 5;
    tl::core::EnchantOutcome out =
        tl::core::perform_enchant(EnchantService::Shrine, item, gold, 12, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Enchanted);
    TL_CHECK_EQ(out.price, 0);
    TL_CHECK_EQ(gold, 5);
    for (const ItemStats::RolledAffix& rolled : item.affixes) TL_CHECK_EQ(rolled.level, 12u);
    TL_CHECK_EQ(out.message,
                std::string("You plunge the Sword into the light ... and adds new enchantments!"));

    rig.globals.shrine_disenchant_chance = 100.0f;
    rig.globals.shrine_max_disenchant_chance = 100.0f;
    out = tl::core::perform_enchant(EnchantService::Shrine, item, gold, 12, "Sword", rig.context());
    TL_CHECK(out.result == EnchantResult::Disenchanted);
    // The strip's line has no "into the light".
    TL_CHECK_EQ(out.message,
                std::string("You plunge the Sword ... and accidentally removes all enchantments!"));
}

namespace {

ItemStats gem(const std::string& name, const std::string& unittype) {
    ItemStats it;
    it.name = name;
    it.unittype = unittype;
    it.level = 10;
    it.authored_affixes.push_back("Of_Fire");
    return it;
}

}  // namespace

TL_TEST(enchanting_gems_come_out_front_first_with_the_last_moved_forward) {
    // a, b, c come out a, c, b.
    Rig rig;
    ItemStats host = sword(10);
    host.sockets = 3;
    host.socketed = {gem("A", "SOCKETABLE"), gem("B", "SOCKETABLE"), gem("C", "SOCKETABLE")};
    const std::vector<ItemStats> out = tl::core::take_out_socketed_gems(host, rig.context());
    TL_CHECK(host.socketed.empty());
    TL_CHECK_EQ(out.size(), size_t{3});
    TL_CHECK_EQ(out[0].name, std::string("A"));
    TL_CHECK_EQ(out[1].name, std::string("C"));
    TL_CHECK_EQ(out[2].name, std::string("B"));
    TL_CHECK_EQ(host.sockets, 3);
}

TL_TEST(enchanting_a_recovered_gem_gets_back_its_authored_affixes_unless_random_magic) {
    Rig rig;
    ItemStats host = sword(10);
    host.sockets = 2;
    // What `removeAffixesThatDontSupportUnitType` took when they went in.
    host.socketed = {gem("Ruby", "SOCKETABLE"), gem("Odd", "RANDOMMAGIC SOCKETABLE")};
    const std::vector<ItemStats> out = tl::core::take_out_socketed_gems(host, rig.context());
    TL_CHECK_EQ(out.size(), size_t{2});
    TL_CHECK_EQ(out[0].affixes.size(), size_t{1});
    TL_CHECK_EQ(out[0].affixes[0].affix.name, std::string("Of_Fire"));
    TL_CHECK_EQ(out[0].affixes[0].level, 10u);
    // `ISA 0xa0` skips the reapply.
    TL_CHECK(out[1].affixes.empty());

    // `reapplyAffixes(true)` passes over a name the gem still holds.
    ItemStats host2 = sword(10);
    host2.sockets = 1;
    host2.socketed = {out[0]};
    const std::vector<ItemStats> again = tl::core::take_out_socketed_gems(host2, rig.context());
    TL_CHECK_EQ(again[0].affixes.size(), size_t{1});
}

TL_TEST(enchanting_destroying_gems_keeps_the_item_and_says_so) {
    ItemStats host = sword(10);
    host.sockets = 2;
    host.socketed = {gem("Ruby", "SOCKETABLE")};
    TL_CHECK(tl::core::item_has_gems(host));
    const std::string line = tl::core::destroy_socketed_gems(host, "Sword");
    TL_CHECK(!tl::core::item_has_gems(host));
    TL_CHECK_EQ(host.sockets, 2);
    TL_CHECK_EQ(line, std::string("The gems within the Sword are destroyed!"));
    TL_CHECK_EQ(tl::core::gems_recovered_line(),
                std::string("The item is destroyed, recovering the gems within!"));
}

TL_TEST(enchanting_a_gem_goes_only_into_an_identified_item_with_a_free_socket) {
    Rig rig;
    ItemStats axe;
    axe.name = "Axe";
    axe.unittype = "MAGIC AXE";
    TL_CHECK(!tl::core::item_takes_gem(axe, rig.types));  // no sockets
    axe.sockets = 1;
    TL_CHECK(tl::core::item_takes_gem(axe, rig.types));
    axe.identified = false;
    TL_CHECK(!tl::core::item_takes_gem(axe, rig.types));
    axe.identified = true;
    axe.socketed.push_back(gem("Ruby", "SOCKETABLE"));
    TL_CHECK(!tl::core::item_takes_gem(axe, rig.types));  // full
    // `ISA(item, 2)`: a type outside `item` never lists.
    ItemStats sword_item = sword(10);
    sword_item.sockets = 2;
    TL_CHECK(!tl::core::item_takes_gem(sword_item, rig.types));
}
