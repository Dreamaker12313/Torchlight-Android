// Fishing: what the player does at a `FISHING HOLE` prop.
//
// - `CCharacter::getItemAI` starts it: a
//   targeted `FISHING HOLE` (0x7a), reached, in sight and faced, calls
//   `startFishing` instead of interacting with it.
// - `CPlayer::startFishing`: AI state 0x20, the
//   bobber armed; `CCharacter::fishingAICast`
//   moves to 0x21 once `FISHING_CAST` has finished.
// - `CPlayer::fishingAI` is state 0x21: the bobber,
//   drawn as a ring (`fishing_xenon.layout`) with catch and stop.
// - `CPlayer::catchFish` is state 0x22, and
//   `CPlayer::fishingAICatch` settles it once
//   `FISHING_CATCH` has finished: a hooked fish rolls the level's fish spawn
//   class.
//
// No OGRE and no file I/O: animations, the dialog, the roll and the inventory
// are the caller's.
#pragma once

#include "core/random.h"

#include <string>

namespace tl::core {

/// The player's AI states for fishing (`CCharacter::updateAI`
/// cases 0x20..0x22). `Off` is anything else.
enum class FishingState {
    Off,
    Cast,     ///< 0x20, `fishingAICast` (vtable +0x398)
    Waiting,  ///< 0x21, `fishingAI` (+0x3a8)
    Catch,    ///< 0x22, `fishingAICatch` (+0x3b0)
};

/// `CPlayer +0x850..+0x860` (X360 `+0x77c..+0x78c`).
struct FishingBobber {
    float depth = 0.0f;   ///< +0x850: 2 at the cast, rests at 1 when it dips
    float phase = 0.0f;   ///< +0x854: the ripple's angle, 4 radians a second
    float speed = 0.0f;   ///< +0x858: how fast `depth` changes
    bool hooked = false;  ///< +0x85c: the catch landed inside a bite
    float bite = 0.0f;    ///< +0x860: seconds left of the current bite
};

/// `CPlayer::startFishing`, the bobber's
/// half: `hooked` clear, `speed` a volatile draw in [1, 3], `depth` 2, `bite`
/// and `phase` 0.
void start_fishing(FishingBobber& bobber, RunicRandom& volatile_rng);

/// `CPlayer::fishingAI`: one step of the
/// bobber, `delta` seconds. `animating` is whether `FISHING_CAST` or
/// `FISHING_BITE` is still playing (`animationPlayingSubstring`). Returns true
/// when a bite starts: the caller then plays `CCharacter::fishingSplash`.
///
bool step_fishing(FishingBobber& bobber, float delta, bool animating, RunicRandom& volatile_rng);

/// `CPlayer::catchFish`, the bobber's half:
/// the fish is hooked exactly when the catch lands inside a bite.
void catch_fish(FishingBobber& bobber);

/// `CFishingMenu::update`: the ring's width and
/// height in layout pixels, `(sin(phase) * 0.2 + depth) * 128`, or 128 during a
/// bite.
float fishing_ring_extent(const FishingBobber& bobber);

/// The same function's colour: (1, 0.3, 0.3) during a bite, white otherwise.
inline bool fishing_ring_alert(const FishingBobber& bobber) { return 0.0f < bobber.bite; }

/// `fishingAICatch`'s dialog title, `g_Fishing`: "Fishing".
const char* fishing_dialog_title();

/// `fishingAICatch`'s dialog text.
///
/// - Nothing hooked: "You caught nothing!".
/// - Something hooked and one of the units made ISA `FISH` (0x7c):
///   "You caught a " + name + "!".
/// - Otherwise: "You caught a fish containing:\n" + name + "!".
///
/// `name` is the display name of the **last** unit the roll made (the loop
/// overwrites it for every unit), and empty when it made none - which the
/// original prints as it stands.
std::string fishing_catch_text(bool hooked, bool made_a_fish, const std::string& name);

}  // namespace tl::core
