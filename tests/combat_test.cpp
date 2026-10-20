// The weapon half of `rollAttack` and the item roll behind it:
// `CEquipment::calculateCombatStats`, `setGraphDamage`,
// `CCharacter::maxDamage`, `damageDefense`, `rollAttack`.
#include "core/combat.h"
#include "core/item_effects.h"
#include "core/random.h"
#include "core/stats.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

using tl::core::Blow;
using tl::core::Defender;
using tl::core::ItemStats;
using tl::core::RunicRandom;
using tl::core::StatBook;
using tl::core::StatGraph;
using tl::core::Strike;
using tl::core::UnitCombat;
using tl::core::UnitStats;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

namespace {

AdmSection flat_graph(const std::string& name, float value) {
    AdmSection line;
    line.name = "LINE";
    AdmRecord n;
    n.key = "NAME";
    n.type = AdmType::String;
    n.text = name;
    line.records.push_back(n);
    AdmRecord infer;
    infer.key = "INFER_PASSED_END";
    infer.type = AdmType::Bool;
    infer.integer = 1u;
    line.records.push_back(infer);
    for (float x : {1.0f, 2.0f}) {
        AdmSection p;
        p.name = "POINT";
        AdmRecord px;
        px.key = "X";
        px.type = AdmType::Float;
        px.number = x;
        AdmRecord py;
        py.key = "Y";
        py.type = AdmType::Float;
        py.number = value;
        p.records.push_back(px);
        p.records.push_back(py);
        line.children.push_back(p);
    }
    return line;
}

/// `BASE_WEAPON_DAMAGE` is 21 at level 1 in the shipped curve.
StatBook weapon_graphs() {
    StatBook book;
    book.add("BASE_WEAPON_DAMAGE", StatGraph::from(flat_graph("Base_Weapon_Damage", 21.0f)));
    book.add("ARMOR_TEST", StatGraph::from(flat_graph("Armor_Test", 10.0f)));
    return book;
}

/// The Moldy Staff as `masterresourceunits.dat` has it.
ItemStats moldy_staff() {
    ItemStats staff;
    staff.name = "Moldy Staff";
    staff.level = 1;
    staff.min_damage = 110.0f;
    staff.max_damage = 120.0f;
    staff.rarity_damage_mod = 80;
    staff.speed_damage_mod = 115;
    staff.damage_split = {0, 0, 0, 0, 0, 100, 0};
    return staff;
}

// The console's moldy staff (CEquipment::calculateCombatStats,
// setGraphDamage, the split): a 23 comes out as 22 poison
// (100 * 0.23f is 22.999998). Kept.
int console_staff_poison(int roll) {
    const int scaled =
        static_cast<int>((static_cast<float>(roll) * (115.0f * 0.01f)) * (80.0f * 0.01f));
    const int damage = static_cast<int>(std::ceil((static_cast<float>(scaled) * 21.0f) * 0.01f));
    return static_cast<int>(100.0f * (static_cast<float>(damage) * 0.01f));
}

}  // namespace

// One `MINDAMAGE..MAXDAMAGE` draw through the curve, all of it poison.
TL_TEST(the_moldy_staff_rolls_one_figure_and_it_is_all_poison) {
    for (uint32_t seed : {1u, 2u, 77u, 1234u}) {
        ItemStats staff = moldy_staff();
        RunicRandom rolls(seed);
        RunicRandom mirror(seed);
        tl::core::roll_weapon_damage(staff, /*is_weapon=*/true, weapon_graphs(), rolls);

        const int roll = mirror.integer_between(110, 120);
        const int expected = console_staff_poison(roll);
        TL_CHECK(staff.combat_rolled);
        TL_CHECK_EQ(staff.damage, 0);
        TL_CHECK_EQ(staff.inherent_damage[5], expected);
        TL_CHECK(22 <= staff.inherent_damage[5] && staff.inherent_damage[5] <= 24);
        // `getDamageBonus`: the type is on the copy's list and its
        // inherent amount comes back; a type it never met gives 0.
        const tl::core::EffectManager::Context no_effects;
        TL_CHECK_EQ(staff.damage_types.size(), 1u);
        TL_CHECK_EQ(staff.damage_types[0], 5);
        TL_CHECK_EQ(tl::core::weapon_damage_bonus(staff, 5, no_effects), expected);
        TL_CHECK_EQ(tl::core::weapon_damage_bonus(staff, 2, no_effects), 0);
        // One draw and no more.
        TL_CHECK_EQ(rolls.state(), mirror.state());
    }
}

