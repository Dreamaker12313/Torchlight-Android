// The console's skills tab, `CSkillMenuXenon` (X360): the tab's `GamepadButtonDown`
// handler, the map sheet's `GamepadButtonUp` handler and the skill-set switch.
#include "core/skills_tab.h"
#include "test_support.h"

#include <vector>

using tl::core::PadButton;
using tl::core::SkillsTabCommand;

TL_TEST(skills_tab_buttons_without_the_sheet) {
    // with the sheet down.
    auto down = [](PadButton b) {
        return tl::core::skills_tab_button_down(b, false, false, true).command;
    };
    TL_CHECK(down(PadButton::A) == SkillsTabCommand::Learn);
    TL_CHECK(down(PadButton::X) == SkillsTabCommand::OpenSheet);
    TL_CHECK(down(PadButton::Y) == SkillsTabCommand::None);
    // B falls into Start's and Back's case: all three close the menu.
    TL_CHECK(down(PadButton::B) == SkillsTabCommand::CloseMenu);
    TL_CHECK(down(PadButton::Start) == SkillsTabCommand::CloseMenu);
    TL_CHECK(down(PadButton::Back) == SkillsTabCommand::CloseMenu);
    TL_CHECK(down(PadButton::RightTrigger) == SkillsTabCommand::NextTab);
    TL_CHECK(down(PadButton::LeftTrigger) == SkillsTabCommand::PrevTab);
    TL_CHECK(down(PadButton::RightBumper) == SkillsTabCommand::None);
    TL_CHECK(down(PadButton::DPadLeft) == SkillsTabCommand::None);

    const auto up = tl::core::skills_tab_button_down(PadButton::DPadUp, false, false, true);
    TL_CHECK(up.command == SkillsTabCommand::SkillSet);
    TL_CHECK_EQ(up.value, 0);
    const auto dn = tl::core::skills_tab_button_down(PadButton::DPadDown, false, false, true);
    TL_CHECK(dn.command == SkillsTabCommand::SkillSet);
    TL_CHECK_EQ(dn.value, 1);

    // A on the spell row learns nothing; X on a skill with no level or a
    // passive opens nothing.
    TL_CHECK(tl::core::skills_tab_button_down(PadButton::A, false, true, true).command ==
             SkillsTabCommand::None);
    TL_CHECK(tl::core::skills_tab_button_down(PadButton::X, false, false, false).command ==
             SkillsTabCommand::None);
}

TL_TEST(skills_tab_sticks_move_the_cursor) {
    // Codes 17/21 -> 3, 18/22 -> 4, 19/23 -> 6, 20/24 -> 5.
    const std::vector<std::pair<PadButton, int>> moves = {
        {PadButton::LeftStickUp, 3},    {PadButton::RightStickUp, 3},
        {PadButton::LeftStickDown, 4},  {PadButton::RightStickDown, 4},
        {PadButton::LeftStickLeft, 6},  {PadButton::RightStickLeft, 6},
        {PadButton::LeftStickRight, 5}, {PadButton::RightStickRight, 5},
    };
    for (const auto& [button, value] : moves) {
        const auto step = tl::core::skills_tab_button_down(button, false, false, false);
        TL_CHECK(step.command == SkillsTabCommand::Cursor);
        TL_CHECK_EQ(step.value, value);
    }
}

TL_TEST(skills_tab_buttons_under_the_sheet) {
    // The press under the sheet: B, Y, RT, LT only press; X closes it; A does
    // nothing; neither B nor the triggers leave the tab.
    auto down = [](PadButton b) {
        return tl::core::skills_tab_button_down(b, true, false, true).command;
    };
    TL_CHECK(down(PadButton::B) == SkillsTabCommand::SheetPress);
    TL_CHECK(down(PadButton::Y) == SkillsTabCommand::SheetPress);
    TL_CHECK(down(PadButton::RightTrigger) == SkillsTabCommand::SheetPress);
    TL_CHECK(down(PadButton::LeftTrigger) == SkillsTabCommand::SheetPress);
    TL_CHECK(down(PadButton::X) == SkillsTabCommand::CloseSheet);
    TL_CHECK(down(PadButton::A) == SkillsTabCommand::None);
    TL_CHECK(down(PadButton::Back) == SkillsTabCommand::CloseMenu);
}

TL_TEST(skills_sheet_maps_on_the_release) {
    // on GamepadButtonUp.
    using tl::core::SkillButton;
    TL_CHECK(tl::core::skills_sheet_button_up(PadButton::Y, true) == SkillButton::Y);
    TL_CHECK(tl::core::skills_sheet_button_up(PadButton::B, true) == SkillButton::B);
    TL_CHECK(tl::core::skills_sheet_button_up(PadButton::RightTrigger, true) == SkillButton::RT);
    TL_CHECK(tl::core::skills_sheet_button_up(PadButton::LeftTrigger, true) == SkillButton::LT);
    TL_CHECK(!tl::core::skills_sheet_button_up(PadButton::X, true).has_value());
    TL_CHECK(!tl::core::skills_sheet_button_up(PadButton::A, true).has_value());
    TL_CHECK(!tl::core::skills_sheet_button_up(PadButton::RightBumper, true).has_value());
    TL_CHECK(!tl::core::skills_sheet_button_up(PadButton::Y, false).has_value());
}

TL_TEST(skill_set_switch_is_gated_outside_the_menu) {
    // and its gate.
    using tl::core::SkillSetSwitch;
    tl::core::SkillBook book;
    TL_CHECK(tl::core::switch_skill_set(book, 0, false) == SkillSetSwitch::Unchanged);
    // Nothing on the second set's four buttons: refused outside the menu...
    TL_CHECK(tl::core::switch_skill_set(book, 1, false) == SkillSetSwitch::Refused);
    TL_CHECK_EQ(book.skill_set(), 0);
    // ...and let through with the skills menu open.
    TL_CHECK(tl::core::switch_skill_set(book, 1, true) == SkillSetSwitch::Switched);
    TL_CHECK_EQ(book.skill_set(), 1);
    // Back to the first set, empty too, from play: refused.
    TL_CHECK(tl::core::switch_skill_set(book, 0, false) == SkillSetSwitch::Refused);
    // One skill on the first set's LT (slot 5) is enough.
    std::vector<int64_t> slots(tl::core::kSkillFunctionSlots, -1);
    slots[5] = 42;
    book.set_slots(slots, std::vector<int64_t>(tl::core::kSkillFunctionSlots, -1));
    TL_CHECK(tl::core::switch_skill_set(book, 0, false) == SkillSetSwitch::Switched);
    TL_CHECK_EQ(book.skill_set(), 0);
    // Slots 0 and 1 belong to neither set's buttons.
    slots.assign(tl::core::kSkillFunctionSlots, -1);
    slots[0] = 7;
    slots[1] = 8;
    book.set_slots(slots, std::vector<int64_t>(tl::core::kSkillFunctionSlots, -1));
    TL_CHECK(tl::core::switch_skill_set(book, 1, false) == SkillSetSwitch::Refused);
}

TL_TEST(housing_close_sounds) {
    // Every housing plays slot 0x16 opening; all but the skills tab 0x42 closing.
    using tl::core::HousingTab;
    TL_CHECK(tl::core::housing_plays_close_sound(HousingTab::Inventory));
    TL_CHECK(tl::core::housing_plays_close_sound(HousingTab::Stats));
    TL_CHECK(tl::core::housing_plays_close_sound(HousingTab::Quest));
    TL_CHECK(!tl::core::housing_plays_close_sound(HousingTab::Skills));
}
