// The "Puzzle Input" layout node (CPuzzleRandomizer, type 46), on scenes built in code.

#include "core/logic_events.h"
#include "core/quest.h"
#include "core/random.h"
#include "core/script.h"
#include "formats/cmp.h"
#include "test_seeds.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
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

CmpField uint_field(uint32_t value) {
    CmpField f;
    f.words.push_back(value);
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

/// A puzzle scene whose SUCCESS (0x37), FAILED (0x36) and Was Reset (0x09) each show a
/// target: 101, 102 and 103.
void setup_puzzle_scene(CmpScene& scene, int64_t puzzle_id, bool enabled, uint32_t num_inputs,
                        bool author_inputs = true) {
    CmpNode& puzzle = add_node(scene, 46, puzzle_id, "Puzzle");
    puzzle.fields.push_back(uint_field(enabled ? 1 : 0));  // 0 ENABLED
    if (author_inputs) {
        puzzle.fields.push_back(uint_field(num_inputs));  // 1 NUMBER OF INPUTS
    }

    add_node(scene, tl::formats::cmp_type::kMonster, 101, "TargetSuccess");
    add_node(scene, tl::formats::cmp_type::kMonster, 102, "TargetFailed");
    add_node(scene, tl::formats::cmp_type::kMonster, 103, "TargetReset");

    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 10, "Logic");
    group.fields.push_back(uint_field(0));
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 10;
    wiring.objects.push_back({0, 10, 0.0f, 0.0f, {}});
    wiring.objects.push_back({1,
                              puzzle_id,
                              0.0f,
                              0.0f,
                              {
                                  {2, "SUCCESS", "Show"},
                                  {3, "FAILED", "Show"},
                                  {4, "Reset", "Show"},
                              }});
    wiring.objects.push_back({2, 101, 0.0f, 0.0f, {}});
    wiring.objects.push_back({3, 102, 0.0f, 0.0f, {}});
    wiring.objects.push_back({4, 103, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));
}

bool has_event(const std::vector<ScriptEvent>& events, int64_t target_id) {
    for (const auto& e : events) {
        if (e.node_id == target_id) return true;
    }
    return false;
}

}  // namespace

// Draw count and shuffle order prediction against a reference generator:
TL_TEST(puzzle_shuffle_draw_count_and_order_match_reference) {
    constexpr int32_t kSeed = 0x472910ab;
    tl::core::volatile_random().set_seed(kSeed);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script;
    script.build(scene, {});

    // Now test a Reset with a known seed to verify CPuzzleRandomizer::setActiveInputs draws:
    tl::core::volatile_random().set_seed(kSeed);
    tl::core::RunicRandom ref(kSeed);

    // Compute expected order for n = 3: 5 passes of integer_between(0, 2)
    uint32_t expected_order[6] = {0, 1, 2, 3, 4, 5};
    for (int pass = 0; pass < 5; ++pass) {
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t j = static_cast<uint32_t>(ref.integer_between(0, 2));
            std::swap(expected_order[i], expected_order[j]);
        }
    }

    QuestState quests;
    std::vector<ScriptEvent> events;
    script.send_puzzle_input(1, logic::kReset, quests, events);

    // Order matches reference:
    for (size_t i = 0; i < 6; ++i) {
        TL_CHECK_EQ(script.puzzle_order_at(1, i), expected_order[i]);
    }
    // Draw count is exactly pinned (5 * 3 = 15 draws):
    TL_CHECK_EQ(tl::core::volatile_random().state(), ref.state());
}

