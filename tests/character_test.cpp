// The player character's progression: attributes off the class `.DAT`, the level-up curves
// and the stats screen's allocate/reclaim rules. Figures are the shipped data's.
#include "core/character.h"

#include <cmath>
#include "core/character_sheet.h"
#include "core/effect_manager.h"
#include "core/inventory.h"
#include "core/stats.h"
#include "core/stats_page.h"
#include "core/unit_types.h"
#include "core/weapons.h"
#include "test_support.h"

namespace {

// An inventory wearing one chest piece worth `points` armour (`CEquipment
// +0x338`), which `CInventory::updateBonuses` sums into `+0x10`.
tl::core::Inventory wearing_armour(int points) {
    tl::core::Inventory inventory;
    tl::core::ItemStats chest;
    chest.name = "Chest";
    chest.unittype = "CHEST ARMOR";
    chest.graph_ac = points;
    inventory.pickup_into_slot(chest, 1, tl::core::equip_location::kChest);
    return inventory;
}

/// One `[LINE]` curve, built the way `media/graphs/stats/` authors it.
tl::formats::AdmSection curve(const std::string& name,
                              const std::vector<std::pair<float, float>>& points,
                              bool infer_past_end = true) {
    tl::formats::AdmSection line;
    line.name = "LINE";
    tl::formats::AdmRecord named;
    named.key = "NAME";
    named.type = tl::formats::AdmType::String;
    named.text = name;
    line.records.push_back(named);
    if (infer_past_end) {
        tl::formats::AdmRecord infer;
        infer.key = "INFER_PASSED_END";
        infer.type = tl::formats::AdmType::Bool;
        infer.integer = 1;
        line.records.push_back(infer);
    }
    for (const auto& point : points) {
        tl::formats::AdmSection node;
        node.name = "POINT";
        tl::formats::AdmRecord x;
        x.key = "X";
        x.type = tl::formats::AdmType::Float;
        x.number = point.first;
        tl::formats::AdmRecord y;
        y.key = "Y";
        y.type = tl::formats::AdmType::Float;
        y.number = point.second;
        node.records.push_back(x);
        node.records.push_back(y);
        line.children.push_back(node);
    }
    return line;
}

/// The curves the stats screen needs, at the values `media/graphs/stats/`
/// actually authors for the first few levels.
tl::core::StatBook shipped_curves() {
    tl::core::StatBook book;
    auto add = [&book](const std::string& name,
                       const std::vector<std::pair<float, float>>& points) {
        book.add(name, tl::core::StatGraph::from(curve(name, points)));
    };
    // STAT_POINTS_PER_LEVEL is a single point at Y = 5, which is where "five
    // points a level" lives. It is not a constant in the engine. The file does
    // not set INFER_PASSED_END, and must not here: a one-point curve's slope is
    // its own Y (`CGraph::loadGraphLine`), so with the flag it would climb.
    book.add(tl::core::kStatPointsPerLevelGraph,
             tl::core::StatGraph::from(
                 curve(tl::core::kStatPointsPerLevelGraph, {{0.0f, 5.0f}}, false)));
    add(tl::core::kExperienceGateGraph, {{1.0f, 4000.0f}, {2.0f, 9412.0f}, {3.0f, 17404.0f}});
    add(tl::core::kFameGateGraph, {{1.0f, 1600.0f}, {2.0f, 3760.0f}, {3.0f, 6960.0f}});
    add("HEALTH_PLAYER_DESTROYER", {{1.0f, 300.0f}, {2.0f, 360.0f}, {3.0f, 420.0f}});
    add("MANA_PLAYER_DESTROYER", {{1.0f, 20.0f}, {2.0f, 25.0f}, {3.0f, 30.0f}});
    add("HEALTH_PLAYER_ALCHEMIST", {{1.0f, 200.0f}, {2.0f, 240.0f}, {3.0f, 280.0f}});
    add("MANA_PLAYER_ALCHEMIST", {{1.0f, 30.0f}, {2.0f, 36.0f}, {3.0f, 43.0f}});
    // The Vanquisher's health curve is the Alchemist's, and the two spellcasters
    // differ only in mana - she shares the Destroyer's 20/25/30.
    add("HEALTH_PLAYER_VANQUISHER", {{1.0f, 200.0f}, {2.0f, 240.0f}, {3.0f, 280.0f}});
    add("MANA_PLAYER_VANQUISHER", {{1.0f, 20.0f}, {2.0f, 25.0f}, {3.0f, 30.0f}});
    add(tl::core::kPlayerArmorGraph, {{1.0f, 11.0f}, {2.0f, 16.0f}, {3.0f, 19.0f}});
    add(tl::core::kExperienceMonsterGraph, {{1.0f, 100.0f}, {2.0f, 125.0f}});
    add(tl::core::kExperienceChampionMonsterGraph, {{1.0f, 300.0f}, {2.0f, 375.0f}});
    add(tl::core::kGoldDropGraph, {{1.0f, 3.75f}, {2.0f, 4.8f}});
    return book;
}

/// `media/units/players/Destroyer/DESTROYER.DAT` and its two siblings, as the
/// renderer reads them off the `BASEFILE` chain.
tl::core::ClassProfile destroyer() {
    tl::core::ClassProfile p;
    p.name = "Destroyer";
    p.strength = 10;
    p.dexterity = 4;
    p.magic = 3;
    p.defense = 10;
    p.health_graph = "HEALTH_PLAYER_DESTROYER";
    p.mana_graph = "MANA_PLAYER_DESTROYER";
    return p;
}

tl::core::ClassProfile alchemist() {
    tl::core::ClassProfile p;
    p.name = "Alchemist";
    p.strength = 6;
    p.dexterity = 7;
    p.magic = 10;
    p.defense = 5;
    p.health_graph = "HEALTH_PLAYER_ALCHEMIST";
    p.mana_graph = "MANA_PLAYER_ALCHEMIST";
    return p;
}

tl::core::ClassProfile vanquisher() {
    tl::core::ClassProfile p;
    p.name = "Vanquisher";
    p.strength = 5;
    p.dexterity = 10;
    p.magic = 7;
    p.defense = 6;
    p.health_graph = "HEALTH_PLAYER_VANQUISHER";
    p.mana_graph = "MANA_PLAYER_VANQUISHER";
    return p;
}

}  // namespace

