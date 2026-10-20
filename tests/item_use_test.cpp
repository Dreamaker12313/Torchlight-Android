// A potion drunk, and what it leaves on the drinker; the fixtures are names and numbers in
// the shape of the shipped data.
#include "core/item_use.h"

#include "core/character.h"

#include <cmath>
#include "test_support.h"

namespace {

using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::EffectTypeTable;
using tl::core::ItemStats;
using tl::core::ItemUseTarget;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmSection plain_row(const std::string& name) {
    AdmSection row;
    row.name = "EFFECT";
    AdmRecord n;
    n.key = "NAME";
    n.type = AdmType::String;
    n.text = name;
    row.records.push_back(n);
    AdmRecord t;
    t.key = "TYPE";
    t.type = AdmType::String;
    t.text = "Value";
    row.records.push_back(t);
    return row;
}

/// The rows this block reads, at their shipped indices.
EffectTypeTable shipped_types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    static const struct {
        int index;
        const char* name;
    } kNamed[] = {
        {0x06, "MANA RECHARGE"},     {0x07, "HP RECHARGE"},          {0x34, "DAMAGE"},
        {0x5a, "POTION EFFICIENCY"}, {0x7b, "MANA RECHARGE PLAYER"}, {0x7c, "HP RECHARGE PLAYER"},
        {0x75, "PERMANENT HEALTH"},  {0x76, "PERMANENT DEFENSE"},    {0x8f, "RESPEC"},
    };
    for (int index = 0; index <= 0x8f; ++index) {
        const char* name = nullptr;
        for (const auto& entry : kNamed) {
            if (entry.index == index) name = entry.name;
        }
        file.root.children.push_back(
            plain_row(name != nullptr ? name : "FILLER " + std::to_string(index)));
    }
    EffectTypeTable table;
    table.load(file);
    return table;
}

/// `POTION`, `PLAYER`, `WEAPON`, `FISH` and `PET`, with their ids and ancestors.
tl::core::UnitTypeHierarchy shipped_unit_types() {
    auto type = [](const std::string& name, const std::vector<uint32_t>& ids) {
        AdmSection group;
        group.name = name;
        for (size_t i = 0; i < ids.size(); ++i) {
            AdmRecord r;
            r.key = i == 0 ? "ID" : "child";
            r.type = AdmType::Integer;
            r.integer = ids[i];
            group.records.push_back(r);
        }
        return group;
    };
    AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("Potion", {33, 1, 31, 2, 0}));
    types.children.push_back(type("Player", {28, 25, 0}));
    types.children.push_back(type("Weapon", {8, 2, 0}));
    types.children.push_back(type("Fish", {124, 123, 1, 31, 2, 0, 125}));
    types.children.push_back(type("Pet", {87, 27, 25, 0}));
    AdmSection root;
    root.children.push_back(types);
    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

/// `HEALTHPOTION.DAT`, with its number standing as it is (no curve).
ItemStats health_potion(float per_tick = 100.0f) {
    ItemStats item;
    item.name = "Health Potion";
    item.unittype = "POTION";
    item.unit_guid = 11;
    item.uses = 1;
    item.max_stack = 20;
    item.dont_use_on_full = true;
    Effect hp;
    hp.name = "HPBONUS";
    hp.type = tl::core::kEffectTypeHpRechargePlayer;
    hp.activation = EffectActivation::Dynamic;
    hp.duration = 4.0f;
    hp.authored_duration = true;
    hp.values[0] = per_tick;
    hp.values[1] = per_tick;
    item.effects.push_back(hp);
    return item;
}

/// `FISH_TROLLFISH.DAT`'s shape: one instant `[EFFECT]` naming a pet unit,
/// `MIN`/`MAX` the seconds - or `TRANSFORM PERMANENT` with 0, the giant fish.
ItemStats transforming_fish(bool permanent) {
    ItemStats item;
    item.name = permanent ? "TrollFish_Giant" : "Troll Fish";
    item.unittype = "FISH";
    item.uses = 1;
    item.max_stack = 5;
    Effect turn;
    turn.name = "Pet Troll";
    turn.type =
        permanent ? tl::core::kEffectTypeTransformPermanent : tl::core::kEffectTypeTransform;
    turn.activation = EffectActivation::Dynamic;
    turn.duration = tl::core::kDurationInstant;
    turn.authored_duration = true;
    turn.values[0] = permanent ? 0.0f : 30.0f;
    turn.values[1] = permanent ? 0.0f : 30.0f;
    item.effects.push_back(turn);
    return item;
}

