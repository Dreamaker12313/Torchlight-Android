// The player's pet and the `CMonster` branches for it: `idleAI`, `idleAINormal`,
// `huntAI`, `fleeAI`, `isPetNearDeath`,
// `sendToTown`, `returnToTownAI`, `updateInTownTimer`,
// `updateAnimation`'s speed match.
#include "core/pet.h"

#include "core/character.h"
#include "core/collision.h"
#include "core/effect_manager.h"
#include "core/navigation.h"
#include "core/passability.h"
#include "core/quest.h"
#include "core/random.h"
#include "core/script.h"
#include "core/stats.h"
#include "test_seeds.h"
#include "test_support.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace tl::core;

StatGraph curve(const char* name, float y_at_one, float y_at_two) {
    tl::formats::AdmSection line;
    line.name = "LINE";
    tl::formats::AdmRecord id;
    id.key = "NAME";
    id.type = tl::formats::AdmType::String;
    id.text = name;
    line.records.push_back(id);
    for (int x = 1; x <= 2; ++x) {
        tl::formats::AdmSection point;
        point.name = "POINT";
        tl::formats::AdmRecord px;
        px.key = "X";
        px.type = tl::formats::AdmType::Float;
        px.number = static_cast<float>(x);
        tl::formats::AdmRecord py;
        py.key = "Y";
        py.type = tl::formats::AdmType::Float;
        py.number = x == 1 ? y_at_one : y_at_two;
        point.records.push_back(px);
        point.records.push_back(py);
        line.children.push_back(point);
    }
    return StatGraph::from(line);
}

/// The dog, as `media/units/monsters/Dog/DOG.DAT.adm` authors it.
UnitStats dog_stats() {
    UnitStats stats;
    stats.unittype = "PET";
    stats.display_name = "Dog";
    stats.radius = 0.4f;
    stats.movement.walking_speed = 1.4f;
    stats.movement.running_speed = 6.5f;
    stats.sight_radius = 10.0f;
    stats.alignment = UnitStats::Alignment::Friendly;
    stats.invincible = true;
    stats.invincible_flag = true;
    stats.can_match_speed = true;
    stats.health_graph = "Health_Minion_byLevel";
    return stats;
}

UnitStats ratlin_stats() {
    UnitStats stats;
    stats.radius = 0.4f;
    stats.movement.walking_speed = 1.0f;
    stats.movement.running_speed = 3.0f;
    stats.sight_radius = 0.0f;
    stats.motion_radius = 0.0f;
    stats.alignment = UnitStats::Alignment::Hostile;
    stats.health_graph = "Health_Minion_byLevel";
    return stats;
}

/// A flat level of the given half-size with the dog (-2001) on it, the
/// player's pet at `stance`.
struct PetLevel {
    CollisionWorld world;
    PassabilityMap grid;
    NavGrid nav;
    UnitBook units;
    StatBook graphs;
    LevelScript level;
    QuestState quests;
    std::vector<ScriptEvent> events;

    explicit PetLevel(const Vec3& dog_at, int32_t stance = 2, const UnitStats& dog = dog_stats()) {
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {-80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, 80.0f});
        world.add_triangle({-80.0f, 0.0f, -80.0f}, {80.0f, 0.0f, 80.0f}, {80.0f, 0.0f, -80.0f});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
        graphs.add("HEALTH_MINION_BYLEVEL", curve("Health_Minion_byLevel", 200.0f, 210.0f));
        units.add("dog", dog);
        units.add("ratlin", ratlin_stats());
        level.set_units(&units);
        level.set_stat_graphs(&graphs);
        level.set_navigation(&nav);
        level.set_collision(&world);
        level.set_passability(&grid);
        tl::test::seed_generators();
        LevelScript::reset_near_list_timer();
        level.build(tl::formats::CmpScene{}, {});
        level.add_spawned_unit(-2001, "dog", dog_at, false);
        level.make_player_pet(-2001, quests, events);
        level.set_unit_stance(-2001, stance);
        level.set_player_speeds(1.4f, 6.25f);
    }

    void run(const Vec3& player, float seconds) {
        const int frames = static_cast<int>(seconds * 60.0f + 0.5f);
        for (int i = 0; i < frames; ++i) {
            events.clear();
            level.update(1.0f / 60.0f, player, quests, events);
        }
    }

    float distance_to(const Vec3& at) const {
        Vec3 feet;
        level.unit_position(-2001, feet);
        const float dx = feet.x - at.x;
        const float dz = feet.z - at.z;
        return std::sqrt(dx * dx + dz * dz);
    }
};

}  // namespace