TL_TEST(character_class_base_stats) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression des;
    des.init(destroyer(), book, "TestDestroyer");
    TL_CHECK(des.strength() == 10);
    TL_CHECK(des.dexterity() == 4);
    TL_CHECK(des.magic() == 3);
    TL_CHECK(des.defense() == 10);
    TL_CHECK(des.level() == 1);
    TL_CHECK(des.unspent_stat_points() == 0);

    CharacterProgression vanq;
    vanq.init(vanquisher(), book, "TestVanquisher");
    TL_CHECK(vanq.strength() == 5);
    TL_CHECK(vanq.dexterity() == 10);
    TL_CHECK(vanq.magic() == 7);
    TL_CHECK(vanq.defense() == 6);

    CharacterProgression alc;
    alc.init(alchemist(), book, "TestAlchemist");
    TL_CHECK(alc.strength() == 6);
    TL_CHECK(alc.dexterity() == 7);
    TL_CHECK(alc.magic() == 10);
    TL_CHECK(alc.defense() == 5);
}

TL_TEST(character_health_mana_and_gates_come_from_the_curves) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression alc;
    alc.init(alchemist(), book);
    // HEALTH_PLAYER_ALCHEMIST(1) = 200, MANA_PLAYER_ALCHEMIST(1) = 30. The
    // second is the one an invented table got wrong.
    TL_CHECK(alc.max_hp() == 200.0f);
    TL_CHECK(alc.max_mp() == 30.0f);
    TL_CHECK(alc.hp() == alc.max_hp());
    // EXPERIENCEGATE(1) = 4000 and FAMEGATE(1) = 1600; neither is a formula.
    TL_CHECK(alc.experience_gate() == 4000u);
    TL_CHECK(alc.fame_gate() == 1600u);

    alc.level_up(book);
    TL_CHECK(alc.level() == 2);
    TL_CHECK(alc.max_hp() == 240.0f);
    TL_CHECK(alc.max_mp() == 36.0f);
    TL_CHECK(alc.experience_gate() == 9412u);
    // CCharacter::levelUp() writes maxHP() and maxMana() straight into the
    // current-value fields: a level-up refills.
    TL_CHECK(alc.hp() == 240.0f);
    TL_CHECK(alc.mp() == 36.0f);
}

TL_TEST(character_level_up_grants_points) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression c;
    c.init(destroyer(), book);
    TL_CHECK(c.level() == 1);
    TL_CHECK(c.unspent_stat_points() == 0);

    c.level_up(book);
    TL_CHECK(c.level() == 2);
    TL_CHECK(c.unspent_stat_points() == 5);

    c.level_up(book);
    TL_CHECK(c.level() == 3);
    TL_CHECK(c.unspent_stat_points() == 10);
}

TL_TEST(character_stats_page_keeps_points_pending_until_it_closes) {
    // CStatsXenon (X360): A puts a point on the page, not on the character;
    // only closing the page (from `setOpen(false)`) places it.
    using tl::core::CharacterProgression;
    using tl::core::StatType;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression c;
    c.init(alchemist(), book);
    TL_CHECK(c.magic() == 10);

    // Nothing to place: A does nothing, not even a refresh.
    tl::core::StatsPage page;
    tl::core::open_stats_page(page, c.unspent_stat_points());
    page.dirty = false;
    TL_CHECK(!tl::core::stats_page_add_point(page, c.unspent_stat_points()));
    TL_CHECK(!page.dirty);
    tl::core::close_stats_page(page, c);

    c.level_up(book);
    TL_CHECK(c.unspent_stat_points() == 5);

    tl::core::open_stats_page(page, c.unspent_stat_points());
    TL_CHECK(page.available == 5);
    page.selected = 2;  // magic
    TL_CHECK(tl::core::stats_page_add_point(page, c.unspent_stat_points()));
    TL_CHECK(tl::core::stats_page_add_point(page, c.unspent_stat_points()));
    page.selected = 3;  // defense
    TL_CHECK(tl::core::stats_page_add_point(page, c.unspent_stat_points()));
    TL_CHECK(page.available == 2);
    TL_CHECK(page.pending[2] == 2);
    TL_CHECK(page.pending[3] == 1);
    // The character is untouched while the page is up.
    TL_CHECK(c.magic() == 10);
    TL_CHECK(c.unspent_stat_points() == 5);

    // Y takes back only what is pending on the selection.
    page.selected = 0;
    TL_CHECK(!tl::core::stats_page_remove_point(page, c.unspent_stat_points()));
    page.selected = 2;
    TL_CHECK(tl::core::stats_page_remove_point(page, c.unspent_stat_points()));
    TL_CHECK(page.pending[2] == 1);
    TL_CHECK(page.available == 3);

    tl::core::close_stats_page(page, c);
    TL_CHECK(c.magic() == 11);
    TL_CHECK(c.defense() == 6);
    TL_CHECK(c.unspent_stat_points() == 3);
    TL_CHECK(c.stat(StatType::Magic) == 11);

    // A second close changes nothing (`vfunc_7` compares the open state).
    tl::core::close_stats_page(page, c);
    TL_CHECK(c.magic() == 11);

    // The next opening starts from nothing pending.
    tl::core::open_stats_page(page, c.unspent_stat_points());
    TL_CHECK(page.pending[2] == 0);
    TL_CHECK(page.available == 3);
}

TL_TEST(character_stats_page_refresh_previews_and_reverts) {
    // commit, read, revert. The figures carry the pending
    // points; the character afterwards does not.
    using tl::core::CharacterProgression;
    using tl::core::StatType;
    const tl::core::StatBook book = shipped_curves();
    const tl::core::UnitTypeHierarchy types;

    CharacterProgression c;
    c.init(destroyer(), book);
    c.level_up(book);
    TL_CHECK(c.unspent_stat_points() == 5);

    tl::core::Inventory bag;
    tl::core::StatsPage page;
    tl::core::open_stats_page(page, c.unspent_stat_points());
    page.selected = 0;
    TL_CHECK(tl::core::stats_page_add_point(page, c.unspent_stat_points()));

    const tl::core::CharacterEffects none(nullptr, &bag);
    const tl::core::StatsPageView view =
        tl::core::refresh_stats_page(page, c, none, bag, types, &book, nullptr);
    TL_CHECK(!page.dirty);
    TL_CHECK_EQ(view.points[0], std::string("11"));
    TL_CHECK_EQ(view.points_colour[0], 0xFFFFFFFFu);  // nothing on top of the base
    TL_CHECK_EQ(view.to_spend, std::string("4"));
    TL_CHECK_EQ(view.added[0], std::string("+1"));
    TL_CHECK(view.added_shown[0]);
    TL_CHECK(!view.added_shown[1]);
    TL_CHECK_EQ(view.add_alpha, 1.0f);
    TL_CHECK_EQ(view.remove_alpha, 1.0f);
    TL_CHECK(view.spend_shown);
    TL_CHECK(view.help_shown[0]);
    TL_CHECK(!view.help_shown[1]);
    // Reverted.
    TL_CHECK(c.strength() == 10);
    TL_CHECK(c.unspent_stat_points() == 5);

    // Everything pending: `addPoint` dims, `removePoint` only where points are.
    for (int n = 0; n < 4; ++n) tl::core::stats_page_add_point(page, c.unspent_stat_points());
    TL_CHECK(page.available == 0);
    page.selected = 1;
    const tl::core::StatsPageView full =
        tl::core::refresh_stats_page(page, c, none, bag, types, &book, nullptr);
    TL_CHECK_EQ(full.points[0], std::string("15"));
    TL_CHECK_EQ(full.add_alpha, 0.25f);
    TL_CHECK_EQ(full.remove_alpha, 0.25f);
    TL_CHECK(!full.spend_shown);
    TL_CHECK(!full.help_shown[1]);
    TL_CHECK(c.strength() == 10);
}

