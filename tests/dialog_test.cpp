// The paths a level script walks units along, and the dialogue that paces a scripted scene.
#include "core/dialog.h"
#include "core/ai_clock.h"
#include "core/collision.h"
#include "core/navigation.h"
#include "core/passability.h"
#include "core/quest.h"
#include "core/script.h"
#include "core/units.h"
#include "test_support.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using tl::core::DialogBook;
using tl::core::DialogLine;
using tl::core::LevelScript;
using tl::core::QuestState;
using tl::core::QuestStatus;
using tl::core::ScriptAction;
using tl::core::ScriptEvent;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;

/// A string field: UTF-16LE with a NUL, padded out to whole dwords, which is
/// how a string is stored inside a node's field stream.
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

CmpField float_field(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return uint_field(bits);
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

void pad_to(CmpNode& node, size_t count) {
    while (node.fields.size() < count) node.fields.push_back(uint_field(0));
}

CmpNode& add_node(CmpScene& scene, uint32_t type, int64_t id, const std::string& name) {
    CmpNode node;
    node.type = type;
    node.id = id;
    node.name = name;
    scene.nodes.push_back(std::move(node));
    return scene.nodes.back();
}

/// A `Pathing` node with a unit and a flat run of waypoints, `ENABLED: false`: a wire's
/// `Enable` hands its units the path (`CPathController::setEnabled`).
CmpNode& add_path(CmpScene& scene, int64_t id, const std::string& name, const std::string& unit,
                  const std::vector<float>& points, bool runs = false) {
    namespace f = tl::formats::cmp_field::pathing;
    CmpNode& node = add_node(scene, tl::formats::cmp_type::kPathing, id, name);
    node.fields.push_back(vec3_field(0.0f, 0.0f, 0.0f));  // kPosition

    CmpField waypoints;
    for (float v : points) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        waypoints.words.push_back(bits);
    }
    node.fields.push_back(std::move(waypoints));  // kPoints

    pad_to(node, f::kEnableOnStart);
    node.fields.push_back(uint_field(0));               // ENABLE ON START
    node.fields.push_back(uint_field(0));               // ENABLED
    node.fields.push_back(text_field(name));            // PATH NAME
    node.fields.push_back(uint_field(runs ? 1u : 0u));  // RUN
    pad_to(node, f::kUnit);
    node.fields.push_back(text_field(unit));
    return node;
}

CmpNode& add_monster(CmpScene& scene, int64_t id, const std::string& name, const std::string& unit,
                     float x, float y, float z) {
    namespace f = tl::formats::cmp_field::monster;
    CmpNode& node = add_node(scene, tl::formats::cmp_type::kMonster, id, name);
    node.fields.push_back(vec3_field(x, y, z));  // kPosition
    pad_to(node, f::kMonster);
    node.fields.push_back(text_field(unit));
    return node;
}

CmpNode& add_quest_controller(CmpScene& scene, int64_t id, const std::string& name,
                              const std::string& unit, const std::string& quest) {
    namespace f = tl::formats::cmp_field::quest_controller;
    CmpNode& node = add_node(scene, tl::formats::cmp_type::kQuestController, id, name);
    pad_to(node, f::kUnit);
    node.fields.push_back(text_field(unit));
    node.fields.push_back(text_field(quest));  // kQuest
    return node;
}

/// Step the machine at the engine's own fixed 60 Hz, collecting everything.
std::vector<ScriptEvent> run_for(LevelScript& script, QuestState& quests, float seconds) {
    std::vector<ScriptEvent> events;
    const float step = 1.0f / 60.0f;
    for (float t = 0.0f; t < seconds; t += step) {
        script.update(step, {0.0f, 0.0f, 0.0f}, quests, events);
    }
    return events;
}

/// A flat floor, its passability grid and route grid; `setDestination` needs a pathfinder.
struct Ground {
    tl::core::CollisionWorld world;
    tl::core::PassabilityMap grid;
    tl::core::NavGrid nav;

    Ground(float x0, float z0, float x1, float z1) {
        world.add_triangle({x0, 0.0f, z0}, {x0, 0.0f, z1}, {x1, 0.0f, z1});
        world.add_triangle({x0, 0.0f, z0}, {x1, 0.0f, z1}, {x1, 0.0f, z0});
        world.build();
        grid.bake(world, nullptr);
        nav.build(grid);
    }

    void wire(LevelScript& script) {
        script.set_navigation(&nav);
        script.set_collision(&world);
        script.set_passability(&grid);
    }
};

size_t count_hides(const std::vector<ScriptEvent>& events, int64_t node) {
    size_t n = 0;
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::SetVisible && e.node_id == node && !e.on) ++n;
    }
    return n;
}

/// The dialogue events in a batch, opening or closing, floaty or boxed.
size_t count_dialogs(const std::vector<ScriptEvent>& events, bool on, bool floaty) {
    size_t n = 0;
    for (const ScriptEvent& e : events) {
        if (e.action != ScriptAction::Dialog) continue;
        if (e.on == on && e.floaty == floaty) ++n;
    }
    return n;
}

}  // namespace

// ------------------------------------------------------------------- paths

