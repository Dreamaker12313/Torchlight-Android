// The console's character creation, pinned against `CNewGameMenu`
// (and the grid).
#include "core/new_game_menu.h"
#include "test_support.h"

#include <set>
#include <string>

namespace {

using tl::core::NewGameCommand;
using tl::core::NewGameFocus;

bool everything(const std::string&) { return true; }

NewGameFocus at(int column, int row) {
    NewGameFocus f;
    f.column = column;
    f.row = row;
    return f;
}

}  // namespace

/// The table, as builds it.
TL_TEST(new_game_grid_is_the_consoles) {
    TL_CHECK(std::string(tl::core::new_game_window(at(0, 0))) == "Destroyer");
    TL_CHECK(std::string(tl::core::new_game_window(at(1, 0))) == "Alchemist");
    TL_CHECK(std::string(tl::core::new_game_window(at(2, 0))) == "Vanquisher");
    for (int c = 0; c < 3; ++c) {
        TL_CHECK(std::string(tl::core::new_game_window(at(c, 1))) == "EditBox");
        TL_CHECK(std::string(tl::core::new_game_window(at(c, 3))) == "EditBoxPet");
        TL_CHECK(std::string(tl::core::new_game_window(at(c, 4))) == "DoneButton");
    }
    TL_CHECK(std::string(tl::core::new_game_window(at(2, 2))) == "Chakawary");
}

/// Both axes wrap to the far edge.
TL_TEST(new_game_focus_wraps) {
    NewGameFocus f = tl::core::step_new_game_focus(at(0, 0), -1, 0, everything);
    TL_CHECK_EQ(f.column, 2);
    TL_CHECK_EQ(f.row, 0);
    f = tl::core::step_new_game_focus(at(2, 0), 1, 0, everything);
    TL_CHECK_EQ(f.column, 0);
    f = tl::core::step_new_game_focus(at(1, 0), 0, -1, everything);
    TL_CHECK_EQ(f.row, 4);
    TL_CHECK_EQ(f.column, 1);
    f = tl::core::step_new_game_focus(at(1, 4), 0, 1, everything);
    TL_CHECK_EQ(f.row, 0);
}

/// A hidden window is stepped over in the direction of travel: with Done hidden, down from
/// the pet name goes round to the classes.
TL_TEST(new_game_focus_skips_hidden_windows) {
    const auto no_done = [](const std::string& name) { return name != "DoneButton"; };
    NewGameFocus f = tl::core::step_new_game_focus(at(1, 3), 0, 1, no_done);
    TL_CHECK_EQ(f.row, 0);
    TL_CHECK_EQ(f.column, 1);
    f = tl::core::step_new_game_focus(at(0, 0), 0, -1, no_done);
    TL_CHECK_EQ(f.row, 3);
    // A (0, 0) step from a visible window stays put - how the screen opens.
    f = tl::core::step_new_game_focus(at(0, 0), 0, 0, no_done);
    TL_CHECK_EQ(f.row, 0);
    TL_CHECK_EQ(f.column, 0);
    // Nothing visible at all: the original would spin; this stays where it was.
    f = tl::core::step_new_game_focus(at(2, 2), 1, 0, [](const std::string&) { return false; });
    TL_CHECK_EQ(f.column, 2);
    TL_CHECK_EQ(f.row, 2);
}

/// The layout's `onClick`s, as reads them.
TL_TEST(new_game_commands_come_from_on_click) {
    TL_CHECK(tl::core::new_game_command("GUISELECT1") == NewGameCommand::ChooseClass);
    TL_CHECK(tl::core::new_game_command("GUISELECT2") == NewGameCommand::ChoosePet);
    TL_CHECK(tl::core::new_game_command("GUISELECT49") == NewGameCommand::EditPlayerName);
    TL_CHECK(tl::core::new_game_command("GUISELECT50") == NewGameCommand::EditPetName);
    TL_CHECK(tl::core::new_game_command("GUINEWGAME") == NewGameCommand::Done);
    TL_CHECK(tl::core::new_game_command("") == NewGameCommand::None);
}

/// Done wants both names, and the keyboard allows twelve
/// characters (a 13-character buffer).
TL_TEST(new_game_done_wants_both_names) {
    TL_CHECK(tl::core::new_game_done_visible("Player", "Spot"));
    TL_CHECK(!tl::core::new_game_done_visible("", "Spot"));
    TL_CHECK(!tl::core::new_game_done_visible("Player", ""));
    TL_CHECK_EQ(tl::core::kNewGameNameChars, 12u);
}

/// `update` takes the keyboard's text as 13 characters: cut there, by
/// character and not by byte; a character past the BMP counts as two.
TL_TEST(new_game_keyboard_name_is_cut_at_thirteen_characters) {
    TL_CHECK_EQ(tl::core::new_game_keyboard_name("Spot"), std::string("Spot"));
    TL_CHECK_EQ(tl::core::new_game_keyboard_name(""), std::string());
    TL_CHECK_EQ(tl::core::new_game_keyboard_name("ABCDEFGHIJKLMNOP"), std::string("ABCDEFGHIJKLM"));
    // Thirteen two-byte letters (U+00E9) are 26 bytes and all kept.
    std::string accented;
    for (int i = 0; i < 14; ++i) accented += "\xc3\xa9";
    TL_CHECK_EQ(tl::core::new_game_keyboard_name(accented), accented.substr(0, 26));
    // Twelve letters and a character past the BMP: thirteen units hold only
    // the letters.
    TL_CHECK_EQ(tl::core::new_game_keyboard_name("ABCDEFGHIJKL\xf0\x9f\x90\xb6"),
                std::string("ABCDEFGHIJKL"));
}

/// `handle_SubmitPet` hands the names on only when both are there.
TL_TEST(new_game_submit_pet_wants_both_names) {
    const tl::core::NewGameSubmission both = tl::core::new_game_submit_pet("Player", "Spot");
    TL_CHECK(both.submitted);
    TL_CHECK_EQ(both.player_name, std::string("Player"));
    TL_CHECK_EQ(both.pet_name, std::string("Spot"));
    TL_CHECK(!tl::core::new_game_submit_pet("", "Spot").submitted);
    TL_CHECK(!tl::core::new_game_submit_pet("Player", "").submitted);
}
