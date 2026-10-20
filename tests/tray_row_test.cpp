// What the console's row fill decides for an item: the square behind the
// icon, the name's colour, the lines under the name and the comparison arrow.
#include "core/tray_row.h"

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
using tl::core::ItemStats;
using tl::core::TrayArrow;
using tl::core::TrayBackdrop;
using tl::core::TrayRow;
using tl::core::TrayRowContext;
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
    types.children.push_back(type("CONSUMABLE", {1, 31, 2, 0}));
    types.children.push_back(type("MAGIC", {55}));
    types.children.push_back(type("MAGIC POTION", {56, 33, 1, 31, 2, 0, 55}));
    types.children.push_back(type("MAGIC SWORD", {57, 8, 31, 2, 0, 55}));
    types.children.push_back(type("POTION", {33, 1, 31, 2, 0}));
    types.children.push_back(type("QUESTITEM", {103, 2, 0}));
    types.children.push_back(type("SHIELD", {21, 13, 31, 2, 0}));
    types.children.push_back(type("SWORD", {40, 8, 31, 2, 0}));
    types.children.push_back(type("UNIQUE", {54, 55}));
    types.children.push_back(type("UNIQUE POTION", {58, 33, 1, 31, 2, 0, 54, 55}));
    types.children.push_back(type("WEAPON", {8, 31, 2, 0}));
    AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

EffectTypeTable effect_rows() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    for (int index = 0; index < 0x91; ++index) {
        AdmSection row;
        row.name = "EFFECT";
        AdmRecord name;
        name.key = "NAME";
        name.type = AdmType::String;
        name.text = "ROW " + std::to_string(index);
        row.records.push_back(name);
        AdmRecord kind;
        kind.key = "TYPE";
        kind.type = AdmType::String;
        kind.text = "Value";
        row.records.push_back(kind);
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
    out.unittype = unittype;
    return out;
}

struct Fixture {
    tl::core::UnitTypeHierarchy types = shipped_unit_types();
    EffectTypeTable table = effect_rows();
    tl::core::StatBook graphs;
    EffectManager::Context effects;
    tl::core::ItemLabelColours colours;
    TrayRowContext context;

    Fixture() {
        effects.types = &table;
        effects.graphs = &graphs;
        effects.owner.present = false;
        colours.rare_unselected = {1.0f, 0.1f, 0.2f, 0.3f};
        colours.random_enchant_unselected = {1.0f, 0.4f, 0.5f, 0.6f};
        colours.unique_unselected = {1.0f, 0.7f, 0.8f, 0.9f};
        colours.set_unselected = {1.0f, 0.15f, 0.25f, 0.35f};
        colours.quest_unselected = {1.0f, 0.45f, 0.55f, 0.65f};
        colours.quest = {1.0f, 0.95f, 0.85f, 0.75f};
        context.unit_types = &types;
        context.effects = &effects;
        context.colours = &colours;
        context.level = 10;
        context.strength = 10;
        context.dexterity = 10;
        context.magic = 10;
        context.defense = 10;
    }
};

bool same(const tl::core::LabelColour& a, const tl::core::LabelColour& b) {
    return a.a == b.a && a.r == b.r && a.g == b.g && a.b == b.b;
}

TL_TEST(tray_row_an_unidentified_item_is_red_and_says_nothing) {
    Fixture f;
    ItemStats boots = item("Boots", "BOOTS");
    boots.identified = false;
    boots.graph_ac = 7;
    const TrayRow row = tl::core::tray_row(boots, 1, f.context);
    TL_CHECK(row.backdrop == TrayBackdrop::Red);
    TL_CHECK(row.detail == "???");
    TL_CHECK(row.enchantments == "???");
    TL_CHECK(row.arrow == TrayArrow::None);
    TL_CHECK(same(row.name_colour, {1.0f, 1.0f, 0.0f, 0.0f}));
    TL_CHECK(row.requirement == "Press A to identify");
    // With the merchant's screen open the foot line goes on to the
    // requirements instead, and a plain pair of boots has none.
    f.context.merchant_open = true;
    TL_CHECK(tl::core::tray_row(boots, 1, f.context).requirement.empty());
}