// A unit's second path starts from where the first left it, not from where it was placed.
TL_TEST(a_second_path_starts_the_unit_where_the_first_left_it) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Brink", "MercPet", 0.0f, 0.0f, 0.0f);
    add_path(scene, 3, "First", "MercPet", {0.0f, 0.0f, 10.0f});
    // Inside the active range (16) of the point looked at, the origin.
    add_path(scene, 4, "Second", "MercPet", {0.0f, 0.0f, 15.0f});

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Enable"}}});
    wiring.objects.push_back({1, 3, 0.0f, 0.0f, {{2, "End of Path Reached", "Enable"}}});
    wiring.objects.push_back({2, 4, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    Ground ground(-6.0f, -6.0f, 6.0f, 26.0f);
    LevelScript script;
    ground.wire(script);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    // Long enough for both legs at the fallback walking speed of 2.5.
    const std::vector<ScriptEvent> moves = run_for(script, quests, 12.0f);

    // Every step is forward. A jump back to the spawn shows as a z that
    // decreases somewhere along the way, which is exactly the defect.
    float previous = -1.0f;
    size_t steps = 0;
    float furthest = 0.0f;
    for (const ScriptEvent& e : moves) {
        if (e.action != ScriptAction::MoveUnit) continue;
        TL_CHECK(e.position.z >= previous - 1e-3f);
        previous = e.position.z;
        furthest = e.position.z;
        ++steps;
    }
    TL_CHECK(steps > 0);
    // Near the last waypoint: `setDestination` re-issued from the goal cell finds
    // no route (`findPath` is false for the start's own cell) and the path ends.
    TL_CHECK(furthest > 14.4f);
}

// A path whose one waypoint the unit stands on ends at once (`updatePathFollowing`)
// and does not turn it.
TL_TEST(a_path_that_moves_nobody_leaves_the_orientation_alone) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Brink", "MercPet", 5.0f, 0.0f, 5.0f);
    add_path(scene, 3, "Degenerate", "MercPet", {5.0f, 0.0f, 5.0f});
    add_monster(scene, 4, "After", "Marker", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Enable"}}});
    wiring.objects.push_back({1, 3, 0.0f, 0.0f, {{2, "End of Path Reached", "Hide"}}});
    wiring.objects.push_back({2, 4, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    // `Hide` on a monster is `setMeshVisible(false, false)`: it fades for a
    // second (`updateOpacity`) before it is hidden.
    const std::vector<ScriptEvent> moves = run_for(script, quests, 1.5f);

    for (const ScriptEvent& e : moves) {
        if (e.action != ScriptAction::MoveUnit) continue;
        TL_CHECK(!e.turned);
    }
    TL_CHECK_EQ(count_hides(moves, 4), size_t{1});
}

// A unit walks at its own authored speed, not at the player's.
TL_TEST(a_unit_walks_at_the_speed_its_own_dat_authors) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Syl", "seer", 0.0f, 0.0f, 0.0f);
    add_path(scene, 3, "Walk", "seer", {0.0f, 0.0f, 15.0f}, /*runs=*/false);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Enable"}}});
    wiring.objects.push_back({1, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    // The seer's own WALKINGSPEED, from media/units/monsters/NPC/TOWN_SEER.DAT.
    tl::core::UnitBook units;
    tl::core::UnitStats seer;
    seer.movement.walking_speed = 1.2f;
    seer.movement.running_speed = 8.0f;
    units.add("seer", seer);

    Ground ground(-6.0f, -6.0f, 6.0f, 20.0f);
    LevelScript script;
    ground.wire(script);
    script.set_units(&units);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    const std::vector<ScriptEvent> moves = run_for(script, quests, 1.0f);
    float reached = 0.0f;
    for (const ScriptEvent& e : moves) {
        if (e.action == ScriptAction::MoveUnit) reached = e.position.z;
    }
    // One second at 1.2, not at the player's 2.5.
    TL_CHECK(reached > 1.1f && reached < 1.3f);
}

// ----------------------------------------------------------------- dialogue

// `Interact` opens the dialog and holds `Interacted` back until it is over.
TL_TEST(interact_waits_for_the_dialog_before_firing_interacted) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "SeerAsksForHelp", "seer", "INTROTOGAMEPT1");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "SEER";
    line.texts = {"Brink! Over here!"};
    line.seconds = 4.0f;
    book.add("INTROTOGAMEPT1", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;

    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);

    TL_CHECK(script.dialog_open());
    TL_CHECK_EQ(script.dialog_text(), std::string("Brink! Over here!"));
    TL_CHECK_EQ(count_hides(opening, 3), size_t{0});

    // Still nothing: `DialogBook::box_time()` floors a boxed line at five seconds.
    const std::vector<ScriptEvent> early = run_for(script, quests, 4.5f);
    TL_CHECK_EQ(count_hides(early, 3), size_t{0});
    TL_CHECK(script.dialog_open());

    const std::vector<ScriptEvent> after = run_for(script, quests, 1.5f);
    TL_CHECK_EQ(count_hides(after, 3), size_t{1});
    TL_CHECK(!script.dialog_open());
}

// The player can click through it, which is what the original always does; the
// timeout only stands in for a player who is not watching.
TL_TEST(dismissing_a_dialog_releases_the_scene_at_once) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Talk", "seer", "Q");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "seer";
    line.texts = {"a long line nobody wants to sit through"};
    line.seconds = 60.0f;
    book.add("Q", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK(script.dialog_open());

    // Closing the menu is `CQuestController::menuEventOccured`;
    // `Interacted` comes on the next level update.
    std::vector<ScriptEvent> dismissed;
    script.dismiss_dialog(quests, dismissed);
    TL_CHECK(!script.dialog_open());
    TL_CHECK_EQ(count_hides(dismissed, 3), size_t{0});
    const std::vector<ScriptEvent> next = run_for(script, quests, 1.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 3), size_t{1});
}

// A controller whose unit has nothing to say: `CQuestDialogMenu::setNPC`'s error path
// sends menu event 1 at once, `Interacted` on the next update.
TL_TEST(a_controller_with_no_line_fires_interacted_at_once) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "BrinksSmallComments", "MercPet", "INTROTOGAMEPT1");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    add_monster(scene, 4, "Brink", "MercPet", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "SEER";
    line.texts = {"not Brink's line"};
    line.seconds = 4.0f;
    book.add("INTROTOGAMEPT1", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    TL_CHECK(!script.dialog_open());
    const std::vector<ScriptEvent> after = run_for(script, quests, 2.0f / 60.0f);
    TL_CHECK_EQ(count_hides(after, 3), size_t{1});
}

// Two lines opened in one frame are spoken one after the other rather than on
// top of each other, which is what "the scene reads as chaos" looks like.
TL_TEST(two_dialogs_queue_rather_than_overlapping) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "First", "seer", "A");
    add_quest_controller(scene, 3, "Second", "seer", "B");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back(
        {0,
         1,
         0.0f,
         0.0f,
         {{1, "Level Activated", "Interact"}, {2, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine a;
    a.section = "PASSIVE";
    a.unit = "seer";
    a.texts = {"first"};
    a.seconds = 2.0f;
    book.add("A", a);
    DialogLine b = a;
    b.texts = {"second"};
    b.seconds = 2.0f;
    book.add("B", b);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    // Past the five-second floor of `DialogBook::box_time()`, not the two-second recording.
    TL_CHECK_EQ(script.dialog_text(), std::string("first"));
    run_for(script, quests, 5.5f);
    TL_CHECK_EQ(script.dialog_text(), std::string("second"));
    run_for(script, quests, 5.5f);
    TL_CHECK(!script.dialog_open());
}

// The section follows the quest's state, and the line within it follows the
// controller's unit.
TL_TEST(the_dialog_section_follows_the_quests_state) {
    DialogBook book;
    DialogLine intro;
    intro.section = "INTRO";
    intro.unit = "SEER";
    intro.texts = {"offer"};
    book.add("Q", intro);

    DialogLine ret;
    ret.section = "RETURN";
    ret.unit = "SEER";
    ret.texts = {"come back"};
    book.add("Q", ret);

    DialogLine other;
    other.section = "INTRO";
    other.unit = "BRINK";
    other.texts = {"somebody else"};
    book.add("Q", other);

    const DialogLine* first = book.find("q", "seer", QuestStatus::Unknown);
    TL_CHECK(first != nullptr);
    TL_CHECK_EQ(first->texts[0], std::string("offer"));

    const DialogLine* again = book.find("Q", "Seer", QuestStatus::Active);
    TL_CHECK(again != nullptr);
    TL_CHECK_EQ(again->texts[0], std::string("come back"));

    // A unit with nothing to say gets nothing, never somebody else's line.
    TL_CHECK(book.find("Q", "Ratlin", QuestStatus::Unknown) == nullptr);
}

// A boxed line is held for its text reveal or its recording, whichever is longer, at least
// five seconds.
TL_TEST(a_boxed_line_is_held_for_the_reveal_or_the_recording) {
    // Short text, short bark: the five-second floor.
    TL_CHECK_EQ(DialogBook::box_time("hi", 1.15f), 5.0f);
    // 400 characters is 6 seconds of reveal, which beats a short recording.
    const float revealed = DialogBook::box_time(std::string(400, 'x'), 1.15f);
    TL_CHECK(revealed > 5.9f && revealed < 6.1f);
    // ... and a long recording beats the reveal.
    TL_CHECK_EQ(DialogBook::box_time(std::string(400, 'x'), 25.42f), 25.42f);
}

// ------------------------------------------------------------------- hunts

// `CMonster::idleAINormal` goes after what it sees on the other side (`CMonster::huntAI` ->
// `followCharacter`), stopping within `RANGE_MULTIPLIER * (ATTACK_RANGE + REACH_BONUS *
// scale + 0.2)`.
TL_TEST(hunt_closes_on_the_nearest_unit_on_the_other_side) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, -3785986764603190818, "Brink", "MercPet", 0.0f, 0.0f, 0.0f);
    add_monster(scene, -3785986382351101474, "Troll", "TrollGray", 0.0f, 0.0f, 6.0f);

    tl::core::UnitBook units;
    tl::core::UnitStats brink;
    brink.movement.running_speed = 6.5f;
    brink.sight_radius = 8.0f;
    brink.reach = 0.75f;
    brink.radius = 0.8f;
    brink.movement.walking_speed = 0.0f;  // no idle wander: only the chase moves him
    brink.alignment = tl::core::UnitStats::Alignment::Friendly;
    brink.mesh = "media/models/brink/brink";  // a model, so `selectAttack` finds his attack
    units.add("MercPet", brink);
    tl::core::UnitStats troll;
    troll.radius = 0.82f;
    // Hostile units run their idle AI too; this one stands still so where Brink
    // stops says only where Brink stops.
    troll.movement.running_speed = 0.0f;
    troll.movement.walking_speed = 0.0f;  // and no idle wander
    units.add("TrollGray", troll);

    Ground ground(-8.0f, -8.0f, 8.0f, 14.0f);
    LevelScript script;
    ground.wire(script);
    script.set_units(&units);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    script.arm_hostiles();

    const std::vector<ScriptEvent> moves = run_for(script, quests, 4.0f);
    const ScriptEvent* last = nullptr;
    size_t announced = 0;
    for (const ScriptEvent& e : moves) {
        if (e.action == ScriptAction::MoveUnit && e.node_id == -3785986764603190818) last = &e;
        if (e.action == ScriptAction::Hunt && e.on && !e.text.empty() &&
            e.node_id == -3785986764603190818) {
            ++announced;
        }
    }
    TL_CHECK_EQ(announced, size_t{1});
    TL_CHECK(last != nullptr);
    // In range between the centres; he stops within one AI tick of running past that.
    const float stop = 0.8f + 0.82f + 0.375f + 0.75f + 0.2f;
    TL_CHECK(last->position.z > 6.0f - stop - 0.02f);
    TL_CHECK(last->position.z <= 6.0f - stop + 6.5f * 0.1f + 0.02f);
    // And looking at it: +Z is a yaw of zero.
    TL_CHECK(last->facing > -0.05f && last->facing < 0.05f);
}

// `CLevel::findCharacterWithinView`: inside `MOTION_RADIUS` always; else within
// `SIGHT_RADIUS` and `VIEW_ANGLE`, with a grace under 2 units and 30 degrees.
TL_TEST(can_see_follows_find_character_within_view) {
    using tl::core::can_see;
    const tl::core::Vec3 o{0.0f, 0.0f, 0.0f};
    const float motion = 4.5f, view = 90.0f, sight = 7.0f;

    // Behind: exactly at the motion radius is still inside it.
    TL_CHECK(can_see(o, 0.0f, {0.0f, 0.0f, -4.5f}, motion, view, sight));
    TL_CHECK(!can_see(o, 0.0f, {0.0f, 0.0f, -5.0f}, motion, view, sight));
    // Ahead, up to the sight radius.
    TL_CHECK(can_see(o, 0.0f, {0.0f, 0.0f, 6.9f}, motion, view, sight));
    TL_CHECK(!can_see(o, 0.0f, {0.0f, 0.0f, 7.1f}, motion, view, sight));
    // `VIEW_ANGLE` is measured either side of facing, so 90 is the front half.
    TL_CHECK(can_see(o, 0.0f, {6.0f, 0.0f, 0.1f}, motion, view, sight));
    TL_CHECK(!can_see(o, 0.0f, {6.0f, 0.0f, -0.5f}, motion, view, sight));
    // Facing -Z turns the cone round with it.
    TL_CHECK(can_see(o, 3.14159265f, {0.0f, 0.0f, -6.0f}, motion, view, sight));
    TL_CHECK(!can_see(o, 3.14159265f, {0.0f, 0.0f, 6.0f}, motion, view, sight));

    // The grace, with no motion radius: under 30 degrees is seen; a cone of 0 gets none.
    TL_CHECK(can_see(o, 0.0f, {0.5f, 0.0f, 1.5f}, 0.0f, 10.0f, sight));
    TL_CHECK(!can_see(o, 0.0f, {1.0f, 0.0f, 1.0f}, 0.0f, 10.0f, sight));
    TL_CHECK(!can_see(o, 0.0f, {0.5f, 0.0f, 1.5f}, 0.0f, 0.0f, sight));
}

namespace {

/// A placed hostile at (0, 0, z) whose `FORWARD` is +Z or -Z, standing still.
struct Lookout {
    CmpScene scene;
    tl::core::UnitBook units;
    LevelScript script;
    QuestState quests;

    Lookout(float z, bool facing_player, bool scripted_hunt = false) {
        namespace f = tl::formats::cmp_field::monster;
        CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
        group.fields.push_back(uint_field(0));
        CmpNode& node = add_node(scene, tl::formats::cmp_type::kMonster, 2, "Ratlin");
        node.fields.push_back(vec3_field(0.0f, 0.0f, z));                             // kPosition
        node.fields.push_back(vec3_field(0.0f, 0.0f, facing_player ? -1.0f : 1.0f));  // kForward
        pad_to(node, f::kMonster);
        node.fields.push_back(text_field("Ratlin"));
        if (scripted_hunt) {
            tl::formats::CmpLogicGroup wiring;
            wiring.node_id = 1;
            // `Alert Monster` hands a unit the player (`CMonsterDescriptor::InputLogicEvent`
            // case 0x3d); `Hunt` (0x41) only sets the AI state.
            wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Alert Monster"}}});
            wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
            scene.logic_groups.push_back(std::move(wiring));
        }

        // The monster base's numbers; still, so distances stay put.
        tl::core::UnitStats ratlin;
        ratlin.movement.running_speed = 0.0f;
        ratlin.movement.walking_speed = 0.0f;  // and no idle wander
        units.add("Ratlin", ratlin);

        script.set_units(&units);
        // These units stand past the indoor active range on purpose; what is
        // tested is sight and follow radius, not `calculateActiveRange`.
        script.set_unit_active_range(1000.0f);
        script.build(scene, {});
        script.level_activated(quests, pending);
        if (!scripted_hunt) script.arm_hostiles();
    }

    /// What the level's activation said, counted by the first `run`.
    std::vector<ScriptEvent> pending;

    /// Hunt events over `seconds` with the player standing at (0, 0, player_z):
    /// +1 for each target taken, -1 for each given up.
    int run(float seconds, float player_z, std::string* text = nullptr) {
        std::vector<ScriptEvent> events;
        events.swap(pending);
        const float step = 1.0f / 60.0f;
        for (float t = 0.0f; t < seconds; t += step) {
            script.update(step, {0.0f, 0.0f, player_z}, quests, events);
        }
        int net = 0;
        for (const ScriptEvent& e : events) {
            if (e.action != ScriptAction::Hunt || e.node_id != 2) continue;
            if (e.on && !e.text.empty()) {
                ++net;
                if (text != nullptr) *text = e.text;
            } else if (!e.on) {
                --net;
            }
        }
        return net;
    }
};

}  // namespace

// The player is one candidate of `findCharacterWithinView`, seen only by that test, widened by
// the level's `ACTIVERANGEMOD`.
TL_TEST(a_hostile_notices_the_player_only_when_it_can_see_them) {
    Lookout facing_away(6.0f, /*facing_player=*/false);
    TL_CHECK_EQ(facing_away.run(1.0f, 0.0f), 0);

    Lookout facing(6.0f, /*facing_player=*/true);
    TL_CHECK_EQ(facing.run(1.0f, 0.0f), 1);

    Lookout far(30.0f, /*facing_player=*/true);
    TL_CHECK_EQ(far.run(2.0f, 0.0f), 0);

    Lookout boss_room(30.0f, /*facing_player=*/true);
    boss_room.script.set_active_range_mod(1000.0f);
    TL_CHECK_EQ(boss_room.run(2.0f, 0.0f), 1);
}

// `CMonster::huntAI` gives its target up at `FOLLOW_RADIUS` or further.
TL_TEST(a_hunter_gives_up_a_target_past_its_follow_radius) {
    Ground ground(-10.0f, -60.0f, 10.0f, 12.0f);
    Lookout ratlin(6.0f, /*facing_player=*/true);
    ground.wire(ratlin.script);
    tl::core::UnitStats runner;
    runner.movement.running_speed = 1.0f;
    runner.movement.walking_speed = 0.0f;
    ratlin.units.add("Ratlin", runner);
    TL_CHECK_EQ(ratlin.run(1.5f, 0.0f), 1);
    // Far past 18: given up within a hunt tick, and out of sight not taken again.
    TL_CHECK_EQ(ratlin.run(1.0f, -40.0f), -1);
}

// `Alert Monster` hands a monster the player (`CMonsterDescriptor::InputLogicEvent`);
// `MONSTER_HUNT`'s sight and view-angle bonuses let it see from anywhere.
TL_TEST(a_scripted_hunt_takes_the_player_from_anywhere) {
    Lookout ambusher(50.0f, /*facing_player=*/false, /*scripted_hunt=*/true);
    std::string text;
    TL_CHECK_EQ(ambusher.run(1.0f, 0.0f, &text), 1);
    TL_CHECK_EQ(text, std::string("the player"));
}

// A hostile a spawner releases without `TARGET PLAYER` still hunts - once it
// can see the player. It used to be given no AI at all and stood idle.
TL_TEST(a_spawned_hostile_hunts_without_target_player) {
    CmpScene scene;
    tl::core::UnitBook units;
    tl::core::UnitStats ratlin;
    ratlin.movement.running_speed = 0.0f;
    ratlin.movement.walking_speed = 0.0f;  // and no idle wander
    units.add("Ratlin", ratlin);
    LevelScript script;
    script.set_units(&units);
    script.build(scene, {});
    QuestState quests;
    // At (0, 0, 6) facing -Z, towards a player at the origin.
    script.add_spawned_unit(900, "Ratlin", {0.0f, 0.0f, 6.0f}, /*hunts=*/false, 3.14159265f);
    // The near-unit list is first built after 1.0 s (`CLevel::update`); the timer is
    // process-wide.
    LevelScript::reset_near_list_timer();
    std::vector<ScriptEvent> events;
    for (int i = 0; i < 90; ++i) script.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, quests, events);
    bool hunted = false;
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::Hunt && e.node_id == 900 && e.on && !e.text.empty())
            hunted = true;
    }
    TL_CHECK(hunted);
}

