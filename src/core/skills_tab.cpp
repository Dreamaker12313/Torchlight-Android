#include "core/skills_tab.h"

namespace tl::core {

SkillsTabStep skills_tab_button_down(PadButton button, bool sheet_open, bool cursor_in_spells,
                                     bool selection_has_sheet) {
    // a switch on the button code less one.
    SkillsTabStep step;
    switch (button) {
        case PadButton::DPadUp:
            step.command = SkillsTabCommand::SkillSet;
            step.value = 0;
            break;
        case PadButton::DPadDown:
            step.command = SkillsTabCommand::SkillSet;
            step.value = 1;
            break;
        case PadButton::A:
            // Not on the spell row (`+0xb5`), and not under the sheet.
            if (!cursor_in_spells && !sheet_open) step.command = SkillsTabCommand::Learn;
            break;
        case PadButton::B:
            // Under the sheet a press; otherwise the case falls into Start's
            // and Back's.
            step.command = sheet_open ? SkillsTabCommand::SheetPress : SkillsTabCommand::CloseMenu;
            break;
        case PadButton::Start:
        case PadButton::Back: step.command = SkillsTabCommand::CloseMenu; break;
        case PadButton::X:
            if (sheet_open) {
                step.command = SkillsTabCommand::CloseSheet;
            } else if (selection_has_sheet) {
                step.command = SkillsTabCommand::OpenSheet;
            }
            break;
        case PadButton::Y:
            if (sheet_open) step.command = SkillsTabCommand::SheetPress;
            break;
        case PadButton::RightTrigger:
            step.command = sheet_open ? SkillsTabCommand::SheetPress : SkillsTabCommand::NextTab;
            break;
        case PadButton::LeftTrigger:
            step.command = sheet_open ? SkillsTabCommand::SheetPress : SkillsTabCommand::PrevTab;
            break;
        case PadButton::LeftStickUp:
        case PadButton::RightStickUp:
            step.command = SkillsTabCommand::Cursor;
            step.value = 3;
            break;
        case PadButton::LeftStickDown:
        case PadButton::RightStickDown:
            step.command = SkillsTabCommand::Cursor;
            step.value = 4;
            break;
        case PadButton::LeftStickLeft:
        case PadButton::RightStickLeft:
            step.command = SkillsTabCommand::Cursor;
            step.value = 6;
            break;
        case PadButton::LeftStickRight:
        case PadButton::RightStickRight:
            step.command = SkillsTabCommand::Cursor;
            step.value = 5;
            break;
        default: break;
    }
    return step;
}

std::optional<SkillButton> skills_sheet_button_up(PadButton button, bool sheet_open) {
    // codes outside 6..11, and 7 (X) and 10 (RB), do nothing;
    // the other four only while the sheet (`+0x7a0`) is up.
    if (!sheet_open) return std::nullopt;
    switch (button) {
        case PadButton::Y: return SkillButton::Y;
        case PadButton::RightTrigger: return SkillButton::RT;
        case PadButton::B: return SkillButton::B;
        case PadButton::LeftTrigger: return SkillButton::LT;
        default: return std::nullopt;
    }
}

SkillSetSwitch switch_skill_set(SkillBook& book, int set, bool skills_menu_open) {
    // only a change is gated and heard.
    const int want = set == 0 ? 0 : 1;
    if (want == book.skill_set()) return SkillSetSwitch::Unchanged;
    // the skills menu open lets anything through;
    // otherwise one of the set's four buttons must hold a skill.
    if (!skills_menu_open) {
        bool mapped = false;
        for (SkillButton button :
             {SkillButton::Y, SkillButton::B, SkillButton::LT, SkillButton::RT}) {
            if (book.slot(skill_button_slot(button, want)) != -1) mapped = true;
        }
        if (!mapped) return SkillSetSwitch::Refused;
    }
    book.set_skill_set(want);
    return SkillSetSwitch::Switched;
}

// PORT: whether a housing tab's close plays the menu sound (not the skills tab).
bool housing_plays_close_sound(HousingTab tab) { return tab != HousingTab::Skills; }

}  // namespace tl::core
