// What an item's details window says (`CGameUI::showEquipmentTooltip` and the
// `CEquipment` text builders).
#include "core/equipment_tooltip.h"

#include "core/stats.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::EffectTypeTable;
using tl::core::EquipmentTooltip;
using tl::core::EquipmentTooltipContext;
using tl::core::ItemStats;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

tl::core::UnitTypeHierarchy shipped_unit_types() {
    auto type = [](const std::string& name, const std::vector<uint32_t>& ids) {
        AdmSection group;
        group.name = name;
        for (size_t i = 0; i < ids.size(); ++i) {
            AdmRecord r;
            r.key = i == 0 ? "ID" : "CHILD";
            r.type = AdmType::Integer;
            r.integer = ids[i];
            group.records.push_back(r);
        }
        return group;
    };
    AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("ARMOR", {13, 31, 2, 0}));
    types.children.push_back(type("BOOTS", {16, 13, 31, 2, 0}));
    types.children.push_back(type("MAGIC", {55}));
    types.children.push_back(type("MAGIC SWORD", {300, 11, 9, 8, 31, 2, 0, 55}));
    types.children.push_back(type("POLEARM", {105, 10, 8, 31, 2, 0}));
    types.children.push_back(type("POTION", {33, 1, 31, 2, 0}));
    types.children.push_back(type("QUESTITEM", {103, 2, 0}));
    types.children.push_back(type("RING", {17, 39, 31, 2, 0}));
    types.children.push_back(type("SOCKETABLE", {120, 31, 2, 0}));
    types.children.push_back(type("SWORD", {11, 9, 8, 31, 2, 0}));
    types.children.push_back(type("UNIQUE", {54, 55}));
    types.children.push_back(type("UNIQUE SWORD", {301, 11, 9, 8, 31, 2, 0, 54, 55}));
    AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

/// 0x91 rows, each printing "+[VALUE] row N".
EffectTypeTable effect_rows() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    for (int index = 0; index < 0x91; ++index) {
        AdmSection row;
        row.name = "EFFECT";
        row.records.push_back(str("NAME", "ROW " + std::to_string(index)));
        row.records.push_back(str("TYPE", "Value"));
        row.records.push_back(str("GOODDES", "+[VALUE] row " + std::to_string(index)));
        file.root.children.push_back(row);
    }
    EffectTypeTable table;
    table.load(file);
    return table;
}

Effect passive(int type, float value) {
    Effect e;
    e.name = "E" + std::to_string(type);
    e.type = type;
    e.values[0] = value;
    e.values[1] = value;
    e.duration = tl::core::kDurationAlways;
    e.activation = EffectActivation::Passive;
    return e;
}

ItemStats item(const std::string& name, const std::string& unittype) {
    ItemStats out;
    out.name = name;
    out.display_name = name;
    out.unittype = unittype;
    return out;
}

struct Fixture {
    tl::core::UnitTypeHierarchy types = shipped_unit_types();
    EffectTypeTable table = effect_rows();
    tl::core::StatBook graphs;
    EffectManager::Context effects;
    tl::core::EffectDisplayContext display;
    tl::core::ItemLabelColours colours;
    EquipmentTooltipContext context;

    Fixture() {
        effects.types = &table;
        effects.graphs = &graphs;
        effects.owner.present = false;
        display.types = &table;
        display.graphs = &graphs;
        colours.rare = {1.0f, 0.1f, 0.2f, 0.3f};
        colours.unique = {1.0f, 0.7f, 0.8f, 0.9f};
        colours.quest = {1.0f, 0.45f, 0.55f, 0.65f};
        context.unit_types = &types;
        context.effects = &effects;
        context.display = &display;
        context.colours = &colours;
        context.level = 5;
        context.strength = 10;
        context.dexterity = 10;
        context.magic = 10;
        context.defense = 10;
    }
};

bool same(const tl::core::LabelColour& a, const tl::core::LabelColour& b) {
    return a.a == b.a && a.r == b.r && a.g == b.g && a.b == b.b;
}

