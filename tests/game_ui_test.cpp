// `core/game_ui.h`: the Xbox `+` over the bag and the
// level-up captions (`CPlayer::levelUp`), and `core/dungeon.h`'s
// `CLevel::getName`. Names and numbers only.
#include "core/game_ui.h"
#include "core/dungeon.h"

#include "test_support.h"

using tl::core::AvailablePointsIcon;

TL_TEST(no_points_hides_both_windows) {
    AvailablePointsIcon icon;
    icon.plus = true;
    icon.glow = true;
    icon.update(0.016f, 0, 0);
    TL_CHECK(!icon.plus);
    TL_CHECK(!icon.glow);
}

// The constructor's timer is 0, so the first frame with points already flips
// the phase: `+` on, glow off, and a second on the clock.
TL_TEST(first_frame_with_points_lights_the_plus) {
    AvailablePointsIcon icon;
    icon.update(0.016f, 5, 0);
    TL_CHECK(icon.plus);
    TL_CHECK(!icon.glow);
    TL_CHECK(icon.lit);
    TL_CHECK(icon.timer == 1.0f);
}

TL_TEST(either_kind_of_point_is_enough) {
    AvailablePointsIcon stats;
    stats.update(0.016f, 1, 0);
    TL_CHECK(stats.plus);
    AvailablePointsIcon skills;
    skills.update(0.016f, 0, 1);
    TL_CHECK(skills.plus);
}

// After a second the glow comes on and the `+` goes for that one frame, back the next frame
// under the glow; a second later the glow goes.
TL_TEST(the_glow_comes_and_goes_once_a_second_over_the_plus) {
    AvailablePointsIcon icon;
    icon.update(0.5f, 0, 1);  // flip: lit, timer 1.0
    icon.update(0.5f, 0, 1);  // 0.5 left
    TL_CHECK(icon.plus);
    TL_CHECK(!icon.glow);
    icon.update(0.6f, 0, 1);  // -0.1: unlit
    TL_CHECK(!icon.lit);
    TL_CHECK(!icon.plus);
    TL_CHECK(icon.glow);
    icon.update(0.1f, 0, 1);
    TL_CHECK(icon.plus);
    TL_CHECK(icon.glow);
    icon.update(1.0f, 0, 1);  // -0.1 again: lit
    TL_CHECK(icon.plus);
    TL_CHECK(!icon.glow);
}

TL_TEST(spending_the_last_point_hides_it_and_keeps_the_clock) {
    AvailablePointsIcon icon;
    icon.update(0.2f, 1, 0);
    const float timer = icon.timer;
    icon.update(0.2f, 0, 0);
    TL_CHECK(!icon.plus);
    TL_CHECK(!icon.glow);
    TL_CHECK(icon.timer == timer);
}

TL_TEST(level_up_banner_and_caption) {
    TL_CHECK(tl::core::level_up_banner() == "Level Up!");
    TL_CHECK(tl::core::level_up_caption(2) == "Level 2 reached");
    TL_CHECK(tl::core::level_up_caption(37) == "Level 37 reached");
}

// `media/layouts/mine/RULES.DAT` authors `LEVELNAME: Orden Mines` and
// `SHOWFLOORNUMBER: True`; the town's says `False`.
TL_TEST(level_name_carries_the_floor_when_the_template_says_so) {
    TL_CHECK(tl::core::level_display_name("Orden Mines", true, 0) == "Orden Mines - Floor 1");
    TL_CHECK(tl::core::level_display_name("Orden Mines", true, 2) == "Orden Mines - Floor 3");
    TL_CHECK(tl::core::level_display_name("Torchlight", false, 0) == "Torchlight");
    TL_CHECK(tl::core::level_floor_caption(true, 4) == "Floor 5");
    TL_CHECK(tl::core::level_floor_caption(false, 4).empty());
}