/// The console's `rand()`, which `idleAI`'s re-pick rolls on.
///
TL_TEST(crt_rand_matches_the_consoles_c_library) {
    MsvcRand crt(1);
    TL_CHECK_EQ(crt.next(), 41);
    TL_CHECK_EQ(crt.next(), 18467);
    TL_CHECK_EQ(crt.next(), 6334);
}

TL_TEST(pet_state_folds_the_ai_state) {
    TL_CHECK(pet_state_of_ai(2) == PetState::Following);
    TL_CHECK(pet_state_of_ai(4) == PetState::Attacking);
    TL_CHECK(pet_state_of_ai(0xd) == PetState::Attacking);
    TL_CHECK(pet_state_of_ai(10) == PetState::Fleeing);
    TL_CHECK(pet_state_of_ai(0x29) == PetState::Departing);
    TL_CHECK(pet_state_of_ai(0x2a) == PetState::InTown);
}

/// `idleAI`: any stance but Passive takes on the master's own target
/// when it is an enemy within 8 of the master - and Passive never does.
TL_TEST(a_pet_takes_on_its_masters_target_within_eight_unless_passive) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    for (const int32_t stance : {2, 1, 0}) {
        PetLevel at({1.0f, 0.0f, 1.0f}, stance);
        at.level.add_spawned_unit(-1001, "ratlin", Vec3{5.0f, 0.0f, 0.0f}, false);
        at.level.set_player_attack_target(&at.level, -1001);
        at.run(player, 0.2f);
        LevelScript* machine = nullptr;
        int64_t node = 0;
        const bool targets = at.level.unit_target(-2001, machine, node) && node == -1001;
        if (stance == 0) {
            TL_CHECK(!targets);
            TL_CHECK(pet_state_of_ai(at.level.unit_ai_state(-2001)) != PetState::Attacking);
        } else {
            TL_CHECK(targets);
            TL_CHECK(pet_state_of_ai(at.level.unit_ai_state(-2001)) == PetState::Attacking);
        }
    }
    // Nine from the master: not taken.
    PetLevel far({1.0f, 0.0f, 1.0f}, 2);
    far.level.add_spawned_unit(-1001, "ratlin", Vec3{9.0f, 0.0f, 0.0f}, false);
    far.level.set_player_attack_target(&far.level, -1001);
    far.run(player, 0.2f);
    LevelScript* machine = nullptr;
    int64_t node = 0;
    TL_CHECK(!(far.level.unit_target(-2001, machine, node) && node == -1001));
}

