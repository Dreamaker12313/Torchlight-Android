// `CInteractiveMenu`'s slide (`setVisible`,
// `update`), the cinematic bars.

#include "core/interactive_menu.h"
#include "test_support.h"

#include <cmath>

namespace {

using tl::core::InteractiveBars;

}  // namespace

// Made and closed at once by its `createMenus`: hidden, and once the close has
// run out an update leaves it alone.
TL_TEST(interactive_bars_start_hidden) {
    InteractiveBars bars;
    TL_CHECK(!bars.open);
    TL_CHECK(!bars.window_visible);
    TL_CHECK_EQ(bars.timer, 0.2f);
    bars.update(0.25f);
    TL_CHECK(!bars.window_visible);
    const float timer = bars.timer;
    bars.update(0.1f);
    TL_CHECK_EQ(bars.timer, timer);
}

// Opened, the bars start out of sight and come to rest at 0 and 0.9 after
// 0.2 s; half way they are half way.
TL_TEST(interactive_bars_slide_in) {
    InteractiveBars bars;
    bars.update(1.0f);
    bars.set_visible(true);
    TL_CHECK(bars.window_visible);
    TL_CHECK_EQ(bars.top_y, -0.1f);
    TL_CHECK_EQ(bars.bottom_y, 1.1f);
    bars.update(0.1f);
    const float t = (0.2f - 0.1f) * 5.0f;
    TL_CHECK_EQ(bars.top_y, t * -0.1f);
    TL_CHECK_EQ(bars.bottom_y, -std::fma(1.0f - t, 0.1f, -1.0f));
    bars.update(0.15f);
    TL_CHECK_EQ(bars.top_y, -0.0f);
    TL_CHECK_EQ(bars.bottom_y, 0.9f);
    // Open, it is updated on with the timer run out: nothing moves.
    bars.update(0.5f);
    TL_CHECK_EQ(bars.bottom_y, 0.9f);
    TL_CHECK(bars.window_visible);
    TL_CHECK_EQ(bars.bottom_edge(), 0.9f);
}

// Closed, they slide back out over the top bar's height times two and the
// window goes when they are out; asked again in the same state, nothing.
TL_TEST(interactive_bars_slide_out) {
    InteractiveBars bars;
    bars.update(1.0f);
    bars.set_visible(true);
    bars.update(1.0f);
    bars.set_visible(true);
    TL_CHECK(bars.window_visible);
    bars.set_visible(false);
    TL_CHECK(!bars.open);
    TL_CHECK(std::fabs(bars.timer - 0.2f) < 1e-6f);
    bars.update(0.1f);
    TL_CHECK(bars.window_visible);
    TL_CHECK(bars.top_y < 0.0f && bars.top_y > -0.1f);
    TL_CHECK(bars.bottom_y > 0.9f && bars.bottom_y < 1.0f);
    bars.update(0.11f);
    TL_CHECK(!bars.window_visible);
    TL_CHECK_EQ(bars.bottom_edge(), 1.0f);
}

// A close in the middle of the slide in starts the slide out from the full
// timer, not from where the bars are: the console's quirk.
TL_TEST(interactive_bars_close_restarts_the_slide) {
    InteractiveBars bars;
    bars.update(1.0f);
    bars.set_visible(true);
    bars.update(0.05f);
    bars.set_visible(false);
    TL_CHECK(std::fabs(bars.timer - 0.2f) < 1e-6f);
    // The first frame of the slide out puts the bars where it starts, fully in.
    bars.update(0.0f);
    TL_CHECK(std::fabs(bars.top_y) < 1e-6f);
    TL_CHECK(std::fabs(bars.bottom_y - 0.9f) < 1e-6f);
}

// With the editor running the close is at once.
TL_TEST(interactive_bars_close_at_once_in_the_editor) {
    InteractiveBars bars;
    bars.update(1.0f);
    bars.set_visible(true);
    bars.update(1.0f);
    bars.set_visible(false, /*editor_running=*/true);
    TL_CHECK(!bars.window_visible);
}
