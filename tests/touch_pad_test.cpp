// The on-screen pad: which control a touch lands on, and what it reads.
#include "core/touch_pad.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

using tl::core::TouchPad;

/// A foldable phone's cover screen, landscape: the shorter side is 968. On the
/// screen already, as after the tap that shows it.
TouchPad cover_screen() {
    TouchPad pad;
    pad.set_screen(2376.0f, 968.0f);
    pad.set_visible(true);
    return pad;
}

const TouchPad::Shape* labelled(const std::vector<TouchPad::Shape>& shapes, const char* label) {
    for (const TouchPad::Shape& s : shapes) {
        if (std::string(s.label) == label) return &s;
    }
    return nullptr;
}

}  // namespace

/// A face button is held while its touch is down, and its press is latched
/// until the next take() even when the touch lifted in between.
TL_TEST(touch_pad_button_press_is_latched) {
    TouchPad pad = cover_screen();
    const auto shapes = pad.shapes();
    const TouchPad::Shape* a = labelled(shapes, "A");
    TL_CHECK(a != nullptr);
    pad.touch_down(3, a->x, a->y);
    TL_CHECK(pad.visible());
    TL_CHECK((pad.peek().held & TouchPad::kA) != 0);
    pad.touch_up(3);
    TL_CHECK((pad.peek().held & TouchPad::kA) == 0);
    const TouchPad::State first = pad.take();
    TL_CHECK((first.pressed & TouchPad::kA) != 0);
    TL_CHECK((pad.take().pressed & TouchPad::kA) == 0);
}

/// The left stick is centred where the thumb came down, and reads the slide
/// from there, clamped to the unit disc; y is down, as Android's axes are.
TL_TEST(touch_pad_left_stick_floats) {
    TouchPad pad = cover_screen();
    pad.touch_down(0, 500.0f, 700.0f);
    TL_CHECK(std::fabs(pad.peek().lx) < 1e-6f && std::fabs(pad.peek().ly) < 1e-6f);
    pad.touch_move(0, 500.0f + 0.065f * 968.0f, 700.0f);  // half the stick's radius right
    TL_CHECK(std::fabs(pad.peek().lx - 0.5f) < 1e-3f);
    pad.touch_move(0, 500.0f, 700.0f - 1000.0f);  // far up: full deflection
    TL_CHECK(std::fabs(pad.peek().ly + 1.0f) < 1e-3f);
    TL_CHECK(std::fabs(pad.peek().lx) < 1e-3f);
    pad.touch_up(0);
    TL_CHECK(pad.peek().ly == 0.0f);
}

/// A second thumb on the left stick's zone does not take the stick over.
TL_TEST(touch_pad_left_stick_keeps_its_thumb) {
    TouchPad pad = cover_screen();
    pad.touch_down(0, 500.0f, 700.0f);
    pad.touch_down(1, 300.0f, 800.0f);
    pad.touch_move(1, 100.0f, 800.0f);
    TL_CHECK(std::fabs(pad.peek().lx) < 1e-6f);
}

/// The D-pad reads the direction its touch slides to, diagonals as two.
TL_TEST(touch_pad_dpad_slides) {
    TouchPad pad = cover_screen();
    const float u = 968.0f;
    const float cx = 0.20f * u;
    const float cy = 968.0f - 0.62f * u;
    pad.touch_down(5, cx, cy - 0.085f * u);
    TL_CHECK(pad.peek().held == TouchPad::kUp);
    pad.touch_move(5, cx + 0.06f * u, cy + 0.06f * u);
    TL_CHECK(pad.peek().held == (TouchPad::kDown | TouchPad::kRight));
    pad.touch_move(5, cx, cy);  // the centre: nothing
    TL_CHECK(pad.peek().held == 0);
}

/// The pad starts off the screen. The tap that shows it presses nothing,
/// though it lands on a button; the next one presses it.
TL_TEST(touch_pad_first_tap_only_shows) {
    TouchPad pad;
    pad.set_screen(2376.0f, 968.0f);
    TL_CHECK(!pad.visible());
    const auto shapes = pad.shapes();
    const TouchPad::Shape* a = labelled(shapes, "A");
    TL_CHECK(a != nullptr);
    pad.touch_down(0, a->x, a->y);
    TL_CHECK(pad.visible());
    TL_CHECK(pad.peek().held == 0);
    pad.touch_up(0);
    TL_CHECK(pad.take().pressed == 0);
    pad.touch_down(1, a->x, a->y);
    TL_CHECK((pad.peek().held & TouchPad::kA) != 0);
}

/// A touch on no control does nothing but bring the pad onto the screen.
TL_TEST(touch_pad_empty_touch_only_shows) {
    TouchPad pad = cover_screen();
    pad.touch_down(0, 1188.0f, 500.0f);  // the middle of the screen
    TL_CHECK(pad.visible());
    const TouchPad::State s = pad.take();
    TL_CHECK(s.held == 0 && s.pressed == 0 && s.lx == 0.0f && s.rx == 0.0f);
}

/// The shapes come in a fixed number, whatever is held.
TL_TEST(touch_pad_shape_count_is_fixed) {
    TouchPad pad = cover_screen();
    const std::size_t idle = pad.shapes().size();
    pad.touch_down(0, 500.0f, 700.0f);
    pad.touch_down(1, 2376.0f - 0.23f * 968.0f, 968.0f - 0.33f * 968.0f + 0.11f * 968.0f);
    TL_CHECK(pad.shapes().size() == idle);
    TL_CHECK(idle == 21);  // 2 sticks x 2, R3, L3, D-pad base + 4, ABXY, 4 shoulders, Back, Start
}