/// `idleAINormal`: only an Aggressive pet looks for its own fight, within 35 of its
/// master (the level's list is built after a second, `CLevel::update`'s `UpdateTimer`).
TL_TEST(an_aggressive_pet_finds_its_own_fight_but_not_past_the_leash) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    UnitStats rooted = dog_stats();
    rooted.movement.walking_speed = 0.0f;
    rooted.movement.running_speed = 0.0f;
    const auto hunts = [&](const Vec3& dog_at, const Vec3& ratlin_at, int32_t stance) {
        PetLevel at(dog_at, stance, rooted);
        // Wide enough that the ratlin is on the level's list wherever it is.
        at.level.set_unit_active_range(100.0f);
        at.level.add_spawned_unit(-1001, "ratlin", ratlin_at, false);
        at.run(player, 1.3f);
        LevelScript* machine = nullptr;
        int64_t node = 0;
        return at.level.unit_target(-2001, machine, node) && node == -1001;
    };
    TL_CHECK(hunts({2.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 1));
    // Defensive: the same ratlin is nothing to it.
    TL_CHECK(!hunts({2.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 2));
    // Aggressive, the ratlin 4 from the dog but 37 from the master.
    TL_CHECK(hunts({30.0f, 0.0f, 0.0f}, {34.0f, 0.0f, 0.0f}, 1));
    TL_CHECK(!hunts({33.0f, 0.0f, 0.0f}, {37.0f, 0.0f, 0.0f}, 1));
}

/// `isPetNearDeath`: at its last hit point the pet flees to its master, near death under 15%;
/// healed, it comes out on `fleeAI`'s 75% roll.
TL_TEST(a_pet_at_its_last_hit_point_flees_to_its_master) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    PetLevel at({3.0f, 0.0f, 0.0f}, 2);
    float hp = 0.0f;
    float max = 0.0f;
    at.level.unit_health(-2001, hp, max);
    TL_CHECK(max >= 200.0f);
    TL_CHECK(!at.level.unit_near_death(-2001));
    at.level.set_unit_hp(-2001, 1.0f);
    TL_CHECK(at.level.unit_near_death(-2001));
    at.run(player, 0.2f);
    TL_CHECK_EQ(at.level.unit_ai_state(-2001), ai_state::kFlee);
    at.run(player, 3.0f);
    TL_CHECK(at.distance_to(player) <= 16.0f * 1.5f + 1.0f);
    // 10% of its health while fleeing: still near death.
    at.level.set_unit_hp(-2001, max * 0.1f);
    TL_CHECK(at.level.unit_near_death(-2001));
    // Healed: it leaves the flee.
    at.level.set_unit_hp(-2001, max);
    TL_CHECK(!at.level.unit_near_death(-2001));
    at.run(player, 20.0f);
    TL_CHECK(at.level.unit_ai_state(-2001) != ai_state::kFlee);
}

/// `sendToTown`: `min(monster level * 10, 120)` less the `PET_TOWN_RETURN` share, at least 1;
/// refused on a town level.
TL_TEST(the_town_trip_goes_and_comes_back) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    {
        PetLevel at({2.0f, 0.0f, 0.0f});
        TL_CHECK(!at.level.send_unit_to_town(-2001, 0.0f, 0.0f, /*level_is_town=*/true, at.quests,
                                             at.events));
        at.level.set_level_monster_level(3);
        TL_CHECK(at.level.send_unit_to_town(-2001, 50.0f, 0.0f, false, at.quests, at.events));
        TL_CHECK(std::fabs(at.level.unit_town_timer(-2001) - 15.0f) < 1e-4f);
    }
    {
        PetLevel at({2.0f, 0.0f, 0.0f});
        at.level.set_level_monster_level(20);
        TL_CHECK(at.level.send_unit_to_town(-2001, 0.0f, 0.0f, false, at.quests, at.events));
        TL_CHECK(std::fabs(at.level.unit_town_timer(-2001) - 120.0f) < 1e-4f);
    }
    {
        PetLevel at({2.0f, 0.0f, 0.0f});
        at.level.set_level_monster_level(1);
        at.level.set_unit_hp(-2001, 50.0f);
        TL_CHECK(at.level.send_unit_to_town(-2001, 0.0f, 0.0f, false, at.quests, at.events));
        TL_CHECK_EQ(at.level.unit_ai_state(-2001), ai_state::kGoingToTown);
        bool away = false;
        bool back = false;
        for (int i = 0; i < 60 * 20 && !back; ++i) {
            at.events.clear();
            at.level.update(1.0f / 60.0f, player, at.quests, at.events);
            if (at.level.unit_ai_state(-2001) == ai_state::kInTown) away = true;
            for (const int64_t id : at.level.take_units_back_from_town())
                back = back || id == -2001;
        }
        TL_CHECK(away);
        TL_CHECK(back);
        float hp = 0.0f;
        float max = 0.0f;
        at.level.unit_health(-2001, hp, max);
        TL_CHECK_EQ(hp, max);
        const float d = at.distance_to(player);
        TL_CHECK(d >= 18.0f - 0.5f && d <= 25.0f + 0.5f);
    }
}