// `CCharacter::spawningAI`: a unit still in its entrance does nothing, and
// hunts once it is out.
TL_TEST(a_unit_in_its_entrance_does_not_hunt) {
    Lookout ratlin(6.0f, /*facing_player=*/true);
    ratlin.script.set_spawning(2, true);
    TL_CHECK_EQ(ratlin.run(1.5f, 0.0f), 0);
    ratlin.script.set_spawning(2, false);
    TL_CHECK_EQ(ratlin.run(1.0f, 0.0f), 1);
}

// `g_AI_INTERVALS` and the three bands `CCharacter::calculateActiveRange` compares against.
TL_TEST(ai_interval_follows_the_range_bands) {
    TL_CHECK_EQ(tl::core::ai_interval(0.0f), 0.1f);
    TL_CHECK_EQ(tl::core::ai_interval(10.0f), 0.1f);
    TL_CHECK_EQ(tl::core::ai_interval(10.01f), 0.2f);
    TL_CHECK_EQ(tl::core::ai_interval(15.0f), 0.2f);
    TL_CHECK_EQ(tl::core::ai_interval(15.01f), 0.4f);
    TL_CHECK_EQ(tl::core::ai_interval(20.0f), 0.4f);
    TL_CHECK_EQ(tl::core::ai_interval(20.01f), 0.8f);
    TL_CHECK_EQ(tl::core::ai_interval(500.0f), 0.8f);
}

