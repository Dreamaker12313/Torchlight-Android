// Unit tests for GameSpeed.
// Curves are constructed in code; no game data files are accessed.

#include "core/game_speed.h"
#include "core/stats.h"

#include <cmath>
#include "test_support.h"

namespace {

using tl::core::GameSpeed;
using tl::core::GraphType;
using tl::core::StatGraph;

// Helper to construct a flat horizontal line curve y = y_val over [0, 1].
StatGraph make_flat_curve(float y_val) {
    StatGraph g(GraphType::Linear);
    g.add_point(0.0f, y_val);
    g.add_point(1.0f, y_val);
    g.process_points();
    return g;
}

// Helper to construct a linear ramp curve from (0, 0) to (1, 1).
StatGraph make_ramp_curve() {
    StatGraph g(GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(1.0f, 1.0f);
    g.process_points();
    return g;
}

bool close(float a, float b, float eps = 1e-5f) { return std::abs(a - b) < eps; }

}  // namespace

// Fresh instance reports 1.0f, and calculating on an empty one leaves speed at 1.0f.
// Exact float comparison: 1.0f is exact in IEEE 754 float.
TL_TEST(game_speed_fresh_and_empty_reports_one) {
    GameSpeed gs;
    TL_CHECK_EQ(gs.speed(), 1.0f);
    TL_CHECK_EQ(gs.size(), 0u);

    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 1.0f);
}

// One type-0 instance, flat curve 1, strength 0.5: speed (1 + (1 - 0.5)) / 2 = 0.75, exact.
TL_TEST(game_speed_type0_decelerates_speed) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(&flat, nullptr);

    gs.add_modifier(0, 1.0f, 0.5f);
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 0.75f);
}

// A negative 1 - curve * strength clamps that instance's term to 0.
TL_TEST(game_speed_type0_clamps_term_to_zero) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(&flat, nullptr);

    gs.add_modifier(0, 1.0f, 3.0f);  // 1.0 - 1.0 * 3.0 = -2.0 -> clamped to 0.0
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 0.5f);  // (1.0 + 0.0) / 2 = 0.5
}

// One type-1 instance adds 1 + curve * strength: (1 + 1.5) / 2 = 1.25, exact.
TL_TEST(game_speed_type1_accelerates_speed) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(nullptr, &flat);

    gs.add_modifier(1, 1.0f, 0.5f);
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 1.25f);
}

// The curve is sampled at elapsed / duration after dt is added.
TL_TEST(game_speed_curve_sampled_at_elapsed_after_dt) {
    const StatGraph ramp = make_ramp_curve();
    GameSpeed gs(&ramp, nullptr);

    gs.add_modifier(0, 1.0f, 1.0f);

    // Call 1: elapsed = 0.25, sample = 0.25, term = 1.0 - 0.25 = 0.75, speed = (1 + 0.75)/2 = 0.875
    gs.calculate(0.25f);
    TL_CHECK_EQ(gs.speed(), 0.875f);

    // Call 2: elapsed = 0.50, sample = 0.50, term = 1.0 - 0.50 = 0.50, speed = (1 + 0.50)/2 = 0.75
    gs.calculate(0.25f);
    TL_CHECK_EQ(gs.speed(), 0.75f);
}

// An instance reaching its duration is removed on that call and adds nothing (strict <).
TL_TEST(game_speed_expired_instance_removed_and_contributes_nothing) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(&flat, nullptr);

    gs.add_modifier(0, 0.5f, 0.5f);
    gs.calculate(0.5f);  // elapsed reaches duration exactly
    TL_CHECK_EQ(gs.size(), 0u);
    TL_CHECK_EQ(gs.speed(), 1.0f);
}

// The removal quirk: the instance moved into an expired one's slot is not aged or counted on
// that call. Kept.
TL_TEST(game_speed_removal_quirk_skips_moved_element_this_frame) {
    const StatGraph ramp = make_ramp_curve();
    GameSpeed gs(&ramp, nullptr);

    gs.add_modifier(0, 0.1f, 1.0f);  // inst0: expires on dt=0.1
    gs.add_modifier(0, 1.0f, 1.0f);  // inst1
    gs.add_modifier(0, 1.0f, 1.0f);  // inst2: moved into slot 0 when inst0 expires

    // Call 1: inst0 expires; inst2 moved to slot 0 is NOT aged or counted
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.size(), 2u);
    TL_CHECK(close(gs.speed(), 0.95f));

    // Call 2: inst2 at slot 0 now ages from 0 to 0.1; inst1 at slot 1 ages from 0.1 to 0.2
    gs.calculate(0.1f);
    TL_CHECK(close(gs.speed(), 0.9f));
}

// An unknown type (2) adds only to the divisor.
TL_TEST(game_speed_unknown_type_adds_only_to_divisor) {
    GameSpeed gs;
    gs.add_modifier(2, 1.0f, 5.0f);
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 0.5f);
}

// clear() empties the list and the next calculate gives 1.0.
// Exact float comparisons.
TL_TEST(game_speed_clear_empties_list_and_next_calculate_gives_one) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(&flat, nullptr);

    gs.add_modifier(0, 10.0f, 0.5f);
    gs.calculate(0.1f);
    TL_CHECK(gs.speed() != 1.0f);

    gs.clear();
    TL_CHECK_EQ(gs.size(), 0u);

    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 1.0f);
}

// Insertion order: size() grows by one per add_modifier and records instances.
TL_TEST(game_speed_insertion_order_and_size) {
    GameSpeed gs;
    TL_CHECK_EQ(gs.size(), 0u);

    gs.add_modifier(0, 1.0f, 0.5f);
    TL_CHECK_EQ(gs.size(), 1u);
    TL_CHECK_EQ(gs.instances()[0].type, 0);
    TL_CHECK_EQ(gs.instances()[0].duration, 1.0f);
    TL_CHECK_EQ(gs.instances()[0].strength, 0.5f);

    gs.add_modifier(1, 2.0f, 0.8f);
    TL_CHECK_EQ(gs.size(), 2u);
    TL_CHECK_EQ(gs.instances()[1].type, 1);
    TL_CHECK_EQ(gs.instances()[1].duration, 2.0f);
    TL_CHECK_EQ(gs.instances()[1].strength, 0.8f);
}

// The clamp is `maxss`: a NaN term becomes 0, where a `term < 0` test
// would let it through and poison the speed.
TL_TEST(game_speed_nan_term_clamps_to_zero) {
    const StatGraph flat = make_flat_curve(1.0f);
    GameSpeed gs(&flat, nullptr);
    gs.add_modifier(0, 1.0f, std::nanf(""));
    gs.calculate(0.1f);
    TL_CHECK_EQ(gs.speed(), 0.5f);
}
