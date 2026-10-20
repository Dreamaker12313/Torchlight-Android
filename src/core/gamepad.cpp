#include "core/gamepad.h"

#include <cmath>

namespace tl::core {

namespace {

/// The original's own float clamp of a stick axis after rescaling
/// (+32767 and -32767 as floats, then `fctiwz`).
std::int16_t clamp_axis(float v) {
    if (v > kStickMax) v = kStickMax;
    if (v < -kStickMax) v = -kStickMax;
    return static_cast<std::int16_t>(v);
}

}  // namespace

// CGamepadManager stick filter.
void filter_stick(std::int16_t& x, std::int16_t& y, std::int16_t deadzone) {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    // `fmuls` x*x, then `fmadds`: y*y fused on with one rounding.
    //
    const float magnitude = std::sqrt(std::fma(fy, fy, fx * fx));

    // The deadzone comes off the magnitude, and what is left - never less than
    // zero - is stretched back over the full range.
    const float beyond = magnitude - static_cast<float>(deadzone);
    const float remainder = beyond > 0.0f ? beyond : 0.0f;
    const float scale = kStickMax / static_cast<float>(32767 - deadzone);
    const float value = scale * remainder;

    // Normalise first, so the rescaled magnitude is applied along the
    // direction the player is pushing. The original's guard is 1e-8 as a
    // double, which is the same test as "not exactly centred".
    float nx = fx;
    float ny = fy;
    if (static_cast<double>(magnitude) > 1e-08) {
        const float inv = 1.0f / magnitude;
        nx *= inv;
        ny *= inv;
    }

    x = clamp_axis(nx * value);
    y = clamp_axis(ny * value);
}

// CGamepadManager trigger filter.
void filter_trigger(std::uint8_t& value) {
    if (value < kTriggerThreshold) {
        value = 0;
        return;
    }
    if (value == 0) return;
    // The original's clamp to 255 cannot fire on a byte; left out.
    const float scaled = static_cast<float>(value - kTriggerThreshold) * 1.1333333253860474f;
    value = static_cast<std::uint8_t>(static_cast<std::int64_t>(scaled));
}

// CGamepadManager::isButtonDown.
bool button_down(const PadState& pad, std::uint16_t bits) {
    switch (bits) {
        case kPadDPadUp: return (pad.buttons & kPadDPadUp) != 0 || pad.left_y > kLeftStickDeadzone;
        case kPadDPadDown:
            return (pad.buttons & kPadDPadDown) != 0 || pad.left_y < -kLeftStickDeadzone;
        case kPadDPadLeft:
            return (pad.buttons & kPadDPadLeft) != 0 || pad.left_x < -kLeftStickDeadzone;
        case kPadDPadRight:
            return (pad.buttons & kPadDPadRight) != 0 || pad.left_x > kLeftStickDeadzone;
        case kPadStart:
        case kPadBack:
        case kPadLeftBumper:
        case kPadRightBumper:
        case kPadA:
        case kPadB:
        case kPadX:
        case kPadY: return (pad.buttons & bits) != 0;
        default:
            // The stick clicks and the triggers have no case in the original's
            // switch, and a mask it does not know falls out as false.
            return false;
    }
}

// The stick's point in the world.
bool stick_destination(float stick_x, float stick_y, float camera_yaw, float player_x,
                       float player_z, float facing_x, float facing_z, bool ahead_when_centred,
                       float* out_x, float* out_z) {
    if (std::sqrt(stick_x * stick_x + stick_y * stick_y) <= kStickThreshold) {
        if (!ahead_when_centred) return false;
        // Centred: the point the character is already facing.
        *out_x = player_x + facing_x;
        *out_z = player_z + facing_z;
        return true;
    }

    // The camera's heading less a quarter turn is the stick's rotation; the
    // original builds it from the camera's forward vector and a rotation matrix
    // about +Z, which multiplies out to this.
    const float angle = camera_yaw - 1.57079632679489661923f;
    const float sin_a = std::sin(angle);
    const float cos_a = std::cos(angle);
    const float dx = stick_x * kStickReach;
    const float dy = stick_y * kStickReach;

    *out_z = player_z + (dx * cos_a - dy * sin_a);
    *out_x = player_x + (dx * sin_a + dy * cos_a);
    return true;
}

// The right stick's zoom.
float zoom_dolly(float dolly, std::int16_t right_stick_y, float camera_multiplier, float delta) {
    // DIVERGENCE: per-frame in the original, per-second here (see the header).
    const float frames = delta * 30.0f;
    dolly += static_cast<float>(right_stick_y) * (1.0f / kStickMax) * kZoomStep * frames;
    const float max = camera_multiplier * kZoomDefaultScale;
    if (dolly < kZoomMin) return kZoomMin;
    if (dolly > max) return max;
    return dolly;
}

// `CCameraControl::setCinematicMode(bool)`.
void set_cinematic_mode(CinematicCamera& camera, float& dolly, bool on, float camera_multiplier) {
    if (camera.on == on) return;
    float wanted = kZoomDefaultScale;
    if (on) {
        camera.saved = dolly;
        dolly = kZoomDefaultScale;
    } else {
        wanted = camera.saved;
        dolly = wanted;
        if (wanted < kZoomMin) {
            dolly = kZoomMin;
            camera.on = on;
            return;
        }
    }
    const float farthest = kZoomDefaultScale * camera_multiplier;
    if (farthest < wanted) dolly = farthest;
    camera.on = on;
}

}  // namespace tl::core