TL_TEST(equipment_type_names_the_kind_behind_its_quality) {
    const tl::core::UnitTypeHierarchy types = shipped_unit_types();
    TL_CHECK(tl::core::equipment_type(item("A", "SWORD"), true, types) == "Sword");
    TL_CHECK(tl::core::equipment_type(item("A", "MAGIC SWORD"), true, types) == "Rare Sword");
    TL_CHECK(tl::core::equipment_type(item("A", "UNIQUE SWORD"), true, types) == "Unique Sword");
    // Without the quality (a gambler's wares) the kind stands alone.
    TL_CHECK(tl::core::equipment_type(item("A", "UNIQUE SWORD"), false, types) == "Sword");
    ItemStats enchanted = item("A", "SWORD");
    enchanted.effects.push_back(passive(0x45, 3.0f));
    TL_CHECK(tl::core::equipment_type(enchanted, true, types) == "Enchanted Sword");
    enchanted.identified = false;
    TL_CHECK(tl::core::equipment_type(enchanted, true, types) == "Unidentified Sword");
    TL_CHECK(tl::core::equipment_type(item("A", "POLEARM"), true, types) == "Polearm");
    TL_CHECK(tl::core::equipment_type(item("A", "BOOTS"), true, types) == "Boots");
    TL_CHECK(tl::core::equipment_type(item("A", "RING"), true, types) == "Trinket");
    TL_CHECK(tl::core::equipment_type(item("A", "SOCKETABLE"), true, types) == "Socketable");
    TL_CHECK(tl::core::equipment_type(item("A", "POTION"), true, types) == "Potion");
    TL_CHECK(tl::core::equipment_type(item("A", "QUESTITEM"), true, types) == "(Quest Item)");
}

TL_TEST(attack_speed_string_reads_the_five_thresholds) {
    TL_CHECK(tl::core::attack_speed_string(130.0f) == "Slowest Attack Speed");
    TL_CHECK(tl::core::attack_speed_string(129.0f) == "Slow Attack Speed");
    TL_CHECK(tl::core::attack_speed_string(100.0f) == "Average Attack Speed");
    TL_CHECK(tl::core::attack_speed_string(80.0f) == "Fast Attack Speed");
    TL_CHECK(tl::core::attack_speed_string(79.0f) == "Fastest Attack Speed");
}

TL_TEST(equipment_stats_lists_damage_speed_and_sockets) {
    const tl::core::UnitTypeHierarchy types = shipped_unit_types();
    ItemStats sword = item("Sword", "SWORD");
    sword.damage = 21;
    sword.speed = 100.0f;
    sword.sockets = 2;
    // Half of 21, rounded up, to 21.
    TL_CHECK(tl::core::equipment_stats(sword, types) ==
             "Physical Damage: 11-21\nAverage Attack Speed\nSockets 0/2");
    // An inherent elemental damage of an identified copy gets its own line.
    sword.damage_types = {tl::core::kDamageFire};
    sword.inherent_damage[tl::core::kDamageFire] = 9;
    TL_CHECK(tl::core::equipment_stats(sword, types) ==
             "Physical Damage: 11-21\nFire Damage: 5-9\nAverage Attack Speed\nSockets 0/2");
    sword.identified = false;
    TL_CHECK(tl::core::equipment_stats(sword, types) ==
             "Physical Damage: 11-21\nAverage Attack Speed\nSockets 0/2");
    // Armour has neither damage nor speed.
    TL_CHECK(tl::core::equipment_stats(item("Boots", "BOOTS"), types).empty());
}

