#include "core/fishing.h"

#include <cmath>

namespace tl::core {

void start_fishing(FishingBobber& bobber, RunicRandom& volatile_rng) {
    // `CPlayer::startFishing`, after
    // `CCharacter::startFishing` and the hooked fish's model hidden: hooked
    // cleared, the one draw, depth 2.0 (0x40000000), bite and phase cleared.
    bobber.hooked = false;
    bobber.speed = volatile_rng.between(1.0f, 3.0f);
    bobber.depth = 2.0f;
    bobber.bite = 0.0f;
    bobber.phase = 0.0f;
}

bool step_fishing(FishingBobber& bobber, float delta, bool animating, RunicRandom& volatile_rng) {
    // `CPlayer::fishingAI`.
    bobber.phase = delta * 4.0f + bobber.phase;

    // The speed wanders: pushed down while the bobber floats at 2 or above,
    // up while it lies at 1 or below, either way in between. Written as the
    // original tests it, `2 <= depth || depth <= 1` first; its third arm
    // (`1 < depth` inside that test) cannot be reached.
    float lo;
    float hi;
    if (2.0f <= bobber.depth || bobber.depth <= 1.0f) {
        if (2.0f <= bobber.depth) {
            lo = -8.0f;
            hi = 0.0f;
        } else {
            lo = 0.5f;
            hi = 10.0f;
        }
    } else {
        lo = -4.1f;
        hi = 4.0f;
    }
    const float push = volatile_rng.between(lo, hi);
    bobber.speed = push * delta + bobber.speed;

    // Now and then, near the surface, a tug: 30 to 40 more.
    // DIVERGENCE: the original rolls `randomBetweenVolatile(0, 10000) < 5` once
    // per AI tick, so how often a fish tugs follows the frame rate. The same
    // draw is made here, the threshold scaled to 5 in 10,000 per tick at 60 Hz
    // (the frame-rate exception).
    if (bobber.depth < 1.75f) {
        const float roll = volatile_rng.between(0.0f, 10000.0f);
        if (roll < 5.0f * (delta * 60.0f)) {
            bobber.speed = bobber.speed + volatile_rng.between(30.0f, 40.0f);
        }
    }

    if (0.0f < bobber.bite) bobber.bite = bobber.bite - delta;

    // +0x858 kept in [-4, 10] (a NaN counts as too fast, as `10 <= x || NaN`).
    float speed = bobber.speed;
    if (10.0f <= speed || std::isnan(speed)) {
        speed = 10.0f;
    } else if (speed <= -4.0f) {
        speed = -4.0f;
    }
    bobber.speed = speed;

    float depth = delta * speed + bobber.depth;
    if (2.0f <= depth) depth = 2.0f;
    bobber.depth = depth;

    // The ripple taken with it, the bobber is under: it rests at 1 and, unless
    // the cast or the last bite is still playing, a bite starts - 0.25 to 0.4
    // seconds to catch it in.
    if (!(std::sin(bobber.phase) * 0.2f + depth <= 1.0f)) return false;
    bobber.depth = 1.0f;
    bobber.speed = 1.0f;
    if (animating) return false;
    bobber.bite = volatile_rng.between(0.25f, 0.4f);
    return true;
}

void catch_fish(FishingBobber& bobber) {
    // `CPlayer::catchFish`: +0x85c cleared,
    // then set when +0x860 is still above 0.
    bobber.hooked = false;
    if (0.0f < bobber.bite) bobber.hooked = true;
}

float fishing_ring_extent(const FishingBobber& bobber) {
    // `CFishingMenu::update`: `setSize` to
    // `scaledY(v * 128)` both ways, the position moved by half of it so the
    // ring stays centred.
    const float ripple = std::sin(bobber.phase);
    const float v = 0.0f < bobber.bite ? 1.0f : ripple * 0.2f + bobber.depth;
    return v * 128.0f;
}

// `CPlayer::fishingAICatch`'s dialog title,
// `g_Fishing`. PORT: the PC's English text.
const char* fishing_dialog_title() { return "Fishing"; }

std::string fishing_catch_text(bool hooked, bool made_a_fish, const std::string& name) {
    // `CPlayer::fishingAICatch`:
    // `g_CaughtNothing` + "!"; `g_Caught` + " " + name + "!";
    // `g_CaughtContaining` + ":\n" + name + "!".
    if (!hooked) return "You caught nothing!";
    if (made_a_fish) return "You caught a " + name + "!";
    return "You caught a fish containing:\n" + name + "!";
}

}  // namespace tl::core