/// `FISH_FRENZY.DAT`'s shape: two nameless effects of 120 seconds.
ItemStats frenzy_fish() {
    ItemStats item;
    item.name = "Frenzy Fish";
    item.unittype = "FISH";
    item.uses = 1;
    item.max_stack = 5;
    for (const int type : {0x15, 0x16}) {
        Effect buff;
        buff.type = type;
        buff.activation = EffectActivation::Dynamic;
        buff.duration = 120.0f;
        buff.authored_duration = true;
        buff.values[0] = 80.0f;
        buff.values[1] = 80.0f;
        item.effects.push_back(buff);
    }
    return item;
}

struct Fixture {
    EffectTypeTable types = shipped_types();
    tl::core::StatBook graphs;
    tl::core::UnitTypeHierarchy unit_types = shipped_unit_types();
    EffectManager effects;
    ItemUseTarget target;

    Fixture() {
        EffectManager::Context ctx;
        ctx.types = &types;
        ctx.graphs = &graphs;
        ctx.owner.present = true;
        effects.set_context(ctx);
        target.hp = 40.0f;
        target.max_hp = 100.0f;
        target.mana = 10.0f;
        target.max_mana = 50.0f;
        target.effects = &effects;
    }
};

}  // namespace

// `CCharacter::isEffectValidForUnit`, the health branch.
TL_TEST(a_health_potion_is_refused_at_full_health_and_while_one_is_working) {
    Fixture f;
    const ItemStats potion = health_potion();
    TL_CHECK(tl::core::can_use_on_target(potion, f.target, f.unit_types, f.graphs));

    // `HP()` and `maxHP()` compare as integers: 99.6 of 100 is not full.
    f.target.hp = 99.6f;
    TL_CHECK(tl::core::can_use_on_target(potion, f.target, f.unit_types, f.graphs));
    f.target.hp = 100.0f;
    TL_CHECK(!tl::core::can_use_on_target(potion, f.target, f.unit_types, f.graphs));

    // A second one while `HPBONUS` is still on the drinker is refused.
    f.target.hp = 40.0f;
    int uses = potion.uses;
    tl::core::use_on_target(potion, 3, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK(!tl::core::can_use_on_target(potion, f.target, f.unit_types, f.graphs));

    // An item with no `USES` is never usable.
    ItemStats spent = health_potion();
    spent.uses = 0;
    Fixture g;
    TL_CHECK(!tl::core::can_use_on_target(spent, g.target, g.unit_types, g.graphs));
}

// `CEquipment::useOnTarget` and `performItemUse`.
TL_TEST(drinking_puts_the_effect_on_and_takes_one_from_the_stack) {
    Fixture f;
    const ItemStats potion = health_potion();
    int uses = potion.uses;
    const tl::core::ItemUseResult some =
        tl::core::use_on_target(potion, 3, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(some.applied, 1);
    TL_CHECK(some.potion_counted);
    TL_CHECK(some.play_use_sound);
    TL_CHECK(!some.remove_item);  // two left
    TL_CHECK_EQ(uses, 1);         // a use is spent only off the last of a stack
    TL_CHECK(f.effects.has_effect_named("HPBONUS"));

    Fixture g;
    int last_uses = potion.uses;
    const tl::core::ItemUseResult last =
        tl::core::use_on_target(potion, 1, last_uses, g.target, g.unit_types, g.graphs);
    TL_CHECK(last.remove_item);
    TL_CHECK_EQ(last_uses, 0);
}

// `CCharacter::updateHP` through `getEffectValuesOfDamage`:
// a recharge type's value is per 62.5 Hz tick.
TL_TEST(a_drunk_potion_heals_over_its_duration) {
    Fixture f;
    const ItemStats potion = health_potion(100.0f);
    int uses = potion.uses;
    tl::core::use_on_target(potion, 1, uses, f.target, f.unit_types, f.graphs);
    f.effects.calculate_effect_values();
    const tl::core::RechargeRates rates;  // `HP_RECHARGE_RATE` 0.0
    const float per_second = tl::core::hp_change(100.0f, rates, f.effects, 1.0f);
    TL_CHECK(std::fabs(per_second - 100.0f * 0.016f) < 0.001f);
}

// `CCharacter::updateMana`.
TL_TEST(mana_comes_back_at_the_global_rate) {
    Fixture f;
    tl::core::RechargeRates rates;
    rates.mana = 2.2f;
    // 2.2% of an integer maximum per second.
    TL_CHECK(std::fabs(tl::core::mana_change(50.9f, rates, f.effects, 1.0f) - 1.1f) < 0.0001f);
    TL_CHECK(std::fabs(tl::core::mana_change(50.0f, rates, f.effects, 0.5f) - 0.55f) < 0.0001f);
}

// The `POTION EFFICIENCY` scaling after `applyEffectOnUnit`'s copy.
TL_TEST(potion_efficiency_is_applied_twice_as_the_original_does) {
    Fixture f;
    Effect bonus;
    bonus.name = "UNIQUE_POTION_EFFICIENCY";
    bonus.type = tl::core::kEffectTypePotionEfficiency;
    bonus.activation = EffectActivation::Passive;
    bonus.values[0] = 50.0f;
    bonus.values[1] = 50.0f;
    f.effects.add_effect(bonus);
    f.effects.calculate_effect_values();

    const ItemStats potion = health_potion(100.0f);
    int uses = potion.uses;
    tl::core::use_on_target(potion, 1, uses, f.target, f.unit_types, f.graphs);
    f.effects.calculate_effect_values();
    // 100 scaled to 150, then the value again by 1.5: 225 a tick.
    const float per_second =
        tl::core::hp_change(100.0f, tl::core::RechargeRates{}, f.effects, 1.0f);
    TL_CHECK(std::fabs(per_second - 225.0f * 0.016f) < 0.001f);
}

// `CInventory::getEquipmentOfGuid`.
TL_TEST(the_hotbar_takes_the_smallest_stack_of_its_potion) {
    tl::core::Inventory inv;
    const ItemStats potion = health_potion();
    inv.pickup_into_slot(potion, 5, 19);
    inv.pickup_into_slot(potion, 2, 20);
    inv.pickup_into_slot(potion, 2, 21);
    TL_CHECK_EQ(tl::core::equipment_of_guid(inv, 11), 20);
    TL_CHECK_EQ(tl::core::equipment_of_guid(inv, 12), -1);
    TL_CHECK_EQ(tl::core::equipment_of_guid(inv, -1), -1);
}

// `CItem::isUseable`.
TL_TEST(a_potion_is_useable_and_a_weapon_is_not) {
    const tl::core::UnitTypeHierarchy types = shipped_unit_types();
    TL_CHECK(tl::core::item_is_useable(health_potion(), types));
    ItemStats sword;
    sword.unittype = "WEAPON";
    TL_CHECK(!tl::core::item_is_useable(sword, types));
}

namespace {

/// `TOWNPORTALSCROLL.DAT`: one `OPEN TOWN PORTAL`, `DURATION: INSTANT`.
ItemStats town_portal_scroll() {
    ItemStats item;
    item.name = "Town Portal Scroll";
    item.unittype = "SCROLL";
    item.uses = 1;
    item.max_stack = 10;
    Effect portal;
    portal.name = "PORTAL";
    portal.type = tl::core::kEffectTypeOpenTownPortal;
    portal.activation = EffectActivation::Dynamic;
    portal.duration = tl::core::kDurationInstant;
    portal.authored_duration = true;
    portal.values[0] = 1.0f;
    item.effects.push_back(portal);
    return item;
}

}  // namespace

// `isEffectValidForUnit` 0x2d: refused where the level
// forbids portals, and nothing spent.
TL_TEST(a_town_portal_scroll_is_refused_where_portals_are_not_allowed) {
    Fixture f;
    f.target.level_allows_portals = false;
    const ItemStats scroll = town_portal_scroll();
    TL_CHECK(!tl::core::can_use_on_target(scroll, f.target, f.unit_types, f.graphs));
}

// The instant effect is not copied onto the reader but opens the portal.
TL_TEST(a_town_portal_scroll_opens_the_portal) {
    Fixture f;
    f.target.level_allows_portals = true;
    const ItemStats scroll = town_portal_scroll();
    TL_CHECK(tl::core::can_use_on_target(scroll, f.target, f.unit_types, f.graphs));
    int uses = scroll.uses;
    const tl::core::ItemUseResult used =
        tl::core::use_on_target(scroll, 2, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK(used.open_town_portal);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK(used.play_use_sound);
    TL_CHECK(!used.remove_item);
    TL_CHECK(used.open_map_portals.empty());
    TL_CHECK(!f.effects.has_effect_named("PORTAL"));
}

// A map scroll's `OPEN PORTAL`: valid where the level allows portals or is a town;
// `applyInstantEffect` hands the name to `openMapPortal`.
TL_TEST(a_map_scroll_opens_a_portal_to_the_dungeon_its_effect_names) {
    Fixture f;
    ItemStats map;
    map.name = "mine_map_1-10";
    map.unittype = "SCROLL";
    map.uses = 1;
    map.max_stack = 1;
    Effect portal;
    portal.name = "randommapmine1-10";
    portal.type = tl::core::kEffectTypeOpenPortal;
    portal.activation = EffectActivation::Dynamic;
    portal.duration = tl::core::kDurationInstant;
    portal.authored_duration = true;
    map.effects.push_back(portal);

    // The town portal's flag is not the one asked.
    f.target.level_allows_portals = true;
    f.target.level_allows_map_portals = false;
    TL_CHECK(!tl::core::can_use_on_target(map, f.target, f.unit_types, f.graphs));

    f.target.level_allows_map_portals = true;
    TL_CHECK(tl::core::can_use_on_target(map, f.target, f.unit_types, f.graphs));
    int uses = map.uses;
    const tl::core::ItemUseResult used =
        tl::core::use_on_target(map, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK_EQ(used.open_map_portals.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(used.open_map_portals[0], std::string("randommapmine1-10"));
    TL_CHECK(!used.open_town_portal);
    // The last use of a single scroll: it leaves the bag.
    TL_CHECK(used.remove_item);
    TL_CHECK(!f.effects.has_effect_named("randommapmine1-10"));
}

// `isEffectValidForUnit` 0x40 and 0x50.
TL_TEST(a_stun_is_refused_to_the_stunned_and_a_turn_to_all_but_a_free_enemy) {
    Fixture f;
    Effect stun;
    stun.type = tl::core::kEffectTypeStun;
    TL_CHECK(tl::core::effect_valid_for_unit(f.target, nullptr, stun, 1.0f, f.unit_types));
    f.target.stunned = true;
    TL_CHECK(!tl::core::effect_valid_for_unit(f.target, nullptr, stun, 1.0f, f.unit_types));

    Effect turn;
    turn.type = tl::core::kEffectTypeTurnAlignment;
    // The player, whatever its alignment.
    f.target.alignment = 2;
    TL_CHECK(!tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
    f.target.unittype = "MONSTER";
    TL_CHECK(tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
    // Not one that is dying, nor one of another alignment.
    f.target.dying_or_dead = true;
    TL_CHECK(!tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
    f.target.dying_or_dead = false;
    f.target.alignment = 0;
    TL_CHECK(!tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
    // A monster's pet turns; a player's does not.
    f.target.alignment = 2;
    f.target.has_master = true;
    TL_CHECK(tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
    f.target.master_is_player = true;
    TL_CHECK(!tl::core::effect_valid_for_unit(f.target, nullptr, turn, 1.0f, f.unit_types));
}

namespace {

/// A potion with one `DYNAMIC`, `INSTANT` effect.
ItemStats instant_potion(const char* name, int type, float value) {
    ItemStats item;
    item.name = name;
    item.unittype = "POTION";
    item.uses = 1;
    item.max_stack = 10;
    Effect effect;
    effect.name = name;
    effect.type = type;
    effect.activation = EffectActivation::Dynamic;
    effect.duration = tl::core::kDurationInstant;
    effect.authored_duration = true;
    effect.values[0] = value;
    effect.values[1] = value;
    item.effects.push_back(effect);
    return item;
}

}  // namespace

// `applyInstantEffect` 0x75 and 0x76: a player's `+0x41c` and `+0x430` gain the
// value; nothing is copied onto the drinker.
TL_TEST(permanent_potions_raise_the_players_fields_for_good) {
    Fixture f;
    tl::core::CharacterProgression sheet;
    f.target.character = &sheet;
    const int defense = sheet.defense();

    ItemStats lich = instant_potion("LICHHEALTH", tl::core::kEffectTypePermanentHealth, 25.0f);
    int uses = lich.uses;
    tl::core::ItemUseResult used =
        tl::core::use_on_target(lich, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK(used.maxima_changed);
    TL_CHECK(used.remove_item);
    TL_CHECK_EQ(sheet.permanent_hp(), 25);
    TL_CHECK(!f.effects.has_effect_named("LICHHEALTH"));
    // `maxHP()`: at least 1 under it, plus the bonus.
    TL_CHECK_EQ(sheet.max_hp_of(nullptr), 25);

    ItemStats brom = instant_potion("ROUGHHIDE", tl::core::kEffectTypePermanentDefense, 2.0f);
    uses = brom.uses;
    used = tl::core::use_on_target(brom, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(sheet.defense(), defense + 2);
    TL_CHECK_EQ(sheet.saved().defense, defense + 2);

    // `RESPEC` is reported for the caller's `resetSkills`.
    ItemStats respec = instant_potion("RESPECPOTION", tl::core::kEffectTypeRespec, 0.0f);
    uses = respec.uses;
    used = tl::core::use_on_target(respec, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK(used.respec);

    // Not a player (`isPlayer`): nothing.
    f.target.unittype = "WEAPON";
    uses = lich.uses;
    used = tl::core::use_on_target(lich, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(sheet.permanent_hp(), 25);
    uses = respec.uses;
    TL_CHECK(!tl::core::use_on_target(respec, 1, uses, f.target, f.unit_types, f.graphs).respec);
}

// `(int)(value + (float)field)` and the float `+0x440`, kept across a save.
TL_TEST(permanent_bonuses_truncate_and_survive_a_save) {
    tl::core::CharacterProgression sheet;
    sheet.add_permanent_hp(2.9f);
    sheet.add_permanent_hp(2.9f);
    TL_CHECK_EQ(sheet.permanent_hp(), 4);  // (int)(2.9 + 2) = 4
    sheet.add_permanent_mp(1.5f);
    sheet.add_permanent_mp(1.25f);
    TL_CHECK(sheet.permanent_mp() == 2.75f);
    sheet.award_stat_points(5.0f);
    TL_CHECK_EQ(sheet.unspent_stat_points(), 5);
    tl::core::CharacterProgression back;
    back.apply_saved(sheet.saved(), tl::core::StatBook{});
    TL_CHECK_EQ(back.permanent_hp(), 4);
    TL_CHECK(back.permanent_mp() == 2.75f);
    // `maxMana()`: `(int)(+0x43c + +0x440)`, at least 1.
    TL_CHECK_EQ(back.max_mp_of(nullptr), 2);
}

// `CEquipment::canUseOnTarget`: `PETONLY` is for a unit with a master.
// `useOnTarget` -> `applyEffectOnUnit` -> `TRANSFORM`,
// `TRANSFORM PERMANENT` (-999).
TL_TEST(a_fish_is_for_a_pet_alone_and_its_transformation_is_the_callers) {
    Fixture f;
    const ItemStats fish = transforming_fish(false);
    // The player is no pet.
    TL_CHECK(!tl::core::can_use_on_target(fish, f.target, f.unit_types, f.graphs));

    f.target.unittype = "PET";
    f.target.pet = true;
    TL_CHECK(tl::core::can_use_on_target(fish, f.target, f.unit_types, f.graphs));
    int uses = fish.uses;
    const tl::core::ItemUseResult used =
        tl::core::use_on_target(fish, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(used.applied, 1);
    TL_CHECK_EQ(used.transforms.size(), static_cast<size_t>(1));
    TL_CHECK(used.transforms[0].unit == "Pet Troll");
    TL_CHECK_EQ(used.transforms[0].duration, 30.0f);
    TL_CHECK(!used.potion_counted);
    TL_CHECK(used.play_use_sound);
    TL_CHECK(used.remove_item);
    TL_CHECK(f.effects.live_effects_of_type(tl::core::kEffectTypeTransform).empty());

    Fixture g;
    g.target.unittype = "PET";
    g.target.pet = true;
    const ItemStats giant = transforming_fish(true);
    int giant_uses = giant.uses;
    const tl::core::ItemUseResult for_good =
        tl::core::use_on_target(giant, 2, giant_uses, g.target, g.unit_types, g.graphs);
    TL_CHECK_EQ(for_good.transforms.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(for_good.transforms[0].duration, -999.0f);
    TL_CHECK(!for_good.remove_item);  // one left of the two
}

// The other fish are buffs: each effect is copied onto the pet
// (`CBaseUnit::copyEffect`) and nothing is transformed.
TL_TEST(a_buff_fish_puts_its_effects_on_the_pet) {
    Fixture f;
    f.target.unittype = "PET";
    f.target.pet = true;
    const ItemStats fish = frenzy_fish();
    TL_CHECK(tl::core::can_use_on_target(fish, f.target, f.unit_types, f.graphs));
    int uses = fish.uses;
    const tl::core::ItemUseResult used =
        tl::core::use_on_target(fish, 1, uses, f.target, f.unit_types, f.graphs);
    TL_CHECK_EQ(used.applied, 2);
    TL_CHECK(used.transforms.empty());
    TL_CHECK_EQ(f.effects.live_effects_of_type(0x15).size(), static_cast<size_t>(1));
    TL_CHECK_EQ(f.effects.live_effects_of_type(0x16).size(), static_cast<size_t>(1));
}
