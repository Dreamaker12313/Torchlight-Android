// The Xbox 360 pad layer: every threshold, deadzone and rate, with its address.
#include "core/gamepad.h"
#include "test_support.h"

#include <cmath>

namespace {

using tl::core::PadState;

}  // namespace

/// The deadzones the pad poll passes to the stick filter
/// (`0x1EA9` left, `0x21F1` right).
TL_TEST(test_deadzone_constants) {
    TL_CHECK_EQ(static_cast<int>(tl::core::kLeftStickDeadzone), 7849);
    TL_CHECK_EQ(static_cast<int>(tl::core::kRightStickDeadzone), 8689);
    TL_CHECK_EQ(static_cast<int>(tl::core::kTriggerThreshold), 30);
}

/// The stick filter: inside the deadzone is centred, and the
/// remainder is rescaled so full deflection still reaches full range.
TL_TEST(test_stick_filter) {
    std::int16_t x = 5000;
    std::int16_t y = 0;
    tl::core::filter_stick(x, y, tl::core::kLeftStickDeadzone);
    TL_CHECK_EQ(static_cast<int>(x), 0);
    TL_CHECK_EQ(static_cast<int>(y), 0);

    // Exactly on the deadzone is still centred: the original subtracts and
    // keeps only what is greater than zero.
    x = tl::core::kLeftStickDeadzone;
    y = 0;
    tl::core::filter_stick(x, y, tl::core::kLeftStickDeadzone);
    TL_CHECK_EQ(static_cast<int>(x), 0);

    // Full deflection survives the rescale.
    x = 32767;
    y = 0;
    tl::core::filter_stick(x, y, tl::core::kLeftStickDeadzone);
    TL_CHECK_EQ(static_cast<int>(x), 32767);

    // Half way past the deadzone, on one axis: (m - dz) * 32767/(32767 - dz).
    x = 20000;
    y = 0;
    tl::core::filter_stick(x, y, tl::core::kLeftStickDeadzone);
    const float expected = (20000.0f - 7849.0f) * (32767.0f / (32767.0f - 7849.0f));
    TL_CHECK(std::fabs(static_cast<float>(x) - expected) <= 1.0f);

    // A diagonal is rescaled by its *magnitude*, so it is not longer than a
    // cardinal - the direction is kept and the length is the rescaled one.
    x = 23170;
    y = 23170;
    tl::core::filter_stick(x, y, tl::core::kLeftStickDeadzone);
    const float magnitude = std::sqrt(static_cast<float>(x) * static_cast<float>(x) +
                                      static_cast<float>(y) * static_cast<float>(y));
    TL_CHECK(magnitude <= 32767.0f + 1.0f);
    TL_CHECK(std::fabs(static_cast<float>(x) - static_cast<float>(y)) <= 1.0f);
}

/// The trigger filter: a dead range of 30, then 255/225.
TL_TEST(test_trigger_filter) {
    std::uint8_t t = 29;
    tl::core::filter_trigger(t);
    TL_CHECK_EQ(static_cast<int>(t), 0);

    t = 30;
    tl::core::filter_trigger(t);
    TL_CHECK_EQ(static_cast<int>(t), 0);

    t = 255;
    tl::core::filter_trigger(t);
    TL_CHECK_EQ(static_cast<int>(t), 255);  // 225 * 1.1333333 is exactly 255 as a float

    t = 120;
    tl::core::filter_trigger(t);
    TL_CHECK_EQ(static_cast<int>(t), 102);  // 90 * 1.1333333 rounds to 102.0 as a float

    t = 200;
    tl::core::filter_trigger(t);
    TL_CHECK_EQ(static_cast<int>(t), 192);  // 170 * 1.1333333 = 192.67, truncated by fctidz
}

/// isButtonDown: the D-pad answers to the left stick too,
/// the face buttons are bit tests, the stick clicks are never reported.
TL_TEST(test_button_down) {
    PadState pad;
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadDPadUp));

    pad.left_y = 7850;
    TL_CHECK(tl::core::button_down(pad, tl::core::kPadDPadUp));
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadDPadDown));

    // Strictly past the deadzone, not on it.
    pad.left_y = 7849;
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadDPadUp));

    pad.left_y = 0;
    pad.buttons = tl::core::kPadDPadUp;
    TL_CHECK(tl::core::button_down(pad, tl::core::kPadDPadUp));

    pad.left_x = -30000;
    pad.buttons = 0;
    TL_CHECK(tl::core::button_down(pad, tl::core::kPadDPadLeft));
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadDPadRight));

    pad.left_x = 0;
    pad.buttons = tl::core::kPadA | tl::core::kPadStart;
    TL_CHECK(tl::core::button_down(pad, tl::core::kPadA));
    TL_CHECK(tl::core::button_down(pad, tl::core::kPadStart));
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadB));

    // The original's switch has no case for the stick clicks or the triggers.
    pad.buttons = tl::core::kPadLeftStickClick | tl::core::kPadRightStickClick;
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadLeftStickClick));
    TL_CHECK(!tl::core::button_down(pad, tl::core::kPadRightStickClick));
}