TL_TEST(tray_row_a_gamblers_row_hides_everything_but_the_square_stays_blank) {
    Fixture f;
    f.context.owner_is_gambler = true;
    ItemStats sword = item("Sword", "MAGIC SWORD");
    const TrayRow row = tl::core::tray_row(sword, 1, f.context);
    TL_CHECK(row.backdrop == TrayBackdrop::Blank);
    TL_CHECK(row.gambler_name);
    TL_CHECK(row.detail == "???");
    TL_CHECK(row.enchantments == "???");
    TL_CHECK(same(row.name_colour, {1.0f, 0.8f, 0.8f, 0.8f}));
    TL_CHECK(row.requirement.empty());
}

TL_TEST(tray_row_gear_the_player_cannot_wear_is_red) {
    Fixture f;
    f.context.player_can_equip = [](const ItemStats&) { return false; };
    ItemStats sword = item("Sword", "SWORD");
    sword.speed = 100.0f;
    sword.strength_required = 40;
    const TrayRow row = tl::core::tray_row(sword, 1, f.context);
    TL_CHECK(row.backdrop == TrayBackdrop::Red);
    TL_CHECK(row.requirement == "Requirements not met");
    TL_CHECK(same(row.requirement_colour, {1.0f, 1.0f, 0.0f, 0.0f}));
    // A potion is not asked: only weapons, armour and trinkets are.
    TL_CHECK(tl::core::tray_row(item("Potion", "POTION"), 1, f.context).backdrop ==
             TrayBackdrop::Blank);
}

TL_TEST(tray_row_the_quality_square_is_for_consumables_with_effects) {
    Fixture f;
    // A magic sword with an effect prints the count and keeps a blank square:
    // the count the square is chosen by is cleared once it has been printed.
    ItemStats sword = item("Sword", "MAGIC SWORD");
    sword.speed = 100.0f;
    sword.effects.push_back(passive(0x45, 3.0f));
    const TrayRow gear = tl::core::tray_row(sword, 1, f.context);
    TL_CHECK(gear.enchantments == "Effects:1");
    TL_CHECK(gear.backdrop == TrayBackdrop::Blank);
    TL_CHECK(same(gear.name_colour, f.colours.rare_unselected));

    // A consumable prints no count and takes the square of its quality.
    ItemStats magic_potion = item("Tonic", "MAGIC POTION");
    magic_potion.effects.push_back(passive(0x45, 3.0f));
    const TrayRow potion = tl::core::tray_row(magic_potion, 3, f.context);
    TL_CHECK(potion.enchantments.empty());
    TL_CHECK(potion.backdrop == TrayBackdrop::Blue);
    TL_CHECK(potion.detail == "x3");

    ItemStats unique_potion = item("Elixir", "UNIQUE POTION");
    unique_potion.effects.push_back(passive(0x45, 3.0f));
    TL_CHECK(tl::core::tray_row(unique_potion, 1, f.context).backdrop == TrayBackdrop::Gold);
    unique_potion.set = "SOMESET";
    TL_CHECK(tl::core::tray_row(unique_potion, 1, f.context).backdrop == TrayBackdrop::Purple);

    // Magical without being `MAGIC`: green.
    ItemStats plain_potion = item("Draught", "POTION");
    plain_potion.effects.push_back(passive(0x45, 3.0f));
    TL_CHECK(tl::core::tray_row(plain_potion, 1, f.context).backdrop == TrayBackdrop::Green);
    // With no effects at all the square is blank and a single one has no line.
    const TrayRow empty = tl::core::tray_row(item("Water", "POTION"), 1, f.context);
    TL_CHECK(empty.backdrop == TrayBackdrop::Blank);
    TL_CHECK(empty.detail.empty());
}

TL_TEST(tray_row_name_colours_follow_the_label_order) {
    Fixture f;
    ItemStats quest = item("Ember", "QUESTITEM");
    const TrayRow row = tl::core::tray_row(quest, 1, f.context);
    TL_CHECK(same(row.name_colour, f.colours.quest_unselected));
    TL_CHECK(row.requirement == "(Quest Item)");
    TL_CHECK(same(row.requirement_colour, f.colours.quest));

    ItemStats unique = item("Blade", "UNIQUE");
    TL_CHECK(
        same(tl::core::tray_row(unique, 1, f.context).name_colour, f.colours.unique_unselected));
    unique.set = "SOMESET";
    TL_CHECK(same(tl::core::tray_row(unique, 1, f.context).name_colour, f.colours.set_unselected));
    TL_CHECK(same(tl::core::tray_row(item("Sword", "SWORD"), 1, f.context).name_colour,
                  {1.0f, 0.8f, 0.8f, 0.8f}));
}

