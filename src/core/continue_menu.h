// The Load Character screen: the list of saved characters the main menu's
// `LoadCharacter` opens. `CContinueGameMenu`: `setOpen`,
// `updateCharacterList`,
// `selectCharacter`, `deleteCharacter`,
// the pad's handler (`vfunc_12`),
// the steps along the list and the
// highlight. Layout `media/ui/characterload.uilayout`;
// the drawing is `render/menu.cpp`'s.
//
// Not here: `onClick` and the click handler,
// which only a pointer reaches; the return to the main menu
// with no profile signed, the platform's.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/difficulty_menu.h"
#include "core/gamepad.h"

namespace tl::core {

/// Rows on the screen at once: the loops of `updateCharacterList` and
/// run to 5, and the scroll is held to `count - 5`.
inline constexpr int kContinueRows = 5;

/// What `reloadFiles` writes over a listed
/// character's hit points: a retired one (the file's retired byte)
/// and, in the trial, one above level 7.
inline constexpr float kContinueRetiredHp = -999.0f;
inline constexpr float kContinueLockedHp = -900.0f;

/// The four `TextColour`s a row's name takes.
inline constexpr const char* kContinueColourLiving = "FF63d1ff";
inline constexpr const char* kContinueColourHardcore = "FFff5039";
inline constexpr const char* kContinueColourRetired = "FFffd235";
inline constexpr const char* kContinueColourDead = "FFadadad";

/// One save file as `reloadFiles` files it: an entry in each of the menu's
/// five parallel lists.
struct ContinueEntry {
    std::string path;               ///< `+0x1dc`: the file
    std::string class_name;         ///< `+0x1ec`: the file's first string
    std::string name;               ///< `CCharacterSaveState +0x34`
    int32_t level = 0;              ///< `+0xd8`
    float hp = 0.0f;                ///< `+0xe8`, after the overrides above
    float play_seconds = 0.0f;      ///< `+0x100`
    bool hardcore = false;          ///< the bit vector at `+0x198`
    uint32_t difficulty = 0;        ///< `+0x1ac`: 0 easy .. 3 very hard
    std::vector<std::string> mods;  ///< `+0x240`
};

/// The hit points a file is listed with (`reloadFiles`):
/// the saved ones; -999 when the file's retired byte is set; and
/// then -900 when the full game is not unlocked (!= 1`) and the
/// character is above level 7. The second test comes last, so a retired
/// character above level 7 is "Locked" in the trial.
float continue_listed_hp(float saved_hp, bool retired, int32_t level, bool full_game);

/// One row as `updateCharacterList` fills it.
struct ContinueRow {
    bool shown = false;  ///< `PlayerN`, its name and its description are visible
    std::string name;    ///< `PlayerNName`
    const char* name_colour = kContinueColourLiving;  ///< its `TextColour`
    std::string description;  ///< `PlayerNDesc`: two lines, each ended by '\n'
};

/// The play time `updateCharacterList` builds and then drops:
/// `<h>hrs <m>mins <s>secs`, from the seconds rounded up. Kept for a
/// layout's `PlayerNTime`.
std::string continue_play_time(float play_seconds);

/// What a press asked the game for.
struct ContinuePress {
    bool handled = false;  ///< `vfunc_12` answers 1 for every button it knows
    /// `CGameUI +0x1788` / `+0x178c` were set and the screen closed: state 2
    /// starts the selected character, state 0 goes back to the main menu.
    bool closed = false;
    GameStateRequest request;
    /// The game client's `applyCharacterState` was called for
    /// `selected()`: the character behind the menu is to be this one.
    bool apply_character = false;
    /// `CGameClient +0x146c` was set: A with both bumpers held. The game then
    /// puts the player and the pet on the level's start point when the level
    /// is.
    bool at_level_start = false;
    /// The selected file is to be removed; the host removes it, reads the
    /// directory again and calls `finish_delete`.
    bool delete_file = false;
    std::string delete_path;
    /// (0x18): the interface's `ERROR` sample.
    bool play_error = false;
};

class ContinueMenu {
public:
    /// `setOpen(true)`: `reloadFiles`, the rows, then `selectCharacter(0,
    /// true)`. The answer says whether a character is to be applied (there is
    /// one). The highlight follows.
    ContinuePress open(std::vector<ContinueEntry> entries, const std::vector<std::string>& mods);
    /// `setOpen(false)`: closed, and `DeleteConfirm` hidden with it.
    void close();
    bool is_open() const { return open_; }

