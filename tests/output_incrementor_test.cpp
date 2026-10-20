// The `Output Incrementor` node (`COutputIncrementor`): `increment` fires the count's output,
// wraps at `MAX OUTPUT`, stops when its loops are spent; `Set_setMaxIncrement` caps it.

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

/// Enter a sphere trigger `times` times, each an `Increment`; true when the counter fed by
/// `First Increment` completed `COUNTED` at 2.
bool counted_after(int times, uint32_t max, uint32_t loops) {
    namespace t = tl::formats::cmp_type;
    namespace fi = tl::formats::cmp_field::output_incrementor;
    CmpScene scene;
    CmpNode& trigger = add(scene, t::kPlayerSphereTrigger, 1);
    trigger.fields = {vec3(0.0f, 0.0f, 0.0f), word(1), word(0x40400000)};  // radius 3

    CmpNode& incrementor = add(scene, 43, 2);
    incrementor.fields.resize(4);
    incrementor.fields[fi::kEnabled] = word(1);
    incrementor.fields[fi::kMaxOutput] = word(max);
    incrementor.fields[fi::kLoopCount] = word(loops);
    incrementor.fields[fi::kLoopsForever] = word(0);

    CmpNode& counter = add(scene, t::kCounter, 3);
    counter.fields = {word(1), word(2), word(0), text("")};

    CmpNode& quest = add(scene, t::kQuestController, 4);
    quest.fields.resize(tl::formats::cmp_field::quest_controller::kQuest);
    quest.fields.push_back(text("COUNTED"));

    CmpNode& group = add(scene, t::kLogicGroup, 5);
    group.fields.push_back(word(0));
    tl::formats::CmpLogicGroup wiring;
    wiring.node_id = 5;
    wiring.objects.push_back({0, 1, 0.0f, 0.0f, {{1, "Triggered", "Increment"}}});
    wiring.objects.push_back({1, 2, 0.0f, 0.0f, {{2, "First Increment", "Add"}}});
    wiring.objects.push_back({2, 3, 0.0f, 0.0f, {{3, "Activated", "Force Complete"}}});
    wiring.objects.push_back({3, 4, 0.0f, 0.0f, {}});
    scene.logic_groups.push_back(std::move(wiring));

    LevelScript script;
    script.build(scene, {});
    QuestState quests;
    std::vector<ScriptEvent> events;
    for (int i = 0; i < times; ++i) {
        script.update(0.016f, {0.0f, 0.0f, 0.0f}, quests, events);
        script.update(0.016f, {100.0f, 0.0f, 0.0f}, quests, events);
    }
    return quests.is_complete("COUNTED");
}

}  // namespace

// A maximum of 2 and one loop: entries 1 and 2 are its first and second
// increment, then it has no loop left - a third entry fires nothing.
TL_TEST(output_incrementor_stops_when_its_loops_are_spent) {
    TL_CHECK(!counted_after(3, 2, 1));
    TL_CHECK(!counted_after(5, 2, 1));
}

// Two loops: the count starts again at 2, so the third entry is a first
// increment again.
TL_TEST(output_incrementor_starts_again_at_its_maximum) {
    TL_CHECK(!counted_after(2, 2, 2));
    TL_CHECK(counted_after(3, 2, 2));
}

// `Set_setMaxIncrement` keeps a maximum under 6, 5 otherwise: with 9 and one loop the
// sixth entry fires nothing.
TL_TEST(output_incrementor_maximum_is_five_at_most) {
    TL_CHECK(!counted_after(7, 9, 1));
    TL_CHECK(counted_after(6, 9, 2));
}
