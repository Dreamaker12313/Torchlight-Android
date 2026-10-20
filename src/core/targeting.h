// The pad's target pick:, called once a frame from the client's selector;
// `CGameClient::update` highlights the result.
// The caller supplies the candidates; which one wins is decided here.
#pragma once

#include <cstddef>
#include <cstdint>

namespace tl::core {

/// Half-angle of the cone in front of the character, in degrees
/// (the constant +0x3B0).
constexpr float kTargetConeDegrees = 30.0f;

/// Inside this distance a candidate is taken whatever the angle, and without a
/// line-of-sight check - you are standing on it.
constexpr float kTargetTouchRadius = 0.5f;

/// The reach of the cone when nothing is equipped: the original takes
/// `max(7.0, weapon range + 2.0)`.
constexpr float kTargetMinRange = 7.0f;
constexpr float kTargetWeaponRangeBonus = 2.0f;

/// The second pass: anything at all within this, in any direction
/// (the 3.0 near the end).
constexpr float kTargetFallbackRadius = 3.0f;

/// The item pass searches from a point this far *behind* the character, with
/// this reach (`position - 1.5 * forward`, radius 3.5), so
/// something just walked over is still picked up.
constexpr float kTargetItemBehind = 1.5f;
constexpr float kTargetItemRadius = 3.5f;

/// When the best candidate and a new one are both hostile, the new one has to
/// be no more than this many degrees further off the facing to replace it
/// - hysteresis, so the highlight does not flicker between two monsters.
constexpr float kTargetAngleHysteresis = 5.0f;

/// Both ends of the line-of-sight ray are raised by this much, so it is cast
/// over the floor rather than along it.
constexpr float kTargetSightHeight = 5.0f;

/// One thing that could be targeted, in the caller's own world coordinates.
struct TargetCandidate {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    /// Whether the player can attack it. The original prefers a hostile
    /// candidate over a harmless one whatever the distances
    /// ((player, unit)`).
    bool hostile = false;

    /// Whether anything blocks the line from the player to it. The caller
    /// tests this, because collision lives outside core.
    bool in_sight = true;

    /// Whatever the caller wants back; this code only passes it through.
    std::uint64_t id = 0;
};

/// The reach of the first pass: `max(7.0, weapon_range + 2.0)`.
///
float target_range(float weapon_range);

/// Pick what the character is pointing at, or return false. `facing` is the heading in
/// radians (`Player::facing()`'s convention). Pass one is the cone (line of sight needed
/// beyond `kTargetTouchRadius`); pass two, only if it found nothing, anything within
/// `kTargetFallbackRadius`. Hostile beats harmless, then the nearest.
bool select_target(const TargetCandidate* candidates, std::size_t count, float player_x,
                   float player_z, float facing, float range, std::uint64_t* out_id);

}  // namespace tl::core
