// The Load Character screen, pinned against the console's `CContinueGameMenu`
// (`updateCharacterList`, `selectCharacter`,
// `deleteCharacter`, the pad's `vfunc_12` and its steps
// the highlight).
#include "core/continue_menu.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::ContinueEntry;
using tl::core::ContinueMenu;
using tl::core::ContinuePress;
using tl::core::PadButton;

ContinueEntry saved(const std::string& name, int level, float hp = 100.0f) {
    ContinueEntry entry;
    entry.path = name + ".svt";
    entry.class_name = "Alchemist";
    entry.name = name;
    entry.level = level;
    entry.hp = hp;
    entry.difficulty = 1;
    return entry;
}

std::vector<ContinueEntry> seven() {
    std::vector<ContinueEntry> list;
    for (int i = 0; i < 7; ++i) list.push_back(saved("C" + std::to_string(i), i + 1));
    return list;
}

const std::vector<std::string> kNoMods;

}  // namespace

/// the retired byte first, the trial's lock second and over it.
TL_TEST(continue_listed_hit_points) {
    TL_CHECK_EQ(tl::core::continue_listed_hp(55.0f, false, 20, true), 55.0f);
    TL_CHECK_EQ(tl::core::continue_listed_hp(55.0f, true, 20, true), -999.0f);
    TL_CHECK_EQ(tl::core::continue_listed_hp(55.0f, false, 8, false), -900.0f);
    TL_CHECK_EQ(tl::core::continue_listed_hp(55.0f, false, 7, false), 55.0f);
    TL_CHECK_EQ(tl::core::continue_listed_hp(55.0f, true, 8, false), -900.0f);
}

/// The row's two texts, as `updateCharacterList` concatenates them: no space
/// after either colon, one before `Difficulty`, and a line end after each line.
TL_TEST(continue_row_texts) {
    ContinueMenu menu;
    std::vector<ContinueEntry> list;
    list.push_back(saved("Brink", 12));
    list.back().difficulty = 3;
    ContinueEntry hard = saved("Vasman", 30);
    hard.hardcore = true;
    hard.difficulty = 0;
    list.push_back(hard);
    ContinueEntry fallen = saved("Syl", 9, 0.0f);
    fallen.hardcore = true;
    fallen.difficulty = 2;
    list.push_back(fallen);
    ContinueEntry retired = saved("Alric", 100, tl::core::kContinueRetiredHp);
    retired.difficulty = 7;
    list.push_back(retired);
    list.push_back(saved("Trial", 9, tl::core::kContinueLockedHp));
    menu.open(list, kNoMods);

    const auto rows = menu.rows();
    TL_CHECK(rows[0].shown);
    TL_CHECK_EQ(rows[0].name, std::string("Brink"));
    TL_CHECK_EQ(std::string(rows[0].name_colour), std::string("FF63d1ff"));
    TL_CHECK_EQ(rows[0].description, std::string("Level:12\n Difficulty:Very Hard\n"));

    TL_CHECK_EQ(std::string(rows[1].name_colour), std::string("FFff5039"));
    TL_CHECK_EQ(rows[1].description, std::string("Level:30 - Hardcore\n Difficulty:Easy\n"));

    // Dead: grey over the hardcore red, and the word in the description.
    TL_CHECK_EQ(std::string(rows[2].name_colour), std::string("FFadadad"));
    TL_CHECK_EQ(rows[2].description, std::string("Level:9 - Dead\n Difficulty:Hard\n"));

    // Retired: the prefix runs straight into the name, and the whole is cut
    // at fifteen characters. A difficulty past the four names none.
    TL_CHECK_EQ(rows[3].name, std::string("(Retired)Alric"));
    TL_CHECK_EQ(std::string(rows[3].name_colour), std::string("FFffd235"));
    TL_CHECK_EQ(rows[3].description, std::string("Level:100\n Difficulty:\n"));

    // Locked (the trial): grey, as any character without hit points.
    TL_CHECK_EQ(std::string(rows[4].name_colour), std::string("FFadadad"));
    TL_CHECK_EQ(rows[4].description, std::string("Level:9 - Locked\n Difficulty:Normal\n"));
}

