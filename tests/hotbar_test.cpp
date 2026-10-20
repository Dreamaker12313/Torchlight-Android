// Which of the ten function slots a pad press fires, and what the HUD draws in each. Slots
// as CSkillMenuXenon's map handler writes them, `SkillSlot<N+1>` in the HUD.
#include "core/hotbar.h"

#include "test_support.h"

namespace {

using tl::core::HotbarButton;
using tl::core::HotbarPress;
using tl::core::kSkillFunctionSlots;

using Slots = std::array<int64_t, kSkillFunctionSlots>;

Slots empty_slots() {
    Slots slots;
    slots.fill(-1);
    return slots;
}

std::function<bool(HotbarButton)> only(HotbarButton wanted) {
    return [wanted](HotbarButton button) { return button == wanted; };
}

const std::function<bool(HotbarButton)> kNothing = [](HotbarButton) { return false; };
const std::function<bool(int64_t)> kAllKnown = [](int64_t) { return true; };

}  // namespace

// The layout's two banks: `SkillSet1` holds `SkillSlot4..7`, `SkillSet2`
// `SkillSlot3` and `SkillSlot8..10`, with the bumpers on 1 and 2 in both.
TL_TEST(each_set_puts_its_four_face_buttons_on_the_slots_the_layout_names) {
    TL_CHECK(tl::core::hotbar_slot_button(0, 0) == HotbarButton::LB);
    TL_CHECK(tl::core::hotbar_slot_button(1, 1) == HotbarButton::RB);
    TL_CHECK(tl::core::hotbar_slot_button(3, 0) == HotbarButton::Y);
    TL_CHECK(tl::core::hotbar_slot_button(4, 0) == HotbarButton::B);
    TL_CHECK(tl::core::hotbar_slot_button(5, 0) == HotbarButton::LT);
    TL_CHECK(tl::core::hotbar_slot_button(6, 0) == HotbarButton::RT);
    TL_CHECK(tl::core::hotbar_slot_button(2, 1) == HotbarButton::Y);
    TL_CHECK(tl::core::hotbar_slot_button(7, 1) == HotbarButton::B);
    TL_CHECK(tl::core::hotbar_slot_button(8, 1) == HotbarButton::LT);
    TL_CHECK(tl::core::hotbar_slot_button(9, 1) == HotbarButton::RT);
    // A slot of the set that is not up answers to nothing.
    TL_CHECK(tl::core::hotbar_slot_button(2, 0) == HotbarButton::None);
    TL_CHECK(tl::core::hotbar_slot_button(9, 0) == HotbarButton::None);
    TL_CHECK(tl::core::hotbar_slot_button(3, 1) == HotbarButton::None);
}

TL_TEST(a_mapped_skill_fires_on_the_press_and_keeps_firing_while_held) {
    Slots skills = empty_slots();
    skills[3] = 77;  // Y in the first set
    const Slots items = empty_slots();
    HotbarPress press =
        tl::core::hotbar_press(skills, items, kAllKnown, 0, only(HotbarButton::Y), kNothing);
    TL_CHECK_EQ(press.slot, 3);
    TL_CHECK(press.skill);
    TL_CHECK(!press.continuing);
    press = tl::core::hotbar_press(skills, items, kAllKnown, 0, kNothing, only(HotbarButton::Y));
    TL_CHECK_EQ(press.slot, 3);
    TL_CHECK(press.continuing);
    // The same button in the other set reaches slot 2, which holds nothing.
    press = tl::core::hotbar_press(skills, items, kAllKnown, 1, only(HotbarButton::Y), kNothing);
    TL_CHECK_EQ(press.slot, -1);
}

TL_TEST(an_item_slot_fires_on_the_press_alone) {
    const Slots skills = empty_slots();
    Slots items = empty_slots();
    items[0] = 11;
    HotbarPress press =
        tl::core::hotbar_press(skills, items, kAllKnown, 0, only(HotbarButton::LB), kNothing);
    TL_CHECK_EQ(press.slot, 0);
    TL_CHECK(!press.skill);
    press = tl::core::hotbar_press(skills, items, kAllKnown, 0, kNothing, only(HotbarButton::LB));
    TL_CHECK_EQ(press.slot, -1);
}

// The dispatch tests the skill half first and only falls through to the item
// half when the slot holds no skill it knows.
TL_TEST(a_known_skill_hides_the_item_under_it_and_an_unknown_one_does_not) {
    Slots skills = empty_slots();
    Slots items = empty_slots();
    skills[1] = 55;
    items[1] = 12;
    const std::function<bool(int64_t)> none_known = [](int64_t) { return false; };
    HotbarPress press =
        tl::core::hotbar_press(skills, items, kAllKnown, 0, only(HotbarButton::RB), kNothing);
    TL_CHECK(press.skill);
    press = tl::core::hotbar_press(skills, items, none_known, 0, only(HotbarButton::RB), kNothing);
    TL_CHECK_EQ(press.slot, 1);
    TL_CHECK(!press.skill);
    // Held, the known skill still fires; the item never would.
    press = tl::core::hotbar_press(skills, items, kAllKnown, 0, kNothing, only(HotbarButton::RB));
    TL_CHECK(press.skill);
    press = tl::core::hotbar_press(skills, items, none_known, 0, kNothing, only(HotbarButton::RB));
    TL_CHECK_EQ(press.slot, -1);
}

