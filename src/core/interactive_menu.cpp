#include "core/interactive_menu.h"
#include "core/fused.h"

#include <cmath>

namespace tl::core {

namespace {

/// `CEGUI::Window::setYPosition` on the top bar: the area's
/// size kept, its top moved to `y`.
void set_top_y(InteractiveBars& bars, float y) {
    const float size = bars.top_bottom - bars.top_y;
    bars.top_y = y;
    bars.top_bottom = y + size;
}

}  // namespace

// `CInteractiveMenu::CInteractiveMenu`:
// `+0x20` 0 and `+0x1c` 1, then `createMenus`: the layout
// loaded, its root hidden and `setVisible(false)` through the
// vtable.
InteractiveBars::InteractiveBars() { set_visible(false); }

void InteractiveBars::set_visible(bool on, bool editor_running) {
    if (open == on) return;
    if (on) {
        // the window shown, activated and brought to the front,
        // the bars put out of sight and 0.2
        // to come in.
        window_visible = true;
        set_top_y(*this, -0.1f);
        bottom_y = 1.1f;
        timer = 0.2f;
    } else if (editor_running) {
        // hidden and deactivated at once.
        window_visible = false;
    } else {
        // the top bar's area, bottom scale (`+0x110`) less top
        // scale (`+0x100`) - its height - times 2.0.
        timer = (top_bottom - top_y) * 2.0f;
    }
    open = on;
}

void InteractiveBars::update(float delta) {
    // a run-out timer is only gone on with while open.
    if (!(timer >= 0.0f) && !open) return;
    timer = timer - delta;
    const float scaled = timer * 5.0f;
    const float t = scaled > 0.0f ? scaled : 0.0f;
    if (open) {
        set_top_y(*this, t * -0.1f);
        // `fnmsubs`, -((1 - t) * 0.1 - 1.0) in one rounding.
        bottom_y = fnmsub(1.0f - t, 0.1f, 1.0f);
    } else {
        const float u = 1.0f - t;
        set_top_y(*this, u * -0.1f);
        // `fmadds`.
        bottom_y = std::fma(u, 0.1f, 0.9f);
        if (t <= 0.0f) window_visible = false;
    }
}

}  // namespace tl::core
