#include "core/context_tips.h"

#include "core/settings_menu.h"

namespace tl::core {

namespace {

// `KContextTipNames` (the static constructor), slot 23 by the
// console's name.
constexpr const char* kContextTipNames[kContextTipCount] = {
    "TIP_INVENTORY",
    "TIP_UNIDENTIFIED",
    "TIP_INVENTORYFULL",
    "TIP_HEALTHLOW",
    "TIP_MANALOW",
    "TIP_MERCHANT",
    "TIP_LEVELUP",
    "TIP_STATS",
    "TIP_SPELL",
    "TIP_WELCOME",
    "TIP_SOCKETABLE",
    "TIP_PETFLEE",
    "TIP_GAMBLE",
    "TIP_ENCHANT",
    "TIP_TRANSMUTE",
    "TIP_PETINVENTORY",
    "TIP_PETINVENTORYFULL",
    "TIP_STASH",
    "TIP_FISHING",
    "TIP_SPELLFIND",
    "TIP_QUESTCOMPLETE",
    "TIP_SHAREDSTASH",
    "TIP_HOWTOATTACK",
    "TIP_SKILLS",
    "TIP_TEMP6",
    "TIP_TEMP7",
    "TIP_TEMP8",
    "TIP_TEMP9",
    "TIP_TEMP10",
    "TIP_TEMP11",
    "TIP_TEMP12",
    "TIP_TEMP13",
    "TIP_TEMP14",
    "TIP_TEMP15",
};

/// Slot 23 in the PC's table.
constexpr const char* kPcSlot23Name = "TIP_TEMP5";

}  // namespace

// PORT: a context tip's name in `KContextTipNames`' order.
const char* context_tip_name(int tip) {
    if (tip < 0 || static_cast<size_t>(tip) >= kContextTipCount) return "";
    return kContextTipNames[tip];
}

// PORT: a context tip by its name (`KContextTipNames`, `CGameGlobals`), the PC's
// slot 23 taken as the skills tip.
int context_tip_by_name(const std::string& key) {
    for (size_t i = 0; i < kContextTipCount; ++i) {
        if (key == kContextTipNames[i]) return static_cast<int>(i);
    }
    if (key == kPcSlot23Name) return static_cast<int>(ContextTip::Skills);
    return -1;
}

// CGameUI::queueTip
void TipQueue::queue(ContextTip tip, bool has_player, const TipsSeen& seen, bool show_tips) {
    if (!has_player) return;
    if (seen[static_cast<size_t>(tip)] != 0) return;
    if (!show_tips) return;
    queue_.push_back(tip);
}

// CGameUI::updateIngameUI, the tip queue at
//
TipQueue::Shown TipQueue::update(bool modal_open, bool has_player, TipsSeen& seen,
                                 const ContextTipTexts& texts) {
    Shown shown;
    if (modal_open || !has_player || queue_.empty()) return shown;
    const ContextTip tip = queue_.front();
    const size_t slot = static_cast<size_t>(tip);
    const std::string& text = texts[slot];
    if (!text.empty() && seen[slot] == 0) {
        shown.open = true;
        shown.tip = tip;
        shown.text = text;
    }
    seen[slot] = 1;
    queue_.erase(queue_.begin());
    return shown;
}

// The console tip window's pad handler
TipPress tip_menu_press(int button, bool& show_tips) {
    TipPress press;
    if (button == kMenuPadB) {
        press.handled = true;
        press.closed = true;
    } else if (button == kMenuPadY) {
        show_tips = false;
        press.handled = true;
        press.closed = true;
        press.disable_tips = true;
    }
    return press;
}

// CGameClient::saveCharacter, the flags of
// `CPlayer +0xa17`
void write_tips_seen(const TipsSeen& seen, std::array<uint8_t, kContextTipCount>& out) {
    for (size_t i = 0; i < kContextTipCount; ++i) out[i] = seen[i];
}

// CGameClient::loadCharacter, the flags of `CPlayer +0xa17`
void read_tips_seen(const std::array<uint8_t, kContextTipCount>& in, TipsSeen& seen) {
    for (size_t i = 0; i < kContextTipCount; ++i) seen[i] = in[i] != 0 ? 1 : 0;
}

// CPlayer::update, the tips at
//
std::vector<ContextTip> player_update_tips(int hp, int max_hp, int mana, int max_mana, bool has_pet,
                                           bool pet_near_death) {
    std::vector<ContextTip> tips;
    if (static_cast<float>(hp) <= static_cast<float>(max_hp) * 0.35f) {
        tips.push_back(ContextTip::HealthLow);
    }
    if (static_cast<float>(mana) <= static_cast<float>(max_mana) * 0.2f) {
        tips.push_back(ContextTip::ManaLow);
    }
    if (has_pet && pet_near_death) tips.push_back(ContextTip::PetFlee);
    return tips;
}

// CCharacter::getItem, the tips at
//
std::vector<ContextTip> item_picked_up_tips(bool socketable, bool spell_scroll, bool identified) {
    std::vector<ContextTip> tips;
    if (socketable) {
        tips.push_back(ContextTip::Socketable);
    } else if (spell_scroll) {
        tips.push_back(ContextTip::SpellFind);
    }
    if (!identified) tips.push_back(ContextTip::Unidentified);
    return tips;
}

// CCharacter::equipmentPickedUp
bool pet_bag_full_tip(bool players_pet, int items_in_pane, int pane_size) {
    if (!players_pet) return false;
    return static_cast<uint32_t>(pane_size) <= static_cast<uint32_t>(items_in_pane);
}

}  // namespace tl::core
