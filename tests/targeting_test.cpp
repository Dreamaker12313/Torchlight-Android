// What the console points: the cone, the reach, the touching
// case, line of sight and the order of preference.
#include "core/targeting.h"
#include "test_support.h"

#include <cmath>
#include <vector>

namespace {

using tl::core::TargetCandidate;

/// A candidate `distance` away, `degrees` off the facing, with the character at
/// the origin facing +Z (facing 0).
TargetCandidate at(float distance, float degrees, bool hostile = true, std::uint64_t id = 1) {
    const float radians = degrees * 3.14159265358979323846f / 180.0f;
    TargetCandidate c;
    c.x = distance * std::sin(radians);
    c.z = distance * std::cos(radians);
    c.hostile = hostile;
    c.id = id;
    return c;
}

bool pick(const std::vector<TargetCandidate>& list, float range, std::uint64_t* id) {
    return tl::core::select_target(list.data(), list.size(), 0.0f, 0.0f, 0.0f, range, id);
}

}  // namespace

/// The reach of the cone: max(7.0, weapon range + 2.0).
TL_TEST(target_range_is_seven_or_the_weapon_plus_two) {
    TL_CHECK_EQ(tl::core::target_range(0.0f), 7.0f);
    TL_CHECK_EQ(tl::core::target_range(4.0f), 7.0f);  // 6.0 loses to the floor
    TL_CHECK_EQ(tl::core::target_range(5.0f), 7.0f);  // exactly 7.0, floor wins
    TL_CHECK_EQ(tl::core::target_range(12.0f), 14.0f);
    TL_CHECK_EQ(tl::core::kTargetConeDegrees, 30.0f);
}

/// The 30 degree cone, and the reach inside it.
TL_TEST(target_cone_is_thirty_degrees_and_bounded_by_the_range) {
    std::uint64_t id = 0;
    TL_CHECK(pick({at(5.0f, 0.0f)}, 7.0f, &id));
    TL_CHECK(pick({at(5.0f, 29.0f)}, 7.0f, &id));
    TL_CHECK(pick({at(5.0f, 29.99f)}, 7.0f, &id));
    TL_CHECK(!pick({at(5.0f, 31.0f)}, 7.0f, &id));  // outside the cone
    TL_CHECK(!pick({at(5.0f, -31.0f)}, 7.0f, &id));
    TL_CHECK(pick({at(5.0f, -29.99f)}, 7.0f, &id));  // symmetric

    // A true 30 degrees is rejected: the original converts with 57.295826 (> 180/pi), so it
    // reads back as 30.00002 and fails `<= 30`. Kept.
    TL_CHECK(!pick({at(5.0f, 30.0f)}, 7.0f, &id));
    TL_CHECK(!pick({at(8.0f, 0.0f)}, 7.0f, &id));  // in the cone, out of reach
    TL_CHECK(pick({at(8.0f, 0.0f)}, 14.0f, &id));  // a long weapon reaches it
}

/// Anything within half a unit is taken whatever the angle, and without needing
/// line of sight - the character is standing on it.
TL_TEST(target_within_half_a_unit_is_taken_from_any_angle) {
    std::uint64_t id = 0;
    TL_CHECK(pick({at(0.4f, 180.0f)}, 7.0f, &id));

    TargetCandidate behind = at(0.4f, 150.0f);
    behind.in_sight = false;
    TL_CHECK(pick({behind}, 7.0f, &id));

    // Just outside it, behind the character, the cone rejects it and the
    // fallback pass picks it up instead (it is inside 3.0).
    TL_CHECK(pick({at(0.6f, 150.0f)}, 7.0f, &id));
    // Beyond the fallback radius, behind: nothing.
    TL_CHECK(!pick({at(3.5f, 150.0f)}, 7.0f, &id));
}