// Clamping: 0, 1 -> 2; 6, 9 -> 3 (original quirk); 2, 3, 4, 5 unchanged
TL_TEST(puzzle_clamp_rules) {
    tl::core::volatile_random().set_seed(0x1234);

    auto test_clamp = [](uint32_t authored, uint32_t expected) {
        CmpScene scene;
        setup_puzzle_scene(scene, 1, true, authored);
        LevelScript script;
        script.build(scene, {});
        TL_CHECK_EQ(script.puzzle_active_inputs(1), expected);
    };

    // 0 and 1 clamp to 2
    test_clamp(0, 2);
    test_clamp(1, 2);

    // 2, 3, 4, 5 are kept unchanged
    test_clamp(2, 2);
    test_clamp(3, 3);
    test_clamp(4, 4);
    test_clamp(5, 5);

    // 6 and 9 clamp to 3 (the original's quirk: > 5 -> 3, not 5)
    test_clamp(6, 3);
    test_clamp(9, 3);
}

// Solving sequence: fires nothing until last lever, then SUCCESS on update, nothing after
TL_TEST(puzzle_solve_sequence) {
    tl::core::volatile_random().set_seed(0x98765);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script;
    script.build(scene, {});

    const uint32_t o0 = script.puzzle_order_at(1, 0);
    const uint32_t o1 = script.puzzle_order_at(1, 1);
    const uint32_t o2 = script.puzzle_order_at(1, 2);

    QuestState quests;
    std::vector<ScriptEvent> events;

    // Pull first lever
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o0, quests, events);
    TL_CHECK(events.empty());
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());
    TL_CHECK(!script.puzzle_solved(1));

    // Pull second lever
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o1, quests, events);
    TL_CHECK(events.empty());
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());
    TL_CHECK(!script.puzzle_solved(1));

    // Pull third (last) lever
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o2, quests, events);
    TL_CHECK(events.empty());

    // SUCCESS fires on the next update frame
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 101));
    TL_CHECK(script.puzzle_solved(1));

    // Next update frame fires nothing
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());

    // Further pulls do nothing once solved
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o0, quests, events);
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());
}

// Failure sequence: FAILED on frame 1, Was Reset on frame 2, order UNCHANGED, then solves
TL_TEST(puzzle_failure_two_frame_sequence_and_no_reshuffle) {
    tl::core::volatile_random().set_seed(0xabcdef);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script;
    script.build(scene, {});

    const uint32_t o0 = script.puzzle_order_at(1, 0);
    const uint32_t o1 = script.puzzle_order_at(1, 1);
    const uint32_t o2 = script.puzzle_order_at(1, 2);
    const uint32_t saved_order[6] = {
        script.puzzle_order_at(1, 0), script.puzzle_order_at(1, 1), script.puzzle_order_at(1, 2),
        script.puzzle_order_at(1, 3), script.puzzle_order_at(1, 4), script.puzzle_order_at(1, 5),
    };

    // Wrong pull: choose an input different from o0
    const uint32_t wrong = (o0 + 1) % 3;

    QuestState quests;
    std::vector<ScriptEvent> events;
    script.send_puzzle_input(1, logic::kPuzzleInput1 + wrong, quests, events);
    TL_CHECK(events.empty());

    // Frame 1: FAILED fires (target 102), Was Reset does not fire yet
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 102));
    TL_CHECK(!has_event(events, 103));

    // Frame 2: Was Reset fires (target 103) exactly one frame after FAILED
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(!has_event(events, 102));
    TL_CHECK(has_event(events, 103));

    // Frame 3: nothing fires
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());

    // Order is UNCHANGED after failure: no reshuffle occurred!
    for (size_t i = 0; i < 6; ++i) {
        TL_CHECK_EQ(script.puzzle_order_at(1, i), saved_order[i]);
    }

    // Now pulling the correct sequence solves the puzzle:
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o0, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o1, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o2, quests, events);
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 101));
    TL_CHECK(script.puzzle_solved(1));
}

