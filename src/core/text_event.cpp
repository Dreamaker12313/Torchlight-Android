#include "core/text_event.h"

#include <cmath>

namespace tl::core {

// `CGameUI::getTextEventObject` and
// `CTextEvent::setText`.
TextEvent make_text_event(float x, float y, float z, const std::string& text, float scale,
                          float red, float green, float blue, bool moving) {
    const float alpha = 255.0f;  // `+0x6c`,
    TextEvent event;
    event.x = x;
    event.y = y;
    event.z = z;
    event.text = text;
    event.scale = scale;  // +0x64
    event.red = red;
    event.green = green;
    event.blue = blue;
    event.rise = 0.1f;   // +0x70 = 0x3dcccccd
    event.speed = 1.0f;  // +0x60 = 0x3f800000
    event.alpha = alpha;
    // `TextColour` is set from the colour with the alpha over 255 for its own.
    event.opacity = alpha / 255.0f;
    // `update` runs here once with a zero delta, changing nothing.
    event.fades = moving;
    event.rises = moving;
    return event;
}

// `CTextEvent::update`.
bool update_text_event(TextEvent& event, float delta) {
    const float step = delta * event.speed;
    if (event.fades) {
        const float before = event.alpha;
        const float after = step * -200.0f + before;
        event.alpha = after;
        if (after <= 0.0f) {
            event.alpha = 0.0f;
            return false;
        }
        if (static_cast<int>(after) != static_cast<int>(before)) {
            // Four times the alpha, held at 255: solid for three quarters of its life.
            const float shown = after * 4.0f < 255.0f ? after * 4.0f : 255.0f;
            event.opacity = shown / 255.0f;
        }
    }
    if (event.rises) {
        event.y = step * event.rise + event.y;
        event.rise = step + event.rise;
    }
    // `CGameUI::updateTextEvents` keeps an
    // event while `+0x68` is above zero.
    return 0.0f < event.alpha;
}

// PORT: presentation, a combat text's colour and height (the header has the reading).
CombatTextStyle combat_text_style(CombatText what) {
    switch (what) {
        case CombatText::GlancingBlow: return {1.0f, 0.77f, 0.0f, 1.2f};
        case CombatText::GlancingBlowOnPlayer: return {1.0f, 0.2f, 0.2f, 1.2f};
        case CombatText::CriticalStrike: return {0.196f, 0.745f, 1.0f, 1.2f};
        case CombatText::Blocked:
        case CombatText::Absorbed: return {1.0f, 0.67f, 0.16f, 1.2f};
        case CombatText::Gold: return {1.0f, 0.9f, 0.6f, 1.0f};
        case CombatText::Fame: return {1.0f, 0.77f, 1.0f, 1.0f};
    }
    return {};
}

std::string combat_text(CombatText what, int amount) {
    switch (what) {
        case CombatText::GlancingBlow:
        case CombatText::GlancingBlowOnPlayer:
            return "Glancing Blow!";  // L"Glancing Blow" + L"!"
        case CombatText::CriticalStrike:
            return "Critical Strike!";                  // L"Critical Strike" + L"!"
        case CombatText::Blocked: return "Blocked!";    // L"Blocked" + L"!"
        case CombatText::Absorbed: return "Absorbed!";  // L"Absorbed" + L"!"
        case CombatText::Gold: return std::to_string(amount) + " Gold";
        case CombatText::Fame:
            return std::to_string(amount) + " Fame";  // the number, L" ", L"Fame"
    }
    return {};
}

// `CCharacter::displayDamageAbsorbed(float)`.
bool display_damage(DamageDisplay& shown, bool display_dmg, float delta, int& amount) {
    if (!display_dmg) return false;
    const float left = shown.timer - delta;
    shown.timer = left;
    if (shown.pending == 0.0f || !(left < 0.0f)) return false;
    shown.timer = 0.5f;
    // `STRINGS::GetValueAsString((int)ceilf(+0x1ec))`.
    amount = static_cast<int>(std::ceil(shown.pending));
    shown.pending = 0.0f;
    return true;
}

// PORT: presentation, the number over a character (the header has the reading).
TextEvent make_damage_text(float x, float y, float z, int amount, bool players_side) {
    return make_text_event(x, y + 1.0f, z, std::to_string(amount), 1.0f, 1.0f,
                           players_side ? 0.2f : 0.77f, players_side ? 0.2f : 0.0f, true);
}

// PORT: presentation, a combat text over a unit (the header has the reading).
TextEvent make_combat_text(CombatText what, float x, float y, float z, int amount) {
    const CombatTextStyle style = combat_text_style(what);
    return make_text_event(x, y + style.height, z, combat_text(what, amount), 1.0f, style.red,
                           style.green, style.blue, true);
}

}  // namespace tl::core