/// The fallback pass: anything within 3.0, at any angle, when the cone found
/// nothing.
TL_TEST(target_falls_back_to_anything_within_three_units) {
    std::uint64_t id = 0;
    TL_CHECK(pick({at(2.9f, 90.0f)}, 7.0f, &id));
    TL_CHECK(!pick({at(3.1f, 90.0f)}, 7.0f, &id));

    // The cone wins when both are available: the one in front is chosen even
    // though the one beside is nearer.
    std::vector<TargetCandidate> both{at(5.0f, 0.0f, true, 11), at(1.0f, 90.0f, true, 22)};
    TL_CHECK(pick(both, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(11));
}

/// Line of sight is required in the cone, but not when touching.
TL_TEST(target_needs_line_of_sight_in_the_cone) {
    std::uint64_t id = 0;
    TargetCandidate blocked = at(5.0f, 0.0f);
    blocked.in_sight = false;
    TL_CHECK(!pick({blocked}, 7.0f, &id));

    // The visible one behind the wall's candidate is taken instead.
    TargetCandidate visible = at(6.0f, 10.0f, true, 7);
    TL_CHECK(pick({blocked, visible}, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(7));
}

/// A hostile candidate beats a harmless one whatever the distances, and among
/// equals the nearest wins.
TL_TEST(target_prefers_something_hostile_then_the_nearest) {
    std::uint64_t id = 0;
    std::vector<TargetCandidate> mixed{at(1.5f, 5.0f, false, 100), at(6.0f, 5.0f, true, 200)};
    TL_CHECK(pick(mixed, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(200));

    std::vector<TargetCandidate> harmless{at(6.0f, 5.0f, false, 1), at(2.0f, 5.0f, false, 2)};
    TL_CHECK(pick(harmless, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(2));
}

/// The incumbent's 5 degree protection, applied with the signed bearing, so only against a
/// candidate further clockwise.
TL_TEST(target_hysteresis_protects_the_incumbent_by_five_degrees) {
    std::uint64_t id = 0;

    // Nearer, but 20 degrees further clockwise than the incumbent at 0: the
    // incumbent is kept.
    std::vector<TargetCandidate> clockwise{at(6.0f, 0.0f, true, 1), at(2.0f, 20.0f, true, 2)};
    TL_CHECK(pick(clockwise, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(1));

    // Within the five degrees, the nearer one takes over.
    std::vector<TargetCandidate> close{at(6.0f, 0.0f, true, 1), at(2.0f, 4.0f, true, 2)};
    TL_CHECK(pick(close, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(2));

    // Anticlockwise the signed comparison never fires, so the nearer one wins
    // however far off it is. This is the original's quirk, not a tidy rule.
    std::vector<TargetCandidate> anticlockwise{at(6.0f, 0.0f, true, 1), at(2.0f, -20.0f, true, 2)};
    TL_CHECK(pick(anticlockwise, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(2));
}

/// Nothing in range at all.
TL_TEST(target_returns_nothing_when_there_is_nothing) {
    std::uint64_t id = 99;
    TL_CHECK(!tl::core::select_target(nullptr, 0, 0.0f, 0.0f, 0.0f, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(99));  // left alone
}

/// The facing rotates the cone with the character: the same world point is in
/// front at one heading and behind at another.
TL_TEST(target_cone_turns_with_the_character) {
    TargetCandidate east;
    east.x = 5.0f;
    east.z = 0.0f;
    east.hostile = true;
    east.id = 5;

    std::uint64_t id = 0;
    // Facing +Z, the point is 90 degrees off: out of the cone, out of the
    // fallback radius.
    TL_CHECK(!tl::core::select_target(&east, 1, 0.0f, 0.0f, 0.0f, 7.0f, &id));
    // Facing +X (a quarter turn), it is straight ahead.
    const float quarter = 3.14159265358979323846f / 2.0f;
    TL_CHECK(tl::core::select_target(&east, 1, 0.0f, 0.0f, quarter, 7.0f, &id));
    TL_CHECK_EQ(id, static_cast<std::uint64_t>(5));
}