// A hunter thinks on the AI clock (`CMonster::updateAI`) and moves every frame
// (`CCharacter::updatePathing`).
TL_TEST(a_hunt_thinks_on_the_ai_clock_and_walks_every_frame) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Brink", "MercPet", 0.0f, 0.0f, 30.0f);
    add_monster(scene, 3, "Troll", "TrollGray", 0.0f, 0.0f, 36.0f);

    tl::core::UnitBook units;
    tl::core::UnitStats brink;
    brink.movement.running_speed = 6.5f;
    brink.sight_radius = 8.0f;
    brink.reach = 0.75f;
    brink.radius = 0.8f;
    brink.movement.walking_speed = 0.0f;  // no idle wander: only the chase moves him
    brink.alignment = tl::core::UnitStats::Alignment::Friendly;
    brink.mesh = "media/models/brink/brink";  // a model, so `selectAttack` finds his attack
    units.add("MercPet", brink);
    tl::core::UnitStats troll;
    troll.radius = 0.82f;
    troll.movement.running_speed = 0.0f;
    troll.movement.walking_speed = 0.0f;
    units.add("TrollGray", troll);

    Ground ground(-8.0f, 22.0f, 8.0f, 44.0f);
    LevelScript script;
    ground.wire(script);
    script.set_units(&units);
    script.set_unit_active_range(1000.0f);  // the last AI band lies past 16
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> started;
    script.level_activated(quests, started);

    // Frame by frame: once Brink first moves, every frame until he stops
    // carries a move.
    std::vector<ScriptEvent> events;
    int first = -1;
    int last_frame = -1;
    int moved = 0;
    const ScriptEvent* last = nullptr;
    std::vector<ScriptEvent> all;
    for (int frame = 0; frame < 240; ++frame) {
        events.clear();
        script.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, quests, events);
        for (const ScriptEvent& e : events) {
            if (e.action != ScriptAction::MoveUnit || e.node_id != 2) continue;
            if (first < 0) first = frame;
            last_frame = frame;
            ++moved;
            all.push_back(e);
        }
    }
    TL_CHECK(first >= 0);
    TL_CHECK_EQ(moved, last_frame - first + 1);
    last = all.empty() ? nullptr : &all.back();
    TL_CHECK(last != nullptr);
    // In range at 2.945, noticed only on a thought; he never walks into the troll.
    const float stop = 0.8f + 0.82f + 0.375f + 0.75f + 0.2f;
    TL_CHECK(last->position.z > 36.0f - stop - 0.02f);
    TL_CHECK(last->position.z < 36.0f);
}

// `CLevel::updateNearUnitList` keeps, once a second, the characters within the
// active range of the point the camera looks at.
TL_TEST(a_unit_past_the_active_range_does_not_update) {
    Lookout ratlin(20.0f, /*facing_player=*/true);
    ratlin.script.set_unit_active_range(16.0f);
    tl::core::UnitStats sharp;
    sharp.movement.running_speed = 0.0f;
    sharp.movement.walking_speed = 0.0f;  // and no idle wander
    sharp.sight_radius = 30.0f;
    ratlin.units.add("Ratlin", sharp);
    // Let the level's once-a-second rebuild run with the player out of range.
    TL_CHECK_EQ(ratlin.run(2.1f, -100.0f), 0);
    TL_CHECK_EQ(ratlin.run(1.0f, 0.0f), 0);   // 20 away: still off the list
    TL_CHECK_EQ(ratlin.run(1.1f, 10.0f), 1);  // 10 away: back on, and hunting
}