// Unauthored `DAMAGE_PHYSICAL` (-1) keeps the whole figure physical; an heirloom takes the
// top of the range and 10% more per level, at most five.
TL_TEST(physical_damage_defaults_to_the_whole_figure_and_a_save_overrides_it) {
    ItemStats sword = moldy_staff();
    sword.damage_split = {-1, 0, 0, 0, 0, 0, 0};
    sword.rarity_damage_mod = 100;
    sword.speed_damage_mod = 100;
    RunicRandom rolls(5u);
    tl::core::roll_weapon_damage(sword, true, weapon_graphs(), rolls, /*heirloom=*/2);
    // 120 + 2 * 10 = 140% of 21 = 29.4 -> 30.
    TL_CHECK_EQ(sword.damage, 30);
    TL_CHECK_EQ(sword.inherent_damage[5], 0);

    // The figure a save keeps (`+0x314`) is the one before `DAMAGE_PHYSICAL`.
    TL_CHECK_EQ(sword.kept_damage, 30);
    sword.damage_split = {50, 0, 0, 0, 0, 0, 0};
    sword.damage = 7;
    tl::core::split_weapon_damage(sword, true);
    TL_CHECK_EQ(sword.kept_damage, 7);
    TL_CHECK_EQ(sword.damage, 3);

    // `MAXDAMAGE` 0: no draw, no curve.
    ItemStats shield;
    RunicRandom quiet(5u);
    RunicRandom mirror(5u);
    tl::core::roll_weapon_damage(shield, false, weapon_graphs(), quiet);
    TL_CHECK_EQ(shield.damage, 0);
    TL_CHECK_EQ(quiet.state(), mirror.state());
}

/// `a Buckled Chest` as `masterresourceunits.dat` has it, over a flat
/// `ARMOR_PLAYER_BYLEVEL_FORSET` of 11 (its level-1 value).
ItemStats buckled_chest() {
    ItemStats chest;
    chest.name = "a Buckled Chest";
    chest.level = 1;
    chest.armor_min = 30;
    chest.armor_max = 38;
    chest.rarity_armor_mod = 130;
    chest.special_armor_mod = 135;
    chest.armor_split = {83, 0, 17, 0, 0, 0, 0};
    return chest;
}

StatBook armour_graphs() {
    StatBook book = weapon_graphs();
    book.add(tl::core::kPlayerArmorGraph,
             StatGraph::from(flat_graph("Armor_Player_ByLevel_ForSet", 11.0f)));
    return book;
}

// `calculateCombatStats`' armour half: one volatile draw, `setGraphAC`'s curve, then the
// elemental effects off that figure and `ARMOR_PHYSICAL` replacing it.
TL_TEST(armour_rolls_through_the_curve_then_physical_replaces_it) {
    for (uint64_t seed : {1u, 7u, 12345u}) {
        ItemStats chest = buckled_chest();
        RunicRandom rolls(seed);
        RunicRandom mirror(seed);
        tl::core::roll_item_armour(chest, /*is_armor=*/true, /*from_save=*/false, armour_graphs(),
                                   rolls);
        const int roll = mirror.integer_between(30, 38);
        const int scaled =
            static_cast<int>(static_cast<float>(roll) * (130.0f / 100.0f) * (135.0f / 100.0f));
        const int points =
            static_cast<int>(std::ceil(11.0f * (static_cast<float>(scaled) / 100.0f)));
        TL_CHECK(6 <= points && points <= 8);
        TL_CHECK_EQ(chest.graph_ac,
                    static_cast<int>((83.0f / 100.0f) * static_cast<float>(points)));
        // `ARMOR_FIRE` 17: one passive of type 0x25 worth 17% of the points
        // before the physical share replaced them.
        TL_CHECK_EQ(chest.combat_effects.size(), 1u);
        TL_CHECK_EQ(chest.combat_effects[0].type, 0x25);
        TL_CHECK(chest.combat_effects[0].activation == tl::core::EffectActivation::Passive);
        TL_CHECK(chest.combat_effects[0].duration == tl::core::kDurationAlways);
        TL_CHECK(chest.combat_effects[0].values[0] ==
                 (17.0f / 100.0f) * static_cast<float>(points));
        TL_CHECK_EQ(rolls.state(), mirror.state());
    }
}

