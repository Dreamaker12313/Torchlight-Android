// One row of a console item tray: the row fill and the figures it
// compares against. The caller supplies what needs a character and draws
// the result. No OGRE and no file I/O.
#pragma once

#include "core/effect_manager.h"
#include "core/inventory.h"
#include "core/item_label.h"
#include "core/item_roll.h"
#include "core/unit_types.h"
#include "core/units.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace tl::core {

/// The first six entries of the tray's image table (indexes
/// it; one of its initialisers): what `QualityBk`, the square
/// behind a row's icon, is drawn with.
enum class TrayBackdrop {
    Blank = 0,   ///< `traycolor_blank`
    Gold = 1,    ///< `traycolor_gold`
    Blue = 2,    ///< `traycolor_blue`
    Green = 3,   ///< `traycolor_green`
    Purple = 4,  ///< `traycolor_purple`
    Red = 5,     ///< `traycolor_red`
};
const char* tray_backdrop_image(TrayBackdrop backdrop);

/// `qualityarrow`: table entries 0xe and 0xf.
enum class TrayArrow {
    None,
    Up,    ///< `comparrison_arrow_up`
    Down,  ///< `comparrison_arrow_down`
};
const char* tray_arrow_image(TrayArrow arrow);

/// The items the effect lists hold, an item and what is set into it:
/// each damage type the copy carries a bonus of (`+0x330` above
/// 0), the three lists of its effect manager, and the same of every socketed
/// item. The row prints it as "Effects: N".
uint32_t item_effect_count(const ItemStats& item, const EffectManager::Context& effects);

/// `CEquipment::DPS()`. Weapons only (the console returns
/// 0 for anything without an attack description).
int item_dps(const ItemStats& item, const EffectManager::Context& effects);

/// The twelve figures a tray compares its rows with: hands through
/// locations 2-10; the twelfth is the left hand's armour
/// again, what a shield is compared with.
using TrayWornFigures = std::array<uint32_t, 12>;
TrayWornFigures tray_worn_figures(const Inventory& players, const UnitTypeHierarchy& unit_types,
                                  const EffectManager::Context& effects);

/// What the row fill asks beyond the item itself.
struct TrayRowContext {
    const UnitTypeHierarchy* unit_types = nullptr;
    const EffectManager::Context* effects = nullptr;
    /// `CGameGlobals`' label colours (`core/item_label.h`).
    const ItemLabelColours* colours = nullptr;
    /// The tray's owner (`+0xc0`) ISA `GAMBLER` (0x7f).
    bool owner_is_gambler = false;
    /// The client has a player (`CGameUI +0x38`). Without one the row has no
    /// name colour, no arrow and no foot line.
    bool have_player = true;
    /// The merchant's screen is open (`CGameUI +0x398`'s vtable `+0xc`), which
    /// keeps "Press A to identify" off an unidentified row.
    bool merchant_open = false;
    /// The item's vtable `+0x15c` with the player and true:
    /// `CEquipment::canEquip(CCharacter*, bool)`.
    /// Unset, every item can be worn.
    std::function<bool(const ItemStats&)> player_can_equip;
    /// The player's requirement reductions and the five figures the foot line
    /// tests: level (`+0xf0`), strength, dexterity, magic,
    /// defense.
    RequirementReductions reductions;
    int level = 0;
    int strength = 0;
    int dexterity = 0;
    int magic = 0;
    int defense = 0;
    TrayWornFigures worn{};
};

/// A row as the fill leaves it.
struct TrayRow {
    TrayBackdrop backdrop = TrayBackdrop::Blank;
    /// `SlotText`'s `TextColour` (or grey on a gambler's
    /// tray). The text itself is the caller's: `getFullItemName(false)`, or
    /// "???" on a gambler's tray (`gambler_name`).
    LabelColour name_colour{1.0f, 0.8f, 0.8f, 0.8f};
    bool gambler_name = false;
    /// `DamageArmor`: "DPS: n", "armor n", "xN" for a stack, or "???".
    std::string detail;
    /// `Enchantments`: "Effects:n", or "???".
    std::string enchantments;
    TrayArrow arrow = TrayArrow::None;
    /// `Requirment`, the foot line, and its `TextColour`.
    std::string requirement;
    LabelColour requirement_colour{1.0f, 1.0f, 1.0f, 1.0f};
};

/// for one item with `count` in its stack (`+0x1e4`).
TrayRow tray_row(const ItemStats& item, int count, const TrayRowContext& context);

/// A row's `Alpha`: 1 for the selected row, 0.9 for one that
/// says "Equipped", 0.6 for the rest.
float tray_row_alpha(bool selected, bool equipped);

}  // namespace tl::core