// A path ending on a standing unit still ends: `CBaseUnit::addToAvoidanceMap` stops it short
// and `End of Path Reached` fires; units are not pushed apart.
TL_TEST(a_path_ending_on_a_standing_unit_still_ends) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Brink", "MercPet", 0.0f, 0.0f, 0.0f);
    add_monster(scene, 3, "Foreman", "Ratlin Foreman", 0.0f, 0.0f, 8.0f);
    add_path(scene, 4, "MakeBrinkAttack", "MercPet", {0.0f, 0.0f, 8.0f}, /*runs=*/true);
    add_monster(scene, 5, "After", "Marker", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Enable"}}});
    wiring.objects.push_back({1, 4, 0.0f, 0.0f, {{2, "End of Path Reached", "Hide"}}});
    wiring.objects.push_back({2, 5, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    tl::core::UnitBook units;
    tl::core::UnitStats brink;
    brink.alignment = tl::core::UnitStats::Alignment::Friendly;
    brink.sight_radius = 0.0f;
    brink.motion_radius = 0.0f;
    brink.movement.walking_speed = 1.9f;
    brink.movement.running_speed = 6.5f;
    units.add("MercPet", brink);
    tl::core::UnitStats foreman;
    foreman.sight_radius = 0.0f;
    foreman.motion_radius = 0.0f;
    foreman.movement.walking_speed = 0.0f;
    foreman.movement.running_speed = 0.0f;
    units.add("Ratlin Foreman", foreman);

    Ground ground(-6.0f, -6.0f, 6.0f, 14.0f);
    LevelScript script;
    ground.wire(script);
    script.set_units(&units);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    // One frame first, so the Foreman stands on the grid when the path is handed
    // over - as the town's Foremen have for twelve seconds.
    script.update(1.0f / 60.0f, {0.0f, 0.0f, -5.0f}, quests, events);
    script.level_activated(quests, events);

    const std::vector<ScriptEvent> moves = run_for(script, quests, 4.0f);
    TL_CHECK_EQ(count_hides(moves, 5), size_t{1});
    tl::core::Vec3 brink_at;
    tl::core::Vec3 foreman_at;
    TL_CHECK(script.unit_position(2, brink_at));
    TL_CHECK(script.unit_position(3, foreman_at));
    // The open cell nearest the point, outside the Foreman's circle, by the console's float
    // distance (`findNearestOpen`).
    const float gap = std::sqrt((brink_at.x - foreman_at.x) * (brink_at.x - foreman_at.x) +
                                (brink_at.z - foreman_at.z) * (brink_at.z - foreman_at.z));
    TL_CHECK(brink_at.z > 6.0f);
    TL_CHECK(gap > 0.5f && gap < 1.5f);
    TL_CHECK(std::fabs(foreman_at.z - 8.0f) < 1e-4f);
    for (const ScriptEvent& e : moves) {
        TL_CHECK(!(e.action == ScriptAction::MoveUnit && e.node_id == 3));
    }
}

// -------------------------------------------------------- timeline events

// A timeline's event key fires an input of the target's own list
// (`tl::formats::timeline_event_input()`). A stepped timeline resetting itself recurses (a
// STEP truncates the reset time to 0); it is cut at the logic depth cap as a wired cycle is.
TL_TEST(a_timeline_that_resets_itself_does_not_recurse_for_ever) {
    CmpScene scene;
    CmpNode& timeline = add_node(scene, tl::formats::cmp_type::kTimeline, 1, "Loop");
    timeline.fields.push_back(float_field(2.0f));  // 0 DURATION
    timeline.fields.push_back(float_field(0.5f));  // 1 STEP
    pad_to(timeline, 4);
    timeline.fields.push_back(uint_field(1));  // 4 START ON LOAD
    pad_to(timeline, 8);

    tl::formats::CmpTimeline keys;
    keys.node_id = 1;
    tl::formats::CmpTimelineTrack track;
    track.tag = 7;
    track.is_event = true;
    tl::formats::CmpTimelineKey at_start;
    at_start.time = 0.0f;
    track.keys.push_back(at_start);
    tl::formats::CmpTimelineKey at_quarter;
    at_quarter.time = 0.25f;
    track.keys.push_back(at_quarter);
    keys.targets.push_back({1, {track}});
    scene.timelines.push_back(std::move(keys));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    run_for(script, quests, 1.0f);
    TL_CHECK(script.busy());  // still playing, and the process still here
}

TL_TEST(a_timeline_event_track_fires_the_input_its_tag_names) {
    CmpScene scene;

    CmpNode& timeline = add_node(scene, tl::formats::cmp_type::kTimeline, 1, "Trap");
    timeline.fields.push_back(float_field(2.0f));  // 0 DURATION
    pad_to(timeline, 4);
    timeline.fields.push_back(uint_field(1));  // 4 START ON LOAD
    pad_to(timeline, 8);

    add_monster(scene, -3785986764603190818, "Ratlin", "Ratlin", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpTimeline keys;
    keys.node_id = 1;
    tl::formats::CmpTimelineTrack track;
    // Tag 11 on a `Monster` is `Hide`; tag 10 is `Show`.
    track.tag = 11;
    track.is_event = true;
    tl::formats::CmpTimelineKey at_half;
    at_half.time = 0.5f;
    track.keys.push_back(at_half);
    keys.targets.push_back({-3785986764603190818, {track}});
    scene.timelines.push_back(std::move(keys));

    LevelScript script;
    script.build(scene, {});
    TL_CHECK(script.busy());  // START ON LOAD

    QuestState quests;

    // Nothing before the key.
    const std::vector<ScriptEvent> early = run_for(script, quests, 0.5f);
    TL_CHECK_EQ(count_hides(early, -3785986764603190818), size_t{0});

    // And exactly once after it, not once per frame - once the second's fade
    // a monster's `Hide` takes is over.
    const std::vector<ScriptEvent> after = run_for(script, quests, 2.0f);
    TL_CHECK_EQ(count_hides(after, -3785986764603190818), size_t{1});

    // Nothing in the table is guessed at: a pair the archive never uses is
    // reported rather than invented.
    TL_CHECK(tl::formats::timeline_event_input(tl::formats::cmp_type::kMonster, 11) != nullptr);
    TL_CHECK(tl::formats::timeline_event_input(tl::formats::cmp_type::kMonster, 99) == nullptr);
    TL_CHECK_EQ(std::string(tl::formats::timeline_event_input(tl::formats::cmp_type::kSound, 0)),
                std::string("Play"));
}

// ---------------------------------------------------------- random choice

// `Roll` on a `Random Choice` picks a branch by weight, the same one on every target.
TL_TEST(a_random_choice_is_weighted_and_reproducible) {
    namespace f = tl::formats::cmp_field::random_choice;

    auto build_scene = [](CmpScene& scene) {
        CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
        group.fields.push_back(uint_field(0));

        CmpNode& choice = add_node(scene, 39, -6441741403707600418, "RandomWisp");
        choice.fields.push_back(uint_field(1));  // ENABLED
        pad_to(choice, f::kCount);
        choice.fields.push_back(uint_field(1));   // COUNT
        choice.fields.push_back(uint_field(10));  // ONE
        choice.fields.push_back(uint_field(0));   // TWO
        choice.fields.push_back(uint_field(0));   // THREE
        choice.fields.push_back(uint_field(0));   // FOUR
        choice.fields.push_back(uint_field(0));   // FIVE

        add_node(scene, tl::formats::cmp_type::kMonster, -3785986382351101474, "Wisp");

        tl::formats::CmpLogicGroup wiring;
        wiring.node_id = 1;
        wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Roll"}}});
        wiring.objects.push_back(
            {1, -6441741403707600418, 0.0f, 0.0f, {{2, "One", "Hide"}, {2, "Two", "Show"}}});
        wiring.objects.push_back({2, -3785986382351101474, 0.0f, 0.0f, {}});
        scene.logic_groups.push_back(std::move(wiring));
    };

    // Only `One` has any weight, so only `One` can be drawn.
    CmpScene scene;
    build_scene(scene);
    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK_EQ(count_hides(events, -3785986382351101474), size_t{1});

    // The same layout, built again, draws the same way: a level load reseeds
    // the stream the roll draws from (`game_random()`), not a clock.
    CmpScene again;
    build_scene(again);
    LevelScript twice;
    twice.build(again, {});
    QuestState quests_again;
    std::vector<ScriptEvent> repeat;
    twice.level_activated(quests_again, repeat);
    TL_CHECK_EQ(repeat.size(), events.size());
    for (size_t i = 0; i < repeat.size(); ++i) {
        TL_CHECK(repeat[i].action == events[i].action);
        TL_CHECK_EQ(repeat[i].node_id, events[i].node_id);
        TL_CHECK(repeat[i].on == events[i].on);
    }
}

// `COUNT` is how many picks one roll makes, with replacement: `CRandomChoice::roll`
// never removes a choice, so a branch can fire twice.
TL_TEST(a_random_choice_fires_count_branches) {
    namespace f = tl::formats::cmp_field::random_choice;
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));

    CmpNode& choice = add_node(scene, 39, -6441741403707600418, "Two of five");
    choice.fields.push_back(uint_field(1));  // ENABLED
    pad_to(choice, f::kCount);
    choice.fields.push_back(uint_field(2));  // COUNT
    for (int i = 0; i < 5; ++i) choice.fields.push_back(uint_field(1));

    add_node(scene, tl::formats::cmp_type::kMonster, -3785986382351101474, "A");

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Roll"}}});
    wiring.objects.push_back({1,
                              -6441741403707600418,
                              0.0f,
                              0.0f,
                              {{2, "One", "Hide"},
                               {2, "Two", "Hide"},
                               {2, "Three", "Hide"},
                               {2, "Four", "Hide"},
                               {2, "Five", "Hide"}}});
    wiring.objects.push_back({2, -3785986382351101474, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK_EQ(count_hides(events, -3785986382351101474), size_t{2});
}

// A `FLOATYTEXT` line floats over the speaker and holds nothing up
// (`CGameUI::forceInteractiveDialog` opens no menu for it).
TL_TEST(a_floaty_line_does_not_hold_the_scene_up) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "SeerAsksForHelp", "seer", "INTROTOGAMEPT1");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "SEER";
    line.texts = {"Brink! Over here! I can't hold them back much longer -"};
    // The recording is nearly four seconds and is deliberately *not* what times
    // a floaty line; the fade is.
    line.seconds = 3.96f;
    line.floaty = true;
    book.add("INTROTOGAMEPT1", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;

    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);

    // The line is shown, and shown as floaty...
    TL_CHECK_EQ(count_dialogs(opening, true, true), size_t{1});
    TL_CHECK_EQ(count_dialogs(opening, true, false), size_t{0});
    // ... the box is not open, so nothing gates the cursor or the click ...
    TL_CHECK(!script.dialog_open());
    // ... and the rest of the scene runs as the menu reports closed: its next
    // update, then the level update after.
    const std::vector<ScriptEvent> next = run_for(script, quests, 2.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 3), size_t{1});

    // It is still up most of the way through its fade, and gone after it.
    const std::vector<ScriptEvent> during = run_for(script, quests, 1.9f);
    TL_CHECK_EQ(count_dialogs(during, false, true), size_t{0});
    const std::vector<ScriptEvent> after = run_for(script, quests, 1.0f);
    TL_CHECK_EQ(count_dialogs(after, false, true), size_t{1});
}