// A loaded copy gets no elemental effects (its saved effects come back
// instead); anything not an `Armor` keeps the rolled figure whole.
TL_TEST(armour_effects_are_for_a_fresh_armour_copy_only) {
    ItemStats loaded = buckled_chest();
    RunicRandom a(3u);
    tl::core::roll_item_armour(loaded, true, /*from_save=*/true, armour_graphs(), a);
    TL_CHECK(loaded.combat_effects.empty());

    ItemStats ring = buckled_chest();
    ring.armor = 10;  // a fixed figure: no draw
    RunicRandom quiet(3u);
    RunicRandom mirror(3u);
    tl::core::roll_item_armour(ring, /*is_armor=*/false, false, armour_graphs(), quiet);
    // trunc(10 * 1.3 * 1.35) = 17; ceil(11 * 0.17) = 2, and no physical share.
    TL_CHECK_EQ(ring.graph_ac, 2);
    TL_CHECK(ring.combat_effects.empty());
    TL_CHECK_EQ(quiet.state(), mirror.state());
}

// No `ARMOR` and no roll leaves 0, which an authored `ARMOR_PHYSICAL` then
// raises to the floor of 1; an heirloom adds 10 points per level, five at most.
TL_TEST(armour_floor_and_heirloom_steps) {
    ItemStats bare;
    bare.armor_split[0] = 83;
    RunicRandom rolls(3u);
    tl::core::roll_item_armour(bare, true, false, armour_graphs(), rolls);
    TL_CHECK_EQ(bare.graph_ac, 1);
    ItemStats plain;
    tl::core::roll_item_armour(plain, true, false, armour_graphs(), rolls);
    TL_CHECK_EQ(plain.graph_ac, 0);

    ItemStats heirloom;
    heirloom.armor = 100;
    heirloom.rarity_armor_mod = 100;
    heirloom.special_armor_mod = 100;
    tl::core::roll_item_armour(heirloom, false, false, armour_graphs(), rolls, /*heirloom=*/9);
    // (100 + 5 * 10)% of 11 = 16.5 -> 17.
    TL_CHECK_EQ(heirloom.graph_ac, 17);
}

// Both halves: the armour draw comes before the damage draw.
TL_TEST(combat_stats_draw_armour_before_damage) {
    ItemStats both = moldy_staff();
    both.armor_min = 30;
    both.armor_max = 38;
    RunicRandom rolls(11u);
    RunicRandom mirror(11u);
    tl::core::calculate_combat_stats(both, true, false, false, armour_graphs(), rolls);
    mirror.integer_between(30, 38);
    const int roll = mirror.integer_between(110, 120);
    TL_CHECK_EQ(both.inherent_damage[5], console_staff_poison(roll));
    TL_CHECK_EQ(rolls.state(), mirror.state());
}

// `setLevel`'s `ARMOR_<TYPE>` loop runs off the armour before a physical
// share replaces it: a Ratlin's 60% poison of 10 armour is 6.
TL_TEST(elemental_armour_is_a_share_of_the_armour_before_physical_replaces_it) {
    UnitStats ratlin;
    ratlin.armor = 100;
    ratlin.armor_graph = "ARMOR_TEST";
    ratlin.armor_by_type[5] = 60;
    ratlin.armor_by_type[4] = 30;
    RunicRandom rolls(3u);
    UnitCombat combat =
        tl::core::set_level(ratlin, 1, tl::core::difficulty::kNormal, weapon_graphs(), rolls);
    TL_CHECK_EQ(combat.armor, 10);
    TL_CHECK_EQ(tl::core::damage_defense(combat, 5, 0), 6);
    TL_CHECK_EQ(tl::core::damage_defense(combat, 4, 0), 3);
    TL_CHECK_EQ(tl::core::damage_defense(combat, 2, 0), 0);
    // `defense()` raises it by its percent, rounded up.
    TL_CHECK_EQ(tl::core::damage_defense(combat, 5, 50), 9);

    ratlin.armor_physical = 50;
    RunicRandom again(3u);
    combat = tl::core::set_level(ratlin, 1, tl::core::difficulty::kNormal, weapon_graphs(), again);
    TL_CHECK_EQ(combat.armor, 5);
    TL_CHECK_EQ(combat.damage_defense[0], 0);
    TL_CHECK_EQ(tl::core::damage_defense(combat, 5, 0), 6);
}

