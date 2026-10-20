#include "core/menu_manager.h"

namespace tl::core {

// `CMenuManager::setActiveMenu(EMenu)`.
MenuSwitch set_active_menu(FrontEndMenus& menus, int32_t menu, bool can_continue) {
    MenuSwitch out;
    if (menu == menus.active) return out;
    switch (menu) {
        case kMenuMain:
            out.select_first_character = can_continue;
            menus.difficulty_open = false;
            menus.main_open = true;
            menus.new_game_open = false;
            menus.load_character_open = false;
            break;
        case kMenuNewGame:
        case kMenuNewGameRetirement:
            menus.difficulty_open = false;
            menus.new_game_retirement = menu == kMenuNewGameRetirement;
            menus.new_game_open = true;
            menus.main_open = false;
            menus.load_character_open = false;
            break;
        case kMenuLoadCharacter:
            menus.difficulty_open = false;
            menus.load_character_open = true;
            menus.main_open = false;
            menus.new_game_open = false;
            break;
        case kMenuDifficulty:
        case kMenuDifficultyRetirement:
            menus.difficulty_retirement = menu == kMenuDifficultyRetirement;
            menus.difficulty_open = true;
            menus.new_game_open = false;
            menus.main_open = false;
            menus.load_character_open = false;
            break;
        default: break;
    }
    menus.active = menu;
    return out;
}

// `CMenuManager::closeMenus` (X360 inline in `CGameUI::closeMenus`):
// all four closed, the active menu 6.
void close_menus(FrontEndMenus& menus) {
    menus.main_open = false;
    menus.load_character_open = false;
    menus.new_game_open = false;
    menus.difficulty_open = false;
    menus.active = kMenuNone;
}

// The screen that is open, in the order `CMenuManager::processInput`
// offers them the pad.
int32_t open_menu(const FrontEndMenus& menus) {
    if (menus.main_open) return kMenuMain;
    if (menus.new_game_open)
        return menus.new_game_retirement ? kMenuNewGameRetirement : kMenuNewGame;
    if (menus.difficulty_open)
        return menus.difficulty_retirement ? kMenuDifficultyRetirement : kMenuDifficulty;
    if (menus.load_character_open) return kMenuLoadCharacter;
    return kMenuNone;
}

}  // namespace tl::core