/// The stick's world point.
TL_TEST(test_stick_destination) {
    float x = 0.0f;
    float z = 0.0f;

    // Inside the threshold, with no fallback asked for: no destination at all.
    TL_CHECK(
        !tl::core::stick_destination(0.05f, 0.05f, 0.0f, 10.0f, 20.0f, 0.0f, 1.0f, false, &x, &z));

    // Inside the threshold with the fallback: one unit ahead of the character.
    TL_CHECK(tl::core::stick_destination(0.0f, 0.0f, 0.0f, 10.0f, 20.0f, 0.0f, 1.0f, true, &x, &z));
    TL_CHECK(std::fabs(x - 10.0f) < 1e-4f);
    TL_CHECK(std::fabs(z - 21.0f) < 1e-4f);

    // Pushed forward with the camera at yaw 0: two units along +Z, which is
    // the direction the town's camera looks.
    TL_CHECK(
        tl::core::stick_destination(0.0f, 1.0f, 0.0f, 10.0f, 20.0f, 0.0f, 1.0f, false, &x, &z));
    TL_CHECK(std::fabs(x - 10.0f) < 1e-4f);
    TL_CHECK(std::fabs(z - 22.0f) < 1e-4f);

    // Pushed right: two units along -X at yaw 0, the same handedness the
    // player's own camera-relative movement uses.
    TL_CHECK(
        tl::core::stick_destination(1.0f, 0.0f, 0.0f, 10.0f, 20.0f, 0.0f, 1.0f, false, &x, &z));
    TL_CHECK(std::fabs(x - 8.0f) < 1e-4f);
    TL_CHECK(std::fabs(z - 20.0f) < 1e-4f);

    // The reach is the original's 2.0 regardless of the camera's yaw.
    TL_CHECK(tl::core::stick_destination(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, false, &x, &z));
    TL_CHECK(std::fabs(std::sqrt(x * x + z * z) - 2.0f) < 1e-4f);
}

/// The right stick's zoom: 14 at the near end, 28.5 times the
/// camera multiplier at the far one.
TL_TEST(test_zoom) {
    TL_CHECK_EQ(tl::core::kZoomMin, 14.0f);
    TL_CHECK_EQ(tl::core::kZoomDefaultScale, 28.5f);

    // Pushed up, the dolly comes in and stops at 14.
    float dolly = 28.5f;
    for (int i = 0; i < 200; ++i) dolly = tl::core::zoom_dolly(dolly, 32767, 1.0f, 1.0f / 30.0f);
    TL_CHECK_EQ(dolly, 14.0f);

    // Pushed down, it goes out and stops at 28.5 * the multiplier.
    for (int i = 0; i < 400; ++i) dolly = tl::core::zoom_dolly(dolly, -32767, 2.0f, 1.0f / 30.0f);
    TL_CHECK_EQ(dolly, 57.0f);

    // One frame at full deflection is the original's 0.35 of a unit.
    dolly = 30.0f;
    dolly = tl::core::zoom_dolly(dolly, -32767, 4.0f, 1.0f / 30.0f);
    TL_CHECK(std::fabs(dolly - 30.35f) < 1e-3f);
}

// `CCameraControl::setCinematicMode(bool)`: 28.5 going in, held to
// `28.5 * CAMERAMULT`; the dolly given back coming out, no nearer than 14.
TL_TEST(cinematic_mode_puts_the_dolly_aside_and_gives_it_back) {
    tl::core::CinematicCamera camera;
    float dolly = 20.0f;
    tl::core::set_cinematic_mode(camera, dolly, true, 0.87f);
    TL_CHECK(camera.on);
    TL_CHECK_EQ(camera.saved, 20.0f);
    TL_CHECK_EQ(dolly, 28.5f * 0.87f);
    // Already on: nothing, and the dolly it put aside is kept.
    dolly = 16.0f;
    tl::core::set_cinematic_mode(camera, dolly, true, 0.87f);
    TL_CHECK_EQ(dolly, 16.0f);
    TL_CHECK_EQ(camera.saved, 20.0f);
    tl::core::set_cinematic_mode(camera, dolly, false, 0.87f);
    TL_CHECK(!camera.on);
    TL_CHECK_EQ(dolly, 20.0f);

    tl::core::set_cinematic_mode(camera, dolly, true, 1.2f);
    TL_CHECK_EQ(dolly, 28.5f);
    camera.saved = 3.0f;
    tl::core::set_cinematic_mode(camera, dolly, false, 1.2f);
    TL_CHECK_EQ(dolly, 14.0f);
}
