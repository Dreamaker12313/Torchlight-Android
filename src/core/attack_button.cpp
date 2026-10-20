#include "core/attack_button.h"

namespace tl::core {

AttackButtonFrame attack_button_step(AttackButton& state, const AttackButtonInput& in,
                                     float delta) {
    AttackButtonFrame frame;
    // a player, alive and enabled; otherwise nothing of the
    // button changes.
    if (!in.can_act) return frame;

    // the clock runs down every frame.
    if (0.0f < state.repeat) state.repeat = state.repeat - delta;

    // not paused: the button down sets the hold and aims
    // (the stick's point with the fallback ahead); up clears it.
    // `KEYMAP_HOLDPOS` (`CSettings::CSettings`) is tested first;
    // no pad button reaches it.
    if (!in.paused) {
        state.hold = in.down;
        frame.aim = in.down;
    }

    // a fresh press puts the clock at 0.
    if (in.pressed && !state.was_down) state.repeat = 0.0f;

    // with the button down, the handler. The skill on the
    // button (`castSkill`) is tried first; the port carries none.
    if (!in.down) {
        // the pending targets dropped, as
        // `CGameClient::update` does every frame anyway.
        state.was_down = false;
        return frame;
    }
    const bool fresh = !state.was_down;
    state.was_down = true;

    //
    if (in.paused || !in.state_allows) return frame;
    // refill the pending targets on a fresh press;
    // `CGameClient::update` empties both every frame, so the `setTarget`s from them
    // come to nothing.
    if (in.stick_pushed) frame.let_targets_go = true;
    // a skill holding the character - nothing more.
    if (in.skill_holds) return frame;
    if (fresh) {
        frame.let_targets_go = true;
        frame.reset_ai = true;
    }
    // with the interface flag clear or the hold set; the hold is
    // always set by here, so this always runs.
    if (!state.hold) return frame;

    // `setDestination` whenever the clock has run out, which
    // starts it again at 0.1.
    if (state.repeat <= 0.0f) {
        frame.destination = true;
        state.repeat = 0.1f;
    }
    return frame;
}

// and the skill test at the head of `CCharacter::attack`:
// a skill holds the character while it is cast or its
// animation runs; an unordered time does not hold.
bool skill_holds_character(const ActiveSkill& active) {
    if (active.empty()) return false;
    if (active.casting) return true;
    // `ucomiss`/`jbe`: an unordered time does not hold.
    return 0.0f < active.animation_left && !active.attack_ended;
}

}  // namespace tl::core
