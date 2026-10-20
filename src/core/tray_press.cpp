#include "core/tray_press.h"

#include "core/inventory.h"
#include "core/item_use.h"

namespace tl::core {

namespace {

// The unit names compares a useable item's unit (`+0x178`) with
// (X360 strings).
constexpr const char* kTownPortalScroll = "Town Portal Scroll";
constexpr const char* kIdentifyScroll = "Identify Scroll";

}  // namespace

TrayPress tray_press(const ItemStats& item, size_t slot, const UnitTypeHierarchy& unit_types) {
    //
    const auto isa = [&](uint32_t type) { return unit_types.isa(item.unittype, type); };
    if (item_is_useable(item, unit_types)) {
        // (item, 0x81) or (item, 0x21): a spell goes to the learn
        // test and, a potion to `canUse` and `useOnTarget`.
        if (isa(unit_type::kSpell)) return TrayPress::Learn;
        if (isa(unit_type::kPotion)) return TrayPress::UseOnOwner;
        // ISA 0x7c: the player's first pet (`+0x5ac`) is the target, whoever
        // owns the tray.
        if (isa(unit_type::kFish)) return TrayPress::FeedPet;
        if (item.name == kTownPortalScroll) return TrayPress::TownPortal;
        if (item.name == kIdentifyScroll) return TrayPress::Nothing;
        return TrayPress::UseOnOwner;
    }
    // `+0x31c` clear: identified with a scroll.
    if (!item.identified) return TrayPress::Identify;
    if (!isa(unit_type::kWeapon) && !isa(unit_type::kArmor) && !isa(unit_type::kTrinket) &&
        !isa(unit_type::kShield)) {
        return TrayPress::Nothing;
    }
    // the owner's equipment wears it.
    return Inventory::is_equip_location(slot) ? TrayPress::TakeOff : TrayPress::Wear;
}

std::string tray_a_caption(const ItemStats& item, size_t slot, const UnitTypeHierarchy& unit_types,
                           const TrayCaptionContext& owner) {
    // the ordinary branch (no vendor page), in its order.
    const auto isa = [&](uint32_t type) { return unit_types.isa(item.unittype, type); };
    if (!item.identified) return "Identify(" + std::to_string(owner.identify_scrolls) + ")";
    if (Inventory::is_equip_location(slot)) return "Unequip";
    if (isa(unit_type::kSpell)) return owner.learnable ? "Learn" : "";
    if (isa(unit_type::kSocketable)) return "Socket";
    if (isa(unit_type::kConsumable)) {
        // A fish reads "Use" for the player's own tray or their pet's while
        // the owner is not away; anything else (a fish included) what
        // `canUse` passes, an `Identify Scroll` excepted.
        if (owner.owner_is_players_own && isa(unit_type::kFish) && !owner.owner_away) return "Use";
        return owner.can_use && item.name != kIdentifyScroll ? "Use" : "";
    }
    if (owner.owner_is_player &&
        (isa(unit_type::kWeapon) || isa(unit_type::kArmor) || isa(unit_type::kTrinket))) {
        return "Equip";
    }
    // A pet's tray: its collar (ISA 0x18) and tags (0x11).
    if (owner.owner_is_pet && (isa(unit_type::kNecklace) || isa(unit_type::kRing))) return "Equip";
    return "";
}

}  // namespace tl::core
