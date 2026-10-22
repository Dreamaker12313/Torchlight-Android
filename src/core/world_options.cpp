// The gameplay half of render/renderer_options.cpp (core/world.h).
#include "core/audio.h"
#include "core/run_config.h"
#include "core/settings_menu.h"
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

// `CGameUI`'s Help toggle: the Pause Menu, Options and
// `+0x3ac` closed, then Help's `setOpen(!open)` (`vfunc_3` is
// `isOpen`). Options is never open here: Y closed it first.
void World::toggle_help() {
    if (pause_menu.open) close_pause_menu();
    set_help_open(!howto_menu.open);
}

void World::drive_howto_menu(const tl::core::InputState& in, float dt, bool start_edge,
                             bool back_edge) {
    howto_menu.update(dt, in.stick_y);
    int code = 0;
    if (std::abs(in.stick_y) > 0.5f) {
        if (!howto_stick_held) {
            code = in.stick_y > 0.0f ? tl::core::kMenuPadStickUp : tl::core::kMenuPadStickDown;
        }
        howto_stick_held = true;
    } else {
        howto_stick_held = false;
    }
    if (in.skip && !skip_held) code = tl::core::kMenuPadB;
    if (start_edge) code = tl::core::kMenuPadStart;
    if (back_edge) code = tl::core::kMenuPadBack;
    if (code == 0) return;
    if (howto_menu.press(code).toggle) toggle_help();
}

tl::core::GameSettings& World::game_settings() {
    return config.settings != nullptr ? *config.settings : own_settings;
}

}  // namespace tl::core