TL_TEST(character_stats_page_revert_stops_at_two) {
    // takes a point back only while the attribute is above 2,
    // and places one only while a point is unspent.
    using tl::core::CharacterProgression;
    using tl::core::StatType;
    const tl::core::StatBook book = shipped_curves();
    CharacterProgression c;
    c.init(destroyer(), book);
    c.level_up(book);

    tl::core::StatsPage page;
    page.pending[0] = 7;  // more than the five unspent
    tl::core::commit_stats_page(page, c);
    TL_CHECK(c.strength() == 15);
    TL_CHECK(c.unspent_stat_points() == 0);
    tl::core::revert_stats_page(page, c);
    TL_CHECK(c.strength() == 8);  // 15 less seven
    TL_CHECK(c.unspent_stat_points() == 7);
}

TL_TEST(character_base_armour_bonus) {
    // CCharacter::baseArmorBonus: own armour plus what is
    // worn, plus the base defense's percentage of the two.
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression c;
    c.init(destroyer(), book);
    const tl::core::Inventory bag = wearing_armour(12);
    const tl::core::CharacterEffects worn(nullptr, &bag);
    const int sum = 12 + c.natural_armor();
    const int defense = c.base_stat(tl::core::StatType::Defense);
    TL_CHECK_EQ(c.base_armour_bonus(worn),
                sum + static_cast<int>(std::ceil((static_cast<float>(defense) * 0.01f) *
                                                 static_cast<float>(sum))));
}

/// `CPlayer::calculateMaxHP` and `calculateMaxMana`: the
/// curve at the level, rounded up to a whole point. `calculateNaturalArmor`:
/// the class's `ARMOR` as a percentage of `ARMOR_MONSTER_BYLEVEL`
/// at the level, rounded up - and nothing at all while `ARMOR` is 0, as on the
/// three shipped classes.
TL_TEST(a_players_maxima_and_natural_armour_are_its_curves_rounded_up) {
    tl::core::StatBook book = shipped_curves();
    book.add("HEALTH_PLAYER_TEST", tl::core::StatGraph::from(curve(
                                       "HEALTH_PLAYER_TEST", {{1.0f, 100.25f}, {2.0f, 150.0f}})));
    book.add("MANA_PLAYER_TEST",
             tl::core::StatGraph::from(curve("MANA_PLAYER_TEST", {{1.0f, 19.5f}, {2.0f, 25.0f}})));
    book.add(tl::core::kMonsterArmorGraph,
             tl::core::StatGraph::from(
                 curve(tl::core::kMonsterArmorGraph, {{1.0f, 21.0f}, {2.0f, 30.0f}})));
    tl::core::ClassProfile p = destroyer();
    p.health_graph = "HEALTH_PLAYER_TEST";
    p.mana_graph = "MANA_PLAYER_TEST";
    tl::core::CharacterProgression plain;
    plain.init(p, book);
    TL_CHECK_EQ(plain.max_hp(), 101.0f);
    TL_CHECK_EQ(plain.max_mp(), 20.0f);
    TL_CHECK_EQ(plain.armour_bonus(), 0);

    p.armor = 50;
    tl::core::CharacterProgression armoured;
    armoured.init(p, book);
    TL_CHECK_EQ(armoured.armour_bonus(), 11);  // ceil(21 * 0.5)
}

TL_TEST(character_armour_class_follows_the_binary) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression c;
    c.init(destroyer(), book);
    // Nothing worn and no ARMOR on the class unit: AC is zero, not a default.
    TL_CHECK(c.armour_bonus() == 0);
    TL_CHECK(c.armour_class() == 0);

    // CCharacter::AC() = armorBonus + ceil(armorBonus * defense / 100).
    // The Destroyer's defence is 10, so 40 points of kit read as 44.
    // CCharacter::armorBonus reads what the inventory wears (`+0x10`).
    const tl::core::Inventory worn = wearing_armour(40);
    const tl::core::CharacterEffects total(nullptr, &worn);
    TL_CHECK(c.armour_bonus(total) == 40);
    TL_CHECK(c.armour_class(total) == 44);
    // Without the inventory the kit is not counted.
    TL_CHECK(c.armour_bonus() == 0);

    // CCharacter::minimumAC() and minimumDamageForDisplay() are both
    // ceil(maximum * 0.5), so the screen's left-hand figure is never independent.
    TL_CHECK(CharacterProgression::display_minimum(44) == 22);
    TL_CHECK(CharacterProgression::display_minimum(25) == 13);
    TL_CHECK(CharacterProgression::display_minimum(0) == 0);
    TL_CHECK(CharacterProgression::display_range(25) == "13-25");
    TL_CHECK(CharacterProgression::display_range(0) == "0-0");
    // Below 0 as the console's stats page shows it.
    TL_CHECK(CharacterProgression::display_minimum(-23) == -11);
    TL_CHECK(CharacterProgression::display_range(-23) == "-11--23");
}

