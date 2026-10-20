#include "core/spell_remove.h"

namespace tl::core {

SpellRemoveCommand spell_remove_command_for_code(int code) {
    // `CMessageSpellRemoveXenon::vfunc_2`.
    switch (code) {
        case 5: return SpellRemoveCommand::Confirm;
        case 7:
        case 8:
        case 0x11:
        case 0x12:
        case 0x17:
        case 0x18: return SpellRemoveCommand::Ignore;
        case 0x13: return SpellRemoveCommand::Left;
        case 0x14: return SpellRemoveCommand::Right;
        default: return SpellRemoveCommand::Cancel;
    }
}

int wrap_spell_remove_choice(int choice, bool pet) {
    // `vfunc_5`, the owner (`+0x2c`) ISA 0x57 or not.
    if (choice < 0) return pet ? 1 : 3;
    if (choice > 3 || (pet && choice > 1)) return 0;
    return choice;
}

SpellRemoveRow spell_remove_row(int row, bool pet, const KnownSkill* skill, bool chosen) {
    // a row past the second is hidden for a pet
    // (`setVisible(false)` on `spellN`) and nothing else is done;
    // otherwise it is shown, its box is `spells_window` or, chosen,
    // `spells_window_rollover`, and `spellimageN` takes the skill's icon
    // (`+0x5c -> +0x94`), or none for an empty slot.
    SpellRemoveRow out;
    if (row > 1 && pet) {
        out.shown = false;
        return out;
    }
    out.box_image = chosen ? "spells_window_rollover" : "spells_window";
    if (skill != nullptr && skill->def != nullptr) out.icon_image = skill->def->icon;
    return out;
}

}  // namespace tl::core
