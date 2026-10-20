#include "core/game_ui.h"

namespace tl::core {

void AvailablePointsIcon::update(float delta, int unspent_stat_points, int unspent_skill_points) {
    // `(+0x187c == 0 || player+0x3c8 < 1) && player+0x3c4 < 1`
    // hides both. The window always exists here, so only the two counts decide.
    if (unspent_skill_points < 1 && unspent_stat_points < 1) {
        plus = false;
        glow = false;
        return;
    }
    plus = true;
    timer -= delta;
    if (timer < 0.0f) {
        lit = !lit;
        if (lit) {
            plus = true;
            glow = false;
        } else {
            plus = false;
            glow = true;
        }
        timer = 1.0f;  // 0x3f800000, both arms
    }
}

// `CPlayer::levelUp`: the big banner, `Level Up` and "!".
// PORT: the PC's English text.
std::string level_up_banner() { return std::string("Level Up") + "!"; }

// `CPlayer::levelUp`: the small banner,
// `Level N reached`. PORT: the PC's English text.
std::string level_up_caption(int level) {
    return std::string("Level") + " " + std::to_string(level) + " " + "reached";
}

}  // namespace tl::core