TL_TEST(character_weapon_damage_rows) {
    using tl::core::weapon_damage_kind;
    using tl::core::WeaponDamageKind;

    // ITEMCATEGORYMARTIAL (162) in media/UNITTYPES.HIE.
    TL_CHECK(weapon_damage_kind("NORMAL SWORD") == WeaponDamageKind::Melee);
    TL_CHECK(weapon_damage_kind("MAGIC AXE") == WeaponDamageKind::Melee);
    TL_CHECK(weapon_damage_kind("POLEARM") == WeaponDamageKind::Melee);
    // ITEMCATEGORYRANGED (163). CROSSBOW must not fall through to BOW.
    TL_CHECK(weapon_damage_kind("NORMAL BOW") == WeaponDamageKind::Ranged);
    TL_CHECK(weapon_damage_kind("UNIQUE CROSSBOW") == WeaponDamageKind::Ranged);
    TL_CHECK(weapon_damage_kind("MAGIC PISTOL") == WeaponDamageKind::Ranged);
    // ITEMCATEGORYMAGIC (164). The Alchemist's Moldy Staff is a NORMAL STAFF.
    TL_CHECK(weapon_damage_kind("NORMAL STAFF") == WeaponDamageKind::Magic);
    TL_CHECK(weapon_damage_kind("MAGIC WAND") == WeaponDamageKind::Magic);
    // Not a weapon at all.
    TL_CHECK(weapon_damage_kind("MAGIC CHEST ARMOR") == WeaponDamageKind::None);
    TL_CHECK(weapon_damage_kind("") == WeaponDamageKind::None);
}

TL_TEST(character_default_naming) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression des;
    des.init(destroyer(), book);
    TL_CHECK(des.name() == "Destroyer");
    TL_CHECK(des.player_class() == "Destroyer");

    CharacterProgression custom;
    custom.init(alchemist(), book, "MyHero");
    TL_CHECK(custom.name() == "MyHero");
    TL_CHECK(custom.player_class() == "Alchemist");
}

TL_TEST(character_journal_stats) {
    using tl::core::CharacterProgression;
    using tl::core::JournalStatistic;

    CharacterProgression c;
    c.journal().set(JournalStatistic::GoldGathered, 123456);
    c.journal().set(JournalStatistic::LevelsExplored, 3);
    c.journal().set(JournalStatistic::StepsTaken, 5400);
    c.journal().set(JournalStatistic::QuestsCompleted, 2);
    c.journal().set(JournalStatistic::MonstersDefeated, 42);
    c.journal().set(JournalStatistic::ChampionsDefeated, 4);
    c.journal().set(JournalStatistic::Deaths, 1);
    c.journal().set(JournalStatistic::SkillsCast, 88);
    c.journal().set(JournalStatistic::ChestsOpened, 7);
    c.journal().set(JournalStatistic::TrapsSprung, 0);
    c.journal().set(JournalStatistic::BarrelsBroken, 25);
    c.journal().set(JournalStatistic::PotionsUsed, 12);
    c.journal().set(JournalStatistic::PortalsUsed, 3);
    c.journal().set(JournalStatistic::FishCaught, 5);
    c.journal().set(JournalStatistic::TimesGambled, 6);
    c.journal().set(JournalStatistic::ItemsTransmuted, 1);
    c.journal().set(JournalStatistic::ItemsEnchanted, 2);

    TL_CHECK_EQ(c.journal().value(JournalStatistic::GoldGathered), 123456);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::LevelsExplored), 3);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::StepsTaken), 5400);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::QuestsCompleted), 2);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::MonstersDefeated), 42);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::ChampionsDefeated), 4);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::Deaths), 1);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::SkillsCast), 88);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::ChestsOpened), 7);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::TrapsSprung), 0);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::BarrelsBroken), 25);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::PotionsUsed), 12);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::PortalsUsed), 3);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::FishCaught), 5);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::TimesGambled), 6);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::ItemsTransmuted), 1);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::ItemsEnchanted), 2);

    // Initializing progression for a new character resets journal statistics
    tl::core::ClassProfile profile;
    tl::core::StatBook curves = shipped_curves();
    c.init(profile, curves, "NewChar");
    TL_CHECK_EQ(c.journal().value(JournalStatistic::GoldGathered), 0);
    TL_CHECK_EQ(c.journal().value(JournalStatistic::MonstersDefeated), 0);
}

TL_TEST(monster_experience_foreman_and_champion) {
    const tl::core::StatBook book = shipped_curves();

    // Ratlin Foreman (level 1, XP: 130): the XP only switches the award on.
    // EXPERIENCE_MONSTER(1) = 100: (int)((100 * 100) * 0.01f) = 100.
    const uint64_t foreman_xp = tl::core::monster_experience(1, 130, false, 1, book);
    TL_CHECK(foreman_xp == 100u);

    // Champion: EXPERIENCE_CHAMPIONMONSTER(1) = 300: (300 * 300) * 0.01f = 900.
    const uint64_t champ_xp = tl::core::monster_experience(1, 130, true, 1, book);
    TL_CHECK(champ_xp == 900u);

    // XP 0 pays nothing; a negative XP still pays (the console tests `!= 0`).
    TL_CHECK(tl::core::monster_experience(1, 0, false, 1, book) == 0u);
    TL_CHECK(tl::core::monster_experience(1, -5, false, 1, book) == 100u);
}

TL_TEST(forty_foremen_take_alchemist_from_zero_to_level_up) {
    using tl::core::CharacterProgression;
    const tl::core::StatBook book = shipped_curves();

    CharacterProgression alc;
    alc.init(alchemist(), book);

    TL_CHECK(alc.level() == 1);
    TL_CHECK(alc.experience() == 0u);
    TL_CHECK(alc.experience_gate() == 4000u);
    TL_CHECK(alc.unspent_stat_points() == 0);
    TL_CHECK(alc.hp() == 200.0f);
    TL_CHECK(alc.max_hp() == 200.0f);
    TL_CHECK(alc.experience_progress(book) == 0.0f);

    // 39 Foremen: 39 * 100 = 3900 XP. Gate is 4000 XP. Not leveled up yet.
    for (int i = 0; i < 39; ++i) {
        const uint64_t xp = tl::core::monster_experience(1, 130, false, 1, book);
        const bool leveled = alc.award_experience(xp, book);
        TL_CHECK(!leveled);
    }
    TL_CHECK(alc.experience() == 3900u);
    TL_CHECK(alc.level() == 1);
    TL_CHECK(alc.unspent_stat_points() == 0);
    TL_CHECK(alc.experience_progress(book) == 3900.0f / 4000.0f);

    // 40th Foreman: 4000 XP meets the gate; level 2, +5 stat points, health 240 and mana 36
    // refilled, next gate 9412.
    const uint64_t xp40 = tl::core::monster_experience(1, 130, false, 1, book);
    const bool leveled = alc.award_experience(xp40, book);
    TL_CHECK(leveled);
    TL_CHECK(alc.level() == 2);
    TL_CHECK(alc.experience() == 4000u);
    TL_CHECK(alc.unspent_stat_points() == 5);
    TL_CHECK(alc.hp() == 240.0f);
    TL_CHECK(alc.max_hp() == 240.0f);
    TL_CHECK(alc.mp() == 36.0f);
    TL_CHECK(alc.max_mp() == 36.0f);
    TL_CHECK(alc.experience_gate() == 9412u);

    // Progress at level 2: (4000 - 4000) / (9412 - 4000) = 0.
    const float expected_progress = 0.0f;
    TL_CHECK(std::abs(alc.experience_progress(book) - expected_progress) < 0.001f);
}