TL_TEST(tray_row_a_weapon_shows_its_dps_against_both_hands) {
    Fixture f;
    ItemStats sword = item("Sword", "SWORD");
    sword.speed = 100.0f;
    sword.damage = 22;
    // 22 / (1.0 * 0.73333335) = 29.99.., up to 30.
    TL_CHECK(tl::core::item_dps(sword, f.effects) == 30);
    // Both hands empty: better than nothing.
    TrayRow row = tl::core::tray_row(sword, 1, f.context);
    TL_CHECK(row.detail == "DPS: 30");
    TL_CHECK(row.arrow == TrayArrow::Up);
    // Equal to either hand: no arrow.
    f.context.worn[0] = 30;
    TL_CHECK(tl::core::tray_row(sword, 1, f.context).arrow == TrayArrow::None);
    // Below one hand: down, whatever the other holds.
    f.context.worn[0] = 31;
    TL_CHECK(tl::core::tray_row(sword, 1, f.context).arrow == TrayArrow::Down);
    f.context.worn[0] = 10;
    f.context.worn[1] = 12;
    TL_CHECK(tl::core::tray_row(sword, 1, f.context).arrow == TrayArrow::Up);
    // A faster swing is worth more: 22 / (1.25 * 0.73333335) = 24.
    sword.speed = 125.0f;
    TL_CHECK(tl::core::item_dps(sword, f.effects) == 24);
}

TL_TEST(tray_row_armour_is_compared_at_its_own_location) {
    Fixture f;
    ItemStats boots = item("Boots", "BOOTS");
    boots.graph_ac = 7;
    f.context.worn[6] = 5;
    TrayRow row = tl::core::tray_row(boots, 1, f.context);
    TL_CHECK(row.detail == "armor 7");
    TL_CHECK(row.arrow == TrayArrow::Up);
    f.context.worn[6] = 7;
    TL_CHECK(tl::core::tray_row(boots, 1, f.context).arrow == TrayArrow::None);
    f.context.worn[6] = 9;
    TL_CHECK(tl::core::tray_row(boots, 1, f.context).arrow == TrayArrow::Down);
    // An effect is worth one more: 7 + 1 against a worn 8 is a tie.
    boots.effects.push_back(passive(0x45, 3.0f));
    f.context.worn[6] = 8;
    TL_CHECK(tl::core::tray_row(boots, 1, f.context).arrow == TrayArrow::None);
    // A shield is compared with the twelfth figure, the left hand's armour.
    ItemStats shield = item("Buckler", "SHIELD");
    shield.graph_ac = 4;
    f.context.worn[11] = 6;
    TL_CHECK(tl::core::tray_row(shield, 1, f.context).arrow == TrayArrow::Down);
}

TL_TEST(tray_worn_figures_read_the_players_twelve_locations) {
    Fixture f;
    tl::core::Inventory inv;
    ItemStats sword = item("Sword", "SWORD");
    sword.speed = 100.0f;
    sword.damage = 22;
    ItemStats shield = item("Buckler", "SHIELD");
    shield.graph_ac = 4;
    shield.effects.push_back(passive(0x45, 3.0f));
    ItemStats boots = item("Boots", "BOOTS");
    boots.graph_ac = 7;
    TL_CHECK(inv.pickup_into_slot(sword, 1, tl::core::equip_location::kRightHand));
    TL_CHECK(inv.pickup_into_slot(shield, 1, tl::core::equip_location::kLeftHand));
    TL_CHECK(inv.pickup_into_slot(boots, 1, tl::core::equip_location::kBoots));
    const tl::core::TrayWornFigures worn = tl::core::tray_worn_figures(inv, f.types, f.effects);
    TL_CHECK(worn[0] == 30);
    TL_CHECK(worn[1] == 5);  // the shield's armour, one more for its effect
    TL_CHECK(worn[6] == 7);
    TL_CHECK(worn[3] == 0);
    TL_CHECK(worn[11] == 5);  // the left hand again
}

TL_TEST(tray_row_alpha_is_one_of_three) {
    TL_CHECK(tl::core::tray_row_alpha(true, false) == 1.0f);
    TL_CHECK(tl::core::tray_row_alpha(true, true) == 1.0f);
    TL_CHECK(tl::core::tray_row_alpha(false, true) == 0.9f);
    TL_CHECK(tl::core::tray_row_alpha(false, false) == 0.6f);
}

}  // namespace