/// Over fifteen 16-bit units: the first fifteen and three dots.
TL_TEST(continue_long_names_are_cut) {
    ContinueMenu menu;
    std::vector<ContinueEntry> list;
    list.push_back(saved("ABCDEFGHIJKLMNO", 1));   // 15: kept
    list.push_back(saved("ABCDEFGHIJKLMNOP", 1));  // 16: cut
    list.push_back(saved("LongName", 1, tl::core::kContinueRetiredHp));
    list.push_back(
        saved("\xc3\x84"
              "BCDEFGHIJKLMNOP",
              1));  // a two-byte first letter
    menu.open(list, kNoMods);
    const auto rows = menu.rows();
    TL_CHECK_EQ(rows[0].name, std::string("ABCDEFGHIJKLMNO"));
    TL_CHECK_EQ(rows[1].name, std::string("ABCDEFGHIJKLMNO..."));
    TL_CHECK_EQ(rows[2].name, std::string("(Retired)LongNa..."));
    TL_CHECK_EQ(rows[3].name, std::string("\xc3\x84"
                                          "BCDEFGHIJKLMNO..."));
}

/// seconds rounded up, then hours, minutes and seconds.
TL_TEST(continue_play_time_text) {
    TL_CHECK_EQ(tl::core::continue_play_time(0.0f), std::string("0hrs 0mins 0secs"));
    TL_CHECK_EQ(tl::core::continue_play_time(59.2f), std::string("0hrs 1mins 0secs"));
    TL_CHECK_EQ(tl::core::continue_play_time(3725.0f), std::string("1hrs 2mins 5secs"));
}

/// Opening selects the first file and asks for it behind the menu.
TL_TEST(continue_open_selects_the_first) {
    ContinueMenu menu;
    const ContinuePress opened = menu.open(seven(), kNoMods);
    TL_CHECK(menu.is_open());
    TL_CHECK(opened.apply_character);
    TL_CHECK_EQ(menu.selected(), 0);
    TL_CHECK_EQ(menu.top(), 0);
    TL_CHECK_EQ(menu.current_file(), std::string("C0.svt"));
    TL_CHECK_EQ(menu.highlight_row(), 0);
    TL_CHECK(menu.highlight_shown());
    TL_CHECK(!menu.scroll_up_shown());
    TL_CHECK(menu.scroll_down_shown());
    TL_CHECK(menu.can_continue());

    ContinueMenu empty;
    const ContinuePress nothing = empty.open({}, kNoMods);
    TL_CHECK(!nothing.apply_character);
    TL_CHECK(!empty.highlight_shown());
    TL_CHECK(!empty.can_continue());
    TL_CHECK(!empty.rows()[0].shown);
}

/// the list scrolls only as the selection leaves the fifth row, held to
/// `count - 5`.
TL_TEST(continue_down_scrolls_past_the_fifth_row) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    for (int i = 0; i < 4; ++i) menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 4);
    TL_CHECK_EQ(menu.top(), 0);
    TL_CHECK_EQ(menu.highlight_row(), 4);
    menu.press(PadButton::LeftStickDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 5);
    TL_CHECK_EQ(menu.top(), 1);
    TL_CHECK_EQ(menu.highlight_row(), 4);
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 6);
    TL_CHECK_EQ(menu.top(), 2);
    TL_CHECK(menu.scroll_up_shown());
    TL_CHECK(!menu.scroll_down_shown());
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 6);
    TL_CHECK_EQ(menu.top(), 2);
    // Moving the selection applies nobody: only X and A do.
    TL_CHECK_EQ(menu.current_file(), std::string("C0.svt"));
}