/// `updateInTownTimer`: during a cinematic a run-out clock is put back to 0.1 s.
TL_TEST(the_town_clock_waits_out_a_cinematic) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    PetLevel at({2.0f, 0.0f, 0.0f});
    at.level.set_level_monster_level(1);
    TL_CHECK(at.level.send_unit_to_town(-2001, 0.0f, 0.0f, false, at.quests, at.events));
    for (int i = 0; i < 60 * 9 && at.level.unit_ai_state(-2001) != ai_state::kInTown; ++i) {
        at.events.clear();
        at.level.update(1.0f / 60.0f, player, at.quests, at.events);
    }
    TL_CHECK_EQ(at.level.unit_ai_state(-2001), ai_state::kInTown);
    at.level.set_ui_cinematic(true);
    at.run(player, 15.0f);
    TL_CHECK(at.level.take_units_back_from_town().empty());
    TL_CHECK_EQ(at.level.unit_ai_state(-2001), ai_state::kInTown);
    const float left = at.level.unit_town_timer(-2001);
    TL_CHECK(0.0f < left && left <= 0.1f);
    at.level.set_ui_cinematic(false);
    at.run(player, 0.5f);
    bool back = false;
    for (const int64_t id : at.level.take_units_back_from_town()) back = back || id == -2001;
    TL_CHECK(back);
    TL_CHECK(at.level.unit_ai_state(-2001) != ai_state::kInTown);
}

/// `idleAINormal`: the Aggressive search also wants a game being played and
/// `PLAYERNOTARGET` off.
TL_TEST(an_aggressive_pet_looks_for_no_fight_outside_a_game_or_under_playernotarget) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    UnitStats rooted = dog_stats();
    rooted.movement.walking_speed = 0.0f;
    rooted.movement.running_speed = 0.0f;
    const auto hunts = [&](bool playing, bool untargetable) {
        PetLevel at({2.0f, 0.0f, 0.0f}, 1, rooted);
        at.level.set_unit_active_range(100.0f);
        at.level.set_game_playing(playing);
        at.level.set_player_untargetable(untargetable);
        at.level.add_spawned_unit(-1001, "ratlin", Vec3{6.0f, 0.0f, 0.0f}, false);
        at.run(player, 1.3f);
        LevelScript* machine = nullptr;
        int64_t node = 0;
        return at.level.unit_target(-2001, machine, node) && node == -1001;
    };
    TL_CHECK(hunts(true, false));
    TL_CHECK(!hunts(false, false));
    TL_CHECK(!hunts(true, true));
}

/// `CLevel::findCharacterWithinView`: a pet near death is passed over.
TL_TEST(a_pet_near_death_is_no_ones_target) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    UnitStats rooted = dog_stats();
    rooted.movement.walking_speed = 0.0f;
    rooted.movement.running_speed = 0.0f;
    UnitStats hunter = ratlin_stats();
    hunter.sight_radius = 10.0f;
    hunter.motion_radius = 4.5f;
    const auto hunted = [&](bool near_death) {
        PetLevel at({30.0f, 0.0f, 0.0f}, 2, rooted);
        at.units.add("hunter", hunter);
        at.level.set_unit_active_range(100.0f);
        at.level.set_game_playing(true);
        at.level.add_spawned_unit(-1001, "hunter", Vec3{33.0f, 0.0f, 0.0f}, false);
        if (near_death) at.level.set_unit_hp(-2001, 1.0f);
        at.run(player, 6.0f);
        LevelScript* machine = nullptr;
        int64_t node = 0;
        return at.level.unit_target(-1001, machine, node) && node == -2001;
    };
    TL_CHECK(hunted(false));
    TL_CHECK(!hunted(true));
}

