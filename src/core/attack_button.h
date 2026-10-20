// The console's attack button (X): what holding it does. The in-game dispatch
// and the handler it calls every frame the button is down.
// Holding X sets "hold position": the
// stick aims instead of walking, the targets are let go, and every 0.1 s the
// handler asks for a destination, which under the hold stops the character and
// calls `CCharacter::attack()`
// with no target: the blow lands on whatever is in front.
//
// Pure state and decisions; the renderer applies the result.
#pragma once

#include "core/skill_animation.h"

namespace tl::core {

/// The client's and the player's state for the button.
struct AttackButton {
    /// `CCharacter +0x1fe`: hold position. Set or cleared every unpaused frame
    /// the player can act; kept as it is otherwise.
    bool hold = false;
    /// `CGameClient +0x55`: the button was down at the last dispatch that ran.
    bool was_down = false;
    /// `CGameClient +0x40`: seconds until the handler asks for a destination
    /// again. Counted down every frame, set to 0.1 when it asks, and to 0 on a
    /// fresh press.
    float repeat = 0.0f;
};

/// What the dispatch reads for the button this frame.
struct AttackButtonInput {
    /// X down: pressed this frame or held.
    bool down = false;
    /// X pressed this frame (`+0x2a1`).
    bool pressed = false;
    /// The player alive and enabled: guards the block.
    bool can_act = false;
    /// `getIsPaused`, which both the aim and the handler test.
    bool paused = false;
    /// The handler's own state test: the player's AI state is
    /// neither 0x22 (the fishing catch) nor 0x14.
    bool state_allows = true;
    /// The left stick past 0.1.
    bool stick_pushed = false;
    /// a skill holds the character (`skill_holds_character`).
    bool skill_holds = false;
};

/// What the frame asks the renderer to do, in this order.
struct AttackButtonFrame {
    /// The X block: the stick's point, or one ahead of the player
    /// with the stick at rest (`stick_destination` with the fallback), handed
    /// to `setTargetDirection`.
    bool aim = false;
    /// `setTarget(0)` and the target item (`+0x28c`) cleared: the stick pushed,
    /// or a fresh press.
    bool let_targets_go = false;
    /// A fresh press: `setAIState(0)` (vtable +0x170).
    bool reset_ai = false;
    /// `setDestination`: at the stick's point
    /// when it is pushed, else at the point last aimed at. With the hold set it
    /// stops the character and attacks.
    bool destination = false;
};

/// One dispatch of the button (for the button's
/// share, with inlined), advancing `state`.
AttackButtonFrame attack_button_step(AttackButton& state, const AttackButtonInput& in, float delta);

/// and the skill test at the head of `CCharacter::attack`:
/// a skill is held and its animation is up, or has
/// time left before its swing is over.
bool skill_holds_character(const ActiveSkill& active);

}  // namespace tl::core