/// up scrolls when the selection was on the first row shown, and
/// at the first file only the scroll moves.
TL_TEST(continue_up_scrolls_from_the_first_row) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    for (int i = 0; i < 6; ++i) menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.top(), 2);
    for (int i = 0; i < 4; ++i) menu.press(PadButton::DPadUp, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 2);
    TL_CHECK_EQ(menu.top(), 2);
    TL_CHECK_EQ(menu.highlight_row(), 0);
    menu.press(PadButton::LeftStickUp, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 1);
    TL_CHECK_EQ(menu.top(), 1);
    menu.press(PadButton::DPadUp, false, false, kNoMods);
    menu.press(PadButton::DPadUp, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 0);
    TL_CHECK_EQ(menu.top(), 0);
}

/// A on a living character starts it (state 2); with both bumpers held the
/// game is also told to stand the player on the level's start.
TL_TEST(continue_a_starts_a_living_character) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    const ContinuePress go = menu.press(PadButton::A, false, false, kNoMods);
    TL_CHECK(go.handled);
    TL_CHECK(go.closed);
    TL_CHECK_EQ(go.request.state, 2);
    TL_CHECK_EQ(go.request.menu, 0);
    TL_CHECK(go.apply_character);
    TL_CHECK(!go.at_level_start);
    TL_CHECK(!menu.is_open());
    TL_CHECK_EQ(menu.current_file(), std::string("C1.svt"));

    ContinueMenu held;
    held.open(seven(), kNoMods);
    TL_CHECK(held.press(PadButton::A, true, false, kNoMods).at_level_start);
}

/// A on a dead hardcore character opens the box; any of A, B and X closes it
/// and does nothing else. A on a retired one only plays the error.
TL_TEST(continue_dead_and_retired) {
    ContinueMenu menu;
    std::vector<ContinueEntry> list;
    list.push_back(saved("Dead", 5, 0.0f));
    list.push_back(saved("Gone", 5, tl::core::kContinueRetiredHp));
    menu.open(list, kNoMods);
    TL_CHECK(!menu.can_continue());

    ContinuePress a = menu.press(PadButton::A, false, false, kNoMods);
    TL_CHECK(!a.closed);
    TL_CHECK(menu.dead_confirm_shown());
    // The list does not move under the box.
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 0);
    a = menu.press(PadButton::A, false, false, kNoMods);
    TL_CHECK(!menu.dead_confirm_shown());
    TL_CHECK(!a.closed);
    menu.press(PadButton::A, false, false, kNoMods);
    const ContinuePress b = menu.press(PadButton::B, false, false, kNoMods);
    TL_CHECK(!b.closed);
    TL_CHECK(!menu.dead_confirm_shown());
    TL_CHECK(menu.is_open());

    menu.press(PadButton::DPadDown, false, false, kNoMods);
    const ContinuePress retired = menu.press(PadButton::A, false, false, kNoMods);
    TL_CHECK(retired.play_error);
    TL_CHECK(!retired.closed);
    TL_CHECK(!menu.dead_confirm_shown());
}

/// B leaves for the main menu (state 0).
TL_TEST(continue_b_goes_back) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    const ContinuePress back = menu.press(PadButton::B, false, false, kNoMods);
    TL_CHECK(back.closed);
    TL_CHECK_EQ(back.request.state, 0);
    TL_CHECK(!back.apply_character);
    TL_CHECK(!menu.is_open());
}

/// X applies the selected character behind the menu - unless a level is
/// still coming in.
TL_TEST(continue_x_views_the_selection) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    const ContinuePress waiting = menu.press(PadButton::X, false, true, kNoMods);
    TL_CHECK(!waiting.apply_character);
    TL_CHECK_EQ(menu.current_file(), std::string("C0.svt"));
    const ContinuePress view = menu.press(PadButton::X, false, false, kNoMods);
    TL_CHECK(view.apply_character);
    TL_CHECK(!view.closed);
    TL_CHECK_EQ(menu.selected(), 2);
    TL_CHECK_EQ(menu.current_file(), std::string("C2.svt"));
}