namespace {

tl::core::EffectManager make_bonus_manager(const tl::core::StatBook& book, int type,
                                           float percent) {
    tl::core::EffectManager mgr;
    tl::core::EffectManager::Context ctx;
    ctx.graphs = &book;
    ctx.owner.present = true;
    ctx.owner.level = 1;
    mgr.set_context(ctx);

    tl::core::Effect eff;
    eff.type = type;
    eff.values[0] = percent;
    eff.values[1] = percent;
    mgr.add_effect(eff);
    mgr.calculate_effect_values();
    return mgr;
}

}  // namespace

TL_TEST(combat_progression_crit_and_block) {
    const tl::core::StatBook book = shipped_curves();

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // Alchemist starting stats: Dexterity = 7, Defense = 6.
    // CCharacter::getCriticalChance: base is 3 (without effects).
    // Dexterity does NOT contribute to crit chance (transcribed from native disassembly).
    TL_CHECK(alc.crit_chance() == 3);
    // Block chance without shield/effects is 0.
    TL_CHECK(alc.block_chance() == 0);

    // Give 5 stat points to spend on Dexterity
    alc.award_experience(4000, book);  // level 2, gives 5 points
    for (int i = 0; i < 5; ++i) alc.move_stat_point(tl::core::StatType::Dexterity, 1);
    // Dexterity is now 12 (7 + 5). Crit chance remains 3.
    TL_CHECK(alc.dexterity() == 12);
    TL_CHECK(alc.crit_chance() == 3);

    // With an EffectManager that has effect 0x37 (CRITICAL CHANCE) = 5.2f:
    // ceil(3.0 + 5.2) = ceil(8.2) = 9.
    tl::core::EffectManager effects =
        make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, 5.2f);
    TL_CHECK(alc.crit_chance(&effects) == 9);
}

TL_TEST(award_experience_bonus_and_shared_and_nullptr) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::EffectManager effects = make_bonus_manager(book, 0x44, 50.0f);

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // +50% XP bonus: 3 * 0.5 = 1.5, ceilf(1.5) = 2, total = 5
    TL_CHECK(!alc.award_experience(3, book, &effects));
    TL_CHECK(alc.experience() == 5u);

    // nullptr effects: bonus is 0, award is 3
    TL_CHECK(!alc.award_experience(3, book, nullptr));
    TL_CHECK(alc.experience() == 8u);

    // shared == true: takes no bonus, award is 3
    TL_CHECK(!alc.award_experience(3, book, &effects, true));
    TL_CHECK(alc.experience() == 11u);

    // What the pets are awarded carries the character's bonus;
    // a gain is refused while dying, a loss is not and takes no
    // bonus (only an amount above 0 does).
    int share = 0;
    TL_CHECK(alc.pet_experience_share(3, &effects, false, share) && share == 5);
    TL_CHECK(alc.pet_experience_share(3, nullptr, false, share) && share == 3);
    TL_CHECK(!alc.pet_experience_share(3, &effects, true, share));
    TL_CHECK(alc.pet_experience_share(-3, &effects, true, share) && share == -3);
}

TL_TEST(award_experience_negative_penalty_and_floor) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::EffectManager effects = make_bonus_manager(book, 0x44, 50.0f);

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    alc.award_experience(10, book);
    TL_CHECK(alc.experience() == 10u);

    // A loss takes no bonus: -3 is -3
    alc.award_experience(-3, book, &effects);
    TL_CHECK(alc.experience() == 7u);

    // Large negative award floors at 0
    alc.award_experience(-100, book, &effects);
    TL_CHECK(alc.experience() == 0u);
}

TL_TEST(award_experience_saturation_at_int32_max) {
    // Build a custom curve with a gate exceeding INT32_MAX so no level-up occurs
    tl::core::StatBook book;
    book.add(tl::core::kExperienceGateGraph,
             tl::core::StatGraph::from(curve(tl::core::kExperienceGateGraph,
                                             {{1.0f, 2147483647.0f}, {2.0f, 2147483647.0f}})));

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // Set experience near INT32_MAX
    alc.award_experience(2147483640, book);
    TL_CHECK(alc.experience() == 2147483640u);

    // Adding 100 would overflow int32_t (0x7fffffff = 2147483647); must saturate
    alc.award_experience(100, book);
    TL_CHECK(alc.experience() == 2147483647u);
}

TL_TEST(award_experience_single_level_up_and_cap_clamp) {
    const tl::core::StatBook book = shipped_curves();

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    TL_CHECK(alc.level() == 1);
    TL_CHECK(alc.experience_gate() == 4000u);

    // Native binary CCharacter::awardExperience calls levelUp ONCE per call
    // Award 15000 XP: crosses gate 1 (4000) and gate 2 (9412), but levels up once to level 2
    TL_CHECK(alc.award_experience(15000, book));
    TL_CHECK(alc.level() == 2);
    TL_CHECK(alc.experience() == 15000u);
    TL_CHECK(alc.experience_gate() == 9412u);

    // Next call advances to level 3 (max level in shipped_curves() which has 3 points)
    TL_CHECK(alc.award_experience(1, book));
    TL_CHECK(alc.level() == 3);
    TL_CHECK(alc.experience() == 15001u);
    TL_CHECK(alc.experience_gate() == 17404u);

    // At max level, award crossing or reaching gate clamps experience to the gate and does not level up
    TL_CHECK(!alc.award_experience(5000, book));
    TL_CHECK(alc.level() == 3);
    TL_CHECK(alc.experience() == 17404u);
}

TL_TEST(award_fame_bonus_and_negative_and_shared) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::EffectManager effects = make_bonus_manager(book, 0x4d, 50.0f);

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // +50% Fame bonus: 3 * 0.5 = 1.5, ceilf(1.5) = 2, total = 5
    TL_CHECK(!alc.award_fame(3, book, &effects));
    TL_CHECK(alc.fame() == 5u);

    // shared == true takes the bonus too: awardFame does not read it
    TL_CHECK(!alc.award_fame(3, book, &effects, true));
    TL_CHECK(alc.fame() == 10u);

    // Negative fame floors at 0
    alc.award_fame(-100, book);
    TL_CHECK(alc.fame() == 0u);
}