TL_TEST(item_visual_description_merges_like_effects) {
    Fixture f;
    ItemStats ring = item("Ring", "RING");
    ring.effects.push_back(passive(0x45, 3.0f));
    ring.effects.push_back(passive(0x45, 4.0f));
    ring.effects.push_back(passive(0x08, 2.0f));
    // The two of type 0x45 become one line of 7; the other keeps its own.
    TL_CHECK(tl::core::item_visual_description(ring, EffectActivation::Passive, false, f.effects,
                                               f.display, f.types) == "+7 row 69\n+2 row 8");
    // and the whole closes on a line break.
    TL_CHECK(tl::core::equipment_effects(ring, f.effects, f.display, f.types) ==
             "+7 row 69\n+2 row 8\n");
    // An unidentified copy says nothing of its effects.
    ring.identified = false;
    TL_CHECK(tl::core::equipment_effects(ring, f.effects, f.display, f.types).empty());
}

TL_TEST(equipment_tooltip_fills_the_windows_of_a_weapon) {
    Fixture f;
    ItemStats sword = item("Keen Blade", "MAGIC SWORD");
    sword.damage = 22;
    sword.speed = 100.0f;
    sword.min_level_required = 9;
    sword.strength_required = 4;
    sword.description = "Sharp.\\nVery.";
    sword.sell_price = 40;
    f.context.full_name = "Keen Blade";
    const EquipmentTooltip tip = tl::core::equipment_tooltip(sword, f.context);
    TL_CHECK(tip.name.text == "Keen Blade");
    TL_CHECK(same(tip.name.colour, f.colours.rare));
    TL_CHECK(tip.type.text == "Rare Sword");
    TL_CHECK(tip.big.shown && tip.big.text == "30");
    TL_CHECK(tip.big_label.text == "damage per second");
    TL_CHECK(tip.stats.text == "(1-handed)\nPhysical Damage: 11-22\nAverage Attack Speed");
    TL_CHECK(!tip.effects.shown);
    // Level 9 against the player's 5 is red; strength 4 against 10 is white.
    TL_CHECK(tip.requirements[0].shown && tip.requirements[0].text == "Requires Level 9");
    TL_CHECK(same(tip.requirements[0].colour, {1.0f, 1.0f, 0.0f, 0.0f}));
    TL_CHECK(tip.requirements[1].text == "requires Strength 4");
    TL_CHECK(same(tip.requirements[1].colour, {1.0f, 1.0f, 1.0f, 1.0f}));
    TL_CHECK(!tip.requirements[2].shown);
    TL_CHECK(tip.description.text == "Sharp.\nVery.");
    // The player's own item shows a price only at a merchant's.
    TL_CHECK(!tip.price.shown);
    f.context.merchant_open = true;
    TL_CHECK(tl::core::equipment_tooltip(sword, f.context).price.text == "sell price :40");
}

TL_TEST(equipment_tooltip_hides_an_unidentified_copy) {
    Fixture f;
    ItemStats sword = item("Keen Blade", "MAGIC SWORD");
    sword.damage = 22;
    sword.speed = 100.0f;
    sword.identified = false;
    sword.min_level_required = 9;
    f.context.full_name = "Mysterious Blade";
    const EquipmentTooltip tip = tl::core::equipment_tooltip(sword, f.context);
    TL_CHECK(tip.type.text == "Unidentified Sword");
    TL_CHECK(tip.big.text == "??");
    TL_CHECK(tip.stats.text == "(1-handed)\n");
    TL_CHECK(!tip.effects.shown);
    TL_CHECK(!tip.requirements[0].shown);
    TL_CHECK(tip.description.text ==
             "Unidentified items cannot be used-use an Identify Scroll to reveal enchantments");
}

TL_TEST(equipment_tooltip_a_comparison_says_where_it_is_worn) {
    Fixture f;
    ItemStats sword = item("Old Blade", "SWORD");
    sword.damage = 10;
    sword.speed = 100.0f;
    f.context.full_name = "Old Blade";
    f.context.comparison = true;
    f.context.worn_location = 0;
    f.context.merchant_open = true;
    EquipmentTooltip tip = tl::core::equipment_tooltip(sword, f.context);
    TL_CHECK(tip.type.text == "(equipped-right hand) Sword");
    TL_CHECK(!tip.price.shown);
    f.context.worn_location = 1;
    TL_CHECK(tl::core::equipment_tooltip(sword, f.context).type.text ==
             "(equipped-left hand) Sword");
    // Armour has no hand to name.
    ItemStats boots = item("Boots", "BOOTS");
    boots.graph_ac = 6;
    tip = tl::core::equipment_tooltip(boots, f.context);
    TL_CHECK(tip.type.text == "(equipped) Boots");
    TL_CHECK(tip.big.text == "6" && tip.big_label.text == "armor");
}

