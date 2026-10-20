#include "core/howto_menu.h"

#include "core/settings_menu.h"

namespace tl::core {

namespace {

constexpr float kScrollStep = 2.0f;
constexpr float kScrollLimit = 10.0f;  // and -10

}  // namespace

void HowToMenu::set_open(bool now_open) {
    // nothing when the state does not change.
    if (now_open == open) return;
    open = now_open;
    if (open) scroll_speed = 0.0f;
}

HowToPress HowToMenu::press(int code) {
    HowToPress result;
    if (!open) return result;
    result.handled = true;
    switch (code) {
        case kMenuPadB:
        case kMenuPadStart:
        case kMenuPadBack:
            // `CGameUI`'s toggle.
            result.toggle = true;
            break;
        case kMenuPadStickUp: {
            // from no less than 0, two more, held to 10.
            const float from = scroll_speed > 0.0f ? scroll_speed : 0.0f;
            const float to = from + kScrollStep;
            scroll_speed = to < kScrollLimit ? to : kScrollLimit;
            break;
        }
        case kMenuPadStickDown: {
            // from no more than 0, two less, held to -10.
            const float from = scroll_speed < 0.0f ? scroll_speed : 0.0f;
            const float to = from - kScrollStep;
            scroll_speed = to > -kScrollLimit ? to : -kScrollLimit;
            break;
        }
        default: break;
    }
    return result;
}

// `vfunc_9`: while open and the left stick is off centre
// vertically, the speed is multiplied by `1 - dt`.
void HowToMenu::update(float dt, float stick_y) {
    if (!open || stick_y == 0.0f) return;
    scroll_speed = (1.0f - dt) * scroll_speed;
}

}  // namespace tl::core
