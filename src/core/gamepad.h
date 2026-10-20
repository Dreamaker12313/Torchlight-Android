// The Xbox 360 build's pad handling, pure arithmetic over a pad's raw state:
// the stick and trigger filters the per-frame poll runs; which buttons a menu
// counts as "down" (the stick doubling as the D-pad); and how the left stick
// becomes a point in the world. The platform layers supply raw
// `XINPUT_GAMEPAD`-shaped numbers, which must not be filtered twice.
#pragma once

#include <cstdint>

namespace tl::core {

/// The button numbers the Xbox build uses internally.
///
/// `CGamepadManager`'s keystroke map
/// turns XAM's `VK_PAD_*` codes into these, in the original's
/// order; the stick directions are buttons in their own right.
enum class PadButton : std::uint8_t {
    None = 0,
    DPadUp = 1,
    DPadDown = 2,
    DPadLeft = 3,
    DPadRight = 4,
    A = 5,
    B = 6,
    X = 7,
    Y = 8,
    RightTrigger = 9,
    RightBumper = 10,
    LeftTrigger = 11,
    LeftBumper = 12,
    Start = 13,
    Back = 14,
    LeftStickClick = 15,
    RightStickClick = 16,
    LeftStickUp = 17,
    LeftStickDown = 18,
    LeftStickLeft = 19,
    LeftStickRight = 20,
    RightStickUp = 21,
    RightStickDown = 22,
    RightStickLeft = 23,
    RightStickRight = 24,
};

/// `wButtons` bits, as XInput defines them and the original tests them.
///
enum PadBits : std::uint16_t {
    kPadDPadUp = 0x0001,
    kPadDPadDown = 0x0002,
    kPadDPadLeft = 0x0004,
    kPadDPadRight = 0x0008,
    kPadStart = 0x0010,
    kPadBack = 0x0020,
    kPadLeftStickClick = 0x0040,
    kPadRightStickClick = 0x0080,
    kPadLeftBumper = 0x0100,
    kPadRightBumper = 0x0200,
    kPadA = 0x1000,
    kPadB = 0x2000,
    kPadX = 0x4000,
    kPadY = 0x8000,
};

/// A pad exactly as the original reads it: signed 16-bit sticks, 8-bit
/// triggers, a bitmask of buttons.
struct PadState {
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;
    std::int16_t right_x = 0;
    std::int16_t right_y = 0;
};

/// XInput's own deadzones, which the pad poll passes to the stick filter
/// (0x1EA9 for the left stick, 0x21F1 for the right).
constexpr std::int16_t kLeftStickDeadzone = 7849;
constexpr std::int16_t kRightStickDeadzone = 8689;

/// Below this raw value a trigger reads as zero.
constexpr std::uint8_t kTriggerThreshold = 30;

/// Full scale for a stick axis. The original divides by it with the literal
/// 3.0518509e-05, which is this reciprocal to a float.
constexpr float kStickMax = 32767.0f;

/// Radial deadzone with the remainder rescaled to full deflection, in place,
/// on the raw axes.
///
/// The magnitude less the deadzone, stretched over the full range by
/// `32767 / (32767 - deadzone)`, along the *normalised* direction, so a
/// diagonal is not faster than a cardinal.
void filter_stick(std::int16_t& x, std::int16_t& y, std::int16_t deadzone);

/// A trigger's dead range and rescale, in place.
///
/// Under `kTriggerThreshold` the trigger is zero; above it the remainder is
/// stretched over the full byte by 1.1333333 (255/225), and the result is
/// truncated, not rounded, because the original converts with `fctidz`.
void filter_trigger(std::uint8_t& value);

/// Whether a menu counts `bits` as held.
///
/// The four D-pad bits also answer to the left stick pushed past its deadzone,
/// which is how every `_xenon` menu is navigated with either. The comparison
/// is strict, and the stick clicks and triggers are *not* answered here - the
/// original's switch has no case for them and falls through to false.
bool button_down(const PadState& pad, std::uint16_t bits);

/// A point in the world, in front of the player, that the left stick is asking
/// for.
///
/// The console does not steer the character with the pad: the stick's
/// direction is rotated by the camera's yaw, a point placed `kStickReach` away,
/// and the click-to-move path asked to walk there (sets the
/// target and the direction). A centred stick asks for the point directly
/// ahead, which puts an attack where the player faces.
///
/// `stick_x`/`stick_y` are the filtered stick in -1..1, `camera_yaw` the
/// camera's heading in radians; the result goes to `out_x`/`out_z`. False when
/// the stick is inside `kStickThreshold` and no fallback was asked for.
constexpr float kStickReach = 2.0f;

/// Below this deflection the stick is not asking to go anywhere
/// (`sqrt(x*x + y*y) <= 0.1`).
constexpr float kStickThreshold = 0.1f;

bool stick_destination(float stick_x, float stick_y, float camera_yaw, float player_x,
                       float player_z, float facing_x, float facing_z, bool ahead_when_centred,
                       float* out_x, float* out_z);

/// The right stick's zoom step, in dolly units, for one frame's deflection:
/// `stick / 32767 * -0.35` added to the dolly, clamped
/// between 14 and `28.5 * the camera multiplier`.
///
/// DIVERGENCE: the original's step is per frame; this is scaled by the frame
/// time against 30 fps, so the zoom does not depend on the frame rate.
constexpr float kZoomStep = -0.35f;
constexpr float kZoomMin = 14.0f;
constexpr float kZoomDefaultScale = 28.5f;

float zoom_dolly(float dolly, std::int16_t right_stick_y, float camera_multiplier, float delta);

/// `CCameraControl +0x98`, cinematic mode, and `+0x94`, the dolly it put
/// aside. While it is on, `processInput` does not zoom.
struct CinematicCamera {
    bool on = false;
    float saved = 0.0f;
};

/// `CCameraControl::setCinematicMode(bool)`, from a `Game State
/// Controller` in `INTERACTIVE UI`: going in, the dolly (`+0x38`) is put aside
/// and set to 28.5; coming out it is given back, no nearer than 14. Either way
/// no farther than `28.5 * CAMERAMULT` (`+0x8c`). Nothing when already so.
void set_cinematic_mode(CinematicCamera& camera, float& dolly, bool on, float camera_multiplier);

}  // namespace tl::core