/// `CCharacter::updateMana`: the base rate, then `MANA RECHARGE` and
/// `MANA RECHARGE PLAYER` effects, a second each.
TL_TEST(a_pets_mana_takes_its_effects_recharge) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    // `media/EffectsList.dat`'s row index is the effect type.
    tl::formats::AdmFile list;
    list.ok = true;
    list.root.name = "EFFECTLIST";
    for (int index = 0; index <= 0x7b; ++index) {
        tl::formats::AdmSection row;
        row.name = "EFFECT";
        tl::formats::AdmRecord name;
        name.key = "NAME";
        name.type = tl::formats::AdmType::String;
        name.text = index == 6      ? "MANA RECHARGE"
                    : index == 0x7b ? "MANA RECHARGE PLAYER"
                                    : "FILLER " + std::to_string(index);
        tl::formats::AdmRecord type;
        type.key = "TYPE";
        type.type = tl::formats::AdmType::String;
        type.text = "Value";
        row.records.push_back(name);
        row.records.push_back(type);
        list.root.children.push_back(row);
    }
    EffectTypeTable types;
    types.load(list);
    const StatBook no_graphs;
    EffectManager::Context context;
    context.types = &types;
    context.graphs = &no_graphs;
    context.owner.present = true;
    context.owner.level = 1;

    const auto recharge = [](int type, float value) {
        Effect effect;
        effect.name = "RECHARGE";
        effect.type = type;
        effect.values[0] = value;
        effect.values[1] = value;
        effect.duration = kDurationAlways;
        effect.activation = EffectActivation::Dynamic;
        return effect;
    };

    PetLevel at({2.0f, 0.0f, 0.0f});
    LevelScript::UnitSkills hooks;
    hooks.load = [](const std::string&) { return std::vector<LevelScript::UnitSkill>{}; };
    hooks.mana_recharge_rate = 0.0f;
    at.level.set_unit_skills(hooks);
    auto effects = std::make_shared<EffectManager>();
    effects->set_context(context);
    at.level.set_unit_effects(-2001, effects);

    float mana = 0.0f;
    float max = 0.0f;
    TL_CHECK(at.level.unit_mana(-2001, mana, max));
    // `calculateMaxMana`: the level times 5, plus 20.
    TL_CHECK_EQ(max, 25.0f);
    TL_CHECK_EQ(mana, 25.0f);

    // A drain of 4 a second: 21 left after one.
    effects->add_effect(recharge(6, -4.0f));
    effects->calculate_effect_values();
    at.run(player, 1.0f);
    TL_CHECK(at.level.unit_mana(-2001, mana, max));
    TL_CHECK(std::fabs(mana - 21.0f) < 0.05f);

    // 3 a second back from each of the two types: a net +2, 23 after one.
    effects->add_effect(recharge(6, 3.0f));
    effects->add_effect(recharge(0x7b, 3.0f));
    effects->calculate_effect_values();
    at.run(player, 1.0f);
    TL_CHECK(at.level.unit_mana(-2001, mana, max));
    TL_CHECK(std::fabs(mana - 23.0f) < 0.05f);

    // And never past the pool (`modifyMana`).
    at.run(player, 5.0f);
    TL_CHECK(at.level.unit_mana(-2001, mana, max));
    TL_CHECK_EQ(mana, 25.0f);
}

/// `CANMATCHSPEED`: a slower running pet moves at its master's running speed + 2.
TL_TEST(a_pet_that_matches_speed_runs_at_its_masters_pace_plus_two) {
    const Vec3 player{0.0f, 0.0f, 0.0f};
    const auto travelled = [&](bool match) {
        UnitStats dog = dog_stats();
        dog.can_match_speed = match;
        PetLevel at({0.0f, 0.0f, 20.0f}, 0, dog);
        Vec3 before;
        // Far enough that it runs straight back; one think starts it.
        at.run(player, 0.2f);
        at.level.unit_position(-2001, before);
        at.run(player, 0.5f);
        Vec3 after;
        at.level.unit_position(-2001, after);
        const float dx = after.x - before.x;
        const float dz = after.z - before.z;
        return std::sqrt(dx * dx + dz * dz);
    };
    const float matched = travelled(true);
    const float own = travelled(false);
    TL_CHECK(matched > 0.5f * 6.5f * 1.1f);
    TL_CHECK(own < 0.5f * 6.5f * 1.05f);
    TL_CHECK(matched > own);
}