// One floaty line at a time (`CQuestDialogMenu` holds one text event): a new one replaces it.
TL_TEST(a_second_floaty_line_replaces_the_first) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "First", "seer", "Q1");
    add_quest_controller(scene, 3, "Second", "MercPet", "Q2");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);
    add_monster(scene, 5, "Brink", "MercPet", 1.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Interact"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine syl;
    syl.section = "PASSIVE";
    syl.unit = "seer";
    syl.texts = {"Brink! Over here!"};
    syl.floaty = true;
    book.add("Q1", syl);
    DialogLine brink;
    brink.section = "PASSIVE";
    brink.unit = "MercPet";
    brink.texts = {"Help or stay out of the way!"};
    brink.floaty = true;
    book.add("Q2", brink);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    // The first line, and two updates later the second takes its place.
    std::vector<ScriptEvent> all = events;
    const std::vector<ScriptEvent> next = run_for(script, quests, 2.0f / 60.0f);
    all.insert(all.end(), next.begin(), next.end());
    TL_CHECK_EQ(count_dialogs(all, true, true), size_t{2});
    TL_CHECK_EQ(count_dialogs(all, false, true), size_t{1});

    // And only one is left to expire.
    const std::vector<ScriptEvent> after = run_for(script, quests, 4.0f);
    TL_CHECK_EQ(count_dialogs(after, false, true), size_t{1});
}

// A boxed line still waits, and a floaty one queued behind it does not have to.
TL_TEST(a_floaty_line_is_not_queued_behind_a_boxed_one) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Boxed", "seer", "Q1");
    add_quest_controller(scene, 3, "Floaty", "MercPet", "Q2");
    add_node(scene, tl::formats::cmp_type::kMonster, 4, "After");
    add_monster(scene, 5, "Syl", "seer", 0.0f, 0.0f, 0.0f);
    add_monster(scene, 6, "Brink", "MercPet", 1.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back(
        {0,
         1,
         0.0f,
         0.0f,
         {{1, "Level Activated", "Interact"}, {2, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {{3, "Interacted", "Hide"}}});
    wiring.objects.push_back({3, 4, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine boxed;
    boxed.section = "PASSIVE";
    boxed.unit = "seer";
    boxed.texts = {"a line that waits for a click"};
    boxed.seconds = 60.0f;
    book.add("Q1", boxed);
    DialogLine floaty;
    floaty.section = "PASSIVE";
    floaty.unit = "MercPet";
    floaty.texts = {"Help or stay out of the way!"};
    floaty.floaty = true;
    book.add("Q2", floaty);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);

    // The box is up and holding its own tail back...
    TL_CHECK(script.dialog_open());
    // ... while the floaty line shows at once; its `Interacted` waits out the box's pause
    // (`CGameClient::getIsPaused`).
    TL_CHECK_EQ(count_dialogs(events, true, true), size_t{1});
    std::vector<ScriptEvent> next = run_for(script, quests, 2.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 4), size_t{0});
    TL_CHECK(script.dialog_open());
    std::vector<ScriptEvent> answered;
    script.dismiss_dialog(quests, answered);
    next = run_for(script, quests, 2.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 4), size_t{1});
}

// A box pauses the level (`CGameClient::getIsPaused`); accepting (menu event 3)
// fires `Interacted` then `Interacted Accepted` on the next update.
TL_TEST(a_quest_box_pauses_the_level_until_it_is_answered) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Offer", "seer", "MAINQUEST");
    CmpNode& timer = add_node(scene, tl::formats::cmp_type::kTimer, 3, "Clock");
    timer.fields.push_back(uint_field(0));      // 0 ENABLED
    timer.fields.push_back(float_field(0.5f));  // 1 TIME
    pad_to(timer, 5);
    add_quest_controller(scene, 4, "TimerFired", "", "TIMERFIRED");
    add_quest_controller(scene, 5, "Accepted", "", "ACCEPTED");
    add_monster(scene, 6, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back(
        {0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}, {2, "Level Activated", "Enable"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{4, "Interacted Accepted", "Force Complete"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {{3, "Activated", "Force Complete"}}});
    wiring.objects.push_back({3, 4, 0.0f, 0.0f, {}});
    wiring.objects.push_back({4, 5, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine offer;
    offer.section = "INTRO";
    offer.unit = "seer";
    offer.texts = {"Please, catch up to Brink."};
    book.add("MAINQUEST", offer);

    LevelScript script;
    script.set_dialog(&book);
    script.set_dialogs_wait_for_player(true);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK(script.dialog_open());

    run_for(script, quests, 3.0f);
    TL_CHECK(script.dialog_open());
    TL_CHECK(!quests.is_complete("TIMERFIRED"));

    std::vector<ScriptEvent> answered;
    script.answer_dialog(true, quests, answered);
    bool accepted = false;
    for (const ScriptEvent& e : answered) {
        if (e.action == ScriptAction::Dialog && !e.on && e.offer && e.menu_event == 3)
            accepted = true;
    }
    TL_CHECK(accepted);
    run_for(script, quests, 1.0f / 60.0f);
    TL_CHECK(quests.is_complete("ACCEPTED"));
    TL_CHECK(!quests.is_complete("TIMERFIRED"));
    run_for(script, quests, 0.6f);
    TL_CHECK(quests.is_complete("TIMERFIRED"));
}

// A controller's `UNIT` may be a prop: `CQuestController::interactWithUnit` uses
// `CLevel::getBaseUnitByDataGroup`, over both unit lists.
TL_TEST(a_quest_controller_reads_a_prop_as_its_unit) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "TalkToBook");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "ReadBook", "OverseerJournalTrigger", "OVERSEER");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    CmpNode& book_prop = add_node(scene, tl::formats::cmp_type::kUnitTrigger, 4, "Book");
    book_prop.fields.push_back(vec3_field(0.0f, 0.0f, 0.0f));
    pad_to(book_prop, tl::formats::cmp_field::unit_trigger::kEnabled);
    book_prop.fields.push_back(uint_field(1));
    book_prop.fields.push_back(text_field("OverseerJournalTrigger"));

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 4, 0.0f, 0.0f, {{1, "Triggered", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "OverseerJournalTrigger";
    line.texts = {"The council has ordered the necropolis sealed."};
    line.seconds = 34.0f;
    book.add("OVERSEER", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK(!script.dialog_open());

    script.interact_unit_trigger(4, quests, events);
    // `Triggered` is the book's next `CTriggerUnit::update`.
    TL_CHECK(!script.dialog_open());
    script.update_unit_trigger(4, quests, events);
    TL_CHECK(script.dialog_open());
    TL_CHECK_EQ(script.dialog_text(),
                std::string("The council has ordered the necropolis sealed."));

    std::vector<ScriptEvent> dismissed;
    script.dismiss_dialog(quests, dismissed);
    const std::vector<ScriptEvent> next = run_for(script, quests, 1.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 3), size_t{1});
}

// A `Monster`'s `Interact` (`CMonsterDescriptor::InputLogicEvent`, case 0x30) opens
// its line; the close broadcasts `Interacted` on it (`CQuestDialogMenu::onClick`).
TL_TEST(a_monsters_interact_opens_its_line_and_interacted_follows_the_close) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "KillGhosts");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "ghost_minion1", "ghost_minion1", 0.0f, 0.0f, 0.0f);
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    DialogLine line;
    line.section = "PASSIVE";
    line.unit = "ghost_minion1";
    line.texts = {"Who dares interrupt our slumber?"};
    line.seconds = 5.0f;
    book.add("GHOSTSISTERS", line);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);

    TL_CHECK(script.dialog_open());
    TL_CHECK_EQ(script.dialog_text(), std::string("Who dares interrupt our slumber?"));
    TL_CHECK_EQ(count_hides(opening, 3), size_t{0});
    TL_CHECK(script.unimplemented().empty());

    // The unit hears the close in `onClick` itself, not a level update later.
    std::vector<ScriptEvent> dismissed;
    script.dismiss_dialog(quests, dismissed);
    TL_CHECK(!script.dialog_open());
    TL_CHECK_EQ(count_hides(dismissed, 3), size_t{1});
}

namespace {

/// A controller `Talk` on `seer` (quest `Q`), Syl placed as node 4, and the
/// outputs `wires` of Syl's own node, each hiding node 3.
CmpScene controller_on_syl(const std::vector<std::string>& wires) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Talk", "seer", "Q");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "After");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    tl::formats::CmpLogicObject syl{3, 4, 0.0f, 0.0f, {}};
    for (const std::string& wire : wires) syl.links.push_back({2, wire, "Hide"});
    wiring.objects.push_back(syl);
    scene.logic_groups.push_back(std::move(wiring));
    return scene;
}

DialogLine syl_line(const char* section) {
    DialogLine line;
    line.section = section;
    line.unit = "seer";
    line.texts = {"Thank you, friend."};
    line.seconds = 4.0f;
    return line;
}

}  // namespace

