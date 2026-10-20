// Context tips: CGameUI::queueTip, CGameUI::updateIngameUI,
// the tip window's pad handler, KContextTipNames, and the
// callers' conditions (CPlayer::update, CCharacter::getItem).
#include "core/context_tips.h"
#include "core/settings_menu.h"
#include "test_support.h"

namespace {

using tl::core::ContextTip;
using tl::core::ContextTipTexts;
using tl::core::TipQueue;
using tl::core::TipsSeen;

ContextTipTexts texts_for(std::initializer_list<ContextTip> tips) {
    ContextTipTexts texts;
    for (ContextTip tip : tips)
        texts[static_cast<size_t>(tip)] = tl::core::context_tip_name(static_cast<int>(tip));
    return texts;
}

}  // namespace

/// The table's order is the enum's and the save file's.
TL_TEST(context_tip_names_follow_the_original_table) {
    TL_CHECK(std::string(tl::core::context_tip_name(0)) == "TIP_INVENTORY");
    TL_CHECK(std::string(tl::core::context_tip_name(4)) == "TIP_MANALOW");
    TL_CHECK(std::string(tl::core::context_tip_name(11)) == "TIP_PETFLEE");
    TL_CHECK(std::string(tl::core::context_tip_name(20)) == "TIP_QUESTCOMPLETE");
    TL_CHECK(std::string(tl::core::context_tip_name(22)) == "TIP_HOWTOATTACK");
    TL_CHECK(std::string(tl::core::context_tip_name(33)) == "TIP_TEMP15");
    TL_CHECK(std::string(tl::core::context_tip_name(34)).empty());
    TL_CHECK(std::string(tl::core::context_tip_name(-1)).empty());
    TL_CHECK(tl::core::context_tip_by_name("TIP_SPELLFIND") == 19);
    // Slot 23: the console's name and the PC's.
    TL_CHECK(tl::core::context_tip_by_name("TIP_SKILLS") == 23);
    TL_CHECK(tl::core::context_tip_by_name("TIP_TEMP5") == 23);
    // Authored, and read by nothing.
    TL_CHECK(tl::core::context_tip_by_name("TIP_CANTUSE") == -1);
}

/// No player, a tip already seen, or Show Tips off: nothing is queued.
TL_TEST(a_tip_is_queued_only_for_a_player_who_has_not_seen_it_with_tips_on) {
    TipQueue queue;
    TipsSeen seen{};
    queue.queue(ContextTip::HealthLow, false, seen, true);
    TL_CHECK(queue.size() == 0);
    queue.queue(ContextTip::HealthLow, true, seen, false);
    TL_CHECK(queue.size() == 0);
    seen[static_cast<size_t>(ContextTip::HealthLow)] = 1;
    queue.queue(ContextTip::HealthLow, true, seen, true);
    TL_CHECK(queue.size() == 0);
    queue.queue(ContextTip::ManaLow, true, seen, true);
    // The queue does not look for a tip already in it.
    queue.queue(ContextTip::ManaLow, true, seen, true);
    TL_CHECK(queue.size() == 2);
}

/// One tip a frame, the head first; it is marked seen as it leaves, so its
/// second copy in the queue is dropped without opening the window.
TL_TEST(the_queue_shows_its_head_once_and_marks_it_seen) {
    TipQueue queue;
    TipsSeen seen{};
    const ContextTipTexts texts = texts_for({ContextTip::ManaLow, ContextTip::LevelUp});
    queue.queue(ContextTip::ManaLow, true, seen, true);
    queue.queue(ContextTip::ManaLow, true, seen, true);
    queue.queue(ContextTip::LevelUp, true, seen, true);

    TipQueue::Shown shown = queue.update(false, true, seen, texts);
    TL_CHECK(shown.open);
    TL_CHECK(shown.tip == ContextTip::ManaLow);
    TL_CHECK(shown.text == "TIP_MANALOW");
    TL_CHECK(seen[static_cast<size_t>(ContextTip::ManaLow)] == 1);
    TL_CHECK(queue.size() == 2);

    shown = queue.update(false, true, seen, texts);
    TL_CHECK(!shown.open);
    TL_CHECK(queue.size() == 1);

    shown = queue.update(false, true, seen, texts);
    TL_CHECK(shown.open);
    TL_CHECK(shown.tip == ContextTip::LevelUp);
    TL_CHECK(queue.size() == 0);
    TL_CHECK(!queue.update(false, true, seen, texts).open);
}

/// A modal dialog (an open tip is one) or no player holds the queue as it is.
TL_TEST(the_queue_waits_behind_a_modal_dialog) {
    TipQueue queue;
    TipsSeen seen{};
    const ContextTipTexts texts = texts_for({ContextTip::Welcome});
    queue.queue(ContextTip::Welcome, true, seen, true);
    TL_CHECK(!queue.update(true, true, seen, texts).open);
    TL_CHECK(!queue.update(false, false, seen, texts).open);
    TL_CHECK(queue.size() == 1);
    TL_CHECK(seen[static_cast<size_t>(ContextTip::Welcome)] == 0);
    TL_CHECK(queue.update(false, true, seen, texts).open);
}

