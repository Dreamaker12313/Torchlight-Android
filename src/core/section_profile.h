// PORT: diagnostics, where a frame's time on the game thread goes, by part of the update.
// Accumulated by scopes and read out by the renderer with its timing windows.
#pragma once

#include <chrono>
#include <cstdint>

namespace tl::core {

enum class Section : uint8_t {
    // The renderer's frame update, whole, then its parts in its own order.
    Update,
    Animation,      // poses, scene animations, weapon trails, fidgets
    UnitKeys,       // sounds, particle keys and breakables the poses crossed
    Particles,      // particle triggers, visible links, ParticleUniverse's own step
    Spawners,       // what spawners still owe
    Missiles,       // missiles in flight, and the particles let go below
    Disposal,       // particles let go once they have died out
    Fight,          // the swings, the skill animations, the pet
    EffectsSkills,  // every unit's effects and casts
    Script,         // the level's machines, the events they raise applied
    ScriptEvents,   // those events applied to the scene
    // Inside the machines' update.
    Characters,  // every character's update, below included
    Ai,          // the AI's decision
    Pathing,     // following and re-planning a route
    Motion,      // moving the sphere against the collision
    Count
};

/// Nanoseconds and calls per section since the last reset (per-unit scopes are a few
/// microseconds each, too short to add up in whole microseconds).
struct SectionTotals {
    uint64_t ns[static_cast<int>(Section::Count)] = {};
    uint64_t calls[static_cast<int>(Section::Count)] = {};
};

SectionTotals& section_totals();
const char* section_name(Section section);

/// Adds the time from construction to destruction to `section`.
class SectionScope {
public:
    explicit SectionScope(Section section)
        : section_(section), start_(std::chrono::steady_clock::now()) {}
    ~SectionScope();
    SectionScope(const SectionScope&) = delete;
    SectionScope& operator=(const SectionScope&) = delete;

private:
    Section section_;
    std::chrono::steady_clock::time_point start_;
};

/// `call()` timed as `section`.
template <typename Call>
void timed(Section section, Call&& call) {
    const SectionScope scope(section);
    call();
}

}  // namespace tl::core