// A controller's line closes on its unit: `onClick` broadcasts `Interacted Closed`;
// `setNPC` turns the unit off while the box is up.
TL_TEST(a_controllers_line_closes_on_its_unit) {
    CmpScene scene = controller_on_syl({"Interacted Closed"});
    DialogBook book;
    book.add("Q", syl_line("PASSIVE"));

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);
    TL_CHECK(script.dialog_open());
    TL_CHECK(!script.node_enabled(4));

    std::vector<ScriptEvent> dismissed;
    script.dismiss_dialog(quests, dismissed);
    TL_CHECK(script.node_enabled(4));
    TL_CHECK_EQ(count_hides(dismissed, 3), size_t{1});
}

// The line comes from `CQuest::getQuestDialog(quest, unit)`; its section decides
// the buttons.
TL_TEST(a_controllers_line_is_the_quests_dialog_for_the_unit) {
    CmpScene scene = controller_on_syl({"Interacted Accepted"});
    DialogBook book;
    book.add("Q", syl_line("PASSIVE"));
    const DialogLine intro = syl_line("INTRO");

    LevelScript script;
    script.set_dialog(&book);
    std::string asked_quest;
    std::string asked_unit;
    script.set_quest_dialog([&](const std::string& quest, const std::string& unit,
                                tl::core::QuestSection* section) -> const DialogLine* {
        asked_quest = quest;
        asked_unit = unit;
        *section = tl::core::QuestSection::Intro;
        return &intro;
    });
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);
    TL_CHECK_EQ(asked_quest, std::string("Q"));
    TL_CHECK_EQ(asked_unit, std::string("seer"));
    bool offered = false;
    for (const ScriptEvent& e : opening) {
        if (e.action == ScriptAction::Dialog && e.on && !e.floaty) offered = e.offer;
    }
    TL_CHECK(offered);

    // Accepting: `Interacted Accepted` on Syl's node in `onClick` itself.
    std::vector<ScriptEvent> answered;
    script.answer_dialog(true, quests, answered);
    TL_CHECK_EQ(count_hides(answered, 3), size_t{1});
}

// `OK` on a `[COMPLETE]` line: the three broadcasts, `CQuestManager::completeQuest`, then
// two again.
TL_TEST(ok_on_a_complete_line_completes_the_quest_between_the_broadcasts) {
    CmpScene scene = controller_on_syl({"Interacted Accepted"});
    const DialogLine complete = syl_line("COMPLETE");

    LevelScript script;
    script.set_quest_dialog([&](const std::string&, const std::string&,
                                tl::core::QuestSection* section) -> const DialogLine* {
        *section = tl::core::QuestSection::Complete;
        return &complete;
    });
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);
    TL_CHECK(script.dialog_open());

    std::vector<ScriptEvent> dismissed;
    script.dismiss_dialog(quests, dismissed);
    size_t hide_at = dismissed.size();
    size_t complete_at = dismissed.size();
    size_t completions = 0;
    for (size_t i = 0; i < dismissed.size(); ++i) {
        const ScriptEvent& e = dismissed[i];
        if (e.action == ScriptAction::SetVisible && e.node_id == 3 && !e.on &&
            hide_at == dismissed.size()) {
            hide_at = i;
        }
        if (e.action == ScriptAction::CompleteQuest) {
            ++completions;
            complete_at = i;
            TL_CHECK_EQ(e.quest, std::string("Q"));
        }
    }
    TL_CHECK_EQ(completions, size_t{1});
    TL_CHECK(hide_at < complete_at);
}

// No line for the unit: `setNPC`'s error path broadcasts all three at once, with no box.
TL_TEST(no_line_for_the_unit_broadcasts_on_it_at_once) {
    CmpScene scene = controller_on_syl({"Interacted Accepted"});
    DialogBook book;
    DialogLine other = syl_line("PASSIVE");
    other.unit = "MercPet";
    book.add("Q", other);

    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);
    TL_CHECK(!script.dialog_open());
    TL_CHECK(script.node_enabled(4));
    TL_CHECK_EQ(count_hides(opening, 3), size_t{1});
}