/// Y asks; B takes the question back; A deletes, and the selection moves one up.
TL_TEST(continue_delete_asks_first) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    for (int i = 0; i < 3; ++i) menu.press(PadButton::DPadDown, false, false, kNoMods);
    menu.press(PadButton::Y, false, false, kNoMods);
    TL_CHECK(menu.delete_confirm_shown());
    // The list does not move, and X does nothing, under the question.
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    TL_CHECK_EQ(menu.selected(), 3);
    TL_CHECK(!menu.press(PadButton::X, false, false, kNoMods).apply_character);
    const ContinuePress no = menu.press(PadButton::B, false, false, kNoMods);
    TL_CHECK(!no.closed);
    TL_CHECK(!menu.delete_confirm_shown());

    menu.press(PadButton::Y, false, false, kNoMods);
    const ContinuePress yes = menu.press(PadButton::A, false, false, kNoMods);
    TL_CHECK(yes.delete_file);
    TL_CHECK_EQ(yes.delete_path, std::string("C3.svt"));
    TL_CHECK(!menu.delete_confirm_shown());

    std::vector<ContinueEntry> left = seven();
    left.erase(left.begin() + 3);
    const ContinuePress after = menu.finish_delete(left, kNoMods);
    TL_CHECK(!after.closed);
    TL_CHECK(after.apply_character);
    TL_CHECK_EQ(menu.selected(), 2);
    TL_CHECK_EQ(menu.top(), 0);
    TL_CHECK_EQ(menu.current_file(), std::string("C2.svt"));
}

/// The last file deleted sends the screen back to the main menu.
TL_TEST(continue_deleting_the_last_file_closes) {
    ContinueMenu menu;
    std::vector<ContinueEntry> one;
    one.push_back(saved("Only", 3));
    menu.open(one, kNoMods);
    menu.press(PadButton::Y, false, false, kNoMods);
    TL_CHECK(menu.press(PadButton::A, false, false, kNoMods).delete_file);
    const ContinuePress after = menu.finish_delete({}, kNoMods);
    TL_CHECK(after.closed);
    TL_CHECK_EQ(after.request.state, 0);
    TL_CHECK(!after.apply_character);
    TL_CHECK(!menu.is_open());
}

/// `selectCharacter`'s warning: the save's mods against the game's, in order,
/// case aside.
TL_TEST(continue_mods_warning) {
    ContinueMenu menu;
    std::vector<ContinueEntry> list;
    list.push_back(saved("Plain", 1));
    ContinueEntry modded = saved("Modded", 1);
    modded.mods = {"BigPack", "Pets"};
    list.push_back(modded);
    menu.open(list, kNoMods);
    TL_CHECK(!menu.mods_warning());
    menu.press(PadButton::DPadDown, false, false, kNoMods);
    menu.press(PadButton::X, false, false, kNoMods);
    TL_CHECK(menu.mods_warning());
    menu.press(PadButton::X, false, false, {"BIGPACK", "pets"});
    TL_CHECK(!menu.mods_warning());
    menu.press(PadButton::X, false, false, {"pets", "BIGPACK"});
    TL_CHECK(menu.mods_warning());
}

/// The pointer's path through `selectCharacter`: the row already selected,
/// not forced, only names the file again.
TL_TEST(continue_select_same_row_unforced) {
    ContinueMenu menu;
    menu.open(seven(), kNoMods);
    ContinuePress out;
    TL_CHECK(menu.select_character(0, false, kNoMods, out));
    TL_CHECK(!out.apply_character);
    TL_CHECK_EQ(menu.current_file(), std::string("C0.svt"));
    TL_CHECK(menu.select_character(3, false, kNoMods, out));
    TL_CHECK(out.apply_character);
    TL_CHECK_EQ(menu.selected(), 3);
}
