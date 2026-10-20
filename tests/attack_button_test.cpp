// The console's X ('s share of the button):
// the hold, the aim, the targets let go and the 0.1 s repeat.
#include "core/attack_button.h"
#include "test_support.h"

using tl::core::ActiveSkill;
using tl::core::attack_button_step;
using tl::core::AttackButton;
using tl::core::AttackButtonFrame;
using tl::core::AttackButtonInput;
using tl::core::skill_holds_character;

namespace {

constexpr float kFrame = 1.0f / 60.0f;

AttackButtonInput down(bool pressed) {
    AttackButtonInput in;
    in.can_act = true;
    in.down = true;
    in.pressed = pressed;
    return in;
}

}  // namespace

// A press: the hold set, the aim taken, the targets let go, the AI state
// reset, and a destination (an attack in place) at once; held, another every
// 0.1 s and nothing else.
TL_TEST(attack_button_press_holds_aims_and_attacks_every_tenth) {
    AttackButton button;
    AttackButtonFrame f = attack_button_step(button, down(true), kFrame);
    TL_CHECK(button.hold);
    TL_CHECK(button.was_down);
    TL_CHECK(f.aim);
    TL_CHECK(f.let_targets_go);
    TL_CHECK(f.reset_ai);
    TL_CHECK(f.destination);
    int destinations = 1;
    for (int i = 1; i < 60; ++i) {
        f = attack_button_step(button, down(false), kFrame);
        TL_CHECK(f.aim);
        TL_CHECK(!f.reset_ai);
        TL_CHECK(!f.let_targets_go);
        if (f.destination) ++destinations;
    }
    // One second: the press and then one each time 0.1 has run down.
    TL_CHECK(destinations >= 9 && destinations <= 11);

    // Let go: the hold clears, nothing else is asked.
    AttackButtonInput up;
    up.can_act = true;
    f = attack_button_step(button, up, kFrame);
    TL_CHECK(!button.hold);
    TL_CHECK(!button.was_down);
    TL_CHECK(!f.aim && !f.destination && !f.let_targets_go);
}

// The stick pushed while held lets the targets go every frame.
TL_TEST(attack_button_stick_lets_the_targets_go) {
    AttackButton button;
    attack_button_step(button, down(true), kFrame);
    AttackButtonInput in = down(false);
    in.stick_pushed = true;
    const AttackButtonFrame f = attack_button_step(button, in, kFrame);
    TL_CHECK(f.let_targets_go);
    TL_CHECK(!f.reset_ai);
}

// Paused: the hold stays as it was and the handler does nothing, but the
// button still counts as down (`+0x55`), so letting the pause go is no fresh
// press.
TL_TEST(attack_button_paused_keeps_the_hold) {
    AttackButton button;
    attack_button_step(button, down(true), kFrame);
    AttackButtonInput in = down(false);
    in.paused = true;
    AttackButtonFrame f = attack_button_step(button, in, kFrame);
    TL_CHECK(button.hold);
    TL_CHECK(!f.aim && !f.destination);
    AttackButtonInput up;
    up.can_act = true;
    up.paused = true;
    f = attack_button_step(button, up, kFrame);
    TL_CHECK(button.hold);
    TL_CHECK(!button.was_down);
}

// A player who cannot act: nothing changes at all.
TL_TEST(attack_button_ignored_without_a_player_to_act) {
    AttackButton button;
    AttackButtonInput in = down(true);
    in.can_act = false;
    const AttackButtonFrame f = attack_button_step(button, in, kFrame);
    TL_CHECK(!button.hold && !button.was_down);
    TL_CHECK(!f.aim && !f.destination);
}

// A skill holding the character: the press aims and sets the hold, but
// neither resets the AI nor attacks; the stick still lets the targets go.
TL_TEST(attack_button_waits_for_a_skill_that_holds_the_character) {
    AttackButton button;
    AttackButtonInput in = down(true);
    in.skill_holds = true;
    in.stick_pushed = true;
    const AttackButtonFrame f = attack_button_step(button, in, kFrame);
    TL_CHECK(button.hold);
    TL_CHECK(f.aim);
    TL_CHECK(f.let_targets_go);
    TL_CHECK(!f.reset_ai);
    TL_CHECK(!f.destination);
    // The press was spent: when the skill lets go the next frame is a held
    // one, with no reset.
    const AttackButtonFrame g = attack_button_step(button, down(false), kFrame);
    TL_CHECK(!g.reset_ai);
    TL_CHECK(g.destination);
}

// The fishing catch (AI state 0x22): the handler does nothing.
TL_TEST(attack_button_handler_skips_the_fishing_catch) {
    AttackButton button;
    AttackButtonInput in = down(true);
    in.state_allows = false;
    const AttackButtonFrame f = attack_button_step(button, in, kFrame);
    TL_CHECK(f.aim);
    TL_CHECK(!f.destination && !f.reset_ai);
}

// and `attack()`'s head: a held skill whose animation is up,
// or has time left before its swing is over.
TL_TEST(a_skill_holds_the_character_until_its_swing_is_over) {
    ActiveSkill active;
    TL_CHECK(!skill_holds_character(active));
    active.skill = "Ember Lightning";
    TL_CHECK(!skill_holds_character(active));
    active.casting = true;
    TL_CHECK(skill_holds_character(active));
    active.casting = false;
    active.animation_left = 0.3f;
    TL_CHECK(skill_holds_character(active));
    active.attack_ended = true;
    TL_CHECK(!skill_holds_character(active));
}
