#include "core/tray_row.h"

#include "core/item_effects.h"

#include <cmath>

namespace tl::core {

const char* tray_backdrop_image(TrayBackdrop backdrop) {
    // The image table indexes, entries 0 to 5.
    switch (backdrop) {
        case TrayBackdrop::Gold: return "traycolor_gold";
        case TrayBackdrop::Blue: return "traycolor_blue";
        case TrayBackdrop::Green: return "traycolor_green";
        case TrayBackdrop::Purple: return "traycolor_purple";
        case TrayBackdrop::Red: return "traycolor_red";
        case TrayBackdrop::Blank: break;
    }
    return "traycolor_blank";
}

// PORT: presentation, the comparison arrow's image (entries 0xe and 0xf).
const char* tray_arrow_image(TrayArrow arrow) {
    // Entries 0xe and 0xf, spelt as the imageset spells them.
    switch (arrow) {
        case TrayArrow::Up: return "comparrison_arrow_up";
        case TrayArrow::Down: return "comparrison_arrow_down";
        case TrayArrow::None: break;
    }
    return "";
}

//
uint32_t item_effect_count(const ItemStats& item, const EffectManager::Context& effects) {
    uint32_t count = 0;
    // The damage types the copy carries (`+0x320`) whose bonus (`+0x330`) is
    // above 0.
    for (const int type : item.damage_types) {
        if (type < 0 || type >= static_cast<int>(item.damage_bonus.size())) continue;
        if (0 < item.damage_bonus[static_cast<size_t>(type)]) ++count;
    }
    // The manager's three lists (`+0x1c`, `+0x2c`, `+0x3c`).
    const EffectManager manager = item_effect_manager(item, effects);
    count += static_cast<uint32_t>(manager.effects(EffectActivation::Passive).size());
    count += static_cast<uint32_t>(manager.effects(EffectActivation::Dynamic).size());
    count += static_cast<uint32_t>(manager.effects(EffectActivation::Transfer).size());
    // Then every socketed item's (`+0x398`, count `+0x39c`).
    for (const ItemStats& socketed : item.socketed) count += item_effect_count(socketed, effects);
    return count;
}

// `CEquipment::DPS()`.
int item_dps(const ItemStats& item, const EffectManager::Context& effects) {
    const float speed = item.speed / 100.0f;
    float total = static_cast<float>(item.damage);
    for (int type = 0; type != 7; ++type) {
        total += static_cast<float>(weapon_damage_bonus(item, type, effects));
    }
    for (uint32_t socket = 0; socket < static_cast<uint32_t>(item.sockets); ++socket) {
        if (!(socket < item.socketed.size())) continue;
        const ItemStats& socketed = item.socketed[socket];
        total += static_cast<float>(socketed.damage);
        for (int type = 1; type != 7; ++type) {
            total += static_cast<float>(weapon_damage_bonus(socketed, type, effects));
        }
    }
    return static_cast<int>(std::ceil(total / (speed * 0.73333335f)));
}

namespace {

// the armour (`+0x30c`), one more with effects; 0 for an
// empty location.
uint32_t worn_armour_figure(const Inventory& players, int location,
                            const EffectManager::Context& effects) {
    if (!players.occupied(static_cast<size_t>(location))) return 0;
    const ItemStats& item = players.slots()[static_cast<size_t>(location)].item;
    return static_cast<uint32_t>(item_effect_count(item, effects) != 0) +
           static_cast<uint32_t>(item.graph_ac);
}

// a weapon's DPS in place of the armour.
uint32_t worn_hand_figure(const Inventory& players, int location,
                          const UnitTypeHierarchy& unit_types,
                          const EffectManager::Context& effects) {
    if (!players.occupied(static_cast<size_t>(location))) return 0;
    const ItemStats& item = players.slots()[static_cast<size_t>(location)].item;
    if (!item_isa(item, unit_type::kWeapon, unit_types)) {
        return worn_armour_figure(players, location, effects);
    }
    return static_cast<uint32_t>(item_dps(item, effects)) +
           static_cast<uint32_t>(item_effect_count(item, effects) != 0);
}

std::string number(long long value) { return std::to_string(value); }

}  // namespace

//
TrayWornFigures tray_worn_figures(const Inventory& players, const UnitTypeHierarchy& unit_types,
                                  const EffectManager::Context& effects) {
    TrayWornFigures figures{};
    figures[0] = worn_hand_figure(players, equip_location::kRightHand, unit_types, effects);
    figures[1] = worn_hand_figure(players, equip_location::kLeftHand, unit_types, effects);
    for (int location = 2; location <= 10; ++location) {
        figures[static_cast<size_t>(location)] = worn_armour_figure(players, location, effects);
    }
    figures[11] = worn_armour_figure(players, equip_location::kLeftHand, effects);
    return figures;
}

namespace {

// `SlotText`'s colour for an item the player's client owns.
LabelColour row_name_colour(const ItemStats& item, const TrayRowContext& context) {
    const UnitTypeHierarchy& types = *context.unit_types;
    // No player, or a gambler's: `FFd6d6d7`.
    if (!context.have_player || context.owner_is_gambler) {
        return LabelColour{1.0f, 0xd6 / 255.0f, 0xd6 / 255.0f, 0xd7 / 255.0f};
    }
    if (!item.identified) return LabelColour{1.0f, 1.0f, 0.0f, 0.0f};
    // The unselected five of `CGameGlobals`' label colours (`+0x570`..`+0x5e0`).
    static const ItemLabelColours kNone;
    const ItemLabelColours& colours = context.colours != nullptr ? *context.colours : kNone;
    // `getIsQuestUnit`.
    if (item_isa(item, unit_type::kQuestItem, types) || item.quest_guid != -1) {
        return colours.quest_unselected;
    }
    if (!item.set.empty()) return colours.set_unselected;
    if (item_isa(item, unit_type::kUnique, types)) return colours.unique_unselected;
    if (item_is_magical(item, types) || item_isa(item, unit_type::kRandomMagicSocketable, types)) {
        return item_isa(item, unit_type::kMagic, types) ? colours.rare_unselected
                                                        : colours.random_enchant_unselected;
    }
    return LabelColour{1.0f, 0.8f, 0.8f, 0.8f};
}

// the foot line.
void row_requirement(const ItemStats& item, const TrayRowContext& context, TrayRow& row) {
    const UnitTypeHierarchy& types = *context.unit_types;
    if (!context.have_player) return;
    if (item_isa(item, unit_type::kQuestItem, types)) {
        row.requirement = "(Quest Item)";
        if (context.colours != nullptr) row.requirement_colour = context.colours->quest;
        return;
    }
    if (context.owner_is_gambler) return;
    if (!context.merchant_open && !item.identified) {
        row.requirement = "Press A to identify";
        row.requirement_colour = LabelColour{1.0f, 1.0f, 0x36 / 255.0f, 0x36 / 255.0f};
        return;
    }
    // Level, strength, dexterity, magic, defense: the first one the player is
    // short of.
    const RequirementReductions* reductions = &context.reductions;
    const int level = item_level_requirement(item, reductions, types);
    bool short_of = 0 < level && context.level < level;
    if (!short_of) {
        const int strength = item_strength_requirement(item, reductions, types);
        short_of = 0 < strength && context.strength < strength;
    }
    if (!short_of) {
        const int dexterity = item_dexterity_requirement(item, reductions, types);
        short_of = 0 < dexterity && context.dexterity < dexterity;
    }
    if (!short_of) {
        const int magic = item_magic_requirement(item, reductions, types);
        short_of = 0 < magic && context.magic < magic;
    }
    if (!short_of) {
        const int defense = item_defense_requirement(item, reductions, types);
        short_of = 0 < defense && context.defense < defense;
    }
    if (!short_of) return;
    row.requirement = "Requirements not met";
    row.requirement_colour = LabelColour{1.0f, 1.0f, 0.0f, 0.0f};
}

}  // namespace

//
TrayRow tray_row(const ItemStats& item, int count, const TrayRowContext& context) {
    TrayRow row;
    if (context.unit_types == nullptr || context.effects == nullptr) return row;
    const UnitTypeHierarchy& types = *context.unit_types;
    const bool gambler = context.owner_is_gambler;

    if (!item.identified || gambler) {
        // Both lines "???", no arrow, and the square red - blank
        // on a gambler's tray, whose rows also hide the name.
        row.enchantments = "???";
        row.detail = "???";
        row.backdrop = gambler ? TrayBackdrop::Blank : TrayBackdrop::Red;
    } else {
        // "Effects:" and the count, for anything but a consumable. The count
        // the square is then chosen by is 0 for a row that printed it (the
        // register is cleared), so the quality squares are left
        // to the consumables that carry effects.
        uint32_t effects = item_effect_count(item, *context.effects);
        if (effects != 0 && !item_isa(item, unit_type::kConsumable, types)) {
            row.enchantments = "Effects:" + number(effects);
            effects = 0;
        }
        const bool wearable = item_isa(item, unit_type::kWeapon, types) ||
                              item_isa(item, unit_type::kArmor, types) ||
                              item_isa(item, unit_type::kTrinket, types);
        if (wearable && context.player_can_equip && !context.player_can_equip(item)) {
            row.backdrop = TrayBackdrop::Red;
        } else if (effects == 0) {
            row.backdrop = TrayBackdrop::Blank;
        } else if (!item.set.empty()) {
            row.backdrop = TrayBackdrop::Purple;
        } else if (item_isa(item, unit_type::kUnique, types)) {
            row.backdrop = TrayBackdrop::Gold;
        } else if (item_is_magical(item, types) ||
                   item_isa(item, unit_type::kRandomMagicSocketable, types)) {
            row.backdrop =
                item_isa(item, unit_type::kMagic, types) ? TrayBackdrop::Blue : TrayBackdrop::Green;
        } else {
            row.backdrop = TrayBackdrop::Blank;
        }

        // The line under the name and its arrow.
        const uint32_t has_effects = item_effect_count(item, *context.effects) != 0 ? 1u : 0u;
        if (item_isa(item, unit_type::kWeapon, types)) {
            const int dps = item_dps(item, *context.effects);
            row.detail = "DPS: " + number(dps);
            const uint32_t figure = static_cast<uint32_t>(dps) + has_effects;
            row.arrow = TrayArrow::Down;
            if (context.worn[0] <= figure && context.worn[1] <= figure) {
                row.arrow = (figure == context.worn[0] || figure == context.worn[1])
                                ? TrayArrow::None
                                : TrayArrow::Up;
            }
        } else if (item_isa(item, unit_type::kArmor, types)) {
            // The location the piece is compared at, by its kind.
            int location = -1;
            if (context.have_player) {
                if (item_isa(item, unit_type::kHelmet, types)) location = 3;
                else if (item_isa(item, unit_type::kGloves, types)) location = 2;
                else if (item_isa(item, unit_type::kChestArmor, types)) location = 4;
                else if (item_isa(item, unit_type::kShoulderArmor, types)) location = 5;
                else if (item_isa(item, unit_type::kBoots, types)) location = 6;
                else if (item_isa(item, unit_type::kBelt, types)) location = 7;
                else if (item_isa(item, unit_type::kShield, types)) location = 11;
            }
            row.detail = "armor " + number(item.graph_ac);
            const uint32_t figure = has_effects + static_cast<uint32_t>(item.graph_ac);
            if (location != -1) {
                const uint32_t worn = context.worn[static_cast<size_t>(location)];
                if (figure != worn) row.arrow = worn <= figure ? TrayArrow::Up : TrayArrow::Down;
            }
        } else if (1 < count) {
            row.detail = "x" + number(count);
        }
    }

    row.gambler_name = gambler;
    row.name_colour =
        gambler ? LabelColour{1.0f, 0.8f, 0.8f, 0.8f} : row_name_colour(item, context);
    row_requirement(item, context, row);
    return row;
}

float tray_row_alpha(bool selected, bool equipped) {
    // 1.0, 0.9 (0x3feccccc..), 0.6 (0x3fe33333..).
    if (selected) return 1.0f;
    return equipped ? 0.9f : 0.6f;
}

}  // namespace tl::core