// `setLevel`'s `DAMAGE_<TYPE>` loop: a share of the maximum damage per type, the
// physical one in place of the maximum.
TL_TEST(a_monsters_damage_by_type_is_a_share_of_its_maximum) {
    UnitStats shocker;
    shocker.min_damage = 50.0f;
    shocker.max_damage = 200.0f;
    shocker.damage_graph = "ARMOR_TEST";  // a flat 10
    RunicRandom rolls(3u);
    UnitCombat combat =
        tl::core::set_level(shocker, 1, tl::core::difficulty::kNormal, weapon_graphs(), rolls);
    TL_CHECK_EQ(combat.attack.max_damage, 20);
    TL_CHECK_EQ(combat.attack.min_damage, 5);
    for (int type = 1; type < tl::core::kDamageTypeCount; ++type) {
        TL_CHECK_EQ(combat.attack.damage_by_type[static_cast<size_t>(type)], 0);
    }

    shocker.damage_by_type[0] = 50;  // DAMAGE_PHYSICAL
    shocker.damage_by_type[4] = 75;  // DAMAGE_ELECTRIC
    shocker.damage_by_type[2] = 33;  // DAMAGE_FIRE
    RunicRandom again(3u);
    combat = tl::core::set_level(shocker, 1, tl::core::difficulty::kNormal, weapon_graphs(), again);
    TL_CHECK_EQ(combat.attack.max_damage, 10);
    TL_CHECK_EQ(combat.attack.min_damage, 5);
    TL_CHECK_EQ(combat.attack.damage_by_type[4], 15);
    TL_CHECK_EQ(combat.attack.damage_by_type[2], 7);  // ceil(6.6)
    TL_CHECK_EQ(combat.attack.damage_by_type[3], 0);

    // The blow: physical 5..10, then fire 4..7 and electric 8..15 on top,
    // against a defender with nothing.
    Defender bare;
    bare.max_hp = 1000;
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        RunicRandom draw(seed);
        const Blow blow = tl::core::roll_attack(combat, bare, draw);
        if (blow.blocked || blow.critical) continue;
        TL_CHECK(5 + 4 + 8 <= blow.damage && blow.damage <= 10 + 7 + 15);
    }
}

// `CCharacter::defense()`: a unit that is not a player keeps the
// constructor's 10, so its armour stands a tenth higher.
TL_TEST(a_units_base_defense_is_the_constructors_ten) {
    TL_CHECK_EQ(tl::core::kCharacterBaseDefense, 10);
    TL_CHECK_EQ(tl::core::unit_defense(nullptr), 10);
    UnitCombat combat;
    combat.armor = 100;
    combat.damage_defense[5] = 40;
    TL_CHECK_EQ(tl::core::armor_class(combat, tl::core::unit_defense(nullptr), nullptr), 110);
    TL_CHECK_EQ(tl::core::damage_defense(combat, 5, tl::core::unit_defense(nullptr)), 44);
    combat.armor = 15;
    TL_CHECK_EQ(tl::core::armor_class(combat, tl::core::unit_defense(nullptr), nullptr),
                17);  // ceil(1.5)
}

// The staff's blow: no physical figure still lands 1 through armour; the poison part rolls
// half to all, less half to all of the target's scaled poison defence.
TL_TEST(a_poison_weapon_blow_rolls_through_the_elemental_loops) {
    Strike strike;
    strike.weapon = true;
    strike.share = 6.0f;         // the Alchemist's strength
    strike.magic_share = 15.0f;  // and a magic of 15 (raised_bonus takes each times 0.01f)
    strike.max_physical = 0;
    strike.bonus[5] = 23;
    Defender ratlin;
    ratlin.armor = 10;
    ratlin.max_hp = 20;
    ratlin.damage_defense[5] = 6;

    int checked = 0;
    for (uint32_t seed = 1; seed < 60 && checked < 10; ++seed) {
        RunicRandom rolls(seed);
        RunicRandom mirror(seed);
        const Blow blow = tl::core::roll_attack(strike, ratlin, rolls);

        // The mirror, draw by draw.
        const bool critical = tl::core::roll_critical(mirror);
        if (critical) continue;         // the ordinary blow only
        mirror.integer_between(0, 0);   // physical: ceil(0 * 0.5)..0
        mirror.integer_between(5, 10);  // its armour roll
        const int top = 23 + static_cast<int>(std::ceil(23.0f * (0.15f + 0.06f)));  // 28
        TL_CHECK_EQ(top, 28);
        const int part = mirror.integer_between(14, 28);
        const int resist = mirror.integer_between(3, 6);
        const int poison = part - static_cast<int>(static_cast<float>(resist) * 1.0f *
                                                   (static_cast<float>(part) / 28.0f));
        TL_CHECK(!blow.critical);
        TL_CHECK(!blow.glancing);
        if (!blow.blocked) TL_CHECK_EQ(blow.damage, 1 + poison);
        ++checked;
    }
    TL_CHECK(checked > 0);
}

