// What A does on a console item tray and the caption it shows,
// for the player's tray and the pet's.
#include "core/tray_press.h"

#include "core/item_use.h"
#include "core/spell_remove.h"

#include "test_support.h"

namespace {

using tl::core::ItemStats;
using tl::core::TrayCaptionContext;
using tl::core::TrayPress;
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
    types.children.push_back(type("CONSUMABLE", {1, 31, 2, 0}));
    types.children.push_back(type("FISH", {124, 123, 1, 31, 2, 0, 125}));
    types.children.push_back(type("NECKLACE", {24, 39, 31, 2, 0, 164}));
    types.children.push_back(type("POTION", {33, 1, 31, 2, 0}));
    types.children.push_back(type("QUESTITEM", {103, 2, 0}));
    types.children.push_back(type("RING", {17, 39, 31, 2, 0, 164}));
    types.children.push_back(type("SOCKETABLE", {120, 31, 2, 0}));
    types.children.push_back(type("SPELL", {129, 1, 31, 2, 0}));
    types.children.push_back(type("WEAPON", {8, 31, 2, 0}));
    AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

ItemStats item(const std::string& name, const std::string& unittype) {
    ItemStats out;
    out.name = name;
    out.unittype = unittype;
    return out;
}

constexpr size_t kBagSlot = 20;  // a general pane, not an equip location

}  // namespace

TL_TEST(tray_press_follows_the_console_handler) {
    const auto types = shipped_unit_types();
    TL_CHECK(tl::core::tray_press(item("Heal", "SPELL"), kBagSlot, types) == TrayPress::Learn);
    TL_CHECK(tl::core::tray_press(item("Health Potion", "POTION"), kBagSlot, types) ==
             TrayPress::UseOnOwner);
    TL_CHECK(tl::core::tray_press(item("Web Fish", "FISH"), kBagSlot, types) == TrayPress::FeedPet);
    TL_CHECK(tl::core::tray_press(item("Town Portal Scroll", "CONSUMABLE"), kBagSlot, types) ==
             TrayPress::TownPortal);
    TL_CHECK(tl::core::tray_press(item("Identify Scroll", "CONSUMABLE"), kBagSlot, types) ==
             TrayPress::Nothing);
    TL_CHECK(tl::core::tray_press(item("Collar", "NECKLACE"), kBagSlot, types) == TrayPress::Wear);
    TL_CHECK(tl::core::tray_press(item("Collar", "NECKLACE"), 3, types) == TrayPress::TakeOff);
    ItemStats unknown = item("Sword", "WEAPON");
    unknown.identified = false;
    TL_CHECK(tl::core::tray_press(unknown, kBagSlot, types) == TrayPress::Identify);
    TL_CHECK(tl::core::tray_press(item("Map", "QUESTITEM"), kBagSlot, types) == TrayPress::Nothing);
}

TL_TEST(tray_caption_depends_on_the_owner) {
    const auto types = shipped_unit_types();
    TrayCaptionContext player;
    TrayCaptionContext pet;
    pet.owner_is_player = false;
    pet.owner_is_pet = true;

    // Equipment: the player wears weapons, armour and trinkets; a pet only its
    // collar and tags.
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Sword", "WEAPON"), kBagSlot, types, player),
                std::string("Equip"));
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Sword", "WEAPON"), kBagSlot, types, pet),
                std::string(""));
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Collar", "NECKLACE"), kBagSlot, types, pet),
                std::string("Equip"));
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Tag", "RING"), kBagSlot, types, pet),
                std::string("Equip"));
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Tag", "RING"), 4, types, pet),
                std::string("Unequip"));

    // A spell reads "Learn" only when the owner can learn it.
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Heal", "SPELL"), kBagSlot, types, pet),
                std::string(""));
    pet.learnable = true;
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Heal", "SPELL"), kBagSlot, types, pet),
                std::string("Learn"));

    // A consumable reads "Use" when `canUse` passes, never an Identify Scroll.
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Health Potion", "POTION"), kBagSlot, types, pet),
                std::string(""));
    pet.can_use = true;
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Health Potion", "POTION"), kBagSlot, types, pet),
                std::string("Use"));
    TL_CHECK_EQ(
        tl::core::tray_a_caption(item("Identify Scroll", "CONSUMABLE"), kBagSlot, types, pet),
        std::string(""));
    // A fish on the player's own trays reads "Use" whatever `canUse` says,
    // unless the owner is away.
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Web Fish", "FISH"), kBagSlot, types, player),
                std::string("Use"));
    player.owner_away = true;
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Web Fish", "FISH"), kBagSlot, types, player),
                std::string(""));

    ItemStats unknown = item("Sword", "WEAPON");
    unknown.identified = false;
    player.identify_scrolls = 3;
    TL_CHECK_EQ(tl::core::tray_a_caption(unknown, kBagSlot, types, player),
                std::string("Identify(3)"));
    TL_CHECK_EQ(tl::core::tray_a_caption(item("Ruby", "SOCKETABLE"), kBagSlot, types, player),
                std::string("Socket"));
}

