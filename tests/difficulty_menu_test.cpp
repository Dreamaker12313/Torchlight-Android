// The difficulty screen, pinned against `CDifficultyMenu` (`onClick`,
// `setOpen`) and the
// console's tab list.
#include "core/difficulty_menu.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::DifficultyClick;
using tl::core::DifficultyMenu;
using tl::core::DifficultyTab;

/// The five windows of `difficulty.uilayout`, in the order the file has them.
std::vector<DifficultyTab> shipped() {
    return {
        {"i_4765", 0, ""},
        {"Achievements", 3, "GUISELECT4"},
        {"NewCharacter", 1, "GUISELECT2"},
        {"ContinueLast", 5, "GUISELECT1"},
        {"Hardcore", 4, ""},
        {"LoadCharacter", 2, "GUISELECT3"},
    };
}

bool all(size_t) { return true; }

}  // namespace

/// `GUISELECTn` is layout function 0x0D + n.
TL_TEST(difficulty_layout_functions) {
    TL_CHECK_EQ(tl::core::difficulty_layout_function("GUISELECT1"), 0x0e);
    TL_CHECK_EQ(tl::core::difficulty_layout_function("GUISELECT4"), 0x11);
    TL_CHECK_EQ(tl::core::difficulty_layout_function(""), 0);
    TL_CHECK_EQ(tl::core::difficulty_layout_function("GUISELECT5"), 0);
}

/// Sorted by `tabOrder`, the frame (0) left out: the list starts on Normal and
/// ends on Easy.
TL_TEST(difficulty_tab_list_starts_on_normal) {
    const std::vector<DifficultyTab> list = tl::core::difficulty_tab_list(shipped());
    TL_CHECK_EQ(list.size(), static_cast<size_t>(5));
    TL_CHECK(list[0].window == "NewCharacter");
    TL_CHECK(list[1].window == "LoadCharacter");
    TL_CHECK(list[2].window == "Achievements");
    TL_CHECK(list[3].window == "Hardcore");
    TL_CHECK(list[4].window == "ContinueLast");
    TL_CHECK_EQ(tl::core::difficulty_first_focus(list.size(), all), 0);
}

/// The walk wraps at both ends and passes over what cannot take the focus.
TL_TEST(difficulty_focus_walk) {
    TL_CHECK_EQ(tl::core::step_difficulty_focus(0, true, 5, all), 1);
    TL_CHECK_EQ(tl::core::step_difficulty_focus(4, true, 5, all), 0);
    TL_CHECK_EQ(tl::core::step_difficulty_focus(0, false, 5, all), 4);
    const auto no_checkbox = [](size_t i) { return i != 3; };
    TL_CHECK_EQ(tl::core::step_difficulty_focus(2, true, 5, no_checkbox), 4);
    TL_CHECK_EQ(tl::core::step_difficulty_focus(4, false, 5, no_checkbox), 2);
    const auto only_two = [](size_t i) { return i == 2; };
    TL_CHECK_EQ(tl::core::step_difficulty_focus(2, true, 5, only_two), 2);
    TL_CHECK_EQ(tl::core::difficulty_first_focus(5, [](size_t) { return false; }), -1);
}

/// A pick stores the difficulty and the checkbox and starts the game: state 3,
/// or 4 after a retirement.
TL_TEST(difficulty_pick_starts_the_game) {
    DifficultyMenu menu;
    menu.set_open(true);
    menu.toggle_hardcore();
    DifficultyClick click = menu.on_click(0x10);
    TL_CHECK(click.handled);
    TL_CHECK(click.chosen);
    TL_CHECK_EQ(click.difficulty, 2);
    TL_CHECK(click.hardcore);
    TL_CHECK_EQ(click.request.state, 3);
    TL_CHECK_EQ(click.request.menu, 0);
    TL_CHECK(!menu.open);

    menu.retirement = true;
    menu.set_open(true);
    click = menu.on_click(0x0e);
    TL_CHECK_EQ(click.difficulty, 0);
    TL_CHECK(!click.hardcore);
    TL_CHECK_EQ(click.request.state, 4);
}

/// Opening unticks the checkbox every time; the focus is not touched.
TL_TEST(difficulty_open_unticks_hardcore) {
    DifficultyMenu menu;
    menu.focus = 3;
    menu.set_open(true);
    menu.toggle_hardcore();
    TL_CHECK(menu.hardcore);
    menu.set_open(false);
    menu.set_open(true);
    TL_CHECK(!menu.hardcore);
    TL_CHECK_EQ(menu.focus, 3);
}

/// Back goes to New Game: menu 1, or 2 in the retirement mode.
TL_TEST(difficulty_back_returns_to_new_game) {
    DifficultyMenu menu;
    menu.set_open(true);
    DifficultyClick click = menu.on_click(7);
    TL_CHECK(click.handled);
    TL_CHECK(!click.chosen);
    TL_CHECK_EQ(click.request.state, 0);
    TL_CHECK_EQ(click.request.menu, 1);

    menu.retirement = true;
    menu.set_open(true);
    click = menu.back();
    TL_CHECK_EQ(click.request.menu, 2);
}

/// A closed screen takes no clicks, and an unknown function does nothing.
TL_TEST(difficulty_ignores_what_is_not_its) {
    DifficultyMenu menu;
    TL_CHECK(!menu.on_click(0x0f).handled);
    menu.set_open(true);
    TL_CHECK(!menu.on_click(0x12).handled);
    TL_CHECK(!menu.on_click(2).handled);
    TL_CHECK(menu.open);
}