// A monster's blow is unchanged by the generalisation: with no bonuses the
// loops draw nothing.
TL_TEST(a_blow_with_no_bonuses_draws_as_the_monster_blow_did) {
    UnitCombat monster;
    monster.attack.max_damage = 12;
    Defender player;
    player.armor = 4;
    player.max_hp = 200;
    for (uint32_t seed : {3u, 9u, 21u}) {
        RunicRandom a(seed);
        RunicRandom b(seed);
        const Blow one = tl::core::roll_attack(monster, player, a);
        Strike strike;
        strike.max_physical = 12;
        const Blow two = tl::core::roll_attack(strike, player, b);
        TL_CHECK_EQ(one.damage, two.damage);
        TL_CHECK_EQ(a.state(), b.state());
    }
}

namespace {

// A manager holding passive effects of the given types and values.
tl::core::EffectManager manager_with(const StatBook& book,
                                     const std::vector<std::pair<int, float>>& effects) {
    tl::core::EffectManager manager;
    tl::core::EffectManager::Context context;
    context.graphs = &book;
    context.owner.present = true;
    context.owner.level = 1;
    manager.set_context(context);
    for (const auto& entry : effects) {
        tl::core::Effect effect;
        effect.type = entry.first;
        effect.values[0] = entry.second;
        effect.values[1] = entry.second;
        manager.add_effect(effect);
    }
    manager.calculate_effect_values();
    return manager;
}

}  // namespace

// `rollAttack`: a critical is the maximum, its half, and `PERCENT CRITICAL
// DAMAGE` of the half; `CRITICAL CHANCE` joins `rollCritical`'s draw.
TL_TEST(a_critical_takes_the_critical_damage_percent_of_its_half) {
    Defender bare;
    bare.armor = 0;
    bare.max_hp = 1000;
    Strike plain;
    plain.max_physical = 100;
    Strike skilled = plain;
    skilled.critical_percent = 12.0f;
    int criticals = 0;
    for (uint32_t seed = 1; seed < 400 && criticals < 3; ++seed) {
        RunicRandom a(seed);
        RunicRandom b(seed);
        const Blow one = tl::core::roll_attack(plain, bare, a);
        const Blow two = tl::core::roll_attack(skilled, bare, b);
        TL_CHECK_EQ(a.state(), b.state());
        if (!one.critical || one.blocked) continue;
        TL_CHECK_EQ(one.damage, 150);
        TL_CHECK_EQ(two.damage, 156);  // 100 + 50 + (int)(50 * 0.12)
        ++criticals;
    }
    TL_CHECK(criticals > 0);

    // A chance of 96 turns every draw of 1..100 into a critical.
    Strike certain = plain;
    certain.critical_chance = 96.0f;
    for (uint32_t seed : {2u, 5u, 11u}) {
        RunicRandom rolls(seed);
        TL_CHECK(tl::core::roll_attack(certain, bare, rolls).critical);
        RunicRandom draw(seed);
        TL_CHECK(tl::core::roll_critical(draw, 96.0f));
    }
}

// `maxDamage` and `armorBonus` read the master's `PERCENT PET DAMAGE`
// and `PERCENT PET ARMOR`, for the pet and every summoned minion.
TL_TEST(a_masters_pet_bonuses_raise_its_minions_damage_and_armour) {
    const StatBook book;
    UnitCombat minion;
    minion.attack.max_damage = 50;
    minion.armor = 100;
    const tl::core::EffectManager master = manager_with(book, {{0x5b, 20.0f}, {0x61, 8.0f}});
    TL_CHECK_EQ(tl::core::max_damage(minion, nullptr), 50);
    TL_CHECK_EQ(tl::core::max_damage(minion, nullptr, &master), 60);
    const int defense = tl::core::unit_defense(nullptr);
    TL_CHECK_EQ(tl::core::armor_class(minion, defense, nullptr), 110);
    // 100 + ceil(100 * 0.08) = 108, then a tenth: ceil(10.8) = 11.
    TL_CHECK_EQ(tl::core::armor_class(minion, defense, nullptr, &master), 119);

    // The minion's own `PERCENT DAMAGE BONUS` of all (Burning Bind) joins the
    // master's share before the one `ceilf`.
    const tl::core::EffectManager enraged = manager_with(book, {{0x19, 50.0f}});
    TL_CHECK_EQ(tl::core::max_damage(minion, &enraged), 75);
    TL_CHECK_EQ(tl::core::max_damage(minion, &enraged, &master), 85);
}

