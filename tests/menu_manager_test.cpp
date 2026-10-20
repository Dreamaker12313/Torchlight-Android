// `CMenuManager::setActiveMenu`'s table (core/menu_manager), as the console's
// and Linux's bodies both have it.

#include "core/menu_manager.h"
#include "test_support.h"

using namespace tl::core;

namespace {

bool only(const FrontEndMenus& m, bool main, bool new_game, bool difficulty, bool load) {
    return m.main_open == main && m.new_game_open == new_game && m.difficulty_open == difficulty &&
           m.load_character_open == load;
}

}  // namespace

TL_TEST(menu_manager_starts_with_nothing_open) {
    FrontEndMenus m;
    TL_CHECK_EQ(m.active, static_cast<int32_t>(kMenuNone));
    TL_CHECK_EQ(open_menu(m), static_cast<int32_t>(kMenuNone));
}

TL_TEST(menu_manager_each_menu_opens_alone) {
    FrontEndMenus m;
    set_active_menu(m, kMenuMain, false);
    TL_CHECK(only(m, true, false, false, false));
    set_active_menu(m, kMenuNewGame, false);
    TL_CHECK(only(m, false, true, false, false));
    TL_CHECK(!m.new_game_retirement);
    set_active_menu(m, kMenuDifficulty, false);
    TL_CHECK(only(m, false, false, true, false));
    TL_CHECK(!m.difficulty_retirement);
    set_active_menu(m, kMenuLoadCharacter, false);
    TL_CHECK(only(m, false, false, false, true));
    set_active_menu(m, kMenuMain, false);
    TL_CHECK(only(m, true, false, false, false));
}

// New Game -> Back is menu 0: the main menu, not a way out.
TL_TEST(menu_manager_new_game_back_is_the_main_menu) {
    FrontEndMenus m;
    set_active_menu(m, kMenuMain, false);
    set_active_menu(m, kMenuNewGame, false);
    set_active_menu(m, kMenuMain, false);
    TL_CHECK_EQ(open_menu(m), static_cast<int32_t>(kMenuMain));
}

// After a retirement: menu 2, its OK menu 5, the difficulty's Back menu 2 again.
TL_TEST(menu_manager_retirement_modes_follow_the_menu_asked_for) {
    FrontEndMenus m;
    set_active_menu(m, kMenuNewGameRetirement, false);
    TL_CHECK(m.new_game_retirement);
    TL_CHECK_EQ(open_menu(m), static_cast<int32_t>(kMenuNewGameRetirement));
    set_active_menu(m, kMenuDifficultyRetirement, false);
    TL_CHECK(m.difficulty_retirement);
    TL_CHECK_EQ(open_menu(m), static_cast<int32_t>(kMenuDifficultyRetirement));
    set_active_menu(m, kMenuNewGameRetirement, false);
    TL_CHECK_EQ(open_menu(m), static_cast<int32_t>(kMenuNewGameRetirement));
    // An ordinary New Game clears the mode again.
    set_active_menu(m, kMenuNewGame, false);
    TL_CHECK(!m.new_game_retirement);
}

// The menu already active: nothing at all, not even the Continue selection.
TL_TEST(menu_manager_same_menu_does_nothing) {
    FrontEndMenus m;
    set_active_menu(m, kMenuMain, true);
    m.main_open = false;  // whatever a screen did to itself since
    const MenuSwitch again = set_active_menu(m, kMenuMain, true);
    TL_CHECK(!again.select_first_character);
    TL_CHECK(!m.main_open);
}

// Menu 0 selects Load Character's first row only when there is one to continue.
TL_TEST(menu_manager_main_menu_selects_the_first_character) {
    FrontEndMenus m;
    TL_CHECK(set_active_menu(m, kMenuMain, true).select_first_character);
    FrontEndMenus n;
    TL_CHECK(!set_active_menu(n, kMenuMain, false).select_first_character);
}

// Any other menu opens and closes nothing; `closeMenus` closes all four.
TL_TEST(menu_manager_other_menus_and_close) {
    FrontEndMenus m;
    set_active_menu(m, kMenuLoadCharacter, false);
    set_active_menu(m, 7, false);
    TL_CHECK(only(m, false, false, false, true));
    TL_CHECK_EQ(m.active, 7);
    close_menus(m);
    TL_CHECK(only(m, false, false, false, false));
    TL_CHECK_EQ(m.active, static_cast<int32_t>(kMenuNone));
}
