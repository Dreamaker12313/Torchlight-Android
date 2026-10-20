// The pad on the console's skills tab: `CSkillMenuXenon` (X360), the master
// menu's fourth housing. The tab's handler hears `GamepadButtonDown`, the map
// sheet's `GamepadButtonUp`:
// the skill goes on the button as it is released. The book's side is
// `SkillBook::learn` and `map`.
#pragma once

#include "core/gamepad.h"
#include "core/inventory_screen.h"
#include "core/skills.h"

#include <optional>

namespace tl::core {

/// What the tab's `GamepadButtonDown` handler does with a button.
enum class SkillsTabCommand {
    None,
    /// with `value` (0 for the D-pad's up, 1 for its down).
    SkillSet,
    /// on the cursor's skill, then a point spent, `setSkillLevel`,
    /// `POINTASSIGN` (slot 0x1b) and the legend; nothing when it refuses.
    Learn,
    /// `HelpSkillDialog` hidden, then with 1.
    OpenSheet,
    /// with 0.
    CloseSheet,
    /// and the legend's refresh (the press frames are null on the console).
    SheetPress,
    /// `setOpen(false)`, then `CGameUI`'s HUD toggle when `+0x1811` is set.
    CloseMenu,
    /// the housing ring's next screen (RT).
    NextTab,
    /// the previous one (LT).
    PrevTab,
    /// with `value`: 3 up, 4 down, 5 right, 6 left.
    Cursor,
};

struct SkillsTabStep {
    SkillsTabCommand command = SkillsTabCommand::None;
    int value = 0;
};

/// the tab's `GamepadButtonDown` handler. `sheet_open` is
/// on the sheet; `selection_has_sheet` is the X test: the cursor's skill has an
/// effective level and is not a passive.
SkillsTabStep skills_tab_button_down(PadButton button, bool sheet_open, bool cursor_in_spells,
                                     bool selection_has_sheet);

/// the sheet's `GamepadButtonUp` handler: the button a released B, Y, RT or
/// LT puts the cursor's skill on while the sheet is up. Then (`SkillBook::map`):
/// mapped, `ASSIGNSKILL` (slot 0x1e) and the sheet closes; refused, the sheet stays.
std::optional<SkillButton> skills_sheet_button_up(PadButton button, bool sheet_open);

/// What did.
enum class SkillSetSwitch {
    Unchanged,  ///< the set asked for is the one up: nothing played
    Switched,   ///< `ASSIGNSKILL` (slot 0x1e), and the four buttons refreshed
    Refused,    ///< slot 0x23, and the set stays
};

/// (`CGameUI +0x1464`), from the D-pad. With the skills menu open the switch
/// goes through; otherwise needs a mapped button in the new set. The caller checks
/// the gate in front of it.
SkillSetSwitch switch_skill_set(SkillBook& book, int set, bool skills_menu_open);

/// Whether a housing screen's `setOpen(false)` plays `InventoryClose` (slot 0x42): all but
/// the skills tab (`CInventoryMenuXenon`, `CStatsMenuXenon`,
/// `CQuestMenuXenon`, `CSkillMenuXenon`; `CSkillMenuXenon::vfunc_7`
/// calls only with 0x16).
bool housing_plays_close_sound(HousingTab tab);

}  // namespace tl::core