// `maxDamage` with a weapon: the attribute, the dual-wielding bonus
// and the weapon category's bonus are one share; the flat bonuses follow.
TL_TEST(max_damage_reads_the_weapon_category_and_dual_wielding_bonuses) {
    const StatBook book;
    const tl::core::EffectManager held =
        manager_with(book, {{0x67, 4.0f}, {99, 7.0f}, {0x66, 9.0f}, {0x58, 6.0f}, {0, 3.0f}});
    tl::core::WeaponTraits staff;
    staff.held = true;
    staff.magic_item = true;
    // 100 + ceil(100 * (0.10 strength + 0.04 magic item)) + 3 flat.
    TL_CHECK_EQ(tl::core::max_damage(100, 0, 10, &held, staff, {}, 1.0f), 117);
    staff.dual = true;
    TL_CHECK_EQ(tl::core::max_damage(100, 0, 10, &held, staff, {}, 1.0f), 123);
    tl::core::WeaponTraits sword;
    sword.held = true;
    sword.martial = true;
    sword.ranged_item = true;  // not read on the melee branch
    TL_CHECK_EQ(tl::core::max_damage(100, 0, 10, &held, sword, {}, 1.0f), 120);
    tl::core::WeaponTraits pistol;
    pistol.held = true;
    pistol.ranged = true;
    pistol.ranged_item = true;
    pistol.martial = true;  // not read on the ranged branch
    // The ranged branch takes `RANGEDDAMAGEBONUS` (1), which nothing here holds.
    TL_CHECK_EQ(tl::core::max_damage(100, 0, 10, &held, pistol, {}, 1.0f), 119);
}

// `rollAttack`: a flat `DAMAGE BONUS` opens its type's loop even on a weapon
// without it; `PERCENT DAMAGE BONUS` joins every type's share.
TL_TEST(strike_effects_fill_what_rollattack_asks_of_the_attacker) {
    const StatBook book;
    const tl::core::EffectManager held =
        manager_with(book, {{0x59, 12.0f}, {0x37, 1.0f}, {0x67, 4.0f}, {0xf, 5.0f}});
    Strike strike;
    tl::core::WeaponTraits staff;
    staff.held = true;
    staff.magic_item = true;
    tl::core::strike_effects(strike, &held, staff);
    TL_CHECK_EQ(strike.critical_percent, 12.0f);
    TL_CHECK_EQ(strike.critical_chance, 1.0f);
    TL_CHECK_EQ(strike.item_percent[2], 4.0f);
    TL_CHECK(strike.item_kind[2] && !strike.item_kind[0]);
    TL_CHECK_EQ(strike.item_percent[0], 0.0f);
    TL_CHECK_EQ(strike.attack_percent, 5.0f);
    TL_CHECK_EQ(strike.dual_percent, 0.0f);

    // The poison staff of the test above with the magic-item bonus: the top is
    // 23 + ceil(23 * (0.05 + 0.06 + 0.04 + 0.15)) = 30.
    strike.weapon = true;
    strike.share = 6.0f;
    strike.magic_share = 15.0f;
    strike.bonus[5] = 23;
    strike.critical_chance = 0.0f;
    Defender bare;
    bare.max_hp = 1000;
    for (uint32_t seed = 1; seed < 40; ++seed) {
        RunicRandom rolls(seed);
        const Blow blow = tl::core::roll_attack(strike, bare, rolls);
        if (blow.critical || blow.blocked) continue;
        TL_CHECK(blow.damage >= 1 + 15 && blow.damage <= 1 + 30);
    }
}