// Reset: fires Was Reset, reshuffles (advancing volatile stream), clears progress, re-enables
TL_TEST(puzzle_reset_reshuffles_clears_progress_and_reenables) {
    tl::core::volatile_random().set_seed(0x13579);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script;
    script.build(scene, {});

    QuestState quests;
    std::vector<ScriptEvent> events;

    // Pull first lever in order
    const uint32_t o0 = script.puzzle_order_at(1, 0);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o0, quests, events);

    // Disable node
    script.send_puzzle_input(1, logic::kDisable, quests, events);
    TL_CHECK(!script.node_enabled(1));

    // Reset: should advance generator by 5 * 3 = 15 draws, clear progress, re-enable node
    const uint64_t state_before = tl::core::volatile_random().state();
    script.send_puzzle_input(1, logic::kReset, quests, events);
    const uint64_t state_after = tl::core::volatile_random().state();
    TL_CHECK(state_before != state_after);
    TL_CHECK(script.node_enabled(1));

    // On update: Was Reset fires
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 103));

    // Progress was cleared: pulling new order slot 0, 1, 2 works
    const uint32_t new_o0 = script.puzzle_order_at(1, 0);
    const uint32_t new_o1 = script.puzzle_order_at(1, 1);
    const uint32_t new_o2 = script.puzzle_order_at(1, 2);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + new_o0, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + new_o1, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + new_o2, quests, events);
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 101));
    TL_CHECK(script.puzzle_solved(1));
}

// Disabled: Disable then any pull fires nothing and draws nothing
TL_TEST(puzzle_disabled_ignores_pulls) {
    tl::core::volatile_random().set_seed(0x24680);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, false, 3);

    LevelScript script;
    script.build(scene, {});

    TL_CHECK(!script.node_enabled(1));
    const uint64_t state_before = tl::core::volatile_random().state();

    QuestState quests;
    std::vector<ScriptEvent> events;

    // Any pull when disabled fires nothing and takes no draws
    script.send_puzzle_input(1, logic::kPuzzleInput1, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + 1, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + 2, quests, events);
    TL_CHECK_EQ(tl::core::volatile_random().state(), state_before);

    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());
    TL_CHECK(!script.puzzle_solved(1));
}

// Out of range input (k >= n) ignored, leaves progress intact
TL_TEST(puzzle_out_of_range_input_ignored) {
    tl::core::volatile_random().set_seed(0x9999);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script;
    script.build(scene, {});

    const uint32_t o0 = script.puzzle_order_at(1, 0);
    const uint32_t o1 = script.puzzle_order_at(1, 1);
    const uint32_t o2 = script.puzzle_order_at(1, 2);

    QuestState quests;
    std::vector<ScriptEvent> events;

    // Pull kPuzzleInput1 + 4 and kPuzzleInput1 + 5 (k >= 3)
    script.send_puzzle_input(1, logic::kPuzzleInput1 + 4, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + 5, quests, events);
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(events.empty());

    // Subsequent valid pulls still work normally
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o0, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o1, quests, events);
    script.send_puzzle_input(1, logic::kPuzzleInput1 + o2, quests, events);
    events.clear();
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    TL_CHECK(has_event(events, 101));
    TL_CHECK(script.puzzle_solved(1));
}

// Level state store and restore: enabled is restored, progress is not saved/restored
TL_TEST(puzzle_level_state_store_and_restore) {
    tl::core::volatile_random().set_seed(0x7777);

    CmpScene scene;
    setup_puzzle_scene(scene, 1, true, 3);

    LevelScript script1;
    script1.build(scene, {});

    QuestState quests;
    std::vector<ScriptEvent> events;

    // Disable node and make partial progress
    script1.send_puzzle_input(1, logic::kDisable, quests, events);
    TL_CHECK(!script1.node_enabled(1));

    // Store state
    std::vector<tl::formats::LogicNodeState> states;
    script1.store_logic_states(0, 0, states);

    // Verify Puzzle Input is present in states
    bool found_puzzle = false;
    for (const auto& s : states) {
        if (s.descriptor == "Puzzle Input" && s.object_id == 1) {
            found_puzzle = true;
            TL_CHECK_EQ(s.flags[0], 0);  // enabled = false
        }
    }
    TL_CHECK(found_puzzle);

    // Build fresh scene with enabled puzzle
    LevelScript script2;
    script2.build(scene, {});
    TL_CHECK(script2.node_enabled(1));

    // Restore state into script2
    script2.restore_logic_states(0, 0, states, quests, events);
    // Enabled flag restored to false:
    TL_CHECK(!script2.node_enabled(1));
}

