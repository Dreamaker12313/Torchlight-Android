// The `Cinematic` node (`CCinematicObject`, core/script.cpp): `Play` opens its
// `[CINEMATIC]` and listens to the cinematic menu; the menu's close (event 2)
// fires the node's output 0x5c on the next level update. The scenes are made up.

#include "core/quest.h"
#include "core/script.h"
#include "formats/cmp.h"
#include "test_support.h"

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

CmpField word(uint32_t value) {
    CmpField f;
    f.words.push_back(value);
    return f;
}

CmpField text(const std::string& s) {
    CmpField f;
    std::vector<uint16_t> units(s.begin(), s.end());
    units.push_back(0);
    if (units.size() % 2 != 0) units.push_back(0);
    for (size_t i = 0; i < units.size(); i += 2) {
        f.words.push_back(static_cast<uint32_t>(units[i]) |
                          (static_cast<uint32_t>(units[i + 1]) << 16));
    }
    return f;
}

CmpField vec3(float x, float y, float z) {
    CmpField f;
    for (float v : {x, y, z}) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof bits);
        f.words.push_back(bits);
    }
    return f;
}

CmpNode& add(CmpScene& scene, uint32_t type, int64_t id) {
    CmpNode node;
    node.type = type;
    node.id = id;
    node.name = "node" + std::to_string(id);
    scene.nodes.push_back(std::move(node));
    return scene.nodes.back();
}

/// A sphere trigger at the origin `Play`s a `Cinematic` of `name`, whose
/// output 0x5c (`Interacted`) completes `SEEN` through a counter of 1.
CmpScene scene_of(const std::string& name) {
    namespace t = tl::formats::cmp_type;
    CmpScene scene;
    CmpNode& trigger = add(scene, t::kPlayerSphereTrigger, 1);
    trigger.fields = {vec3(0.0f, 0.0f, 0.0f), word(1), word(0x40400000)};  // radius 3

    CmpNode& cinematic = add(scene, t::kCinematic, 2);
    cinematic.fields.resize(tl::formats::cmp_field::cinematic::kCinematic + 1);
    cinematic.fields[tl::formats::cmp_field::cinematic::kCinematic] = text(name);

    CmpNode& counter = add(scene, t::kCounter, 3);
    counter.fields = {word(1), word(1), word(0), text("")};

    CmpNode& quest = add(scene, t::kQuestController, 4);
    quest.fields.resize(tl::formats::cmp_field::quest_controller::kQuest);
    quest.fields.push_back(text("SEEN"));

    CmpNode& group = add(scene, t::kLogicGroup, 5);
    group.fields.push_back(word(0));
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 5;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Triggered", "Play"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "Interacted", "Add"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {{3, "Activated", "Force Complete"}}});
    wiring.objects.push_back({3, 4, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));
    return scene;
}

bool opened(const std::vector<ScriptEvent>& events, const std::string& name) {
    for (const ScriptEvent& e : events) {
        if (e.action == ScriptAction::OpenCinematic && e.text == name) return true;
    }
    return false;
}

}  // namespace

// `Play` opens it; nothing fires until the menu closes, then 0x5c on the next
// update.
TL_TEST(cinematic_node_fires_once_its_menu_closes) {
    LevelScript script;
    script.build(scene_of("ACT1_INTRO"), {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(opened(events, "ACT1_INTRO"));
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(!quests.is_complete("SEEN"));
    script.cinematic_menu_event(2);
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(quests.is_complete("SEEN"));
}

// An event other than 2, 3 or 4 lets the node go without firing.
TL_TEST(cinematic_node_ignores_other_menu_events) {
    LevelScript script;
    script.build(scene_of("ACT1_INTRO"), {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    script.cinematic_menu_event(1);
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(!quests.is_complete("SEEN"));
    // It has stopped listening: a later close reaches it no more.
    script.cinematic_menu_event(2);
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(!quests.is_complete("SEEN"));
}

// An empty `CINEMATIC` opens nothing and listens to nothing.
TL_TEST(cinematic_node_with_no_name_does_nothing) {
    LevelScript script;
    script.build(scene_of(""), {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(!opened(events, ""));
    script.cinematic_menu_event(2);
    script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
    TL_CHECK(!quests.is_complete("SEEN"));
}