// `applyInstantEffect`'s `DAMAGE`: block, bonus, critical, defence (at least 1),
// then the force draw, in that order.
TL_TEST(an_instant_damage_effect_goes_through_block_bonus_critical_and_defence) {
    const StatBook book;
    Defender ratlin;
    ratlin.armor = 10;
    ratlin.max_hp = 500;
    ratlin.damage_defense[2] = 20;

    // No attacker: the block draw, the defence roll, the force draw.
    for (uint32_t seed : {1u, 7u, 19u}) {
        RunicRandom rolls(seed);
        RunicRandom mirror(seed);
        const tl::core::InstantDamage hit =
            tl::core::instant_damage(100.0f, 2, 0.0f, 0.0f, ratlin, nullptr, rolls);
        mirror.integer_between(0, 100);  // `blocked()`, chance 0
        const int resist = mirror.integer_between(10, 20);
        mirror.between(0.0f, 0.0f);
        TL_CHECK(!hit.blocked);
        TL_CHECK(!hit.critical);
        TL_CHECK_EQ(hit.damage, 100 - resist);
        TL_CHECK_EQ(rolls.state(), mirror.state());
    }

    // The sign of the value does not matter, and a blow the defence swallows is 1.
    {
        RunicRandom a(3);
        RunicRandom b(3);
        TL_CHECK_EQ(tl::core::instant_damage(100.0f, 2, 0.0f, 0.0f, ratlin, nullptr, a).damage,
                    tl::core::instant_damage(-100.0f, 2, 0.0f, 0.0f, ratlin, nullptr, b).damage);
        Defender wall = ratlin;
        wall.damage_defense[2] = 100000;
        RunicRandom c(3);
        TL_CHECK_EQ(tl::core::instant_damage(5.0f, 2, 0.0f, 0.0f, wall, nullptr, c).damage, 1);
    }

    // An attacker with +50% fire damage and a certain critical worth +12%:
    // -100 -> ceil(-50) + -100 = -150; half -75, 12% of it -9, so -234.
    const tl::core::EffectManager held =
        manager_with(book, {{0x19, 50.0f}, {0x37, 96.0f}, {0x59, 12.0f}});
    const tl::core::CharacterEffects attacker(&held);
    Defender bare;
    bare.max_hp = 500;
    bare.interrupt_resistance = 200.0f;
    RunicRandom rolls(11);
    const tl::core::InstantDamage hit =
        tl::core::instant_damage(100.0f, 0, 1.0f, 4.0f, bare, &attacker, rolls);
    TL_CHECK(hit.critical);
    TL_CHECK(!hit.interrupts);  // the resistance is above every draw of 0..100
    TL_CHECK_EQ(hit.damage, 234);
    TL_CHECK(hit.force >= 1.0f && hit.force <= 4.0f);

    // A certain block stops everything after one draw.
    Defender shield = bare;
    shield.block_chance = 50;
    int blocks = 0;
    for (uint32_t seed = 1; seed < 40; ++seed) {
        RunicRandom draw(seed);
        RunicRandom mirror(seed);
        const tl::core::InstantDamage stopped =
            tl::core::instant_damage(100.0f, 2, 0.0f, 0.0f, shield, nullptr, draw);
        if (!stopped.blocked) continue;
        mirror.integer_between(0, 100);
        TL_CHECK_EQ(stopped.damage, 0);
        TL_CHECK_EQ(draw.state(), mirror.state());
        ++blocks;
    }
    TL_CHECK(blocks > 0);
}

// `applyDamageEffects`: an elemental part decides its status affix on one draw
// against the target's defence; a critical forces it; a type with no affix ends the walk.
TL_TEST(elemental_damage_leaves_its_status_affix) {
    std::array<int, tl::core::kDamageTypeCount> own{};
    std::array<int, tl::core::kDamageTypeCount> parts{};
    RunicRandom rolls(5);
    const uint32_t before = rolls.state();

    parts[0] = 40;
    TL_CHECK_EQ(tl::core::roll_damage_status(40.0f, parts, 1, 0, own, true, false, rolls).type, -1);
    TL_CHECK_EQ(rolls.state(), before);  // nothing drawn

    parts[5] = 30;
    const tl::core::DamageStatus poison =
        tl::core::roll_damage_status(70.0f, parts, 2, 0, own, true, false, rolls);
    TL_CHECK_EQ(poison.type, 5);
    TL_CHECK(std::string(poison.affix) == "DAMAGEEFFECTPOISONED");
    TL_CHECK_EQ(poison.duration_min, 3.0f);
    TL_CHECK_EQ(poison.duration_max, 5.0f);
    TL_CHECK_EQ(rolls.state(), before);  // forced: still nothing drawn

    // Fire comes before poison in the walk, and a player's affix is the twin.
    parts[2] = 10;
    const tl::core::DamageStatus fire =
        tl::core::roll_damage_status(80.0f, parts, 3, 0, own, true, true, rolls);
    TL_CHECK_EQ(fire.type, 2);
    TL_CHECK(std::string(fire.affix) == "DAMAGEEFFECTONFIREPLAYER");
    TL_CHECK_EQ(fire.duration_max, 7.0f);
    const tl::core::DamageStatus ice =
        tl::core::roll_damage_status(10.0f, {0, 0, 0, 10, 0, 0, 0}, 1, 3, own, true, true, rolls);
    TL_CHECK_EQ(ice.duration_min, 1.5f);  // a player thaws sooner
    TL_CHECK_EQ(
        tl::core::roll_damage_status(10.0f, {0, 0, 0, 10, 0, 0, 0}, 1, 3, own, true, false, rolls)
            .duration_max,
        4.0f);

    // Magical damage (1) has no affix and ends the walk before fire is asked.
    TL_CHECK_EQ(
        tl::core::roll_damage_status(20.0f, {0, 10, 10, 0, 0, 0, 0}, 2, 1, own, true, false, rolls)
            .type,
        -1);

    // Unforced: the chance is damage / ((defence + damage) * 5), a fifth at
    // best, and one draw per type asked.
    int passed = 0;
    for (uint32_t seed = 1; seed <= 400; ++seed) {
        RunicRandom draw(seed);
        RunicRandom mirror(seed);
        const tl::core::DamageStatus status = tl::core::roll_damage_status(
            30.0f, {0, 0, 0, 0, 0, 30, 0}, 1, 5, own, false, false, draw);
        const bool expect = mirror.between(0.0f, 1.0f) <= 30.0f / ((0.0f + 30.0f) * 5.0f);
        TL_CHECK_EQ(status.type == 5, expect);
        TL_CHECK_EQ(draw.state(), mirror.state());
        if (status.type == 5) ++passed;
    }
    TL_CHECK(passed > 40 && passed < 120);
}