TL_TEST(award_fame_multi_level_up_and_cap_clamp) {
    tl::core::StatBook book = shipped_curves();
    // Made-up points, different at every level, so the test says which level
    // `fameUp` reads: the character's (1), giving 1 + 1, not the fame level's 7 + 9.
    book.add(tl::core::kSkillPointsPerFameLevelGraph,
             tl::core::StatGraph::from(curve(tl::core::kSkillPointsPerFameLevelGraph,
                                             {{1.0f, 1.0f}, {2.0f, 7.0f}, {3.0f, 9.0f}})));

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    TL_CHECK(alc.fame_level() == 1);
    TL_CHECK(alc.fame_gate() == 1600u);
    const int initial_skill_pts = alc.unspent_skill_points();

    // Unlike experience, CCharacter::awardFame LOOPS until fame < gate or cap reached
    // Award 5000 fame: crosses gate 1 (1600) -> fame 2, and gate 2 (3760) -> fame 3 (cap = 3)
    TL_CHECK(alc.award_fame(5000, book));
    TL_CHECK(alc.fame_level() == 3);
    TL_CHECK(alc.fame() == 5000u);
    TL_CHECK(alc.unspent_skill_points() == initial_skill_pts + 2);

    // At max fame level (3), award crossing gate clamps to fame_gate and gains no more fame levels
    TL_CHECK(!alc.award_fame(5000, book));
    TL_CHECK(alc.fame_level() == 3);
    TL_CHECK(alc.fame() == 6960u);
}

TL_TEST(award_fame_saturation_at_int32_max) {
    tl::core::StatBook book;
    book.add(tl::core::kFameGateGraph,
             tl::core::StatGraph::from(
                 curve(tl::core::kFameGateGraph, {{1.0f, 2147483647.0f}, {2.0f, 2147483647.0f}})));

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    alc.award_fame(2147483640, book);
    TL_CHECK(alc.fame() == 2147483640u);

    alc.award_fame(100, book);
    TL_CHECK(alc.fame() == 2147483647u);
}

TL_TEST(award_experience_refusal_when_dead) {
    const tl::core::StatBook book = shipped_curves();

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    alc.award_experience(100, book);
    TL_CHECK(alc.experience() == 100u);

    // Dying (AI state 5 or 6): a positive award is refused...
    TL_CHECK(!alc.award_experience(50, book, nullptr, false, true));
    TL_CHECK(alc.experience() == 100u);

    // ...and a penalty still taken.
    alc.award_experience(-30, book, nullptr, false, true);
    TL_CHECK(alc.experience() == 70u);
}

TL_TEST(award_fame_reaching_the_cap_in_the_loop_keeps_the_surplus) {
    const tl::core::StatBook book = shipped_curves();

    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // 8000 crosses 1600 and 3760 and stops at the cap (3) above its gate, 6960:
    // the loop's exit does not clamp.
    TL_CHECK(alc.award_fame(8000, book));
    TL_CHECK(alc.fame_level() == 3);
    TL_CHECK(alc.fame() == 8000u);

    // An award that starts at the cap clamps.
    TL_CHECK(!alc.award_fame(1, book));
    TL_CHECK(alc.fame() == 6960u);
}

TL_TEST(crit_chance_rounds_up_and_caps_at_100) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // 0 effect -> 3
    TL_CHECK(alc.crit_chance(nullptr) == 3);

    // 0.1f effect -> 4 (due to ceilf(3.1f))
    {
        auto eff = make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, 0.1f);
        TL_CHECK(alc.crit_chance(&eff) == 4);
    }

    // 96.9f effect -> 100 (ceilf(99.9f) = 100)
    {
        auto eff = make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, 96.9f);
        TL_CHECK(alc.crit_chance(&eff) == 100);
    }

    // 97.1f effect -> 100 (ceilf(100.1f) = 101, capped at 100)
    {
        auto eff = make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, 97.1f);
        TL_CHECK(alc.crit_chance(&eff) == 100);
    }

    // 150.0f effect -> 100
    {
        auto eff = make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, 150.0f);
        TL_CHECK(alc.crit_chance(&eff) == 100);
    }

    // Negative effect: -4.0f -> -1 (ceilf(-1.0f) = -1, no lower bound in original)
    {
        auto eff = make_bonus_manager(book, tl::core::kEffectTypeCriticalChance, -4.0f);
        TL_CHECK(alc.crit_chance(&eff) == -1);
    }
}

TL_TEST(block_chance_sums_two_effects_truncates_and_caps_at_50) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    // 0 effects -> 0
    TL_CHECK(alc.block_chance(nullptr) == 0);

    // effect 0x3e=10.9f, 0x21=5.2f -> sum 16.1f -> 16 (truncation toward zero, no ceilf)
    {
        tl::core::EffectManager mgr;
        tl::core::EffectManager::Context ctx;
        ctx.graphs = &book;
        mgr.set_context(ctx);
        tl::core::Effect e1;
        e1.type = tl::core::kEffectTypePercentBlockChanceBase;
        e1.values[0] = 10.9f;
        e1.values[1] = 10.9f;
        mgr.add_effect(e1);
        mgr.calculate_effect_values();
        tl::core::Effect e2;
        e2.type = tl::core::kEffectTypePercentBlockChance;
        e2.values[0] = 5.2f;
        e2.values[1] = 5.2f;
        mgr.add_effect(e2);
        mgr.calculate_effect_values();
        TL_CHECK(alc.block_chance(&mgr) == 16);
    }

    // effect 0x3e=30.0f, 0x21=25.0f -> sum 55.0f -> 50 (capped at 50)
    {
        tl::core::EffectManager mgr;
        tl::core::EffectManager::Context ctx;
        ctx.graphs = &book;
        mgr.set_context(ctx);
        tl::core::Effect e1;
        e1.type = tl::core::kEffectTypePercentBlockChanceBase;
        e1.values[0] = 30.0f;
        e1.values[1] = 30.0f;
        mgr.add_effect(e1);
        mgr.calculate_effect_values();
        tl::core::Effect e2;
        e2.type = tl::core::kEffectTypePercentBlockChance;
        e2.values[0] = 25.0f;
        e2.values[1] = 25.0f;
        mgr.add_effect(e2);
        mgr.calculate_effect_values();
        TL_CHECK(alc.block_chance(&mgr) == 50);
    }

    // Negative effect: sum -5.5f -> -5 (truncated toward zero)
    {
        tl::core::EffectManager mgr;
        tl::core::EffectManager::Context ctx;
        ctx.graphs = &book;
        mgr.set_context(ctx);
        tl::core::Effect e1;
        e1.type = tl::core::kEffectTypePercentBlockChance;
        e1.values[0] = -5.5f;
        e1.values[1] = -5.5f;
        mgr.add_effect(e1);
        mgr.calculate_effect_values();
        TL_CHECK(alc.block_chance(&mgr) == -5);
    }
}