TL_TEST(equipment_stats_prints_the_set_and_its_bonuses_in_two_colours) {
    Fixture f;
    f.colours.set = {1.0f, 0.0f, 1.0f, 0.0f};             // FF00FF00
    f.colours.set_unselected = {1.0f, 0.5f, 0.5f, 0.5f};  // FF808080
    tl::core::ItemSetBook sets;
    tl::core::ItemSet set;
    set.name = "CRAFT";
    set.display_name = "Bolem's Craft";
    tl::core::ItemSetBonus two;
    two.affix.effects.push_back(passive(0x45, 5.0f));
    two.count = 2;
    tl::core::ItemSetBonus three;
    three.affix.effects.push_back(passive(0x08, 9.0f));
    three.count = 3;
    set.bonuses = {two, three};
    sets.add(set);

    ItemStats boots = item("Boots", "BOOTS");
    boots.set = "CRAFT";
    ItemStats ring = item("Ring", "RING");
    ring.set = "CRAFT";
    tl::core::Inventory worn;
    TL_CHECK(worn.pickup_into_slot(boots, 1, tl::core::equip_location::kBoots));
    TL_CHECK(worn.pickup_into_slot(ring, 1, tl::core::equip_location::kRing1));

    tl::core::EquipmentSetContext context;
    context.sets = &sets;
    context.owner = &worn;
    context.display = &f.display;
    context.colours = &f.colours;
    // Two pieces worn: the two-piece bonus in the set colour, the three-piece
    // one in the unselected colour.
    TL_CHECK(tl::core::equipment_stats(boots, f.types, context) ==
             "\n\n|cFF00FF00Set: Bolem's Craft|u"
             "\n  |cFF00FF002 pc: +5 row 69|u"
             "\n  |cFF8080803 pc: +9 row 8|u");
    // In no inventory nothing is earned.
    context.owner = nullptr;
    TL_CHECK(tl::core::equipment_stats(boots, f.types, context) ==
             "\n\n|cFF00FF00Set: Bolem's Craft|u"
             "\n  |cFF8080802 pc: +5 row 69|u"
             "\n  |cFF8080803 pc: +9 row 8|u");
    // A set the book does not know prints nothing.
    boots.set = "OTHER";
    TL_CHECK(tl::core::equipment_stats(boots, f.types, context).empty());
}

TL_TEST(equipment_effects_prints_a_socketed_gems_lines_in_the_socket_colour) {
    Fixture f;
    ItemStats sword = item("Sword", "SWORD");
    sword.sockets = 1;
    ItemStats gem = item("Gem", "SOCKETABLE");
    gem.effects.push_back(passive(0x45, 3.0f));
    sword.socketed.push_back(gem);
    TL_CHECK(tl::core::equipment_effects(sword, f.effects, f.display, f.types,
                                         {1.0f, 0.0f, 0.0f, 1.0f}) == "|cFF0000FF+3 row 69|u\n");
}

