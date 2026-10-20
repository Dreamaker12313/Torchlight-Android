#include "core/difficulty_menu.h"

#include <algorithm>

namespace tl::core {

// The layout function an `onClick` names: `GUISELECT1`..`GUISELECT4` are
// 0x0E..0x11 (`GUISELECTn` arrives as 0x0D + n). PORT: asset parsing of the layout.
int difficulty_layout_function(const std::string& on_click) {
    if (on_click == "GUISELECT1") return kLayoutFunctionSelect1;
    if (on_click == "GUISELECT2") return kLayoutFunctionSelect1 + 1;
    if (on_click == "GUISELECT3") return kLayoutFunctionSelect1 + 2;
    if (on_click == "GUISELECT4") return kLayoutFunctionSelect1 + 3;
    return 0;
}

// (gather: `tabOrder` above zero) and (sort by it).
std::vector<DifficultyTab> difficulty_tab_list(std::vector<DifficultyTab> windows) {
    windows.erase(std::remove_if(windows.begin(), windows.end(),
                                 [](const DifficultyTab& tab) { return tab.tab_order <= 0; }),
                  windows.end());
    // DIVERGENCE: stable, so two windows of the same order keep the layout's;
    // the original's sort promises nothing for them.
    std::stable_sort(
        windows.begin(), windows.end(),
        [](const DifficultyTab& a, const DifficultyTab& b) { return a.tab_order < b.tab_order; });
    return windows;
}

// the first visible window of the sorted list.
int difficulty_first_focus(size_t count, const std::function<bool(size_t)>& usable) {
    for (size_t i = 0; i < count; ++i) {
        if (usable(i)) return static_cast<int>(i);
    }
    return -1;
}

//
int step_difficulty_focus(int focus, bool forward, size_t count,
                          const std::function<bool(size_t)>& usable) {
    if (count == 0) return focus;
    const int n = static_cast<int>(count);
    // With no current window the original starts from the first enabled one.
    int start = focus;
    if (start < 0 || start >= n) {
        start = difficulty_first_focus(count, usable);
        if (start < 0) start = 0;
    }
    int at = start;
    do {
        if (forward) {
            at = at + 1 == n ? 0 : at + 1;
        } else {
            at = at == 0 ? n - 1 : at - 1;
        }
        if (usable(static_cast<size_t>(at))) return at;
    } while (at != start);
    return focus;
}

// `CDifficultyMenu::setOpen`.
void DifficultyMenu::set_open(bool opening) {
    open = opening;
    if (!opening) return;
    hardcore = false;
}

// `CDifficultyMenu::onClick`.
DifficultyClick DifficultyMenu::on_click(int function) {
    DifficultyClick click;
    if (!open) return click;
    if (function == kLayoutFunctionBack) return back();
    if (function < kLayoutFunctionSelect1 || function > kLayoutFunctionSelect1 + 3) return click;

    click.handled = true;
    click.chosen = true;
    click.difficulty = function - kLayoutFunctionSelect1;
    set_open(false);
    click.hardcore = hardcore;
    click.request.state = retirement ? 4 : 3;
    click.request.menu = 0;
    return click;
}

// (kind 1), and `onClick`'s case 7.
DifficultyClick DifficultyMenu::back() {
    DifficultyClick click;
    click.handled = true;
    set_open(false);
    click.request.state = 0;
    click.request.menu = retirement ? 2 : 1;
    return click;
}

}  // namespace tl::core