TL_TEST(attribute_rounding_and_truncation) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    // Alchemist base: str=6, dex=7, mag=10, def=5.

    tl::core::EffectManager mgr;
    tl::core::EffectManager::Context ctx;
    ctx.graphs = &book;
    mgr.set_context(ctx);

    // Percent strength bonus: 25% on 6 -> 6 * 0.25 = 1.5 -> ceilf(1.5) = 2.
    // Flat strength bonus: 3.2 -> ceilf(3.2) = 4.
    // Total strength: 6 + 2 + 4 = 12.
    tl::core::Effect e_str_pct;
    e_str_pct.type = tl::core::kEffectTypePercentStrengthBonus;
    e_str_pct.values[0] = 25.0f;
    e_str_pct.values[1] = 25.0f;
    mgr.add_effect(e_str_pct);
    mgr.calculate_effect_values();
    tl::core::Effect e_str_flat;
    e_str_flat.type = tl::core::kEffectTypeStrengthBonus;
    e_str_flat.values[0] = 3.2f;
    e_str_flat.values[1] = 3.2f;
    mgr.add_effect(e_str_flat);
    mgr.calculate_effect_values();
    TL_CHECK(alc.strength(&mgr) == 12);

    // Defense sums (val7 + val6) before multiplying:
    // Defense base = 5.
    // val7 (kDamageTypeAny) = 10%, val6 (All) = 15% -> sum = 25%.
    // 5 * 0.25 = 1.25 -> ceilf(1.25) = 2.
    // Flat defense = 2.1 -> ceilf(2.1) = 3.
    // Total defense: 5 + 2 + 3 = 10.
    tl::core::Effect e_def_pct7;
    e_def_pct7.type = tl::core::kEffectTypePercentDefense;
    e_def_pct7.damage_type = tl::core::DamageType::Physical;
    e_def_pct7.values[0] = 10.0f;
    e_def_pct7.values[1] = 10.0f;
    mgr.add_effect(e_def_pct7);
    mgr.calculate_effect_values();
    tl::core::Effect e_def_pct6;
    e_def_pct6.type = tl::core::kEffectTypePercentDefense;
    e_def_pct6.damage_type = tl::core::DamageType::All;
    e_def_pct6.values[0] = 15.0f;
    e_def_pct6.values[1] = 15.0f;
    mgr.add_effect(e_def_pct6);
    mgr.calculate_effect_values();
    tl::core::Effect e_def_flat;
    e_def_flat.type = tl::core::kEffectTypeDefense;
    e_def_flat.values[0] = 2.1f;
    e_def_flat.values[1] = 2.1f;
    mgr.add_effect(e_def_flat);
    mgr.calculate_effect_values();
    TL_CHECK(alc.defense(&mgr) == 10);
}

TL_TEST(armour_class_applies_its_effect_term) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    const tl::core::Inventory worn = wearing_armour(20);

    tl::core::EffectManager mgr;
    tl::core::EffectManager::Context ctx;
    ctx.graphs = &book;
    mgr.set_context(ctx);

    // armor_bonus = base (20) + ceil(20 * pct / 100) + ceil(flat)
    // pct = 15% -> 20 * 0.15 = 3.0 -> ceil(3.0) = 3
    // flat = 4.2 -> ceil(4.2) = 5
    // bonus = 20 + 3 + 5 = 28
    tl::core::Effect e_arm_pct;
    e_arm_pct.type = tl::core::kEffectTypePercentArmorBonus;
    e_arm_pct.values[0] = 15.0f;
    e_arm_pct.values[1] = 15.0f;
    mgr.add_effect(e_arm_pct);
    mgr.calculate_effect_values();
    tl::core::Effect e_arm_flat;
    e_arm_flat.type = tl::core::kEffectTypeArmorBonus;
    e_arm_flat.values[0] = 4.2f;
    e_arm_flat.values[1] = 4.2f;
    mgr.add_effect(e_arm_flat);
    mgr.calculate_effect_values();
    TL_CHECK(alc.armour_bonus(tl::core::CharacterEffects(&mgr, &worn)) == 28);

    // AC = bonus + ceil(bonus * defense / 100) - ceil(degrade)
    // defense is base 5 (no defense effects).
    // bonus * def / 100 = 28 * 5 / 100 = 1.4 -> ceil(1.4) = 2.
    // degrade = 6.2 -> ceil(6.2) = 7.
    // AC = 28 + 2 - 7 = 23.
    tl::core::Effect e_deg;
    e_deg.type = tl::core::kEffectTypeDegradeArmorEffect;
    e_deg.values[0] = 6.2f;
    e_deg.values[1] = 6.2f;
    mgr.add_effect(e_deg);
    mgr.calculate_effect_values();
    TL_CHECK(alc.armour_class(tl::core::CharacterEffects(&mgr, &worn)) == 23);

    // Excessive degrade floors at 0
    tl::core::Effect e_deg_big;
    e_deg_big.type = tl::core::kEffectTypeDegradeArmorEffect;
    e_deg_big.values[0] = 100.0f;
    e_deg_big.values[1] = 100.0f;
    mgr.add_effect(e_deg_big);
    mgr.calculate_effect_values();
    TL_CHECK(alc.armour_class(tl::core::CharacterEffects(&mgr, &worn)) == 0);
}

namespace {

tl::core::WeaponInputs held_weapon(int max_damage, bool ranged) {
    tl::core::WeaponInputs w;
    w.equipped = true;
    w.is_weapon = true;
    w.is_ranged = ranged;
    w.max_damage = max_damage;
    return w;
}

}  // namespace

