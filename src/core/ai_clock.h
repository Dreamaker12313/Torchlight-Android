#pragma once

// How often a monster's AI may think: not once a frame. `CMonster::updateAI`
// adds the delta to an accumulator and runs the
// state handler once it passes `g_AI_INTERVALS[band]`. The band is from
// `CCharacter::calculateActiveRange(const Ogre::Vector3&)`:
// the ground distance to the
// point the camera is aimed at, not the camera (`CGameClient::updateIngame`,
// `CLevel::update`, `CCameraControl::updateGameCamera`), and 0
// for a unit whose master is a player.
//
// The accumulator is tested and zeroed, never decremented: a long frame loses
// ticks instead of firing a burst of them.

namespace tl::core {

/// `g_AI_INTERVALS` at .rodata, in seconds.
inline constexpr float kAiIntervals[4] = {0.1f, 0.2f, 0.4f, 0.8f};

/// The three distances `CCharacter::calculateActiveRange` compares against,
/// in level units. Past the last one the interval is `kAiIntervals[3]`.
inline constexpr float kAiRangeBands[3] = {10.0f, 15.0f, 20.0f};

/// How long a unit this far from the point the camera is aimed at waits between
/// AI ticks.
inline constexpr float ai_interval(float distance_to_view) {
    for (int i = 0; i < 3; ++i) {
        if (distance_to_view <= kAiRangeBands[i]) return kAiIntervals[i];
    }
    return kAiIntervals[3];
}

}  // namespace tl::core