/// `ceil(PRICE_PLAYERSELL_<quality>(level) * VALUE/100)` per item, times the stack.
TL_TEST(pet_sells_its_bag_at_the_authored_price) {
    StatBook graphs;
    graphs.add("PRICE_PLAYERSELL_NORMAL", curve("Price_PlayerSell_Normal", 15.0f, 16.0f));
    graphs.add("PRICE_PLAYERSELL_MAGIC", curve("Price_PlayerSell_Magic", 45.0f, 48.0f));
    graphs.add("PRICE_PLAYERSELL_UNIQUE", curve("Price_PlayerSell_Unique", 180.0f, 191.0f));

    ItemStats potion;
    potion.name = "Health Potion";
    potion.unittype = "POTION";
    potion.level = 1;
    potion.value = 50;
    potion.max_stack = 20;
    TL_CHECK_EQ(item_sell_price(potion, graphs, 1), 8);
    TL_CHECK_EQ(item_sell_price(potion, graphs, 4), 32);

    // An item that authors no `VALUE` is 100% of the curve, not worthless -
    // the same default `MINDAMAGE` takes.
    ItemStats staff;
    staff.name = "Moldy Staff";
    staff.unittype = "NORMAL STAFF";
    staff.level = 1;
    TL_CHECK_EQ(item_sell_price(staff, graphs, 1), 15);

    // Quality is the first word of `UNITTYPE`, and it picks the curve.
    ItemStats rare = staff;
    rare.unittype = "UNIQUE SWORD";
    TL_CHECK_EQ(item_sell_price(rare, graphs, 1), 180);

    // And the bag sells into a total (`updateInTownTimer`): a quest
    // unit (`getIsQuestUnit`: a `QUEST_GUID` other than -1) stays.
    UnitTypeHierarchy unit_types;
    Pet pet;
    pet.spawn("Dog", dog_stats(), Vec3{}, 0.0f);
    pet.bag().add_item(potion, 4, unit_types);
    pet.bag().add_item(staff, 1, unit_types);
    ItemStats wanted = staff;
    wanted.name = "Wanted Staff";
    wanted.quest_guid = 77;
    pet.bag().add_item(wanted, 1, unit_types);
    int sold = 0;
    const int gold = pet.sell_bag(
        [&](const ItemStats& item, int count) { return item_sell_price(item, graphs, count); },
        [](const ItemStats& item) { return item.quest_guid != -1; }, sold);
    TL_CHECK_EQ(sold, 2);
    TL_CHECK_EQ(gold, 47);
    TL_CHECK_EQ(pet.bag().size(), static_cast<size_t>(1));
    TL_CHECK_EQ(pet.bag().count_by_name("Wanted Staff"), 1);
}

/// The stance cycles the way the one button a pad can spare has to make it.
TL_TEST(pet_stance_cycles) {
    Pet pet;
    TL_CHECK(pet.stance() == kDefaultStance);
    pet.set_stance(PetStance::Aggressive);
    pet.cycle_stance();
    TL_CHECK(pet.stance() == PetStance::Defensive);
    pet.cycle_stance();
    TL_CHECK(pet.stance() == PetStance::Passive);
    pet.cycle_stance();
    TL_CHECK(pet.stance() == PetStance::Aggressive);
}

