// `CTriggerUnit`'s state machine in the level script: `trigger`,
// `setTriggerState`, `resetTrigger`, `update`'s
// broadcast and `applySaveState`.

#include "core/logic_events.h"
#include "core/quest.h"
#include "core/script.h"
#include "formats/cmp.h"
#include "test_support.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using tl::core::LevelScript;
using tl::core::QuestState;
using tl::core::ScriptAction;
using tl::core::ScriptEvent;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;
namespace logic = tl::core::logic;

constexpr int64_t kSender = 1;  // a Player Sphere Trigger whose output drives the prop
constexpr int64_t kProp = 201;  // the Unit Trigger
constexpr int64_t kOnTriggered = 104;
constexpr int64_t kOnStateTwo = 105;

CmpField uint_field(uint32_t value) {
    CmpField f;
    f.words.push_back(value);
    return f;
}

/// A string field as the compiled form stores one: UTF-16LE, a NUL, padded to
/// whole dwords.
CmpField text_field(const std::string& text) {
    std::vector<uint16_t> units(text.begin(), text.end());
    units.push_back(0);
    if (units.size() % 2 != 0) units.push_back(0);
    CmpField f;
    for (size_t i = 0; i < units.size(); i += 2) {
        f.words.push_back(static_cast<uint32_t>(units[i]) |
                          (static_cast<uint32_t>(units[i + 1]) << 16));
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

/// A lever wired from the sender's `Triggered` to `input`, and to two listeners; an empty
/// `loop_style` leaves `LOOP STYLE` unauthored.
void build(LevelScript& script, const std::string& input, const std::string& loop_style = "") {
    CmpScene scene;
    add_node(scene, tl::formats::cmp_type::kPlayerSphereTrigger, kSender, "Sender");
    CmpNode& lever = add_node(scene, tl::formats::cmp_type::kUnitTrigger, kProp, "Lever");
    while (lever.fields.size() < tl::formats::cmp_field::unit_trigger::kModel) {
        lever.fields.push_back(uint_field(0));
    }
    lever.fields[tl::formats::cmp_field::unit_trigger::kEnabled] = uint_field(1);
    // "lever" as the compiled form stores a string: UTF-16LE, NUL, dword-padded.
    CmpField model;
    model.words = {0x0065006cu, 0x00650076u, 0x00000072u};
    lever.fields.push_back(model);
    if (!loop_style.empty()) lever.fields.push_back(text_field(loop_style));
    add_node(scene, tl::formats::cmp_type::kMonster, kOnTriggered, "OnTriggered");
    add_node(scene, tl::formats::cmp_type::kMonster, kOnStateTwo, "OnStateTwo");

    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 10, "Logic");
    group.fields.push_back(uint_field(0));
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 10;
    wiring.objects.push_back({0, kSender, 0.0f, 0.0f, {{1, "Triggered", input}}});
    wiring.objects.push_back(
        {1, kProp, 0.0f, 0.0f, {{2, "Triggered", "Show"}, {3, "State Two", "Show"}}});
    wiring.objects.push_back({2, kOnTriggered, 0.0f, 0.0f, {}});
    wiring.objects.push_back({3, kOnStateTwo, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));
    script.build(scene, {});
}

bool reached(const std::vector<ScriptEvent>& events, int64_t node) {
    for (const ScriptEvent& e : events) {
        if (e.node_id == node && e.action != ScriptAction::TriggerState) return true;
    }
    return false;
}

const ScriptEvent* state_event(const std::vector<ScriptEvent>& events) {
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::TriggerState && e.node_id == kProp) return &e;
    }
    return nullptr;
}

LevelScript::UnitTriggerView view_of(const LevelScript& script) {
    LevelScript::UnitTriggerView view;
    TL_CHECK(script.unit_trigger(kProp, view));
    return view;
}

void activate(LevelScript& script, QuestState& quests) {
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
}

}  // namespace