// `rollAttack`: `INTERRUPT CHANCE` (a way out below 0) and the target's
// `INTERRUPT RESISTANCE`, which the last 1..100 draw has to beat.
TL_TEST(interrupt_chance_and_resistance_decide_the_interrupt) {
    Defender target;
    target.max_hp = 100000;
    target.armor = 0;
    Strike strike;
    strike.max_physical = 100;
    Strike certain = strike;
    certain.interrupt_chance = 200.0f;
    Defender stubborn = target;
    stubborn.interrupt_resistance = 100.0f;
    for (uint32_t seed = 1; seed < 30; ++seed) {
        RunicRandom a(seed);
        RunicRandom b(seed);
        const Blow yes = tl::core::roll_attack(certain, target, a);
        const Blow no = tl::core::roll_attack(certain, stubborn, b);
        TL_CHECK(yes.interrupts);
        TL_CHECK(!no.interrupts);
    }
}

TL_TEST(a_blow_reports_what_each_damage_type_dealt) {
    Strike strike;
    strike.weapon = true;
    strike.max_physical = 10;
    strike.bonus[5] = 20;
    Defender bare;
    bare.max_hp = 1000;
    for (uint32_t seed = 1; seed < 20; ++seed) {
        RunicRandom rolls(seed);
        const Blow blow = tl::core::roll_attack(strike, bare, rolls);
        if (blow.blocked) continue;
        TL_CHECK_EQ(blow.parts, 2);
        TL_CHECK_EQ(blow.by_type[0] + blow.by_type[5], blow.damage);
        TL_CHECK(blow.by_type[5] > 0);
    }
}

// `setLevel(level, true)`'s attack half: a player's reads plain `DAMAGE_MONSTER`
// whatever `DAMAGE_GRAPH` and the difficulty say.
TL_TEST(the_players_own_attack_reads_damage_monster) {
    StatBook book = weapon_graphs();
    book.add("DAMAGE_MONSTER", StatGraph::from(flat_graph("Damage_Monster", 40.0f)));
    book.add("DAMAGE_MONSTER_HARD", StatGraph::from(flat_graph("Damage_Monster_Hard", 80.0f)));
    UnitStats alchemist;
    alchemist.min_damage = 20.0f;  // ALCHEMIST.DAT's MINDAMAGE and MAXDAMAGE
    alchemist.max_damage = 20.0f;

    const tl::core::AttackDescription own =
        tl::core::own_attack(alchemist, 1, tl::core::difficulty::kHard, true, book);
    TL_CHECK_EQ(own.max_damage, 8);  // ceil(40 * 0.2)
    TL_CHECK_EQ(own.min_damage, 8);
    // A monster on the same numbers reads the difficulty's curve...
    TL_CHECK_EQ(
        tl::core::own_attack(alchemist, 1, tl::core::difficulty::kHard, false, book).max_damage,
        16);
    // ... or its own `DAMAGE_GRAPH`, which a player ignores.
    alchemist.damage_graph = "ARMOR_TEST";
    TL_CHECK_EQ(
        tl::core::own_attack(alchemist, 1, tl::core::difficulty::kHard, false, book).max_damage, 2);
    TL_CHECK_EQ(
        tl::core::own_attack(alchemist, 1, tl::core::difficulty::kHard, true, book).max_damage, 8);

    RunicRandom rolls(5u);
    const UnitCombat combat =
        tl::core::set_level(alchemist, 1, tl::core::difficulty::kHard, book, rolls);
    TL_CHECK_EQ(combat.attack.max_damage, 2);
    TL_CHECK_EQ(combat.attack.min_damage, 2);
}
