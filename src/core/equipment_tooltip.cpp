#include "core/equipment_tooltip.h"

#include "core/inventory.h"
#include "core/item_effects.h"
#include "core/item_sets.h"
#include "core/tray_row.h"

#include <cmath>
#include <vector>

namespace tl::core {

namespace {

// PORT: an integer as text.
std::string number(long long value) { return std::to_string(value); }

// PORT: a text's leading and trailing newlines taken off.
void trim_newlines(std::string& text) {
    while (!text.empty() && text.back() == '\n') text.pop_back();
    size_t lead = 0;
    while (lead < text.size() && text[lead] == '\n') ++lead;
    text.erase(0, lead);
}

/// `removeWhiteSpace` as
/// `getEquipmentEffects` uses it: the line breaks either side of a description
/// go.
std::string trimmed(std::string text) {
    trim_newlines(text);
    return text;
}

bool quest_unit(const ItemStats& item, const UnitTypeHierarchy& types) {
    // `CBaseUnit::getIsQuestUnit`.
    return item_isa(item, unit_type::kQuestItem, types) || item.quest_guid != -1;
}

// The unit types `getEquipmentType` names, as the ids its ISA calls pass.
constexpr uint32_t kSword = 0x0b, kBowKind = 0x24, kAxe = 0x2c, kMace = 0x59, kPolearm = 0x69,
                   kStaff = 0x3d, kPistolKind = 0x5a, kRifle = 0x74, kCrossbow = 0x6e, kWand = 0x62,
                   kPotionKind = 0x21, kScroll = 0x2a;

}  // namespace

// `CEquipment::getEquipmentType(bool)`.
std::string equipment_type(const ItemStats& item, bool with_quality,
                           const UnitTypeHierarchy& types) {
    std::string text;
    if (quest_unit(item, types)) text = "(Quest Item)";

    // What leads a weapon, an armour or a trinket.
    const auto lead = [&]() {
        if (!item.identified) {
            text += "Unidentified";
            text += " ";
        } else if (with_quality) {
            if (item_isa(item, unit_type::kUnique, types)) {
                text += "Unique";
                text += " ";
            } else if (item_isa(item, unit_type::kMagic, types)) {
                text += "Rare";
                text += " ";
            } else if (item_is_magical(item, types)) {
                text += "Enchanted";
                text += " ";
            }
        }
    };
    const auto isa = [&](uint32_t type) { return item_isa(item, type, types); };

    if (isa(unit_type::kWeapon)) {
        lead();
        if (isa(kSword)) text += "Sword";
        else if (isa(kBowKind)) text += "Bow";
        else if (isa(kAxe)) text += "Axe";
        else if (isa(kMace)) text += "Mace";
        else if (isa(kPolearm)) text += "Polearm";
        else if (isa(kStaff)) text += "Staff";
        else if (isa(kPistolKind)) text += "Pistol";
        else if (isa(kRifle)) text += "Rifle";
        else if (isa(kCrossbow)) text += "Crossbow";
        else if (isa(kWand)) text += "Wand";
        else text += "Weapon";
    } else if (isa(unit_type::kArmor)) {
        lead();
        if (isa(unit_type::kHelmet)) text += "Helmet";
        else if (isa(unit_type::kGloves)) text += "Gloves";
        else if (isa(unit_type::kBoots)) text += "Boots";
        else if (isa(unit_type::kBelt)) text += "Belt";
        else if (isa(unit_type::kChestArmor)) text += "Chest Armor";
        else if (isa(unit_type::kShoulderArmor)) text += "Shoulder Armor";
        else if (isa(unit_type::kShield)) text += "Shield";
        else text += "Armor";
    } else if (isa(unit_type::kSocketable)) {
        text = "Socketable";
    } else if (isa(unit_type::kTrinket)) {
        lead();
        text += "Trinket";
    } else if (isa(kPotionKind)) {
        text = "Potion";
    } else if (isa(kScroll)) {
        text = "Scroll";
    }
    return item_label_text(text);
}

// `CEquipment::getAttackSpeedString` over
// `KWeaponSpeedValues`.
std::string attack_speed_string(float speed_percent) {
    static constexpr float kThresholds[5] = {130.0f, 110.0f, 90.0f, 80.0f, 0.0f};
    static const char* const kWords[5] = {"Slowest Attack Speed", "Slow Attack Speed",
                                          "Average Attack Speed", "Fast Attack Speed",
                                          "Fastest Attack Speed"};
    for (int i = 0; i != 5; ++i) {
        if (kThresholds[i] <= speed_percent) return kWords[i];
    }
    return std::string();
}

// `CEquipment::getEquipmentStats()`.
std::string equipment_stats(const ItemStats& item, const UnitTypeHierarchy& types,
                            const EquipmentSetContext& set) {
    std::string text;
    if (item_isa(item, unit_type::kWeapon, types)) {
        // The copy's damage ends; the roll writes one figure to both
        // (`calculateCombatStats`), so the low end
        // printed is half the high, rounded up.
        const int low = item.damage;
        const int high = item.damage;
        if (0 < low && 0 < high) {
            if (low == high) {
                const int half = static_cast<int>(std::ceil(static_cast<float>(high) * 0.5f));
                text = std::string("Physical Damage") + ": " + number(half) + "-" + number(low);
            } else {
                text = std::string("Physical Damage") + ": " + number(low) + "-" + number(high);
            }
        }
        if (item.identified) {
            // The inherent damage of each type the copy carries (`+0x380`).
            for (const int type : item.damage_types) {
                if (type < 0 || type >= static_cast<int>(item.inherent_damage.size())) continue;
                const int amount = item.inherent_damage[static_cast<size_t>(type)];
                if (amount == 0) continue;
                const int half = static_cast<int>(std::ceil(static_cast<float>(amount) * 0.5f));
                if (!text.empty()) text += "\n";
                text += damage_type_display_string(static_cast<DamageType>(type)) + " " + "Damage" +
                        ": " + number(half) + "-" + number(amount);
            }
        }
        // The swing's speed (`+0x70` of the attack description, x 100).
        // the attack's `+0x7c` - `SPEED * 0.01f`, made
        // by `calculateCombatStats` - times 100.0f. `SPEED`
        // 80 comes back 79.999995 (0.8 lies on a float midpoint and rounds to
        // even), "Fastest" rather than "Fast".
        const float stored = item.speed * 0.01f;
        const std::string speed = attack_speed_string(stored * 100.0f);
        if (!speed.empty()) {
            if (!text.empty()) text += "\n";
            text += speed;
        }
    }
    if (item.sockets != 0) {
        if (!text.empty()) text += "\n";
        text += std::string("Sockets") + " " +
                number(static_cast<long long>(item.socketed.size())) + "/" + number(item.sockets);
    }

    // The set block: "Set : <name>" in the set colour, then one
    // line per bonus - those the pieces worn have earned in the set colour,
    // then the rest in the unselected one.
    if (!item.set.empty() && set.sets != nullptr) {
        const ItemSet* found = set.sets->get_set(item.set);
        if (found != nullptr) {
            // `CInventory::getSetCount` of
            // the item's inventory (`+0x240`), 0 for an item in none.
            const int worn = set.owner != nullptr ? set.owner->set_count(*found) : 0;
            static const ItemLabelColours kNoColours;
            const ItemLabelColours& colours = set.colours != nullptr ? *set.colours : kNoColours;
            static const EffectDisplayContext kNoDisplay;
            const EffectDisplayContext& display =
                set.display != nullptr ? *set.display : kNoDisplay;
            // `Set`, then the function's ": ".
            text = text + "\n\n|c" + label_colour_markup(colours.set) + "Set" + ": " +
                   found->display_name + "|u";
            const auto bonus_line = [&](const ItemSetBonus& bonus, const LabelColour& colour) {
                // `CAffix::getDisplayStats`
                // of the bonus, its `\n` escapes made line breaks.
                std::string stats = affix_display_stats(bonus.affix, display);
                size_t at = 0;
                while ((at = stats.find("\\n", at)) != std::string::npos)
                    stats.replace(at, 2, "\n");
                // on: a bonus's further lines each start again
                // with its count, `"  " + n + " pc : "` ('s " : ").
                const std::string again = "\n  " + number(bonus.count) + " " + "pc" + " : ";
                std::string lines;
                size_t from = 0;
                for (size_t nl; (nl = stats.find('\n', from)) != std::string::npos; from = nl + 1) {
                    lines += stats.substr(from, nl - from) + again;
                }
                lines += stats.substr(from);
                // `pc`.
                text = text + "\n  |c" + label_colour_markup(colour) + number(bonus.count) + " " +
                       "pc" + ": " + lines + "|u";
            };
            if (!(worn < 1)) {
                for (const ItemSetBonus& bonus : found->bonuses) {
                    if (bonus.count <= worn) bonus_line(bonus, colours.set);
                }
            }
            for (const ItemSetBonus& bonus : found->bonuses) {
                if (worn < bonus.count) bonus_line(bonus, colours.set_unselected);
            }
        }
    }
    return text;
}

// `CEffectManager::getVisualDescription`.
std::string item_visual_description(const ItemStats& item, EffectActivation activation, bool split,
                                    const EffectManager::Context& effects,
                                    const EffectDisplayContext& display,
                                    const UnitTypeHierarchy& types) {
    if (display.types == nullptr) return std::string();
    const EffectManager manager = item_effect_manager(item, effects);

    // `CEffect::getVisualCalculatedValues(-1, true)`
    // for each effect, merged into the entry
    // of the same kind.
    struct Entry {
        EffectDisplayValues values;
        const LiveEffect* effect = nullptr;
    };
    std::vector<Entry> entries;
    for (const LiveEffect& live : manager.effects(activation)) {
        EffectDisplayValues values;
        values.type = live.effect.type;
        values.duration = live.effect.duration;
        values.force_min = live.effect.values[2];
        values.force_max = live.effect.values[3];
        float low = live.value;
        float high = live.value;
        if (live.effect.duration != kDurationInstant && live.effect.duration != kDurationAlways &&
            effect_is_per_tick(*display.types, live.effect.type)) {
            values.per_tick = true;
            high = high * live.effect.duration * 0.016f;
            low = low * live.effect.duration * 0.016f;
        }
        values.min = low;
        values.max = high;

        bool merged = false;
        for (Entry& entry : entries) {
            if (entry.values.per_tick != values.per_tick || entry.values.type != values.type) {
                continue;
            }
            if (entry.effect->effect.damage_type != live.effect.damage_type) continue;
            if (values.type == 0x52 && entry.effect->effect.name != live.effect.name) continue;
            if (entry.values.duration < values.duration) entry.values.duration = values.duration;
            entry.values.min += values.min;
            entry.values.max += values.max;
            entry.values.force_min += values.force_min;
            entry.values.force_max += values.force_max;
            merged = true;
            break;
        }
        if (!merged) entries.push_back(Entry{values, &live});
    }

    std::string text;
    bool first = true;
    for (const Entry& entry : entries) {
        std::string line = effect_display_stats(entry.effect->effect, entry.values, display);
        if (activation == EffectActivation::Transfer) line.insert(0, std::string("Conveys") + " ");
        if (line.empty()) continue;
        if (split && !entry.effect->affix.empty()) {
            // The effect's affix (`+0xb8`) and `CAffix::canBeAppliedToUnitType`
            // for a weapon and for an armour.
            const Affix* affix = nullptr;
            for (const ItemStats::RolledAffix& rolled : item.affixes) {
                if (rolled.affix.name == entry.effect->affix) affix = &rolled.affix;
            }
            if (affix != nullptr) {
                const auto applies = [&](uint32_t type) {
                    for (const uint32_t allowed : affix->unit_types) {
                        if (types.isa(type, allowed)) return true;
                    }
                    return false;
                };
                const bool weapon = applies(unit_type::kWeapon);
                const bool armour = applies(unit_type::kArmor);
                if (!weapon || !armour) {
                    line.insert(0, std::string(weapon ? "Weapon" : "Armor/Trinkets") + ":\n     ");
                }
            }
        }
        if (first) {
            text = line;
        } else {
            text.append("\n").append(line);
        }
        first = false;
    }
    return text;
}

// `CEquipment::effectsDescription(EEFFECT_ACTIVATION, bool, bool)`.
//
std::string item_effects_description(const ItemStats& item, EffectActivation activation,
                                     bool no_split, bool socketed,
                                     const EffectManager::Context& effects,
                                     const EffectDisplayContext& display,
                                     const UnitTypeHierarchy& types,
                                     const LabelColour& socket_colour) {
    std::string text;
    if (!socketed) {
        if (activation == EffectActivation::Passive) {
            // "+n <type> Damage" for each damage bonus the copy carries.
            for (size_t i = 0; i < item.damage_types.size(); ++i) {
                const int type = item.damage_types[i];
                if (type < 0 || type >= static_cast<int>(item.damage_bonus.size())) continue;
                const int bonus = item.damage_bonus[static_cast<size_t>(type)];
                if (!(0 < bonus)) continue;
                text.append("+")
                    .append(number(bonus))
                    .append(" ")
                    .append(damage_type_display_string(static_cast<DamageType>(type)))
                    .append(" ")
                    .append("Damage");
                if (i != item.damage_types.size() - 1) text += "\n";
            }
        }
        // A gem's own lines say what it does in a weapon and in an armour.
        const bool split = !no_split && item_isa(item, unit_type::kSocketable, types) &&
                           !item_isa(item, unit_type::kRandomMagicSocketable, types);
        const std::string visual =
            item_visual_description(item, activation, split, effects, display, types);
        if (!visual.empty()) {
            if (text.empty()) {
                text = visual;
            } else {
                text = text + "\n" + visual;
            }
        }
    } else {
        std::string gems;
        for (const ItemStats& gem : item.socketed) {
            const std::string line = item_effects_description(gem, activation, /*no_split=*/true,
                                                              /*socketed=*/false, effects, display,
                                                              types, socket_colour);
            if (!line.empty()) {
                if (!gems.empty() && gems.back() != '\n') gems += "\n";
                gems += line;
            }
            if (!gems.empty() && gems.back() == '\n') gems.pop_back();
        }
        if (!gems.empty()) {
            if (!text.empty() && text.back() != '\n') text += "\n";
            // The gems' lines in `SOCKET_COLOR` (`CGameGlobals +0x2b8`).
            text = text + "|c" + label_colour_markup(socket_colour) + gems + "|u";
        }
    }
    if (!text.empty() && text.back() != '\n') text += "\n";
    return text;
}

// `CEquipment::skillDescription()`.
std::string equipment_skill_description(const ItemStats& item, const ItemSkillDescriber& describe) {
    std::string text;
    // Each `[SKILL_TO_GIVE]`: "<name> Level: <level>", a line break before all
    // but the first.
    for (size_t i = 0; i < item.skills_to_give.size(); ++i) {
        const ItemStats::SkillToGive& give = item.skills_to_give[i];
        if (i != 0) text += "\n";
        text += give.name + " " + "Level" + ": " + std::to_string(give.level);
    }
    // Then the item's skill manager (X360 `+0x184`), each skill enabled and
    // not hidden described in turn - and the line break goes by the skill's
    // place in the manager, not by whether anything came before it.
    if (!describe) return text;
    for (size_t i = 0; i < item.skills.size(); ++i) {
        std::string description;
        if (!describe(item.skills[i], description)) continue;
        if (i != 0) text += "\n";
        text += description;
    }
    return text;
}

// `CEquipment::getEquipmentEffects()`.
std::string equipment_effects(const ItemStats& item, const EffectManager::Context& effects,
                              const EffectDisplayContext& display, const UnitTypeHierarchy& types,
                              const LabelColour& socket_colour,
                              const ItemSkillDescriber& describe_skill) {
    if (!item.identified) return std::string();
    static constexpr EffectActivation kOrder[3] = {
        EffectActivation::Dynamic, EffectActivation::Passive, EffectActivation::Transfer};
    std::string text;
    for (const bool socketed : {false, true}) {
        for (const EffectActivation activation : kOrder) {
            const std::string part =
                trimmed(item_effects_description(item, activation, /*no_split=*/false, socketed,
                                                 effects, display, types, socket_colour));
            text = trimmed(text);
            text = trimmed(text.append("\n").append(part));
        }
    }
    // the skills the same way, and a closing line
    // break on whatever was said.
    text = trimmed(text);
    text = trimmed(text + "\n" + trimmed(equipment_skill_description(item, describe_skill)));
    if (!text.empty()) text += "\n";
    return text;
}

// `CGameUI::initTooltip(owner, item, tooltip, main)`, one
// window's text; Linux inlines it in `CGameUI::showEquipmentTooltip`,
// and the console's calls it per window.
EquipmentTooltip equipment_tooltip(const ItemStats& item, const EquipmentTooltipContext& context) {
    EquipmentTooltip tip;
    if (context.unit_types == nullptr || context.effects == nullptr) return tip;
    const UnitTypeHierarchy& types = *context.unit_types;
    static const ItemLabelColours kNoColours;
    const ItemLabelColours& colours = context.colours != nullptr ? *context.colours : kNoColours;
    static const EffectDisplayContext kNoDisplay;
    const EffectDisplayContext& display =
        context.display != nullptr ? *context.display : kNoDisplay;

    // A gambler's wares say nothing of themselves.
    const bool gambler = !context.owner_is_player && context.owner_is_gambler;
    // A magical copy not yet identified.
    const bool hidden = item_is_magical(item, types) && !item.identified;
    const bool weapon = item_isa(item, unit_type::kWeapon, types);
    const bool armour = item_isa(item, unit_type::kArmor, types);

    tip.name.shown = true;
    tip.name.text = gambler ? std::string("???") : context.full_name;
    // A comparison's name past 24 bytes (UTF-8) is cut there and "..."
    // added.
    if (context.comparison && tip.name.text.size() > 24)
        tip.name.text = tip.name.text.substr(0, 24) + "...";

    // The type line; a comparison's says where the item is worn.
    std::string hand;
    if ((item_isa(item, unit_type::kRing, types) || weapon ||
         item_isa(item, unit_type::kShield, types)) &&
        context.comparison) {
        hand =
            (context.worn_location == 9 || context.worn_location == 1) ? "left hand" : "right hand";
    }
    const std::string type = equipment_type(item, !gambler, types);
    tip.type.shown = true;
    if (!context.comparison) {
        tip.type.text = type;
    } else if (hand.empty()) {
        tip.type.text = std::string("(") + "equipped" + ") " + type;
    } else {
        tip.type.text = std::string("(") + "equipped" + "-" + hand + ") " + type;
    }

    // Both lines take the item's label colour (`CGameGlobals +0x268`..`+0x288`).
    LabelColour tint{1.0f, 1.0f, 1.0f, 1.0f};
    if (quest_unit(item, types)) {
        tint = colours.quest;
    } else if (gambler || (!item_is_magical(item, types) &&
                           !item_isa(item, unit_type::kRandomMagicSocketable, types))) {
        tint = LabelColour{1.0f, 1.0f, 1.0f, 1.0f};
    } else if (!item.set.empty()) {
        tint = colours.set;
    } else if (item_isa(item, unit_type::kUnique, types)) {
        tint = colours.unique;
    } else if (item_isa(item, unit_type::kMagic, types)) {
        tint = colours.rare;
    } else {
        tint = colours.random_enchant;
    }
    tip.name.colour = tint;
    tip.type.colour = tint;

    // The big figure and its caption.
    if (!gambler && weapon) {
        tip.big.shown = true;
        tip.big.text = hidden ? std::string("??") : number(item_dps(item, *context.effects));
        tip.big_label.shown = true;
        tip.big_label.text = "damage per second";
    } else if (!gambler && armour) {
        tip.big.shown = true;
        tip.big.text = hidden ? std::string("??") : number(item.graph_ac);
        tip.big_label.shown = true;
        tip.big_label.text = "armor";
    }

    // The stats; a weapon's are led by its handedness.
    EquipmentSetContext set;
    set.sets = context.sets;
    set.owner = context.owner_inventory;
    set.display = context.display;
    set.colours = context.colours;
    std::string stats = (gambler || hidden) ? std::string() : equipment_stats(item, types, set);
    if (weapon) {
        stats = std::string("(") +
                (item_isa(item, unit_type::kTwoHanded, types) ? "2-handed" : "1-handed") + ")\n" +
                stats;
    }
    tip.stats.shown = true;
    tip.stats.text = stats;
    tip.stats.colour = LabelColour{1.0f, 0xd6 / 255.0f, 0xd6 / 255.0f, 0xd7 / 255.0f};

    // The effects.
    tip.effects.text = gambler ? std::string("????\n")
                               : equipment_effects(item, *context.effects, display, types,
                                                   colours.socket, context.describe_skill);
    tip.effects.shown = !tip.effects.text.empty();
    tip.effects.colour = LabelColour{1.0f, 0x64 / 255.0f, 0xbc / 255.0f, 1.0f};

    // The five requirements: white when met, red when not.
    const RequirementReductions* reductions = &context.reductions;
    const int required[5] = {item_level_requirement(item, reductions, types),
                             item_strength_requirement(item, reductions, types),
                             item_dexterity_requirement(item, reductions, types),
                             item_magic_requirement(item, reductions, types),
                             item_defense_requirement(item, reductions, types)};
    const int has[5] = {context.level, context.strength, context.dexterity, context.magic,
                        context.defense};
    static const char* const kCaptions[5] = {"Requires Level", "requires Strength",
                                             "requires Dexterity", "requires Magic",
                                             "requires Defense"};
    for (size_t i = 0; i < 5; ++i) {
        TooltipText& line = tip.requirements[i];
        // On the main window only (`main`), as the description.
        if (context.comparison) continue;
        if (!((gambler || !hidden) && 0 < required[i])) continue;
        line.shown = true;
        line.text =
            std::string(kCaptions[i]) + " " + (gambler ? std::string("???") : number(required[i]));
        line.colour = (gambler || required[i] <= has[i]) ? LabelColour{1.0f, 1.0f, 1.0f, 1.0f}
                                                         : LabelColour{1.0f, 1.0f, 0.0f, 0.0f};
    }

    // The description: what an unidentified copy says, else the flavour text.
    // the flavour on the main window when the owner is a gambler or the
    // copy is not hidden and it says something; else, on any window, the unidentified
    // copy's message.
    const bool flavour = !context.comparison && (gambler || !hidden) && !item.description.empty();
    if (!flavour && hidden) {
        tip.description.shown = true;
        // The two translated strings joined as they are, with
        // no line break between them (as on the console; the window wraps).
        tip.description.text = std::string("Unidentified items cannot be used-") +
                               "use an Identify Scroll to reveal enchantments";
        tip.description.colour = LabelColour{1.0f, 1.0f, 0x36 / 255.0f, 0x36 / 255.0f};
    } else if (flavour) {
        // `CEquipment::getFlavorDescription`, its `\n`
        // escapes made line breaks.
        std::string flavour = item.description;
        size_t at = 0;
        while ((at = flavour.find("\\n", at)) != std::string::npos) flavour.replace(at, 2, "\n");
        tip.description.shown = true;
        tip.description.text = flavour;
        tip.description.colour = LabelColour{1.0f, 0xd6 / 255.0f, 0xd6 / 255.0f, 0xd7 / 255.0f};
    }

    // The price, on the window that is not a comparison.
    if (!context.comparison) {
        static constexpr LabelColour kPrice{1.0f, 1.0f, 0xeb / 255.0f, 0x9a / 255.0f};
        const bool quest = item_isa(item, unit_type::kQuestItem, types);
        if (context.owner_is_player || quest || !context.owner_is_merchant) {
            if (context.merchant_open && !quest) {
                tip.price.shown = true;
                tip.price.text =
                    std::string("sell price") + " :" +
                    number(item_sell_price_of(item, context.count, context.price_bonus));
                tip.price.colour = kPrice;
            }
        } else {
            const int price = item_buy_price_of(item, context.count, context.price_bonus);
            tip.price.shown = true;
            tip.price.text = std::string("price") + " :" + number(price);
            tip.price.colour = context.gold < price ? LabelColour{1.0f, 1.0f, 0.5f, 0.5f} : kPrice;
        }
    }
    return tip;
}

//
void place_equipment_tooltips(TooltipBox& item, TooltipBox& first, TooltipBox& second,
                              unsigned anchor_x, unsigned anchor_y, int screen_width,
                              int screen_height, float ratio) {
    const unsigned safe_width = static_cast<unsigned>(static_cast<float>(screen_width) * 0.9f);
    const unsigned safe_height = static_cast<unsigned>(static_cast<float>(screen_height) * 0.9f);
    const unsigned margin_x = static_cast<unsigned>(
        (static_cast<float>(screen_width) - static_cast<float>(safe_width)) * 0.5f);
    const unsigned margin_y = static_cast<unsigned>(
        (static_cast<float>(screen_height) - static_cast<float>(safe_height)) * 0.5f);
    const float gap30 = ratio * 30.0f;
    const float gap26 = ratio * 26.0f;
    const float item_span = item.width + gap30;
    const float ax = static_cast<float>(anchor_x);
    const float ay = static_cast<float>(anchor_y);
    const float left = static_cast<float>(margin_x);
    const float right = static_cast<float>(safe_width + margin_x);
    const unsigned tray = static_cast<unsigned>(static_cast<long long>(ratio * 330.0f - gap30));
    const float tray_width = static_cast<float>(tray);
    const unsigned half =
        static_cast<unsigned>(static_cast<long long>(static_cast<double>(screen_width) * 0.5));

    float item_x = 0.0f, first_x = ax, second_x = ax;
    if (ax < static_cast<float>(static_cast<unsigned>(static_cast<long long>(
                 static_cast<double>(screen_width) * 0.5 - static_cast<double>(tray_width))))) {
        // The left tray: everything to its right.
        item_x = (tray_width + gap26) + ax;
        if (first.shown) {
            int shift = 0;
            first_x = (gap26 + item_x) + item_span;
            if (second.shown) {
                second_x = first_x;
                if (static_cast<float>(safe_width) < second.width + first_x) {
                    shift =
                        static_cast<int>((static_cast<float>(safe_width) - first_x) - second.width);
                }
            }
            if (right < first.width + first_x) {
                const float back = (right - first_x) - first.width;
                if (back < static_cast<float>(shift)) shift = static_cast<int>(back);
            }
            item_x += static_cast<float>(shift);
            first_x += static_cast<float>(shift);
            second_x += static_cast<float>(shift);
            if (item_x < left) item_x = left;
        }
    } else if (ax <= static_cast<float>(half)) {
        // The middle tray: the item's window to its right, the comparisons
        // against its left.
        item_x = (tray_width + gap26) + ax;
        if (right < item_x + item_span) item_x = right - item_span;
        if (first.shown) {
            int shift = 0;
            first_x = ax - first.width;
            if (first_x < left) shift = static_cast<int>(left - first_x);
            if (second.shown) {
                second_x = ax - second.width;
                if (second_x < left && static_cast<float>(shift) < left - second_x) {
                    shift = static_cast<int>(left - second_x);
                }
            }
            first_x += static_cast<float>(shift);
            second_x += static_cast<float>(shift);
        }
    } else {
        // The right tray: everything to its left.
        item_x = (ax - item_span) + gap26;
        if (item_x < left) item_x = left;
        if (first.shown) {
            int shift = 0;
            first_x = item_x - ((first.width + gap26) + gap30);
            if (second.shown) {
                second_x = item_x - ((second.width + gap26) + gap30);
                if (second_x < left) shift = static_cast<int>(left - second_x);
            }
            if (first_x < left && static_cast<float>(shift) < left - first_x) {
                shift = static_cast<int>(left - first_x);
            }
            first_x += static_cast<float>(shift);
            second_x += static_cast<float>(shift);
            item_x += static_cast<float>(shift);
        }
        if (right < item_x + item_span) item_x = right - item_span;
    }

    // Down the screen: the comparisons one under the other from the anchor,
    // pulled up when they would leave the safe area.
    const float top = static_cast<float>(margin_y);
    const float bottom = static_cast<float>(safe_height + margin_y);
    float item_y = ay, first_y = ay, second_y = ay;
    if (first.shown) {
        float total = first.height;
        if (second.shown) {
            second_y = (first.height + gap26) + ay;
            total = (second.height + first.height) + gap26;
        }
        if (bottom < total + ay) {
            first_y = bottom - total;
            second_y = second_y - (ay - first_y);
        }
        if (first_y < top) {
            second_y = (second_y - first_y) + top;
            first_y = top;
        }
    }
    if (bottom < item_y + item.height) item_y = bottom - item.height;
    if (item_y - gap26 < top) item_y = gap26 + top;

    item.x = item_x;
    item.y = item_y;
    first.x = first_x;
    first.y = first_y;
    second.x = second_x;
    second.y = second_y;
}

}  // namespace tl::core
