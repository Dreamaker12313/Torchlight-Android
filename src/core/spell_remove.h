// The console's replace-spell dialog, `CMessageSpellRemoveXenon`, opened by A on a scroll
// when the spell slots are full (the PC's is
// `CInventoryMenu::handle_SetSpell`). It only chooses a slot; the tray's update
// unlearns it and retries the scroll.
// No OGRE and no file I/O.
#pragma once

#include "core/skills.h"

#include <string>

namespace tl::core {

/// What `vfunc_2` does with a code (the console menus' button
/// enum: A 5, B 6, X 7, Y 8, the left stick 0x11 up, 0x12 down, 0x13 left,
/// 0x14 right).
enum class SpellRemoveCommand {
    Ignore,   ///< 7, 8, 0x11, 0x12, 0x17, 0x18
    Left,     ///< 0x13: the choice one less, a refresh
    Right,    ///< 0x14: one more, a refresh
    Confirm,  ///< 5: closes, the choice kept
    Cancel,   ///< anything else: the choice -1, closes
};
SpellRemoveCommand spell_remove_command_for_code(int code);

/// `vfunc_5`'s wrap of the choice before it draws: under 0 is the
/// last row (1 for a pet, 3 for anyone else), past it is 0.
int wrap_spell_remove_choice(int choice, bool pet);

/// One of the four rows, as the row fill leaves it.
struct SpellRemoveRow {
    bool shown = true;       ///< rows 2 and 3 are hidden for a pet
    std::string box_image;   ///< `spellN`: `spells_window`, or `_rollover` when chosen
    std::string icon_image;  ///< `spellimageN`: the skill's `SKILL_ICON`, or none
};
SpellRemoveRow spell_remove_row(int row, bool pet, const KnownSkill* skill, bool chosen);

}  // namespace tl::core
