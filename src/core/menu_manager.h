// `CMenuManager`: which of the four front-end screens is up - the main menu,
// New Game (character creation), Difficulty and Load Character - and how one
// is swapped for another (`CGameUI::requestSetGameState(state, menu)`, then
// `CGameClient::setGameState` to `setActiveMenu`). The per-frame half of
// `CMenuManager` (`update`, `processInput`) is the port's own front end.
#pragma once

#include <cstdint>

namespace tl::core {

/// `EMenu`, `setActiveMenu`'s argument.
enum FrontEndMenu : int32_t {
    kMenuMain = 0,
    kMenuNewGame = 1,
    kMenuNewGameRetirement = 2,  ///< New Game for a retiring character's heir
    kMenuLoadCharacter = 3,
    kMenuDifficulty = 4,
    kMenuDifficultyRetirement = 5,
    kMenuNone = 6,
};

/// The four screens' open flags and the two the table sets, with the active menu. All start
/// clear: `CMainMenu::CMainMenu`, `CNewGameMenu::CNewGameMenu`,
/// `CDifficultyMenu::CDifficultyMenu`; inline on the console in
/// `CMenuManager::CMenuManager`'s builder.
struct FrontEndMenus {
    bool main_open = false;
    bool new_game_open = false;
    bool difficulty_open = false;
    bool load_character_open = false;
    /// New Game's retirement mode: no `Back`, and `OK` asks for menu 5.
    bool new_game_retirement = false;
    /// Difficulty's retirement mode: its `Back` asks for menu 2, a pick starts game state 4.
    bool difficulty_retirement = false;
    /// The constructor's 6 (`CMenuManager::CMenuManager`).
    int32_t active = kMenuNone;
};

/// What `setActiveMenu` asks of a screen beyond opening and closing.
struct MenuSwitch {
    /// Menu 0 with a character to continue (`CContinueGameMenu::canContinue`):
    /// `selectCharacter(0, true)` on Load Character's list.
    bool select_first_character = false;
};

/// `CMenuManager::setActiveMenu(EMenu)`: nothing for the
/// menu already active; any unknown menu opens and closes nothing but still becomes active.
MenuSwitch set_active_menu(FrontEndMenus& menus, int32_t menu, bool can_continue);

/// `CMenuManager::closeMenus` (X360 inline in
/// `CGameUI::closeMenus`): all four closed, the active menu 6.
void close_menus(FrontEndMenus& menus);

/// The screen that is open - the table leaves one at most - or `kMenuNone`.
/// Main, New Game, Difficulty, Load Character: the order `processInput` offers
/// them the pad.
int32_t open_menu(const FrontEndMenus& menus);

}  // namespace tl::core