// A wire's `Trigger` is `trigger(nullptr)`: the one-shot is spent; `Triggered` waits for
// `update`.
TL_TEST(unit_trigger_wire_trigger_spends_a_one_shot) {
    LevelScript script;
    build(script, "Trigger");
    script.init_unit_trigger(kProp, 2, -1);
    QuestState quests;
    activate(script, quests);

    std::vector<ScriptEvent> events;
    script.raise_output(kSender, logic::kTriggered, quests, events);
    const ScriptEvent* fired = state_event(events);
    TL_CHECK(fired != nullptr);
    TL_CHECK(fired->on);
    TL_CHECK(fired->trigger_state == 1);
    TL_CHECK(fired->trigger_spent);
    TL_CHECK(fired->trigger_skill);
    TL_CHECK(!fired->by_player);
    TL_CHECK(fired->trigger_sound);
    TL_CHECK(!reached(events, kOnTriggered));
    TL_CHECK(!view_of(script).armed);

    events.clear();
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(reached(events, kOnTriggered));
    TL_CHECK(reached(events, kOnStateTwo));
    events.clear();
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(events.empty());

    script.raise_output(kSender, logic::kTriggered, quests, events);
    TL_CHECK(state_event(events) == nullptr);
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(events.empty());
}

// `Set State Two` (0x1f): `setTriggerState(1, 1, 0, 1)` - no treasure, but
// `update` broadcasts `Triggered` and `State Two`; the prop stays armed.
TL_TEST(unit_trigger_set_state_two_broadcasts_on_update) {
    LevelScript script;
    build(script, "Set State Two");
    script.init_unit_trigger(kProp, 2, -1);
    QuestState quests;
    activate(script, quests);

    std::vector<ScriptEvent> events;
    script.raise_output(kSender, logic::kTriggered, quests, events);
    const ScriptEvent* set = state_event(events);
    TL_CHECK(set != nullptr);
    TL_CHECK(!set->on);
    TL_CHECK(set->trigger_state == 1);
    TL_CHECK(!set->trigger_spent);
    TL_CHECK(view_of(script).armed);
    events.clear();
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(reached(events, kOnTriggered));
    TL_CHECK(reached(events, kOnStateTwo));

    // In its last state an unforced call goes through again.
    events.clear();
    script.raise_output(kSender, logic::kTriggered, quests, events);
    TL_CHECK(state_event(events) != nullptr);
}

// Unforced, a prop already in a state that is not its last is left alone:
// `Set State One` on a lever at rest sends nothing and broadcasts nothing.
TL_TEST(unit_trigger_set_state_one_at_rest_is_left_alone) {
    LevelScript script;
    build(script, "Set State One");
    script.init_unit_trigger(kProp, 2, -1);
    QuestState quests;
    activate(script, quests);
    std::vector<ScriptEvent> events;
    script.raise_output(kSender, logic::kTriggered, quests, events);
    TL_CHECK(state_event(events) == nullptr);
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(!reached(events, kOnTriggered));
}

// `LOOPTYPE: Cycle` from the unit, the node authoring none: round its states,
// never spent, the skill rolled each time.
TL_TEST(unit_trigger_cycle_from_the_unit) {
    LevelScript script;
    build(script, "Trigger");
    script.init_unit_trigger(kProp, 2, 1);
    QuestState quests;
    activate(script, quests);
    const int32_t expected[] = {1, 0, 1};
    for (int32_t want : expected) {
        std::vector<ScriptEvent> events;
        script.raise_output(kSender, logic::kTriggered, quests, events);
        const ScriptEvent* fired = state_event(events);
        TL_CHECK(fired != nullptr);
        TL_CHECK(fired->trigger_state == want);
        TL_CHECK(!fired->trigger_spent);
        TL_CHECK(fired->trigger_skill);
        TL_CHECK(view_of(script).armed);
    }
}

// The node's own `LOOP STYLE` is stored after the model's `unitInit`, so it
// wins over the unit's `LOOPTYPE`.
TL_TEST(unit_trigger_node_loop_style_wins) {
    LevelScript script;
    build(script, "Trigger", "No Loop");
    script.init_unit_trigger(kProp, 2, 1);
    TL_CHECK(view_of(script).loop_style == 0);
}