/// `CMessageSpellRemoveXenon`: the
/// codes, the wrap of the choice, and a pet's two rows.
TL_TEST(the_replace_spell_dialog_steps_wraps_and_hides_a_pets_rows) {
    using tl::core::SpellRemoveCommand;
    TL_CHECK(tl::core::spell_remove_command_for_code(5) == SpellRemoveCommand::Confirm);
    TL_CHECK(tl::core::spell_remove_command_for_code(6) == SpellRemoveCommand::Cancel);
    TL_CHECK(tl::core::spell_remove_command_for_code(0xe) == SpellRemoveCommand::Cancel);
    TL_CHECK(tl::core::spell_remove_command_for_code(0x11) == SpellRemoveCommand::Ignore);
    TL_CHECK(tl::core::spell_remove_command_for_code(0x13) == SpellRemoveCommand::Left);
    TL_CHECK(tl::core::spell_remove_command_for_code(0x14) == SpellRemoveCommand::Right);

    TL_CHECK_EQ(tl::core::wrap_spell_remove_choice(-1, true), 1);
    TL_CHECK_EQ(tl::core::wrap_spell_remove_choice(-1, false), 3);
    TL_CHECK_EQ(tl::core::wrap_spell_remove_choice(2, true), 0);
    TL_CHECK_EQ(tl::core::wrap_spell_remove_choice(2, false), 2);
    TL_CHECK_EQ(tl::core::wrap_spell_remove_choice(4, false), 0);

    tl::core::SkillDef heal;
    heal.icon = "spell_heal";
    tl::core::KnownSkill known;
    known.def = &heal;
    const auto chosen = tl::core::spell_remove_row(0, true, &known, true);
    TL_CHECK(chosen.shown);
    TL_CHECK_EQ(chosen.box_image, std::string("spells_window_rollover"));
    TL_CHECK_EQ(chosen.icon_image, std::string("spell_heal"));
    const auto empty = tl::core::spell_remove_row(1, true, nullptr, false);
    TL_CHECK_EQ(empty.box_image, std::string("spells_window"));
    TL_CHECK(empty.icon_image.empty());
    TL_CHECK(!tl::core::spell_remove_row(2, true, &known, false).shown);
    TL_CHECK(tl::core::spell_remove_row(3, false, &known, false).shown);
}

/// `CCharacter::updateHP`: an invincible pet with a master takes
/// `PET_HP_RECHARGE_RATE`, applied even when the player's rate is 0.
TL_TEST(a_pet_regenerates_at_its_own_rate) {
    tl::core::RechargeRates rates;
    rates.hp = 0.0f;
    rates.pet_hp = 2.0f;
    TL_CHECK(tl::core::hp_change(320.5f, rates, nullptr, 1.0f) == 0.0f);
    // ((320 * 2) * 0.5) * 0.01f: one unit under 3.2f.
    TL_CHECK(tl::core::hp_change(320.5f, rates, nullptr, 0.5f, true) == 3.1999998f);
}
