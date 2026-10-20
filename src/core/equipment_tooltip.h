// What an item's details window says: `CGameUI::showEquipmentTooltip`
// (the same captions over `media/UI/tooltip.layout`)
// and the `CEquipment` text builders it calls - `getEquipmentType`,
// `getEquipmentStats`,
// `getEquipmentEffects`
// over `effectsDescription`
// and `CEffectManager::getVisualDescription`.
//
//
// The text, its colours and which windows show are here; where the windows sit
// is the drawing's (`render/item_tooltip.h`). No OGRE and no file I/O.
//
// `CEquipment::skillDescription`, the last
// part of `getEquipmentEffects`: the item's `[SKILL_TO_GIVE]` groups, then the
// description of each skill the item's own skill manager holds.
#pragma once

#include "core/effect_manager.h"
#include "core/effects.h"
#include "core/inventory.h"
#include "core/item_label.h"
#include "core/item_roll.h"
#include "core/item_sets.h"
#include "core/unit_types.h"
#include "core/units.h"

#include <array>
#include <functional>
#include <string>

namespace tl::core {

/// `CEquipment::getEquipmentType(bool)`:
/// "(Quest Item)" for a quest unit, then by kind - a weapon, an armour or a
/// trinket led by "Unidentified" or, with `with_quality`, "Unique", "Rare" or
/// "Enchanted" - and the first `{...}` span removed.
std::string equipment_type(const ItemStats& item, bool with_quality,
                           const UnitTypeHierarchy& unit_types);

/// `CEquipment::getAttackSpeedString` over
/// `KWeaponSpeedValues` (130, 110, 90, 80, 0): the first threshold
/// the swing's speed (`SPEED`) reaches names it, "Slowest Attack Speed" down to
/// "Fastest".
std::string attack_speed_string(float speed_percent);

/// What the set block of `getEquipmentStats` reads: `CSets`, the inventory the
/// item sits in (`+0x240`, null for none), and how its lines are printed.
struct EquipmentSetContext {
    const ItemSetBook* sets = nullptr;
    const Inventory* owner = nullptr;
    const EffectDisplayContext* display = nullptr;
    const ItemLabelColours* colours = nullptr;
};

/// `CEquipment::getEquipmentStats()`: a
/// weapon's "Physical Damage: a-b", each inherent elemental damage of an
/// identified copy, its attack speed, "Sockets n/m", and for a set piece "Set :
/// <name>" and a line per bonus, "<n> pieces : <what it gives>", as `|c`..`|u`
/// colour runs.
std::string equipment_stats(const ItemStats& item, const UnitTypeHierarchy& unit_types,
                            const EquipmentSetContext& set = {});

/// `CEffectManager::getVisualDescription(activation, -1, true, split)`
/// for an item copy's manager: the list's
/// effects merged where they are the same kind (per-tick flag, type, damage
/// type and, for type 0x52, name), each printed from its applied value. A
/// `Transfer` line is led by "Conveys"; with `split`, a line whose affix cannot
/// go on both a weapon and an armour is headed "Weapon:" or "Armor/Trinkets:".
std::string item_visual_description(const ItemStats& item, EffectActivation activation, bool split,
                                    const EffectManager::Context& effects,
                                    const EffectDisplayContext& display,
                                    const UnitTypeHierarchy& unit_types);

/// `CEquipment::effectsDescription(activation, no_split, socketed)`.
/// With `socketed` false: for the passive
/// list the copy's damage bonuses ("+n Fire Damage"), then the manager's visual
/// description. With it true: the same of every socketed item, inside the
/// socketable colour run.
std::string item_effects_description(const ItemStats& item, EffectActivation activation,
                                     bool no_split, bool socketed,
                                     const EffectManager::Context& effects,
                                     const EffectDisplayContext& display,
                                     const UnitTypeHierarchy& unit_types,
                                     const LabelColour& socket_colour = {});

/// `CSkill::getDescription(skill, item, -1)` for one of the item's `[SKILL]`s,
/// when it is shown: false for a skill not enabled (`+0x6d`) or `HIDDEN`
/// (`+0x6b`).
using ItemSkillDescriber = std::function<bool(const ItemStats::Skill& skill, std::string& text)>;

/// `CEquipment::skillDescription()`.
std::string equipment_skill_description(const ItemStats& item, const ItemSkillDescriber& describe);

/// `CEquipment::getEquipmentEffects()`:
/// nothing for an unidentified copy; else the six descriptions - dynamic,
/// passive, transfer, then the same three of the socketed items - each on its
/// own line, then `skillDescription`'s, and a closing line break when
/// anything was said.
std::string equipment_effects(const ItemStats& item, const EffectManager::Context& effects,
                              const EffectDisplayContext& display,
                              const UnitTypeHierarchy& unit_types,
                              const LabelColour& socket_colour = {},
                              const ItemSkillDescriber& describe_skill = {});

/// One text window of the tooltip.
struct TooltipText {
    bool shown = false;
    std::string text;
    LabelColour colour{1.0f, 1.0f, 1.0f, 1.0f};
};

/// The windows of `media/UI/tooltip.layout`, as `showEquipmentTooltip` leaves
/// them.
struct EquipmentTooltip {
    TooltipText name;       ///< `ItemName`
    TooltipText type;       ///< `ItemType`
    TooltipText big;        ///< `BigDPS`: the DPS, the armour, or "??"
    TooltipText big_label;  ///< `DPS`: "damage per second" or "armor"
    TooltipText stats;      ///< `Stats`
    TooltipText effects;    ///< `Effects`, with its two dividers
    /// `Level Requirements`, `Strength`, `Dexterity`, `Magic`, `Defense`.
    std::array<TooltipText, 5> requirements;
    TooltipText description;  ///< `Description`
    TooltipText price;        ///< `Price`
};

/// What `showEquipmentTooltip` asks beyond the item.
struct EquipmentTooltipContext {
    const UnitTypeHierarchy* unit_types = nullptr;
    const EffectManager::Context* effects = nullptr;
    const EffectDisplayContext* display = nullptr;
    const ItemLabelColours* colours = nullptr;
    /// `CSets`, and the inventory the item sits in, for its set block.
    const ItemSetBook* sets = nullptr;
    const Inventory* owner_inventory = nullptr;
    /// The item's owner is the player (the 1st argument is `CGameUI +0x38`), a gambler
    /// (ISA 0x7f) or a merchant (ISA 0x29).
    bool owner_is_player = true;
    bool owner_is_gambler = false;
    bool owner_is_merchant = false;
    /// `CGameUI +0x4f0`'s vtable `+0x20`: the merchant's menu is open, which
    /// is when an item of the player's shows what it sells for.
    bool merchant_open = false;
    /// One of the two comparison windows (the 4th argument not null): the type line is
    /// led by "(equipped)" and there is no price.
    bool comparison = false;
    /// `CInventory::findEquipmentSlot` for a comparison's item: 1 and 9 read
    /// "left hand", anything else "right hand", on a ring, a weapon or a
    /// shield.
    int worn_location = -1;
    /// `getFullItemName(false)` (`core/item_name.h`), which caches on the copy.
    std::string full_name;
    /// The stack the price is for.
    int count = 1;
    RequirementReductions reductions;
    int level = 0;
    int strength = 0;
    int dexterity = 0;
    int magic = 0;
    int defense = 0;
    int gold = 0;                        ///< the player's (`+0x444`)
    const float* price_bonus = nullptr;  ///< the player's effect 0x53
    /// The item's skills' descriptions (`skillDescription`'s second half).
    ItemSkillDescriber describe_skill;
};

/// `CGameUI::initTooltip(owner, item, tooltip, main)`, one
/// window's text; Linux inlines it in `CGameUI::showEquipmentTooltip`,
/// and the console's calls it per window.
EquipmentTooltip equipment_tooltip(const ItemStats& item, const EquipmentTooltipContext& context);

/// A details window's size going in and its place coming out, in pixels.
struct TooltipBox {
    bool shown = false;
    float width = 0.0f, height = 0.0f;
    float x = 0.0f, y = 0.0f;
};

/// Where the console puts the three windows, from the
/// anchor a tray hands over with the item (the selected row's
/// left and top), the screen's size and the interface ratio.
///
/// Everything stays inside the middle 90% of the screen. A tray is 300 x
/// ratio wide. An anchor left of `width / 2 - tray`: the item's window right of
/// the tray and the comparisons right of that. An anchor up to the middle: the
/// item's window right of the tray and the comparisons flush against the
/// tray's left. Anything further right: the item's window left of the tray and
/// the comparisons left of that. The two comparisons share a column, the second
/// 26 x ratio under the first.
void place_equipment_tooltips(TooltipBox& item, TooltipBox& first, TooltipBox& second,
                              unsigned anchor_x, unsigned anchor_y, int screen_width,
                              int screen_height, float ratio);

}  // namespace tl::core