TL_TEST(the_first_slot_in_order_wins) {
    Slots skills = empty_slots();
    skills[4] = 1;  // B
    skills[6] = 2;  // RT
    const auto both = [](HotbarButton b) { return b == HotbarButton::B || b == HotbarButton::RT; };
    const HotbarPress press =
        tl::core::hotbar_press(skills, empty_slots(), kAllKnown, 0, both, kNothing);
    TL_CHECK_EQ(press.slot, 4);
}

// The dispatch's release walk: the active skill
// is let go unless the first held skill slot is the one casting it.
TL_TEST(a_held_skill_is_let_go_when_its_button_is_up) {
    Slots skills = empty_slots();
    skills[3] = 77;  // Y in the first set
    skills[6] = 88;  // RT in the first set
    TL_CHECK(tl::core::hotbar_lets_go(skills, 77, 0, kNothing, kNothing));
    TL_CHECK(!tl::core::hotbar_lets_go(skills, 77, 0, kNothing, only(HotbarButton::Y)));
    TL_CHECK(!tl::core::hotbar_lets_go(skills, 77, 0, only(HotbarButton::Y), kNothing));
    // Another skill's button down lets the active one go.
    TL_CHECK(tl::core::hotbar_lets_go(skills, 77, 0, kNothing, only(HotbarButton::RT)));
    // The first held slot decides, even when a later one holds the active skill.
    const auto both = [](HotbarButton b) { return b == HotbarButton::Y || b == HotbarButton::RT; };
    TL_CHECK(!tl::core::hotbar_lets_go(skills, 77, 0, kNothing, both));
    TL_CHECK(tl::core::hotbar_lets_go(skills, 88, 0, kNothing, both));
}

// `CGameUI::updateSlots`: `(int)ceilf(cooling)`, grey at 1 or more.
TL_TEST(a_cooling_skill_slot_is_grey_with_its_whole_seconds_left) {
    tl::core::HotbarSlotView view = tl::core::skill_slot_view("ember_shield", 0.0f);
    TL_CHECK_EQ(view.icon, std::string("ember_shield"));
    TL_CHECK(!view.dimmed);
    TL_CHECK(view.text.empty());
    view = tl::core::skill_slot_view("ember_shield", 0.2f);
    TL_CHECK(view.dimmed);
    TL_CHECK_EQ(view.text, std::string("1"));
    view = tl::core::skill_slot_view("ember_shield", 4.0f);
    TL_CHECK_EQ(view.text, std::string("4"));
    view = tl::core::skill_slot_view("ember_shield", 4.01f);
    TL_CHECK_EQ(view.text, std::string("5"));
}

TL_TEST(an_item_slot_counts_what_the_bag_holds_and_greys_at_none) {
    tl::core::HotbarSlotView view = tl::core::item_slot_view("healthpotion", 3);
    TL_CHECK_EQ(view.text, std::string("3"));
    TL_CHECK(!view.dimmed);
    view = tl::core::item_slot_view("healthpotion", 0);
    TL_CHECK(view.text.empty());
    TL_CHECK(view.dimmed);
    TL_CHECK_EQ(view.icon, std::string("healthpotion"));
    view = tl::core::item_slot_view("", 3);
    TL_CHECK(view.icon.empty());
    TL_CHECK(view.text.empty());
}

// `CInventory::getEquipmentCountOfGuid`: stacks summed, an item
// that cannot stack counted once.
TL_TEST(the_count_sums_every_stack_of_the_guid) {
    tl::core::Inventory inv;
    tl::core::ItemStats potion;
    potion.name = "Health Potion";
    potion.unit_guid = 11;
    potion.max_stack = 20;
    inv.pickup_into_slot(potion, 5, 19);
    inv.pickup_into_slot(potion, 2, 20);
    tl::core::ItemStats ring;
    ring.name = "Ring";
    ring.unit_guid = 12;
    ring.max_stack = 0;
    inv.pickup_into_slot(ring, 3, 21);
    TL_CHECK_EQ(tl::core::equipment_count_of_guid(inv, 11), 7);
    TL_CHECK_EQ(tl::core::equipment_count_of_guid(inv, 12), 1);
    TL_CHECK_EQ(tl::core::equipment_count_of_guid(inv, 13), 0);
}