// The close button on a `FORCEACCEPT` offer (`handle_CloseButton`) accepts, then
// closes: no `Declined`.
TL_TEST(the_close_button_on_a_forced_offer_accepts_then_closes) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Talk", "seer", "Q");
    add_node(scene, tl::formats::cmp_type::kMonster, 3, "SylAccepted");
    add_monster(scene, 4, "Syl", "seer", 0.0f, 0.0f, 0.0f);
    add_node(scene, tl::formats::cmp_type::kMonster, 5, "SylDeclined");
    add_node(scene, tl::formats::cmp_type::kMonster, 6, "TalkInteracted");
    add_node(scene, tl::formats::cmp_type::kMonster, 7, "TalkAccepted");

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Interact"}}});
    wiring.objects.push_back(
        {1, 2, 0.0f, 0.0f, {{5, "Interacted", "Hide"}, {6, "Interacted Accepted", "Hide"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {}});
    wiring.objects.push_back(
        {3,
         4,
         0.0f,
         0.0f,
         {{2, "Interacted Accepted", "Hide"}, {4, "Interacted Declined", "Hide"}}});
    wiring.objects.push_back({4, 5, 0.0f, 0.0f, {}});
    wiring.objects.push_back({5, 6, 0.0f, 0.0f, {}});
    wiring.objects.push_back({6, 7, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    DialogBook book;
    book.add("Q", syl_line("INTRO"));
    LevelScript script;
    script.set_dialog(&book);
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> opening;
    script.level_activated(quests, opening);
    TL_CHECK(script.dialog_open());

    std::vector<ScriptEvent> closed;
    script.close_button_dialog(quests, closed);
    TL_CHECK(!script.dialog_open());
    TL_CHECK(script.node_enabled(4));
    TL_CHECK_EQ(count_hides(closed, 3), size_t{1});
    TL_CHECK_EQ(count_hides(closed, 5), size_t{0});
    const std::vector<ScriptEvent> next = run_for(script, quests, 1.0f / 60.0f);
    TL_CHECK_EQ(count_hides(next, 6), size_t{1});
    TL_CHECK_EQ(count_hides(next, 7), size_t{0});
}

// `Force Not Accepted`: a held quest leaves the list; `Force Not Complete`
// (-> `resetQuest`): Quest Not Complete whatever the state.
TL_TEST(force_not_accepted_and_not_complete_reset_the_quest) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Drop", "", "Q");
    add_quest_controller(scene, 3, "Reset", "", "Q");
    add_quest_controller(scene, 4, "Listener", "", "Q");
    add_node(scene, tl::formats::cmp_type::kMonster, 5, "NotActive");
    add_node(scene, tl::formats::cmp_type::kMonster, 6, "NotComplete");
    add_node(scene, tl::formats::cmp_type::kMonster, 7, "PokeDrop");
    add_node(scene, tl::formats::cmp_type::kMonster, 8, "PokeReset");

    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 2, 0.0f, 0.0f, {}});
    wiring.objects.push_back({1, 3, 0.0f, 0.0f, {}});
    wiring.objects.push_back(
        {2, 4, 0.0f, 0.0f, {{3, "Quest Not Active", "Hide"}, {4, "Quest Not Complete", "Hide"}}});
    wiring.objects.push_back({3, 5, 0.0f, 0.0f, {}});
    wiring.objects.push_back({4, 6, 0.0f, 0.0f, {}});
    wiring.objects.push_back({5, 7, 0.0f, 0.0f, {{0, "Interacted", "Force Not Accepted"}}});
    wiring.objects.push_back({6, 8, 0.0f, 0.0f, {{1, "Interacted", "Force Not Complete"}}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;

    // Not held: Force Not Accepted does nothing.
    std::vector<ScriptEvent> none;
    script.raise_output(7, tl::core::logic::kInteracted, quests, none);
    TL_CHECK_EQ(count_hides(none, 5), size_t{0});

    quests.activate("Q");
    std::vector<ScriptEvent> dropped;
    script.raise_output(7, tl::core::logic::kInteracted, quests, dropped);
    TL_CHECK_EQ(count_hides(dropped, 5), size_t{1});
    TL_CHECK(quests.status("Q") == QuestStatus::Unknown);

    quests.complete("Q");
    std::vector<ScriptEvent> reset;
    script.raise_output(8, tl::core::logic::kInteracted, quests, reset);
    TL_CHECK_EQ(count_hides(reset, 6), size_t{1});
    TL_CHECK(quests.status("Q") == QuestStatus::Unknown);
    TL_CHECK(script.unimplemented().empty());
}

// The player's own talk (`CGameUI::updateIngameUI` state 0x1e): the same menu on the unit.
TL_TEST(the_players_talk_holds_its_unit_and_closes_on_it) {
    CmpScene scene = controller_on_syl({"Interacted Declined"});
    LevelScript script;
    script.build(scene, {});
    QuestState quests;

    script.menu_opened_on(4);
    TL_CHECK(!script.node_enabled(4));
    std::vector<ScriptEvent> closed;
    script.menu_closed_on(4, 4, false, "Q", quests, closed);
    TL_CHECK(script.node_enabled(4));
    TL_CHECK_EQ(count_hides(closed, 3), size_t{1});
    for (const ScriptEvent& e : closed) TL_CHECK(e.action != ScriptAction::CompleteQuest);
}

// `CCharacter::update`: past the active range but in the near range, a character
// fades out over a second and hides.
TL_TEST(a_character_past_the_active_range_fades_out_and_hides) {
    CmpScene scene;
    add_monster(scene, 2, "Far", "Marker", 0.0f, 0.0f, 20.0f);
    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    const std::vector<ScriptEvent> early = run_for(script, quests, 0.5f);
    TL_CHECK_EQ(count_hides(early, 2), size_t{0});
    const std::vector<ScriptEvent> later = run_for(script, quests, 1.5f);
    TL_CHECK_EQ(count_hides(later, 2), size_t{1});
    TL_CHECK(script.unit_opacity(2) == 0.0f);
}

// The camera test: a character the camera does not see is hidden at once
// (`setVisible(false, true)`), and comes back through the fade once it does.
TL_TEST(a_character_out_of_the_camera_is_hidden_at_once_and_fades_back) {
    CmpScene scene;
    add_monster(scene, 2, "Near", "Marker", 0.0f, 0.0f, 5.0f);
    LevelScript script;
    bool seen = false;
    script.set_view_test([&seen](const LevelScript&, int64_t) { return seen; });
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    const std::vector<ScriptEvent> unseen = run_for(script, quests, 0.1f);
    TL_CHECK_EQ(count_hides(unseen, 2), size_t{1});
    TL_CHECK(script.unit_opacity(2) == 0.0f);
    seen = true;
    run_for(script, quests, 0.3f);
    const float part = script.unit_opacity(2);
    TL_CHECK(0.0f < part && part < 1.0f);
    run_for(script, quests, 1.0f);
    TL_CHECK(script.unit_opacity(2) == 1.0f);
}

// A faded far body is deleted only at 30 frames a second or fewer or under
// `DESTROY DEAD CORPSE`. Kept.
TL_TEST(a_far_body_is_deleted_only_at_thirty_frames_a_second) {
    for (const float fps : {60.0f, 30.0f}) {
        CmpScene scene;
        add_monster(scene, 2, "Body", "Marker", 0.0f, 0.0f, 20.0f);
        LevelScript script;
        script.set_average_fps(fps);
        script.build(scene, {});
        QuestState quests;
        std::vector<ScriptEvent> events;
        script.level_activated(quests, events);
        script.unit_killed(2, quests, events);
        run_for(script, quests, 3.0f);
        TL_CHECK(script.unit_opacity(2) == (fps == 60.0f ? 0.0f : 1.0f));
        float hp = 0.0f;
        float max = 0.0f;
        TL_CHECK_EQ(script.unit_health(2, hp, max), fps == 60.0f);
    }
}

// A monster's summon past 25 from its master is put back 20 from it (`CMonster::idleAI`
// `teleportToMaster`).
TL_TEST(a_monster_summon_is_put_back_by_its_master) {
    CmpScene scene;
    // Well away from the player at the origin: a character with a master sees
    // 35 (`CCharacter::sightRange`), and would hunt it.
    add_monster(scene, 2, "Shaman", "Marker", 100.0f, 0.0f, 12.0f);
    add_monster(scene, 3, "Goblin", "Marker", 100.0f, 0.0f, -14.0f);
    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    TL_CHECK(script.set_unit_master(3, script, 2, quests, events));
    TL_CHECK(!script.set_unit_master(3, script, 99, quests, events));
    TL_CHECK_EQ(script.pets_of(&script, 2).size(), size_t{1});
    run_for(script, quests, 1.0f);
    float hp = 0.0f;
    float max = 0.0f;
    TL_CHECK(script.unit_health(3, hp, max));
    tl::core::Vec3 pet;
    tl::core::Vec3 master;
    TL_CHECK(script.unit_position(3, pet));
    TL_CHECK(script.unit_position(2, master));
    const float dz = master.z - pet.z;
    TL_CHECK(dz > 19.0f && dz < 21.0f);
    // A pet that dies leaves the list (`die` -> `removePet`).
    script.unit_killed(3, quests, events);
    TL_CHECK(script.pets_of(&script, 2).empty());
}

// A controller hears `CQuestManager`'s notices for its own quest, in order.
TL_TEST(a_quest_controller_hears_its_quests_notices) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_quest_controller(scene, 2, "Listener", "seer", "INTROTOGAMEPT1");
    add_quest_controller(scene, 3, "Other", "seer", "CAVEPT1");
    add_monster(scene, 4, "Gone", "Marker", 0.0f, 0.0f, 0.0f);
    add_monster(scene, 5, "Stays", "Marker", 0.0f, 0.0f, 2.0f);
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 2, 0.0f, 0.0f, {{2, "Quest Abandoned", "Hide"}}});
    wiring.objects.push_back({1, 3, 0.0f, 0.0f, {{3, "Quest Abandoned", "Hide"}}});
    wiring.objects.push_back({2, 4, 0.0f, 0.0f, {}});
    wiring.objects.push_back({3, 5, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    events.clear();
    script.quest_notice(tl::core::QuestNotice{"INTROTOGAMEPT1", {0x63, 0x66}}, quests, events);
    const std::vector<ScriptEvent> after = run_for(script, quests, 1.5f);
    events.insert(events.end(), after.begin(), after.end());
    TL_CHECK(count_hides(events, 4) >= size_t{1});
    TL_CHECK_EQ(count_hides(events, 5), size_t{0});
}

// `Kill Pets` (`CMonsterDescriptor::InputLogicEvent`, case 0x53): each `die` leaves
// the list as the index steps on, so every other pet dies. Kept.
TL_TEST(kill_pets_kills_every_other_pet_as_the_original) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "Lich", "Marker", 0.0f, 0.0f, 0.0f);
    for (int64_t id = 3; id <= 6; ++id) {
        add_monster(scene, id, "Skeleton", "Marker", 0.0f, 0.0f, static_cast<float>(id));
    }
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Kill Pets"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    for (int64_t id = 3; id <= 6; ++id)
        TL_CHECK(script.set_unit_master(id, script, 2, quests, events));
    TL_CHECK_EQ(script.pets_of(&script, 2).size(), size_t{4});
    events.clear();
    script.level_activated(quests, events);
    TL_CHECK(script.unit_is_dead(3));
    TL_CHECK(!script.unit_is_dead(4));
    TL_CHECK(script.unit_is_dead(5));
    TL_CHECK(!script.unit_is_dead(6));
    TL_CHECK(!script.unit_is_dead(2));
    TL_CHECK_EQ(script.pets_of(&script, 2).size(), size_t{2});
}

// `Add as Pet` / `Remove as Pet` go to the scene; `removePet` leaves the master.
TL_TEST(add_as_pet_asks_the_scene_and_remove_leaves_the_master) {
    CmpScene scene;
    CmpNode& group = add_node(scene, tl::formats::cmp_type::kLogicGroup, 1, "Logic");
    group.fields.push_back(uint_field(0));
    add_monster(scene, 2, "MercPet", "Marker", 0.0f, 0.0f, 3.0f);
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 1;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Level Activated", "Add as Pet"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.level_activated(quests, events);
    size_t asked = 0;
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::PlayerPet && e.node_id == 2 && e.on) ++asked;
    }
    TL_CHECK_EQ(asked, size_t{1});

    TL_CHECK(script.make_player_pet(2, quests, events));
    TL_CHECK(script.unit_listed_as_player_pet(2));
    TL_CHECK_EQ(script.player_pets().size(), size_t{1});
    TL_CHECK(script.remove_player_pet(2));
    TL_CHECK(!script.remove_player_pet(2));
    TL_CHECK(!script.unit_listed_as_player_pet(2));
    TL_CHECK(script.player_pets().empty());
    TL_CHECK(script.unit_is_player_pet(2));
}