// The constructor shuffles with 3 (15 draws), then `NUMBER OF INPUTS` again (5 * n), both on
// the volatile stream; the order is the second shuffle's.
TL_TEST(puzzle_build_shuffles_twice_from_the_reseeded_volatile_stream) {
    CmpScene scene;
    CmpNode& puzzle = add_node(scene, 46, 1, "Puzzle");
    puzzle.fields.push_back(uint_field(1));  // ENABLED
    puzzle.fields.push_back(uint_field(4));  // NUMBER OF INPUTS

    LevelScript script;
    tl::test::seed_generators();
    script.build(scene, {});

    tl::core::RunicRandom ref(tl::test::kTestVolatileSeed);
    auto shuffle = [&ref](uint32_t n, uint32_t order[6]) {
        for (uint32_t i = 0; i < 6; ++i) order[i] = i;
        for (int pass = 0; pass < 5; ++pass) {
            for (uint32_t i = 0; i < n; ++i) {
                std::swap(order[i], order[ref.integer_between(0, static_cast<int32_t>(n - 1))]);
            }
        }
    };
    uint32_t order[6];
    shuffle(3, order);
    shuffle(4, order);

    TL_CHECK_EQ(script.puzzle_active_inputs(1), 4u);
    for (size_t i = 0; i < 6; ++i) TL_CHECK_EQ(script.puzzle_order_at(1, i), order[i]);
    TL_CHECK_EQ(tl::core::volatile_random().state(), ref.state());
}

// `FAILED` -> `Reset` on each lever: `CTriggerUnit::resetTrigger` re-arms it in
// its first state and says `Was Reset`.
TL_TEST(puzzle_failure_resets_the_levers) {
    tl::core::volatile_random().set_seed(0x5eed);
    CmpScene scene;
    CmpNode& puzzle = add_node(scene, 46, 1, "Puzzle Input0");
    puzzle.fields.push_back(uint_field(1));
    puzzle.fields.push_back(uint_field(3));
    CmpNode& lever = add_node(scene, tl::formats::cmp_type::kUnitTrigger, 201, "Lever 1");
    while (lever.fields.size() < tl::formats::cmp_field::unit_trigger::kModel) {
        lever.fields.push_back(uint_field(0));
    }
    lever.fields[tl::formats::cmp_field::unit_trigger::kEnabled] = uint_field(1);
    // "lever" as the compiled form stores a string: UTF-16LE, NUL, dword-padded.
    CmpField model;
    model.words = {0x0065006cu, 0x00650076u, 0x00000072u};
    lever.fields.push_back(model);
    add_node(scene, tl::formats::cmp_type::kMonster, 104, "LeverWasReset");

    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 10, "Lever Logic");
    group.fields.push_back(uint_field(0));
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 10;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "FAILED", "Reset"}}});
    wiring.objects.push_back({1, 201, 0.0f, 0.0f, {{2, "Reset", "Show"}}});
    wiring.objects.push_back({2, 104, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    events.clear();

    const uint32_t wrong = (script.puzzle_order_at(1, 0) + 1) % 3;
    script.send_puzzle_input(1, logic::kPuzzleInput1 + wrong, quests, events);
    script.update(1.0f / 60.0f, {0, 0, 0}, quests, events);
    bool reset = false;
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::TriggerState && e.node_id == 201) {
            reset = true;
            TL_CHECK(e.unit == "lever");
            TL_CHECK(e.trigger_state == 0);
            TL_CHECK(e.trigger_forced);
        }
    }
    TL_CHECK(reset);
    TL_CHECK(has_event(events, 104));
}
