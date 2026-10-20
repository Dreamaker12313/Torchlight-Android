// A character's target item (`core/item_ai.h`): `inInteractionRange`, `getItemAI`'s
// choice of step, and the console's second pick.
#include "core/item_ai.h"
#include "test_support.h"

#include <vector>

namespace {

using tl::core::ItemAICharacter;
using tl::core::ItemAIStep;
using tl::core::ItemAITarget;
using tl::core::SecondPickCandidate;
using tl::core::Vec3;

ItemAITarget item_at(float x, float z) {
    ItemAITarget item;
    item.at = Vec3{x, 0.0f, z};
    return item;
}

ItemAICharacter character_at(float x, float z) {
    ItemAICharacter me;
    me.at = Vec3{x, 0.0f, z};
    return me;
}

SecondPickCandidate local(float x, float z, bool in_sight = true, bool fishing = false) {
    SecondPickCandidate c;
    c.local_x = x;
    c.local_z = z;
    c.in_sight = in_sight;
    c.fishing_hole = fishing;
    return c;
}

}  // namespace

TL_TEST(interaction_range_is_the_gap_between_the_bodies) {
    // `inInteractionRange`: distance less both radii, within 1.75.
    TL_CHECK(tl::core::in_interaction_range(2.3f, 0.3f, 0.3f, false, false));
    TL_CHECK(!tl::core::in_interaction_range(2.4f, 0.3f, 0.3f, false, false));
    // A fishing hole: 3.45 standing, 2.95 walking.
    TL_CHECK(tl::core::in_interaction_range(4.0f, 0.3f, 0.3f, true, false));
    TL_CHECK(!tl::core::in_interaction_range(4.0f, 0.3f, 0.3f, true, true));
    TL_CHECK(tl::core::in_interaction_range(3.5f, 0.3f, 0.3f, true, true));
    // The height does not count.
    TL_CHECK_EQ(tl::core::item_ground_distance(Vec3{0, 0, 0}, Vec3{3, 9, 4}), 5.0f);
}

TL_TEST(item_ai_takes_a_floor_item_in_reach_without_sight_or_turn) {
    // `getItemAI`: not an interactable - `getItem` once in reach.
    ItemAICharacter me = character_at(0, 0);
    me.line_of_sight = false;
    me.facing = false;
    TL_CHECK(tl::core::item_ai(item_at(2.0f, 0), me) == ItemAIStep::Take);
    // Out of reach, not walking: a route; walking: wait.
    TL_CHECK(tl::core::item_ai(item_at(5.0f, 0), me) == ItemAIStep::Walk);
    me.walking = true;
    TL_CHECK(tl::core::item_ai(item_at(5.0f, 0), me) == ItemAIStep::Wait);
}

TL_TEST(item_ai_uses_an_interactable_in_reach_sight_and_facing) {
    ItemAITarget door = item_at(1.5f, 0);
    door.interactable = true;
    ItemAICharacter me = character_at(0, 0);
    TL_CHECK(tl::core::item_ai(door, me) == ItemAIStep::Interact);
    // Not faced yet: stopped, turning.
    me.facing = false;
    TL_CHECK(tl::core::item_ai(door, me) == ItemAIStep::Turn);
    // A blocked sight line: in reach but walked to (farther than its radius).
    me.facing = true;
    me.line_of_sight = false;
    TL_CHECK(tl::core::item_ai(door, me) == ItemAIStep::Walk);
    // So close that walking cannot help, and no sight: given up.
    me.radius = 2.0f;
    TL_CHECK(tl::core::item_ai(door, me) == ItemAIStep::GiveUp);
}

TL_TEST(item_ai_routes_by_type_once_faced) {
    ItemAICharacter me = character_at(0, 0);
    ItemAITarget hole = item_at(3.5f, 0);
    hole.interactable = true;
    hole.fishing_hole = true;
    TL_CHECK(tl::core::item_ai(hole, me) == ItemAIStep::Fish);
    ItemAITarget shrine = item_at(1.0f, 0);
    shrine.interactable = true;
    shrine.enchant_shrine = true;
    TL_CHECK(tl::core::item_ai(shrine, me) == ItemAIStep::EnchantShrine);
    shrine.enchant_shrine = false;
    shrine.gem_shrine = true;
    TL_CHECK(tl::core::item_ai(shrine, me) == ItemAIStep::GemShrine);
    // A container is never taken or used: struck when breakable and in attack
    // range, else walked to.
    ItemAITarget chest = item_at(1.0f, 0);
    chest.container = true;
    TL_CHECK(tl::core::item_ai(chest, me) == ItemAIStep::Walk);
    chest.breakable = true;
    me.in_attack_range = true;
    TL_CHECK(tl::core::item_ai(chest, me) == ItemAIStep::Attack);
}

TL_TEST(second_pick_takes_the_nearest_in_front) {
    // within 60 degrees and 3, the nearest; behind is passed
    // over while anything is in front.
    const std::vector<SecondPickCandidate> picks = {local(0.0f, 2.5f), local(0.5f, 1.5f),
                                                    local(0.0f, -1.0f), local(2.9f, 0.5f)};
    TL_CHECK_EQ(tl::core::second_pick(picks), 1);
    // Past 3 in front, or wider than 60 degrees: not in the first pass.
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, 3.1f), local(2.0f, 1.0f)}), -1);
}

TL_TEST(second_pick_touch_radius_sight_and_fishing_hole) {
    // Within 0.5 in any direction counts in the first pass.
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, 2.0f), local(0.0f, -0.4f)}), 1);
    // A blocked sight line adds 10: the clear one wins though farther.
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, 1.0f, false), local(0.0f, 2.5f)}), 1);
    // A fishing hole counts 0.8 nearer and is accepted out to 3.8.
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, 3.7f, true, true)}), 0);
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, 2.0f), local(0.0f, 2.7f, true, true)}), 1);
    // A tie keeps the first.
    TL_CHECK_EQ(tl::core::second_pick({local(1.0f, 1.0f), local(-1.0f, 1.0f)}), 0);
}

TL_TEST(second_pick_falls_back_to_anything_within_two) {
    // nothing in front - within 2.0 in any direction.
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, -1.9f), local(0.0f, -1.2f)}), 1);
    TL_CHECK_EQ(tl::core::second_pick({local(0.0f, -2.1f)}), -1);
    TL_CHECK_EQ(tl::core::second_pick({}), -1);
}
