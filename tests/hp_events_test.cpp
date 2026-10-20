// Tests for unit and player HP threshold event broadcasting.
#include "core/hp_events.h"
#include "core/logic_events.h"
#include "core/quest.h"
#include "core/script.h"
#include "core/stats.h"
#include "test_support.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using tl::core::GraphType;
using tl::core::LevelScript;
using tl::core::QuestState;
using tl::core::ScriptAction;
using tl::core::ScriptEvent;
using tl::core::StatBook;
using tl::core::StatGraph;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;
namespace logic = tl::core::logic;

CmpField text_field(const std::string& text) {
    const size_t units = text.size() + 1;
    const size_t dwords = (units * 2 + 3) / 4;
    std::vector<uint8_t> payload(dwords * 4, 0);
    for (size_t i = 0; i < text.size(); ++i) {
        payload[2 * i] = static_cast<uint8_t>(text[i]);
    }
    CmpField f;
    f.words.resize(dwords);
    for (size_t i = 0; i < dwords; ++i) {
        f.words[i] = static_cast<uint32_t>(payload[4 * i]) |
                     (static_cast<uint32_t>(payload[4 * i + 1]) << 8) |
                     (static_cast<uint32_t>(payload[4 * i + 2]) << 16) |
                     (static_cast<uint32_t>(payload[4 * i + 3]) << 24);
    }
    return f;
}

CmpField uint_field(uint32_t value) {
    CmpField f;
    f.words.push_back(value);
    return f;
}

CmpField vec3_field(float x, float y, float z) {
    CmpField f;
    for (float v : {x, y, z}) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        f.words.push_back(bits);
    }
    return f;
}

CmpNode& add_node(CmpScene& scene, uint32_t type, int64_t id, const std::string& name) {
    CmpNode node;
    node.type = type;
    node.id = id;
    node.name = name;
    scene.nodes.push_back(std::move(node));
    return scene.nodes.back();
}

}  // namespace

// ------------------------------------------------------------------ unit broadcaster

TL_TEST(a_blow_through_one_mark_fires_it) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 9999999.0f, -10.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK_EQ(out.size(), size_t{1});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kHp90Pct));
}

TL_TEST(a_blow_through_several_marks_fires_the_lowest_only) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 9999999.0f, -90.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK_EQ(out.size(), size_t{1});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kHp10Pct));
}

TL_TEST(fire_all_fires_every_mark_crossed_in_the_originals_order) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 9999999.0f, -90.0f,
                                       /*fire_all=*/true, out);
    TL_CHECK_EQ(out.size(), size_t{9});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kHp90Pct));
    TL_CHECK_EQ(out[1], static_cast<uint32_t>(logic::kHp80Pct));
    TL_CHECK_EQ(out[2], static_cast<uint32_t>(logic::kHp70Pct));
    TL_CHECK_EQ(out[3], static_cast<uint32_t>(logic::kHp60Pct));
    TL_CHECK_EQ(out[4], static_cast<uint32_t>(logic::kHp50Pct));
    TL_CHECK_EQ(out[5], static_cast<uint32_t>(logic::kHp40Pct));
    TL_CHECK_EQ(out[6], static_cast<uint32_t>(logic::kHp30Pct));
    TL_CHECK_EQ(out[7], static_cast<uint32_t>(logic::kHp20Pct));
    TL_CHECK_EQ(out[8], static_cast<uint32_t>(logic::kHp10Pct));
}

TL_TEST(landing_exactly_on_a_mark_fires_it) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 9999999.0f, -5.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK_EQ(out.size(), size_t{1});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kHp90Pct));
}

TL_TEST(starting_exactly_on_a_mark_does_not) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 90.0f, 9999999.0f, -5.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK(out.empty());
}

TL_TEST(healing_fires_nothing) {
    std::vector<uint32_t> out;
    tl::core::broadcast_unit_hp_events(100.0f, 50.0f, 9999999.0f, 30.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK(out.empty());
}

TL_TEST(a_blow_that_stays_above_min_hp_watermark_fires_nothing) {
    std::vector<uint32_t> out;
    // Unit reached 50 HP in the past (min_hp = 50.0f), then healed to 95.
    // A blow deals 35 damage (95 -> 60). New HP (60) is not strictly less than min_hp (50).
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 50.0f, -35.0f,
                                       /*fire_all=*/true, out);
    TL_CHECK(out.empty());
}