    /// `reloadFiles`: the lists replaced and the scroll put back to 0. **The
    /// selection is not touched** - `deleteCharacter` sets it first.
    void reload(std::vector<ContinueEntry> entries);

    /// `selectCharacter(offset, force)`: the row `offset` below the scroll.
    /// Returns whether the list holds anything. On the row already selected
    /// and not forced, only the game's current file is named again.
    bool select_character(int64_t offset, bool force, const std::vector<std::string>& mods,
                          ContinuePress& out);

    /// `vfunc_12`: one pad button. `bumpers_held` is both shoulder buttons down
    /// (with 0x100 and 0x200); `loading` is the game still bringing
    /// a level in (`CGameClient +0xf90` or `+0xf8c` above 0), which X waits for.
    ContinuePress press(PadButton button, bool bumpers_held, bool loading,
                        const std::vector<std::string>& mods);

    /// The second half of `deleteCharacter`, once the file is gone and the
    /// directory has been read again: the selection one up (never under 0),
    /// `reloadFiles`, `selectCharacter(selection, true)` - **the selection is
    /// passed as the offset from the scroll**, which `reloadFiles` has just put
    /// to 0 - and, with no file left, back to the main menu.
    ContinuePress finish_delete(std::vector<ContinueEntry> entries,
                                const std::vector<std::string>& mods);

    /// `updateCharacterList`: the five rows from the scroll down.
    std::array<ContinueRow, kContinueRows> rows() const;
    // PORT: a test of `rows` sets the scroll it is asked from.
    friend struct ContinueMenuProbe;
    /// `ScrollUp`: the scroll is not at the top. `ScrollDown`: rows lie below
    /// the fifth.
    // PORT: an accessor of the port's own bookkeeping.
    bool scroll_up_shown() const { return top_ != 0; }
    bool scroll_down_shown() const { return top_ + kContinueRows < count(); }
    /// the row the highlight sits on, -1 when the selection is not
    /// among the five; the highlight itself is shown whenever the list is not
    /// empty.
    int highlight_row() const;
    // PORT: an accessor of the port's own bookkeeping.
    bool highlight_shown() const { return count() != 0; }
    /// `CharacterModsWarning` (`+0x174`), as `selectCharacter` left it.
    bool mods_warning() const { return mods_warning_; }
    // PORT: an accessor of the port's own bookkeeping.
    bool delete_confirm_shown() const { return delete_confirm_; }
    bool dead_confirm_shown() const { return dead_confirm_; }

    /// `canContinue`: the selection is a
    /// file whose character has hit points.
    bool can_continue() const;

    int top() const { return top_; }
    int selected() const { return selected_; }
    // PORT: an accessor of the port's own bookkeeping.
    int count() const { return static_cast<int>(entries_.size()); }
    // PORT: an accessor of the port's own bookkeeping.
    const std::vector<ContinueEntry>& entries() const { return entries_; }
    /// The selected entry, or null when the selection is past the list.
    const ContinueEntry* selected_entry() const;
    /// The file `selectCharacter` last named as the game's own (`CGameUI
    /// +0x150c`); empty after `reloadFiles`.
    const std::string& current_file() const { return current_file_; }

private:
    void move_up();    ///<
    void move_down();  ///<

    // `CContinueGameMenu::CContinueGameMenu`: closed, its flags
    // clear, the scroll and the selection 0 and the lists empty.
    bool open_ = false;
    std::vector<ContinueEntry> entries_;  ///< `+0x1bc` and its four companions
    int32_t top_ = 0;                     ///< `+0x10c`: the first row's entry
    int32_t selected_ = 0;                ///< `+0x110`
    bool delete_confirm_ = false;         ///< `+0x178` visible
    bool dead_confirm_ = false;           ///< `+0x17c` visible
    bool mods_warning_ = false;
    std::string current_file_;
};

}  // namespace tl::core