// `CCharacter::maximumDamageForDisplay` with no effects running: the
// attribute still scales a hand (strength 6 -> 0.06, dexterity 7 -> 0.07).
TL_TEST(maximum_damage_for_display_branches) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    tl::core::DisplayInputs in;
    in.progression = &alc;
    in.weapon_in_use = true;
    in.right_hand = held_weapon(20, false);

    // 20 + ceil(20 * 0.06 = 1.2) = 22. The physical base is not also a type entry.
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 22);
    // The ranged row passes a melee weapon over.
    TL_CHECK(tl::core::maximum_damage_for_display(in, false) == 0);
    // Without effects, the base alone.
    TL_CHECK(tl::core::maximum_damage_for_display(in, true, false, true) == 20);
    // Without the base: nothing left, and no bare-handed fallback.
    TL_CHECK(tl::core::maximum_damage_for_display(in, true, true, false) == 0);

    // A bow in location 1: 15 + ceil(15 * 0.07 = 1.05) = 17 on the ranged row;
    // the melee row still reads the sword.
    in.left_hand = held_weapon(15, true);
    TL_CHECK(tl::core::maximum_damage_for_display(in, false) == 17);
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 22);

    // A shield-like non-weapon in location 1 is never counted, even on its row.
    tl::core::WeaponInputs shield = held_weapon(30, false);
    shield.is_weapon = false;
    in.left_hand = shield;
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 22);

    // Poison 22 on a melee weapon with no physical base: the type scales by
    // magic (10 -> 0.1) plus the hand's 0.06: 22 + ceil(22 * 0.16 = 3.52) = 26.
    tl::core::DisplayInputs staff_in;
    staff_in.progression = &alc;
    staff_in.weapon_in_use = true;
    staff_in.right_hand = held_weapon(0, false);
    staff_in.right_hand.damage_types.push_back({5, 22, 0});
    TL_CHECK(tl::core::maximum_damage_for_display(staff_in, true) == 26);
    // The magic row's call keeps the elemental entry.
    TL_CHECK(tl::core::maximum_damage_for_display(staff_in, true, true, false) == 26);
    TL_CHECK(tl::core::maximum_damage_for_display(staff_in, true, false, true) == 22);
}

// No weapon in use: the bare attack, raised by levels gained and strength.
TL_TEST(maximum_damage_for_display_bare_hands) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);

    tl::core::DisplayInputs in;
    in.progression = &alc;
    in.attack.damage_by_type[0] = 50;
    // int(106 * 50 / 100) = 53.
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 53);
    TL_CHECK(tl::core::maximum_damage_for_display(in, true, false, true) == 53);
    // One level above `+0x5c8` (the base level): int(101 * 100 * 0.01) = 101, then
    // int(106 * 101 * 0.01) = 107.
    in.attack.damage_by_type[0] = 100;
    in.base_level = 0;
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 107);
    // The console compares the two unsigned (`cmplw`): a base level of -1 is
    // above every level, and nothing is added.
    in.base_level = -1;
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 106);
    in.attack.damage_by_type[0] = 50;
    // Bare hands are melee only.
    TL_CHECK(tl::core::maximum_damage_for_display(in, false) == 0);
    // No attack at all (`+0x390` null after `selectAttack`).
    in.attack.has_attack = false;
    TL_CHECK(tl::core::maximum_damage_for_display(in, true) == 0);
    TL_CHECK(tl::core::minimum_damage_for_display(in, true) == 0);
}

TL_TEST(combat_sheet_integration) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);
    const tl::core::Inventory worn = wearing_armour(15);

    tl::core::DisplayInputs in;
    in.progression = &alc;
    in.effects = tl::core::CharacterEffects(nullptr, &worn);
    in.weapon_in_use = true;
    in.right_hand = held_weapon(20, false);
    in.right_hand.damage_types.push_back({5, 22, 0});

    const tl::core::CombatSheet sheet = tl::core::combat_sheet(in);
    // 22 (the base, as above) + 26 (the poison, as above).
    TL_CHECK(sheet.melee_max == 48);
    TL_CHECK(sheet.ranged_max == 0);
    // Magic: the larger of the two base-less calls, 26; its low end the smaller
    // non-zero minimum, ceil(26 * 0.5) = 13.
    TL_CHECK(sheet.magic_max == 26);
    TL_CHECK(sheet.magic_min == 13);
    // 15 + ceil(15 * (5 / 100) = 0.75) = 16.
    TL_CHECK(sheet.armour_max == 16);
}

// `damageDefense(type)`: the type's defence effect (`gDAMAGE_DEFENSE_EFFECT_TYPES`,
// 0x28 for poison) raised by the defence percent, rounded up, less the degrade.
TL_TEST(damage_defense_reads_the_type_effect_through_defence) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::CharacterProgression alc;
    alc.init(alchemist(), book);  // defence 5
    tl::core::EffectManager mgr;
    tl::core::EffectManager::Context ctx;
    ctx.graphs = &book;
    mgr.set_context(ctx);
    tl::core::Effect poison;
    poison.type = 0x28;
    poison.values[0] = 10.0f;
    poison.values[1] = 10.0f;
    mgr.add_effect(poison);
    mgr.calculate_effect_values();
    // ceil(0.05 * 10) = 1; 1 + 0 + 10 - 0 = 11. Fire (0x25) has nothing.
    TL_CHECK_EQ(alc.damage_defense(5, &mgr), 11);
    TL_CHECK_EQ(alc.damage_defense(2, &mgr), 0);
    tl::core::Effect degrade;
    degrade.type = tl::core::kEffectTypeDegradeArmorEffect;
    degrade.values[0] = 2.5f;
    degrade.values[1] = 2.5f;
    mgr.add_effect(degrade);
    mgr.calculate_effect_values();
    TL_CHECK_EQ(alc.damage_defense(5, &mgr), 8);   // 11 - ceil(2.5)
    TL_CHECK_EQ(alc.damage_defense(2, &mgr), -3);  // not floored
}

// `AC()` takes `(defense * 0.01f) * armour`: 60 * 0.01f is
// one unit under 0.6f, so 25 of armour adds ceil(14.999999) = 15 - the PC
// build's `25 * (60 / 100)` came to 15.0000009 and added 16.
TL_TEST(attribute_and_armour_operation_order) {
    const tl::core::StatBook book = shipped_curves();
    tl::core::ClassProfile profile = alchemist();
    profile.strength = 25;
    profile.defense = 60;
    profile.armor = 25;
    tl::core::CharacterProgression c;
    c.init(profile, book);

    tl::core::EffectManager mgr;
    tl::core::EffectManager::Context ctx;
    ctx.graphs = &book;
    mgr.set_context(ctx);
    tl::core::Effect pct;
    pct.type = tl::core::kEffectTypePercentStrengthBonus;
    pct.values[0] = 60.0f;
    pct.values[1] = 60.0f;
    mgr.add_effect(pct);
    mgr.calculate_effect_values();

    TL_CHECK(c.strength(&mgr) == 40);
    TL_CHECK(c.armour_class(&mgr) == 40);
}