/// A tip the data has no text for is spent without a window.
///
TL_TEST(a_tip_without_a_text_is_marked_seen_and_not_shown) {
    TipQueue queue;
    TipsSeen seen{};
    const ContextTipTexts texts{};
    queue.queue(ContextTip::HowToAttack, true, seen, true);
    TL_CHECK(!queue.update(false, true, seen, texts).open);
    TL_CHECK(queue.size() == 0);
    TL_CHECK(seen[static_cast<size_t>(ContextTip::HowToAttack)] == 1);
}

/// B closes; Y turns Show Tips off and closes; A, X and the rest do nothing.
TL_TEST(the_console_tip_window_answers_b_and_y) {
    bool show_tips = true;
    tl::core::TipPress press = tl::core::tip_menu_press(tl::core::kMenuPadA, show_tips);
    TL_CHECK(!press.handled && !press.closed && show_tips);
    press = tl::core::tip_menu_press(tl::core::kMenuPadStart, show_tips);
    TL_CHECK(!press.handled && !press.closed && show_tips);
    press = tl::core::tip_menu_press(tl::core::kMenuPadB, show_tips);
    TL_CHECK(press.handled && press.closed && !press.disable_tips && show_tips);
    press = tl::core::tip_menu_press(tl::core::kMenuPadY, show_tips);
    TL_CHECK(press.handled && press.closed && press.disable_tips && !show_tips);
}

/// 35% of health, 20% of mana, both inclusive; the pet's tip needs a pet.
TL_TEST(the_player_update_tips_are_low_health_low_mana_and_a_fleeing_pet) {
    using tl::core::player_update_tips;
    TL_CHECK(player_update_tips(100, 100, 100, 100, true, false).empty());
    // 35 of 100 is at the threshold (35 <= 100 * 0.35f, which is 35.0f); 36 is over it.
    std::vector<ContextTip> tips = player_update_tips(35, 100, 100, 100, false, false);
    TL_CHECK(tips.size() == 1 && tips[0] == ContextTip::HealthLow);
    TL_CHECK(player_update_tips(36, 100, 100, 100, false, false).empty());
    tips = player_update_tips(100, 100, 20, 100, false, false);
    TL_CHECK(tips.size() == 1 && tips[0] == ContextTip::ManaLow);
    TL_CHECK(player_update_tips(100, 100, 21, 100, false, false).empty());
    tips = player_update_tips(1, 100, 0, 100, true, true);
    TL_CHECK(tips.size() == 3);
    TL_CHECK(tips[0] == ContextTip::HealthLow && tips[1] == ContextTip::ManaLow &&
             tips[2] == ContextTip::PetFlee);
    TL_CHECK(player_update_tips(100, 100, 100, 100, false, true).empty());
    // A character with no mana at all is at 0 <= 0.
    tips = player_update_tips(100, 100, 0, 0, false, false);
    TL_CHECK(tips.size() == 1 && tips[0] == ContextTip::ManaLow);
}

/// A socketable takes the scroll's place; unidentified comes after either.
TL_TEST(a_picked_up_item_queues_its_kind_and_then_unidentified) {
    using tl::core::item_picked_up_tips;
    TL_CHECK(item_picked_up_tips(false, false, true).empty());
    std::vector<ContextTip> tips = item_picked_up_tips(true, true, true);
    TL_CHECK(tips.size() == 1 && tips[0] == ContextTip::Socketable);
    tips = item_picked_up_tips(false, true, false);
    TL_CHECK(tips.size() == 2 && tips[0] == ContextTip::SpellFind &&
             tips[1] == ContextTip::Unidentified);
    tips = item_picked_up_tips(false, false, false);
    TL_CHECK(tips.size() == 1 && tips[0] == ContextTip::Unidentified);
}

/// The pet's bag pane with no slot left, and only a player's pet.
TL_TEST(a_full_pet_bag_queues_its_tip_for_a_players_pet) {
    TL_CHECK(!tl::core::pet_bag_full_tip(true, 23, 24));
    TL_CHECK(tl::core::pet_bag_full_tip(true, 24, 24));
    TL_CHECK(tl::core::pet_bag_full_tip(true, 25, 24));
    TL_CHECK(!tl::core::pet_bag_full_tip(false, 24, 24));
}

/// The save's 34 bytes, read back as 0 or 1.
TL_TEST(the_seen_flags_round_trip_through_the_save) {
    TipsSeen seen{};
    seen[3] = 1;
    seen[33] = 1;
    std::array<uint8_t, tl::core::kContextTipCount> bytes{};
    tl::core::write_tips_seen(seen, bytes);
    TL_CHECK(bytes[3] == 1 && bytes[33] == 1 && bytes[0] == 0);
    bytes[5] = 7;
    TipsSeen back{};
    tl::core::read_tips_seen(bytes, back);
    TL_CHECK(back[3] == 1 && back[5] == 1 && back[33] == 1 && back[4] == 0);
}