TL_TEST(a_blow_below_the_watermark_fires_the_marks_it_crosses) {
    std::vector<uint32_t> out;
    // Lowest so far 60, healed to 95, now down to 45: 90..50 are crossed again.
    tl::core::broadcast_unit_hp_events(100.0f, 95.0f, 60.0f, -50.0f,
                                       /*fire_all=*/false, out);
    TL_CHECK_EQ(out.size(), size_t{1});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kHp50Pct));
}

// ------------------------------------------------------------------ player broadcaster

TL_TEST(player_falling_fires_below_in_order) {
    std::vector<uint32_t> out;
    tl::core::broadcast_player_hp_events(100.0f, 95.0f, -90.0f, out);
    TL_CHECK_EQ(out.size(), size_t{9});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kPlayerHpBelow90Pct));
    TL_CHECK_EQ(out[1], static_cast<uint32_t>(logic::kPlayerHpBelow80Pct));
    TL_CHECK_EQ(out[2], static_cast<uint32_t>(logic::kPlayerHpBelow70Pct));
    TL_CHECK_EQ(out[3], static_cast<uint32_t>(logic::kPlayerHpBelow60Pct));
    TL_CHECK_EQ(out[4], static_cast<uint32_t>(logic::kPlayerHpBelow50Pct));
    TL_CHECK_EQ(out[5], static_cast<uint32_t>(logic::kPlayerHpBelow40Pct));
    TL_CHECK_EQ(out[6], static_cast<uint32_t>(logic::kPlayerHpBelow30Pct));
    TL_CHECK_EQ(out[7], static_cast<uint32_t>(logic::kPlayerHpBelow20Pct));
    TL_CHECK_EQ(out[8], static_cast<uint32_t>(logic::kPlayerHpBelow10Pct));
}

TL_TEST(player_rising_fires_above_in_order) {
    std::vector<uint32_t> out;
    tl::core::broadcast_player_hp_events(100.0f, 5.0f, 90.0f, out);
    TL_CHECK_EQ(out.size(), size_t{9});
    TL_CHECK_EQ(out[0], static_cast<uint32_t>(logic::kPlayerHpAbove90Pct));
    TL_CHECK_EQ(out[1], static_cast<uint32_t>(logic::kPlayerHpAbove80Pct));
    TL_CHECK_EQ(out[2], static_cast<uint32_t>(logic::kPlayerHpAbove70Pct));
    TL_CHECK_EQ(out[3], static_cast<uint32_t>(logic::kPlayerHpAbove60Pct));
    TL_CHECK_EQ(out[4], static_cast<uint32_t>(logic::kPlayerHpAbove50Pct));
    TL_CHECK_EQ(out[5], static_cast<uint32_t>(logic::kPlayerHpAbove40Pct));
    TL_CHECK_EQ(out[6], static_cast<uint32_t>(logic::kPlayerHpAbove30Pct));
    TL_CHECK_EQ(out[7], static_cast<uint32_t>(logic::kPlayerHpAbove20Pct));
    TL_CHECK_EQ(out[8], static_cast<uint32_t>(logic::kPlayerHpAbove10Pct));
}

TL_TEST(player_at_zero_previous_hp_fires_nothing) {
    std::vector<uint32_t> out;
    tl::core::broadcast_player_hp_events(100.0f, 0.0f, 50.0f, out);
    TL_CHECK(out.empty());
}

TL_TEST(player_max_zero_fires_nothing) {
    std::vector<uint32_t> out;
    tl::core::broadcast_player_hp_events(0.0f, 10.0f, -5.0f, out);
    TL_CHECK(out.empty());
}

// ------------------------------------------------------------------ LevelScript integration

