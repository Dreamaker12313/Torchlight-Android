// What A does to the selected item of a console item tray, and the caption A
// shows for it: (the press) and the ordinary branch of
// (the legend), both of which work on the tray's owner (`+0xc0`) -
// the player on the bag's tray, the pet on its own.
//
// Pure decisions: the caller supplies the tests that need a character
// (`canUse`, the learn test, whether the pet is away) and carries out the
// press.
#pragma once

#include "core/unit_types.h"
#include "core/units.h"

#include <string>

namespace tl::core {

/// The branch takes for an item.
enum class TrayPress {
    Nothing,     ///< returns with nothing done (an `Identify Scroll`, a quest item)
    Learn,       ///< a spell:, then (learn, or the replace dialog)
    UseOnOwner,  ///< a potion or any other useable: `canUse`, `useOnTarget` on the owner
    FeedPet,     ///< a fish (ISA 0x7c): used on the player's first pet
    TownPortal,  ///< `Town Portal Scroll`: used on the owner, then the screen closes
    Identify,    ///< an unidentified item:
    Wear,        ///< weapon, armour, trinket or shield not worn: `canEquip`, then on
    TakeOff,     ///< one that is worn: off, into the bag or onto the floor
};

/// the function's choice for `item` sitting at `slot` of its owner's
/// inventory. A useable item (`isUseable`) is judged before
/// identification; the rest are identified, then worn or taken off.
TrayPress tray_press(const ItemStats& item, size_t slot, const UnitTypeHierarchy& unit_types);

/// What the legend asks of the tray's owner, the function's ordinary
/// branch.
struct TrayCaptionContext {
    bool owner_is_player = true;  ///< on `+0xc0`
    bool owner_is_pet = false;    ///< ISA 0x57 on `+0xc0`
    /// The owner is the player, or is the player's first pet (`+0x5ac`).
    bool owner_is_players_own = true;
    /// The owner's AI state is 0x29 or 0x2a: gone to town or coming back.
    bool owner_away = false;
    int identify_scrolls = 0;  ///< the function's count
    bool learnable = false;    ///< on the owner
    bool can_use = false;      ///< (item, owner, owner)
};

/// The A caption writes for `item` at `slot`: "Identify(n)",
/// "Unequip", "Learn" (or nothing for a spell the owner cannot learn),
/// "Socket", "Use" (a fish for the player's own, else what `canUse` passes
/// and is not an `Identify Scroll`), "Equip" (weapon, armour or trinket on
/// the player; necklace or ring on a pet), or nothing.
std::string tray_a_caption(const ItemStats& item, size_t slot, const UnitTypeHierarchy& unit_types,
                           const TrayCaptionContext& owner);

}  // namespace tl::core
