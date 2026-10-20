// Fishing: the bobber's draws and their order, the bite, the catch, the ring, the dialog text.
// Draw counts are checked against a generator stepped by hand: a float draw is two steps.
#include "core/fishing.h"
#include "core/random.h"
#include "test_support.h"

#include <cmath>

namespace {

using tl::core::FishingBobber;
using tl::core::RunicRandom;

/// Where a generator seeded with `seed` stands after `floats` float draws.
uint64_t after_draws(uint32_t seed, int floats) {
    RunicRandom rng(seed);
    for (int i = 0; i < floats; ++i) rng.between(0.0f, 1.0f);
    return rng.state();
}

}  // namespace

TL_TEST(fishing_start_arms_the_bobber_with_one_draw) {
    RunicRandom rng(1234u);
    RunicRandom same(1234u);
    FishingBobber bobber;
    bobber.hooked = true;
    bobber.bite = 0.3f;
    bobber.phase = 5.0f;
    tl::core::start_fishing(bobber, rng);
    TL_CHECK(!bobber.hooked);
    TL_CHECK_EQ(bobber.depth, 2.0f);
    TL_CHECK_EQ(bobber.bite, 0.0f);
    TL_CHECK_EQ(bobber.phase, 0.0f);
    TL_CHECK_EQ(bobber.speed, same.between(1.0f, 3.0f));
    TL_CHECK(bobber.speed >= 1.0f && bobber.speed <= 3.0f);
    TL_CHECK_EQ(rng.state(), after_draws(1234u, 1));
}

TL_TEST(fishing_step_on_the_surface_draws_once_and_pushes_down) {
    // Depth 2: the push is a draw in [-8, 0] and there is no tug roll (the
    // bobber is not below 1.75).
    RunicRandom rng(77u);
    RunicRandom same(77u);
    FishingBobber bobber;
    bobber.depth = 2.0f;
    bobber.speed = 2.0f;
    const bool bit = tl::core::step_fishing(bobber, 0.1f, false, rng);
    TL_CHECK(!bit);
    const float push = same.between(-8.0f, 0.0f);
    const float speed = push * 0.1f + 2.0f;
    TL_CHECK_EQ(bobber.speed, speed);
    TL_CHECK_EQ(bobber.phase, 0.4f);
    TL_CHECK_EQ(bobber.depth, std::fmin(0.1f * speed + 2.0f, 2.0f));
    TL_CHECK_EQ(rng.state(), after_draws(77u, 1));
}

TL_TEST(fishing_step_below_the_tug_line_draws_for_the_tug) {
    // Between 1 and 2: the push is in [-4.1, 4]; below 1.75 a second draw in
    // [0, 10000] decides the tug, and a third only when it lands.
    RunicRandom rng(5u);
    RunicRandom same(5u);
    FishingBobber bobber;
    bobber.depth = 1.5f;
    bobber.speed = 0.0f;
    bobber.phase = 1.5707964f;  // sin = 1, so the ripple cannot sink it
    tl::core::step_fishing(bobber, 1.0f / 60.0f, false, rng);
    same.between(-4.1f, 4.0f);
    const float roll = same.between(0.0f, 10000.0f);
    const int expected = roll < 5.0f ? 3 : 2;
    TL_CHECK_EQ(rng.state(), after_draws(5u, expected));
}

TL_TEST(fishing_speed_is_kept_between_minus_four_and_ten) {
    RunicRandom rng(9u);
    FishingBobber bobber;
    bobber.depth = 2.0f;
    bobber.speed = 50.0f;
    tl::core::step_fishing(bobber, 0.01f, false, rng);
    TL_CHECK_EQ(bobber.speed, 10.0f);
    bobber.depth = 2.0f;
    bobber.speed = -50.0f;
    tl::core::step_fishing(bobber, 0.01f, false, rng);
    TL_CHECK_EQ(bobber.speed, -4.0f);
}

TL_TEST(fishing_a_dip_starts_a_bite_unless_an_animation_plays) {
    // Depth 1 with the ripple at 0 is under: it rests at 1, speed 1, and a
    // bite of 0.25 to 0.4 seconds starts.
    FishingBobber bobber;
    bobber.depth = 1.0f;
    bobber.speed = -4.0f;
    bobber.phase = 0.0f;
    RunicRandom rng(3u);
    const bool bit = tl::core::step_fishing(bobber, 0.0f, false, rng);
    TL_CHECK(bit);
    TL_CHECK_EQ(bobber.depth, 1.0f);
    TL_CHECK_EQ(bobber.speed, 1.0f);
    TL_CHECK(bobber.bite >= 0.25f && bobber.bite <= 0.4f);

    FishingBobber busy;
    busy.depth = 1.0f;
    busy.speed = -4.0f;
    RunicRandom rng2(3u);
    TL_CHECK(!tl::core::step_fishing(busy, 0.0f, true, rng2));
    TL_CHECK_EQ(busy.depth, 1.0f);
    TL_CHECK_EQ(busy.bite, 0.0f);
}

TL_TEST(fishing_a_bite_runs_down_with_time) {
    FishingBobber bobber;
    bobber.depth = 2.0f;
    bobber.bite = 0.3f;
    RunicRandom rng(11u);
    tl::core::step_fishing(bobber, 0.1f, false, rng);
    TL_CHECK(std::fabs(bobber.bite - 0.2f) < 1e-6f);
}

TL_TEST(fishing_the_catch_hooks_only_inside_a_bite) {
    FishingBobber bobber;
    bobber.hooked = true;
    bobber.bite = 0.0f;
    tl::core::catch_fish(bobber);
    TL_CHECK(!bobber.hooked);
    bobber.bite = 0.01f;
    tl::core::catch_fish(bobber);
    TL_CHECK(bobber.hooked);
}

TL_TEST(fishing_the_ring_follows_the_bobber_and_fills_during_a_bite) {
    FishingBobber bobber;
    bobber.depth = 2.0f;
    bobber.phase = 0.0f;
    TL_CHECK_EQ(tl::core::fishing_ring_extent(bobber), 256.0f);
    TL_CHECK(!tl::core::fishing_ring_alert(bobber));
    bobber.bite = 0.2f;
    TL_CHECK_EQ(tl::core::fishing_ring_extent(bobber), 128.0f);
    TL_CHECK(tl::core::fishing_ring_alert(bobber));
}

TL_TEST(fishing_the_dialog_says_what_came_up) {
    TL_CHECK_EQ(std::string(tl::core::fishing_dialog_title()), std::string("Fishing"));
    TL_CHECK_EQ(tl::core::fishing_catch_text(false, false, "Dogfish"),
                std::string("You caught nothing!"));
    TL_CHECK_EQ(tl::core::fishing_catch_text(true, true, "Dogfish"),
                std::string("You caught a Dogfish!"));
    TL_CHECK_EQ(tl::core::fishing_catch_text(true, false, "Fish Potion"),
                std::string("You caught a fish containing:\nFish Potion!"));
    // A roll that made nothing still prints, with no name.
    TL_CHECK_EQ(tl::core::fishing_catch_text(true, false, ""),
                std::string("You caught a fish containing:\n!"));
}