TL_TEST(level_script_monster_damage_triggers_counter) {
    CmpScene scene;

    // Monster (node 1): 100 HP, fire_all = false
    CmpNode& monster = add_node(scene, tl::formats::cmp_type::kMonster, 1, "Monster1");
    monster.fields.push_back(vec3_field(0.0f, 0.0f, 0.0f));  // 0 POSITION
    monster.fields.push_back(vec3_field(0.0f, 0.0f, 1.0f));  // 1 FORWARD
    monster.fields.push_back(vec3_field(1.0f, 0.0f, 0.0f));  // 2 RIGHT
    monster.fields.push_back(vec3_field(0.0f, 1.0f, 0.0f));  // 3 UP
    monster.fields.push_back(text_field("TestMonster"));     // 4 MONSTER
    monster.fields.push_back(text_field(""));                // 5 FILE
    monster.fields.push_back(uint_field(1));                 // 6 ENABLED
    monster.fields.push_back(uint_field(0));                 // 7 NO TARGET
    monster.fields.push_back(uint_field(0));                 // 8 FIRE ALL HP EVENTS (false)

    // Counter (node 2): equals 1, start 0, type 0
    CmpNode& counter = add_node(scene, tl::formats::cmp_type::kCounter, 2, "Counter1");
    counter.fields.push_back(uint_field(1));                     // 0 ENABLED
    counter.fields.push_back(uint_field(1));                     // 1 EQUALS VALUE
    counter.fields.push_back(uint_field(0));                     // 2 STARTING VALUE
    counter.fields.push_back(text_field("Activate only once"));  // 3 LOGIC

    // Generic Model (node 3): initially visible
    CmpNode& model = add_node(scene, tl::formats::cmp_type::kModel, 3, "Model1");
    model.fields.push_back(uint_field(1));  // 0 VISIBLE = true

    // Logic Group (node 4): Monster "HP 50 PCT" -> Counter "Add"; "Activated" -> Model "Hide"
    add_node(scene, tl::formats::cmp_type::kLogicGroup, 4, "Logic");
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 4;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "HP 50 PCT", "Add"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Activated", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    StatGraph g(GraphType::Linear, "HEALTH_MONSTER_BYLEVEL");
    g.add_point(0.0f, 100.0f);
    g.add_point(10.0f, 100.0f);
    g.process_points();
    StatBook graphs;
    graphs.add("HEALTH_MONSTER_BYLEVEL", g);
    graphs.add("HEALTH_MONSTER_BYLEVEL_HARD", g);

    LevelScript script;
    script.set_stat_graphs(&graphs);
    script.build(scene, {});

    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    float hp = 0.0f, max = 0.0f;
    TL_CHECK(script.unit_health(1, hp, max));
    TL_CHECK_EQ(hp, 100.0f);
    TL_CHECK_EQ(max, 100.0f);

    // Initial damage from 100 to 90 (amount 10): 50% mark not crossed
    events.clear();
    TL_CHECK(!script.damage_unit(1, 10.0f, 0, quests, events));
    bool model_hidden = false;
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::SetVisible && e.node_id == 3 && !e.on) {
            model_hidden = true;
        }
    }
    TL_CHECK(!model_hidden);

    // Damage from 90 to 45 (amount 45) crosses the 50% mark: the model hides via the counter.
    events.clear();
    TL_CHECK(!script.damage_unit(1, 45.0f, 0, quests, events));
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::SetVisible && e.node_id == 3 && !e.on) {
            model_hidden = true;
        }
    }
    TL_CHECK(model_hidden);
}

/// `CMonsterDescriptor::CreateObject`, `unitInit`: a placed monster
/// takes the level's monster level unless its data has its own `LEVEL`.
TL_TEST(a_placed_monster_takes_the_levels_monster_level) {
    CmpScene scene;
    const char* const names[2] = {"Unlevelled", "Levelled"};
    for (uint32_t i = 0; i < 2; ++i) {
        CmpNode& monster = add_node(scene, tl::formats::cmp_type::kMonster, i + 1, names[i]);
        monster.fields.push_back(vec3_field(0.0f, 0.0f, 0.0f));  // 0 POSITION
        monster.fields.push_back(vec3_field(0.0f, 0.0f, 1.0f));  // 1 FORWARD
        monster.fields.push_back(vec3_field(1.0f, 0.0f, 0.0f));  // 2 RIGHT
        monster.fields.push_back(vec3_field(0.0f, 1.0f, 0.0f));  // 3 UP
        monster.fields.push_back(text_field(names[i]));          // 4 MONSTER
        monster.fields.push_back(text_field(""));                // 5 FILE
        monster.fields.push_back(uint_field(1));                 // 6 ENABLED
        monster.fields.push_back(uint_field(0));                 // 7 NO TARGET
        monster.fields.push_back(uint_field(0));                 // 8 FIRE ALL HP EVENTS
    }

    tl::core::UnitBook units;
    units.add(names[0], tl::core::UnitStats{});
    tl::core::UnitStats levelled;
    levelled.level = 3;
    levelled.level_authored = true;
    units.add(names[1], levelled);

    LevelScript script;
    script.set_units(&units);
    script.set_level_monster_level(7);
    script.build(scene, {});

    int level = 0;
    TL_CHECK(script.unit_level(1, level));
    TL_CHECK_EQ(level, 7);
    TL_CHECK(script.unit_level(2, level));
    TL_CHECK_EQ(level, 3);
}
