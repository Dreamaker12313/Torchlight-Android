// Text that floats up over the world and fades (`CTextEvent`): combat lines, gold, fame, damage
// numbers, floaty dialogue. All come through `CGameUI::addTextEvent`.
// Damage is summed by `CCharacter::applyDamage` and
// shown by `displayDamageAbsorbed`. Drawing is the renderer's.
#pragma once

#include <string>

namespace tl::core {

/// A `CTextEvent` (0x88 bytes), as `CGameUI::getTextEventObject`
/// fills it and `CTextEvent::update`
/// moves it.
struct TextEvent {
    float x = 0.0f;  ///< +0x10, the world position the window is put over
    float y = 0.0f;
    float z = 0.0f;
    std::string text;  ///< +0x20, UTF-8
    float red = 1.0f;  ///< +0x2c..+0x34 of the colour at +0x28
    float green = 1.0f;
    float blue = 1.0f;
    float speed = 1.0f;    ///< +0x60: what `update` scales its delta by
    float scale = 1.0f;    ///< +0x64: the window is the text's extent times this
    float alpha = 255.0f;  ///< +0x68: runs down at 200 a second; gone at 0
    float rise = 0.1f;     ///< +0x70: the upward speed, which itself grows by the delta
    bool fades = true;     ///< +0x80
    bool rises = true;     ///< +0x81
    /// The alpha of the window's `TextColour` and `DropTextColour`, 0..1.
    /// `update` writes it only when the integer part of `alpha` changes.
    float opacity = 1.0f;
};

/// `CGameUI::getTextEventObject(position, text, scale, colour, colour, moving)`,
/// filled by `CTextEvent::setText`: alpha
/// 255, speed 1, rise 0.1; `moving` sets both flags.
TextEvent make_text_event(float x, float y, float z, const std::string& text, float scale,
                          float red, float green, float blue, bool moving);

/// `CTextEvent::update(window, delta)`. False
/// once the alpha has run out, which is when `CGameUI::updateTextEvents`
/// returns the event to the pool.
bool update_text_event(TextEvent& event, float delta);

/// `CGameUI::addTextEvent`, its first test: nothing while
/// `KSETTINGS_FLOATY_NUMBERS` reads 0 (a missing key reads -1). The camera test is the renderer's.
inline bool text_events_shown(bool floaty_numbers) { return floaty_numbers; }
inline constexpr float kTextEventRadius = 0.5f;  // 0x3f000000

/// The text events the combat and loot code raises.
enum class CombatText {
    /// `applyDamage` with
    /// its 7th argument set, over a victim that is neither a player nor a player's own
    /// (`+0x640`).
    GlancingBlow,
    /// The same over a player or a unit whose master is one.
    GlancingBlowOnPlayer,
    /// `rollCritical`, over
    /// the target.
    CriticalStrike,
    /// `rollAttack` and
    /// `applyInstantEffect`,
    /// over the unit that blocked.
    Blocked,
    /// `modifyDamage`, over
    /// the unit whose `DAMAGE ABSORB` took the whole blow.
    Absorbed,
    /// `getItem`: the gold
    /// taken, over the pile.
    Gold,
    /// `awardFame`: the
    /// fame won, over the character.
    Fame,
};

/// The `CEGUI::colour` its caller builds and its height above the unit, in `UNIT_Y`s.
struct CombatTextStyle {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float height = 1.2f;
};
CombatTextStyle combat_text_style(CombatText what);

/// The words: the translated name with `!` after it for the four combat
/// lines, `<amount> Gold` and `<amount> Fame` for the two awards.
std::string combat_text(CombatText what, int amount = 0);

/// A combat text over a unit standing at (`x`, `y`, `z`).
TextEvent make_combat_text(CombatText what, float x, float y, float z, int amount = 0);

/// The damage a character has taken and not yet shown, and
/// the time to the next number.
struct DamageDisplay {
    float pending = 0.0f;
    float timer = 0.0f;
};

/// `CCharacter::displayDamageAbsorbed(float)`: true with the
/// pending sum, rounded up, in `amount` at most every 0.5 s.
bool display_damage(DamageDisplay& shown, bool display_dmg, float delta, int& amount);

/// `CCharacter::forceDisplayOfDamageAbsorbed()`: the delta `modifyHP` passes on a
/// death, so the killing blow's number does not wait.
inline constexpr float kForcedDamageDisplay = 10000.0f;

/// The number over a character: red on the player's side (a player, its minion, or `GOOD`),
/// orange otherwise.
TextEvent make_damage_text(float x, float y, float z, int amount, bool players_side);

}  // namespace tl::core