/// `CPlayer::levelResetting`: each pet is `teleportToMaster(count * 0.25)`, which
/// for a pet on its master is `randomOpenPosition(8)`: open ground.
TL_TEST(an_arriving_pet_lands_on_open_ground_beside_its_master) {
    CollisionWorld world;
    world.add_triangle({-1.0f, 0.0f, -40.0f}, {-1.0f, 0.0f, 40.0f}, {1.0f, 0.0f, 40.0f});
    world.add_triangle({-1.0f, 0.0f, -40.0f}, {1.0f, 0.0f, 40.0f}, {1.0f, 0.0f, -40.0f});
    world.build();
    PassabilityMap grid;
    grid.bake(world, nullptr);
    NavGrid nav;
    nav.build(grid);
    UnitBook units;
    units.add("dog", dog_stats());
    StatBook graphs;
    graphs.add("HEALTH_MINION_BYLEVEL", curve("Health_Minion_byLevel", 200.0f, 210.0f));
    LevelScript level;
    level.set_units(&units);
    level.set_stat_graphs(&graphs);
    level.set_navigation(&nav);
    level.set_collision(&world);
    level.set_passability(&grid);
    tl::test::seed_generators();
    level.build(tl::formats::CmpScene{}, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    const Vec3 master{0.0f, 0.0f, 0.0f};
    for (int64_t id = -2001; id > -2006; --id) {
        level.add_spawned_unit(id, "dog", master, false);
        level.make_player_pet(id, quests, events);
        TL_CHECK(level.put_pet_by_master(id, master, 5 * 0.25f));
        Vec3 at;
        TL_CHECK(level.unit_position(id, at));
        TL_CHECK(grid.map_passable(at));
        TL_CHECK(std::fabs(at.x) <= 1.0f);
        TL_CHECK(std::sqrt(at.x * at.x + at.z * at.z) <= 8.0f);
    }
    // Not one of the player's: left where it is.
    level.add_spawned_unit(-2010, "dog", master, false);
    TL_CHECK(!level.put_pet_by_master(-2010, master, 1.0f));
}

/// `CCharacter::awardExperience` adds the player's award to each pet's;
/// `CCharacter::levelUp` one level an award; a loss floors at 0.
TL_TEST(a_pet_is_awarded_its_masters_experience_and_levels_on_it) {
    PetLevel at({2.0f, 0.0f, 0.0f});
    at.graphs.add("EXPERIENCEGATE", curve("ExperienceGate", 100.0f, 300.0f));
    std::vector<int64_t> levelled;
    int level = 0;
    int32_t experience = 0;
    float hp = 0.0f;
    float max = 0.0f;
    at.level.unit_health(-2001, hp, max);
    const float max_at_one = max;

    at.level.set_unit_hp(-2001, 5.0f);
    at.level.award_pet_experience(60, levelled);
    TL_CHECK(levelled.empty());
    TL_CHECK(at.level.unit_level(-2001, level) && level == 1);
    TL_CHECK(at.level.unit_experience(-2001, experience) && experience == 60);

    at.level.award_pet_experience(60, levelled);
    TL_CHECK(levelled.size() == 1 && levelled[0] == -2001);
    TL_CHECK(at.level.unit_level(-2001, level) && level == 2);
    TL_CHECK(at.level.unit_experience(-2001, experience) && experience == 120);
    at.level.unit_health(-2001, hp, max);
    TL_CHECK(max_at_one < max);
    TL_CHECK(hp == max);

    // The curve has two points: level 2 is the cap.
    levelled.clear();
    at.level.award_pet_experience(1000, levelled);
    TL_CHECK(levelled.empty());
    TL_CHECK(at.level.unit_level(-2001, level) && level == 2);
    TL_CHECK(at.level.unit_experience(-2001, experience) && experience == 300);

    at.level.award_pet_experience(-500, levelled);
    TL_CHECK(at.level.unit_experience(-2001, experience) && experience == 0);

    // Not one of the player's: nothing.
    at.level.add_spawned_unit(-1001, "ratlin", Vec3{6.0f, 0.0f, 0.0f}, false);
    at.level.award_pet_experience(50, levelled);
    TL_CHECK(at.level.unit_experience(-1001, experience) && experience == 0);
}
