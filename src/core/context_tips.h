// Context tips: the one-time help windows ("Your health is low..."). A queue on
// `CGameUI`, a seen flag per tip on the player (saved) and one modal window,
// `CTipMenu`, which shows the head of the queue once no other modal is up.
// `KSETTINGS_SHOW_TIPS` gates the queue, not the window. The texts are
// `GLOBALS.DAT`'s `[CONTEXTTIPS]`; the drawing is `render/renderer_tips.cpp`'s.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::core {

/// `EContextTip`: the index into `KContextTipNames`
/// and into the seen flags. The console's slot 23 is `TIP_SKILLS`
/// (`TIP_TEMP5` on the PC; its strings).
enum class ContextTip : int32_t {
    Inventory = 0,
    Unidentified = 1,
    InventoryFull = 2,
    HealthLow = 3,
    ManaLow = 4,
    Merchant = 5,
    LevelUp = 6,
    Stats = 7,
    Spell = 8,
    Welcome = 9,
    Socketable = 10,
    PetFlee = 11,
    Gamble = 12,
    Enchant = 13,
    Transmute = 14,
    PetInventory = 15,
    PetInventoryFull = 16,
    Stash = 17,
    Fishing = 18,
    SpellFind = 19,
    QuestComplete = 20,
    SharedStash = 21,
    HowToAttack = 22,
    Skills = 23,
    Temp6 = 24,
    Temp7 = 25,
    Temp8 = 26,
    Temp9 = 27,
    Temp10 = 28,
    Temp11 = 29,
    Temp12 = 30,
    Temp13 = 31,
    Temp14 = 32,
    Temp15 = 33,
};

/// `CGameGlobals::reload` reads this many
/// keys; the save file writes this many flags.
inline constexpr size_t kContextTipCount = 34;

/// The `[CONTEXTTIPS]` key of a tip: `KContextTipNames`, with the console's
/// name for slot 23. Empty for an index out of range.
const char* context_tip_name(int tip);

/// The tip a `[CONTEXTTIPS]` key names, or -1. Slot 23 answers to both of its
/// names.
int context_tip_by_name(const std::string& key);

/// `CPlayer +0xa17`: one byte a tip, set when the tip has been taken off the
/// queue. `CGameClient::saveCharacter` writes the 34 as they are.
using TipsSeen = std::array<uint8_t, kContextTipCount>;

/// `CGameGlobals +0xf0`: the 34 texts (`getContextTip`). A key the
/// data does not author leaves its text empty.
using ContextTipTexts = std::array<std::string, kContextTipCount>;

/// The queue of `CGameUI +0x1960` and the two functions that work it.
class TipQueue {
public:
    /// `CGameUI::queueTip`: nothing without a
    /// player, for a tip already seen, or with Show Tips off. The same tip may
    /// be queued any number of times before it is shown.
    void queue(ContextTip tip, bool has_player, const TipsSeen& seen, bool show_tips);

    /// What `update` found at the head of the queue.
    struct Shown {
        bool open = false;  ///< `CTipMenu::setContents` and `setOpen(true)` are due
        ContextTip tip = ContextTip::Inventory;
        std::string text;
    };

    /// The queue's turn in `CGameUI::updateIngameUI`,
    /// once a frame:
    /// with no modal dialog up (`modalDialogOpenPartial`, the tip window among
    /// them) and a player, the head is taken; it is shown when its text is not
    /// empty and it has not been seen since it was queued; either way it is
    /// marked seen and leaves the queue.
    Shown update(bool modal_open, bool has_player, TipsSeen& seen, const ContextTipTexts& texts);

    // PORT: an accessor of the port's tip queue.
    size_t size() const { return queue_.size(); }
    void clear() { queue_.clear(); }

private:
    std::vector<ContextTip> queue_;
};

/// What a button does on the console's tip window.
struct TipPress {
    bool handled = false;       ///< a button this window answers
    bool closed = false;        ///< `setOpen(false)`
    bool disable_tips = false;  ///< Show Tips set to 0 and the settings saved
};

/// The console tip window's pad handler, `button` one of
/// the `kMenuPad*` codes: B closes; Y sets `KSETTINGS_SHOW_TIPS` to 0, saves
/// the settings and closes; nothing else is answered. `show_tips` is written
/// for Y.
TipPress tip_menu_press(int button, bool& show_tips);

/// `CGameClient::saveCharacter` / `loadCharacter`: the 34 flags, each read
/// back as 0 or 1.
void write_tips_seen(const TipsSeen& seen, std::array<uint8_t, kContextTipCount>& out);
void read_tips_seen(const std::array<uint8_t, kContextTipCount>& in, TipsSeen& seen);

/// The low-health, low-mana and fleeing-pet tips of `CPlayer::update`,
/// in the order it
/// queues them: health at or under 35% of the most, mana at or under 20%, a pet
/// near death (`CCharacter::isPetNearDeath`). `float` comparisons of the `int`
/// readings.
std::vector<ContextTip> player_update_tips(int hp, int max_hp, int mana, int max_mana, bool has_pet,
                                           bool pet_near_death);

/// The tips of a player's `CCharacter::getItem`
///
/// once the item is in the bag, in order: a socketable (`ISA 0x78`) is
/// `Socketable`, or else a spell scroll (`ISA 0x81`) is `SpellFind`; then an
/// unidentified item (`CEquipment +0x348` clear) is `Unidentified`.
std::vector<ContextTip> item_picked_up_tips(bool socketable, bool spell_scroll, bool identified);

/// `CCharacter::equipmentPickedUp`, which
/// `CInventory::pickupEquipment` calls on a bag's owner once the item is in:
/// whether it queues `PetInventoryFull`. The owner's master (`+0x640`) is a
/// player (`ISA 0x1c`) and the bag's pane 0 holds as many items as it has
/// slots (`getPaneSize <= itemsInPane`, unsigned).
bool pet_bag_full_tip(bool players_pet, int items_in_pane, int pane_size);

}  // namespace tl::core
