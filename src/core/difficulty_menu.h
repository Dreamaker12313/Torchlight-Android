// The difficulty screen: what New Game asks after the character is made.
// `CDifficultyMenu`'s `onClick` and `setOpen`
// - the console's `vfunc_9` and `vfunc_7` -
// and for the pad the console's tab list (`TabNavigationHandler`)
// and its own B (`vfunc_8`).
// The drawing is `render/menu.cpp`'s.
//
// Layout `media/ui/difficulty.uilayout`: its buttons keep the
// main menu's names - `ContinueLast` Easy, `NewCharacter` Normal,
// `LoadCharacter` Hard, `Achievements` Very Hard - and the `Hardcore` checkbox.
//
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// `ELayoutFunction`, the numbers `onClick` switches on. A layout's
/// `GUISELECTn` arrives as 0x0D + n.
inline constexpr int kLayoutFunctionBack = 7;
inline constexpr int kLayoutFunctionSelect1 = 0x0e;

/// The layout function an `onClick` string names: `GUISELECT1`..`GUISELECT4`
/// are 0x0E..0x11, the four `onClick` gives a difficulty to. 0 for anything
/// else (the checkbox authors an empty one).
int difficulty_layout_function(const std::string& on_click);

/// One window that takes the focus, as the layout authors it.
struct DifficultyTab {
    std::string window;    ///< the window's name
    int tab_order = 0;     ///< its `tabOrder` property
    std::string on_click;  ///< its `onClick` property
};

/// The screen's tab list: every window whose `tabOrder` is above zero,
/// sorted by it. The walk
/// starts on **Normal** (tab order 1).
std::vector<DifficultyTab> difficulty_tab_list(std::vector<DifficultyTab> windows);

/// Where the focus is put when the list is built: the first window in it that
/// is visible. -1 when none is.
int difficulty_first_focus(size_t count, const std::function<bool(size_t)>& usable);

/// One step along the list, forward (down) or back (up), wrapping at both ends
/// and passing over a window that is hidden or disabled; the focus stays where
/// it is when the walk comes round to it.
int step_difficulty_focus(int focus, bool forward, size_t count,
                          const std::function<bool(size_t)>& usable);

/// `CGameUI::requestSetGameState(state, menu)`, as this screen asks
/// for it: the console's `CGameUI +0x1788` and `+0x178c`.
struct GameStateRequest {
    int state = 0;
    int menu = 0;
};

/// What a click did.
struct DifficultyClick {
    bool handled = false;   ///< the screen closed and asked for a game state
    bool chosen = false;    ///< a difficulty was picked (not Back)
    int difficulty = 0;     ///< `CGameClient +0x38ec`: 0 easy .. 3 very hard
    bool hardcore = false;  ///< `CGameClient +0x1b8`, the checkbox as it stood
    GameStateRequest request;
};

/// The screen's own state.
struct DifficultyMenu {
    /// `CDropdownMenu +0x30` (X360 `+0x18`); nothing here animates a close.
    bool open = false;
    /// The `Hardcore` checkbox (`+0xc8`, X360 `+0x10c`), read as its
    /// `isSelected()`.
    bool hardcore = false;
    /// `+0xd0` (X360 `+0x110`): opened as menu 5, after a retirement
    /// (`CMenuManager::setActiveMenu`).
    bool retirement = false;
    /// The tab list's current window, an index into `difficulty_tab_list()`.
    /// **Kept across openings**: the list is built once, and every later
    /// opening only puts the pointer back on the window it was left.
    /// -1 until the list is built.
    int focus = -1;

    /// `setOpen`: opening unticks the checkbox,
    /// every time.
    void set_open(bool opening);

    /// `onClick`. `function` is the layout
    /// function: 7 goes back to New Game (menu 1, or 2 after a retirement);
    /// 0x0E..0x11 store the difficulty and the checkbox and start the game
    /// (state 3, or 4 after a retirement, which keeps the heirloom's hand-off).
    /// Anything else, and anything while the screen is not open, does nothing.
    DifficultyClick on_click(int function);

    /// B on the pad (the event's kind 1): what function 7
    /// does, without the open test.
    DifficultyClick back();

    /// A on the checkbox: the console presses its hidden pointer on the
    /// focused window, and a CEGUI checkbox so pressed flips itself.
    // PORT: an accessor of the difficulty menu.
    void toggle_hardcore() { hardcore = !hardcore; }
};

}  // namespace tl::core