TL_TEST(place_equipment_tooltips_follows_the_console_by_which_tray) {
    // 1280 x 720 at ratio 0.9375: the safe area is 1152 x 648 from (64, 36),
    // a tray 281 wide, the gaps 28.125 and 24.375.
    const auto box = [](float w, float h) {
        tl::core::TooltipBox b;
        b.shown = true;
        b.width = w;
        b.height = h;
        return b;
    };
    // The middle tray (anchor 520): the item's window right of the tray, the
    // comparisons flush against its left, one under the other.
    tl::core::TooltipBox item = box(250.0f, 200.0f), first = box(200.0f, 150.0f),
                         second = box(200.0f, 150.0f);
    tl::core::place_equipment_tooltips(item, first, second, 520, 300, 1280, 720, 0.9375f);
    TL_CHECK(item.x == 281.0f + 24.375f + 520.0f);
    TL_CHECK(item.y == 300.0f);
    TL_CHECK(first.x == 320.0f && second.x == 320.0f);
    TL_CHECK(first.y == 300.0f);
    TL_CHECK(second.y == 150.0f + 24.375f + 300.0f);

    // The right tray (anchor 800): the item's window left of it, the
    // comparison left of that.
    item = box(250.0f, 200.0f);
    first = box(200.0f, 150.0f);
    second = tl::core::TooltipBox{};
    tl::core::place_equipment_tooltips(item, first, second, 800, 300, 1280, 720, 0.9375f);
    TL_CHECK(item.x == (800.0f - (250.0f + 28.125f)) + 24.375f);
    TL_CHECK(first.x == item.x - ((200.0f + 24.375f) + 28.125f));

    // The left tray (anchor 230): everything to its right.
    item = box(250.0f, 200.0f);
    first = box(200.0f, 150.0f);
    tl::core::place_equipment_tooltips(item, first, second, 230, 300, 1280, 720, 0.9375f);
    TL_CHECK(item.x == 281.0f + 24.375f + 230.0f);
    TL_CHECK(first.x == (24.375f + item.x) + (250.0f + 28.125f));

    // A window that would leave the bottom of the safe area is pulled up.
    item = box(250.0f, 400.0f);
    first = tl::core::TooltipBox{};
    tl::core::place_equipment_tooltips(item, first, second, 520, 500, 1280, 720, 0.9375f);
    TL_CHECK(item.y == 684.0f - 400.0f);
}

TL_TEST(equipment_tooltip_a_merchants_price_reddens_when_gold_is_short) {
    Fixture f;
    ItemStats potion = item("Potion", "POTION");
    potion.buy_price = 30;
    f.context.full_name = "Potion";
    f.context.owner_is_player = false;
    f.context.owner_is_merchant = true;
    f.context.merchant_open = true;
    f.context.gold = 10;
    EquipmentTooltip tip = tl::core::equipment_tooltip(potion, f.context);
    TL_CHECK(tip.price.text == "price :30");
    TL_CHECK(same(tip.price.colour, {1.0f, 1.0f, 0.5f, 0.5f}));
    f.context.gold = 30;
    tip = tl::core::equipment_tooltip(potion, f.context);
    TL_CHECK(tip.price.colour.g != 0.5f);
    // A gambler's wares give nothing away.
    f.context.owner_is_gambler = true;
    tip = tl::core::equipment_tooltip(potion, f.context);
    TL_CHECK(tip.name.text == "???");
    TL_CHECK(tip.effects.text == "????\n");
}

}  // namespace

// `CEquipment::skillDescription`: a break before every skill past the
// manager's first, even when the first was not shown.
TL_TEST(skill_description_lists_skills_to_give_then_the_items_skills) {
    ItemStats staff;
    staff.skills_to_give.push_back({"Fire Ball", 3});
    staff.skills_to_give.push_back({"Frost", 1});
    TL_CHECK(tl::core::equipment_skill_description(staff, {}) ==
             "Fire Ball Level: 3\nFrost Level: 1");

    ItemStats wand;
    ItemStats::Skill hidden;
    hidden.name = "HIDDEN ONE";
    ItemStats::Skill shown;
    shown.name = "POLYMORPH MISSILE";
    wand.skills = {hidden, shown};
    const tl::core::ItemSkillDescriber describe = [](const ItemStats::Skill& skill,
                                                     std::string& text) {
        if (skill.name == "HIDDEN ONE") return false;
        text = "Turns a foe";
        return true;
    };
    TL_CHECK(tl::core::equipment_skill_description(wand, describe) == "\nTurns a foe");
}
