// The level machine's `Puzzle Input` node (`CPuzzleRandomizer`); split from script.cpp.
#include "core/script.h"

#include "core/logic_events.h"
#include "core/random.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace tl::core {
using namespace tl::core::logic;

// ---------------------------------------------------------------------------
// Puzzle Input (`CPuzzleRandomizer`)
// ---------------------------------------------------------------------------

// `CPuzzleRandomizer::setActiveInputs`.
void LevelScript::puzzle_set_active_inputs(Object& o, uint32_t count) {
    // Clamping (unsigned): count <= 1 clamps to 2; count >= 6 clamps to 3.
    // The original's quirk: > 5 clamps to 3, not 5.
    if (count <= 1) {
        count = 2;
    } else if (count >= 6) {
        count = 3;
    }
    o.puzzle_active_inputs = count;

    // Refill order array 0..5 and clear all 6 pulled flags.
    // Does not touch puzzle_solved, enabled, or puzzle_state.
    for (uint32_t i = 0; i < 6; ++i) {
        o.puzzle_order[i] = i;
        o.puzzle_pulled[i] = false;
    }

    // Exactly 5 shuffle passes, each swapping order[i] with order[j] =
    // UTILITIES::randomIntegerBetweenVolatile(0, active_inputs - 1)
    // for every i: 5 * active_inputs draws.
    for (int pass = 0; pass < 5; ++pass) {
        for (uint32_t i = 0; i < o.puzzle_active_inputs; ++i) {
            const uint32_t j = static_cast<uint32_t>(volatile_random().integer_between(
                0, static_cast<int32_t>(o.puzzle_active_inputs - 1)));
            std::swap(o.puzzle_order[i], o.puzzle_order[j]);
        }
    }
}

// `CPuzzleRandomizer::trigger`.
void LevelScript::puzzle_trigger(Object& o, uint32_t input_index) {
    if (o.puzzle_solved || !o.enabled) return;

    // Unsigned comparison: input_index >= active_inputs.
    if (input_index >= o.puzzle_active_inputs) {
        // Original logs a warning via Ogre::LogManager; src/core has no log channel.
        return;
    }

    // Find the first unpulled slot.
    uint32_t slot = 0;
    while (slot < o.puzzle_active_inputs && o.puzzle_pulled[slot]) {
        ++slot;
    }

    // Fall-through quirk: if all slots were already pulled, mark solved and state 2.
    if (slot >= o.puzzle_active_inputs) {
        puzzle_add_level_listener(o);
        o.puzzle_solved = true;
        o.puzzle_state = 2;
        return;
    }

    if (o.puzzle_order[slot] != input_index) {
        // Mismatch: failed.
        puzzle_add_level_listener(o);
        o.puzzle_state = 1;
        return;
    }

    // Slot matches.
    o.puzzle_pulled[slot] = true;
    if (slot == o.puzzle_active_inputs - 1) {
        // Last slot: solved.
        puzzle_add_level_listener(o);
        o.puzzle_solved = true;
        o.puzzle_state = 2;
    }
    // If not the last slot, returns immediately without setting state or notifying listener.
}

// `CPuzzleRandomizer::reset`.
void LevelScript::puzzle_reset(Object& o) {
    // Reset re-enables (+0x68 = 1), clears solved (+0x60 = 0), clears all 6 pulled flags (+0x84..+0x89).
    // Does NOT reshuffle: order is untouched.
    o.enabled = true;  // the original's: a reset re-enables
    o.puzzle_solved = false;
    for (bool& pulled : o.puzzle_pulled) pulled = false;
    puzzle_add_level_listener(o);
    o.puzzle_state = 3;  // Was Reset
}

// `CPuzzleRandomizer::updateLevelObject`.
bool LevelScript::puzzle_level_update(Object& o, QuestState& quests,
                                      std::vector<ScriptEvent>& out) {
    if (o.puzzle_state == 2) {
        // SUCCESS (0x37).
        broadcast(o.id, kSuccess, quests, out);
        o.puzzle_state = 0;
        return false;
    }
    if (o.puzzle_state == 3) {
        // Was Reset (0x09).
        broadcast(o.id, kWasReset, quests, out);
        o.puzzle_state = 0;
        return false;
    }
    if (o.puzzle_state == 1) {
        // FAILED (0x36). Broadcasts FAILED, calls reset() (setting state = 3 and listening),
        // and returns 1 (kept in level update list for next frame).
        broadcast(o.id, kFailed, quests, out);
        puzzle_reset(o);
        return true;
    }
    o.puzzle_state = 0;
    return false;
}

// `CPuzzleRandomizer::addLevelListener`.
void LevelScript::puzzle_add_level_listener(Object& o) { add_level_update(o.id); }

// `CLevel::addUpdateObject`: an object
// already listed is not added again.
void LevelScript::add_level_update(int64_t id) {
    if (std::find(level_updates_.begin(), level_updates_.end(), id) == level_updates_.end()) {
        level_updates_.push_back(id);
    }
}

}  // namespace tl::core