// `LOOP STYLE` is read back by name in `gTRIGGER_LOOP_TYPE_NAMES`; any other name or case is
// `No Loop`.
TL_TEST(unit_trigger_loop_style_is_read_by_name) {
    const std::pair<const char*, int32_t> cases[] = {{"No Loop", 0},        {"Cycle", 1},
                                                     {"Back and Forth", 2}, {"Back And Forth", 0},
                                                     {"CYCLE", 0},          {"Loop", 0}};
    for (const auto& c : cases) {
        LevelScript script;
        build(script, "Trigger", c.first);
        TL_CHECK(view_of(script).loop_style == c.second);
    }
}

// A `Cycle` prop the level is put back with in its last state - the town's mine
// entrance after its first use - is still armed: only `No Loop` is spent there.
TL_TEST(unit_trigger_restore_keeps_a_cycle_armed) {
    LevelScript script;
    build(script, "Trigger", "Cycle");
    script.init_unit_trigger(kProp, 2, -1);
    std::vector<ScriptEvent> events;
    script.restore_unit_trigger(kProp, 1, events);
    LevelScript::UnitTriggerView view;
    TL_CHECK(script.unit_trigger(kProp, view));
    TL_CHECK(view.state == 1);
    TL_CHECK(view.armed);
    QuestState quests;
    activate(script, quests);
    events.clear();
    script.interact_unit_trigger(kProp, quests, events);
    const ScriptEvent* fired = state_event(events);
    TL_CHECK(fired != nullptr);
    TL_CHECK(fired->trigger_state == 0);
}

// `Back and Forth` over three states: 1, 2, then back 1, 0, then on again.
TL_TEST(unit_trigger_back_and_forth) {
    LevelScript script;
    build(script, "Trigger", "Back and Forth");
    script.init_unit_trigger(kProp, 3, -1);
    QuestState quests;
    activate(script, quests);
    const int32_t expected[] = {1, 2, 1, 0, 1, 2};
    for (int32_t want : expected) {
        std::vector<ScriptEvent> events;
        script.raise_output(kSender, logic::kTriggered, quests, events);
        const ScriptEvent* fired = state_event(events);
        TL_CHECK(fired != nullptr);
        TL_CHECK(fired->trigger_state == want);
        TL_CHECK(!fired->trigger_skill);
    }
}

// `applySaveState`: a `No Loop` prop restored in its last state is not armed,
// and nothing is broadcast or heard for it.
TL_TEST(unit_trigger_restore_disarms_a_spent_one_shot) {
    LevelScript script;
    build(script, "Trigger");
    script.init_unit_trigger(kProp, 2, -1);
    std::vector<ScriptEvent> events;
    script.restore_unit_trigger(kProp, 1, events);
    const ScriptEvent* restored = state_event(events);
    TL_CHECK(restored != nullptr);
    TL_CHECK(restored->trigger_state == 1);
    TL_CHECK(!restored->trigger_animate);
    TL_CHECK(!restored->trigger_sound);
    TL_CHECK(!view_of(script).armed);
    QuestState quests;
    events.clear();
    script.update_unit_trigger(kProp, quests, events);
    TL_CHECK(events.empty());

    // At rest it stays armed.
    LevelScript other;
    build(other, "Trigger");
    other.init_unit_trigger(kProp, 2, -1);
    events.clear();
    other.restore_unit_trigger(kProp, 0, events);
    LevelScript::UnitTriggerView view;
    TL_CHECK(other.unit_trigger(kProp, view));
    TL_CHECK(view.armed);
    TL_CHECK(view.state == 0);
}

// The player's `interact` reaches `trigger` with a character.
TL_TEST(unit_trigger_interact_is_by_the_player) {
    LevelScript script;
    build(script, "Trigger");
    script.init_unit_trigger(kProp, 2, -1);
    QuestState quests;
    activate(script, quests);
    std::vector<ScriptEvent> events;
    script.interact_unit_trigger(kProp, quests, events);
    const ScriptEvent* fired = state_event(events);
    TL_CHECK(fired != nullptr);
    TL_CHECK(fired->by_player);
    events.clear();
    script.interact_unit_trigger(kProp, quests, events);
    TL_CHECK(events.empty());
}
