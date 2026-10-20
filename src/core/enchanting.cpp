#include "core/enchanting.h"

#include "core/affix_book.h"
#include "core/combat.h"
#include "core/effect_manager.h"
#include "core/item_effects.h"
#include "core/item_roll.h"
#include "core/retirement.h"
#include "core/unit_types.h"

#include <algorithm>
#include <cmath>

namespace tl::core {

float disenchant_chance(uint32_t enchant_count, float chance, float increase, float max_chance) {
    // `CEnchantMenu::performInteraction` (the
    // shrine's half, the pane): the zero-extended count
    // through `fcfid`/`frsp`, then one `fmadds`. `MAX` is taken only when below
    // the chance, so a NaN on either side leaves the chance.
    const float chance_now = std::fma(increase, static_cast<float>(enchant_count), chance);
    return max_chance < chance_now ? max_chance : chance_now;
}

// The enchanter's disenchant chance (the console's pane shows it):
// `disenchant_chance` with the enchanter's globals.
float enchanter_disenchant_chance(const ItemStats& item, const ItemRollGlobals& globals) {
    return disenchant_chance(item.enchant_count, globals.enchanter_disenchant_chance,
                             globals.enchanter_disenchant_increase,
                             globals.enchanter_max_disenchant_chance);
}

bool item_has_effect_manager(const ItemStats& item) {
    // `+0x17c`, made by the first effect added: an affix, an authored effect,
    // the block chance, `calculateCombatStats`' elemental armour effects
    // (`combat_effects`), and one an affix kept dead after
    // `createElementalDamages` - the manager outlives what it held.
    const bool common = !item.affixes.empty() || !item.combat_effects.empty() ||
                        !item.dead_affix_effects.empty() || item.emptied_effect_manager;
    if (item.from_save) {
        // `applySaveState`'s `addNewEffect` makes it for a saved effect.
        return common || !item.saved_effects.empty();
    }
    return common || !item.effects.empty() || item.block_chance != 0;
}

namespace {

// The console's words (looked up by their
// English text), which are `GLOBALS.DAT`'s `THEENCHANTERPERFORMS` & co.
constexpr const char* kEnchanterPerforms = "The enchanter performs the ancient rites on the";
constexpr const char* kYouPlunge = "You plunge the";
constexpr const char* kIntoTheLight = "into the light";
constexpr const char* kNothingHappens = "but nothing happens";
constexpr const char* kCreatesSockets = "and creates new sockets";
constexpr const char* kAddsEnchantments = "and adds new enchantments";
constexpr const char* kRemovesEnchantments = "and accidentally removes all enchantments";

// "<performs> <item> ... <outcome>!". says "<plunge>
// <item> into the light ... <outcome>!", except that its disenchant line has
// no "into the light" (kept).
std::string enchant_line(EnchantService service, const std::string& name, const char* outcome,
                         bool into_the_light) {
    std::string line = service == EnchantService::Enchanter ? kEnchanterPerforms : kYouPlunge;
    line += " ";
    line += name;
    if (service == EnchantService::Shrine && into_the_light) {
        line += " ";
        line += kIntoTheLight;
    }
    line += " ... ";
    line += outcome;
    line += "!";
    return line;
}

}  // namespace

EnchantOutcome perform_enchant(EnchantService service, ItemStats& item, int& gold,
                               uint32_t player_level, const std::string& name,
                               const ItemRollContext& context) {
    // `CEnchantMenuXenon::attemptToEnchantAtMerchant` and
    // `attemptToEnchantAtShrine`; on the PC
    // `CEnchantMenu::performInteraction`
    // cases 0x15 and 0x16. The slot roll comes first.
    RunicRandom& rolls = *context.volatile_rng;
    const ItemRollGlobals& globals = *context.globals;
    const int slots = rolls.integer_between(0, 1);
    const bool enchanter = service == EnchantService::Enchanter;

    EnchantOutcome out;
    if (enchanter) {
        // Case 0x15: the price against the player's gold (+0x444), then
        // `giveGold(-price)` and `GOLDBUY` (the caller's).
        const int price = item_enchant_price(item, *context.graphs, globals);
        if (price > gold) return out;
        gold -= price;
        out.price = price;
    }

    const float socket_chance =
        enchanter ? globals.enchanter_socket_chance : globals.shrine_socket_chance;
    const float magic_chance =
        enchanter ? globals.enchanter_magic_chance : globals.shrine_magic_chance;
    const float chance =
        enchanter ? enchanter_disenchant_chance(item, globals)
                  : disenchant_chance(item.enchant_count, globals.shrine_disenchant_chance,
                                      globals.shrine_disenchant_increase,
                                      globals.shrine_max_disenchant_chance);
    out.result = EnchantResult::NothingHappens;
    out.message = enchant_line(service, name, kNothingHappens, true);

    const float disenchant_roll = rolls.between(0.0f, 1000.0f);
    if (chance * 10.0f < disenchant_roll || !item_has_effect_manager(item)) {
        bool socketed = false;
        if (item.sockets < item_max_sockets(item)) {
            const float socket_roll = rolls.between(0.0f, 1000.0f);
            if (socket_roll < socket_chance * 10.0f) {
                add_sockets(item, context);
                ++item.enchant_count;
                out.result = EnchantResult::Sockets;
                out.message = enchant_line(service, name, kCreatesSockets, true);
                socketed = true;
            }
        }
        if (!socketed) {
            const float magic_roll = rolls.between(0.0f, 1000.0f);
            if (magic_chance * 10.0f >= magic_roll) {
                // Case 0x15 reads the item's level, case 0x16 the player's; either
                // is at least 4 (`3 <` unsigned).
                const uint32_t own = enchanter ? static_cast<uint32_t>(item.level) : player_level;
                const int level = own > 3u ? static_cast<int>(own) : 4;
                add_enchant(item, level, slots + 1, slots + 2, context);
                ++item.enchant_count;
                out.result = EnchantResult::Enchanted;
                out.message = enchant_line(service, name, kAddsEnchantments, true);
            }
        }
    } else {
        // `clearOutAffixEffects` and
        // `clearEffects(true)` on the item's
        // manager: every affix deleted, every effect off the lists but type 0x3e
        // (`block_chance`). `clearDamageBonuses`
        // then empties the damage lists and runs
        // `createElementalDamages`. `CAffix::clearEffectsFromOwner`
        // first rolls each affix's effects again
        // (`calculateBaseValue(0)`): a `g_RandVolatile` draw per
        // effect with a range.
        if (context.graphs != nullptr) {
            for (const ItemStats::RolledAffix& rolled : item.affixes) {
                // The draws are what is kept: the dead effects' among them, in
                // whatever order (each with a range draws once).
                for (const Effect& effect : rolled.affix.effects) {
                    effect_base_value(effect, EffectCalculate::Roll, *context.graphs, EffectOwner{},
                                      &rolls);
                }
            }
            for (const Effect& effect : item.dead_affix_effects) {
                effect_base_value(effect, EffectCalculate::Roll, *context.graphs, EffectOwner{},
                                  &rolls);
            }
        }
        item.dead_affix_effects.clear();
        item.affixes.clear();
        const auto not_block_chance = [](const Effect& effect) {
            return effect.type != kEffectTypePercentBlockChanceBase;
        };
        item.effects.erase(
            std::remove_if(item.effects.begin(), item.effects.end(), not_block_chance),
            item.effects.end());
        // The armour's elemental passives `calculateCombatStats` put on the same
        // manager go with them (on the console an armour's list 0 is empty after).
        item.combat_effects.erase(std::remove_if(item.combat_effects.begin(),
                                                 item.combat_effects.end(), not_block_chance),
                                  item.combat_effects.end());
        // A loaded copy's lists are its saved effects, and the affixes it was
        // enchanted with before the save are among them as plain effects.
        const auto saved_not_block_chance = [](const ItemStats::SavedEffect& saved) {
            return saved.effect.type != kEffectTypePercentBlockChanceBase;
        };
        item.saved_effects.erase(std::remove_if(item.saved_effects.begin(),
                                                item.saved_effects.end(), saved_not_block_chance),
                                 item.saved_effects.end());
        item.emptied_effect_manager = true;
        // `clearDamageBonuses`.
        if (context.effects != nullptr) {
            clear_damage_bonuses(item, *context.effects, *context.unit_types);
        }
        item.enchant_count = 0;
        // The enchanter only: a wand (ISA 0x62) has its damage
        // rolled again, `CEquipment::recalculateInherentDamages`.
        if (enchanter && context.graphs != nullptr && context.unit_types != nullptr &&
            context.unit_types->isa(item.unittype, unit_type::kWand)) {
            roll_weapon_damage(item, context.unit_types->isa(item.unittype, unit_type::kWeapon),
                               *context.graphs, rolls, item.heirloom_level);
        }
        out.result = EnchantResult::Disenchanted;
        out.message = enchant_line(service, name, kRemovesEnchantments, false);
    }
    // Every case: the identified flag set (X360 +0x31c), then
    // `destroyItemText` (the caller's).
    item.identified = true;
    return out;
}

bool item_has_gems(const ItemStats& item) {
    // the tray's selected item and the menu present, and the
    // item's `+0x39c` non-zero.
    return !item.socketed.empty();
}

std::string destroy_socketed_gems(ItemStats& item, const std::string& name) {
    // "The gems within the", the name, "are
    // destroyed", "!"; then deletes every
    // gem. The PC's case 0x19 is the same words (`g_TheGemsWithin`).
    item.socketed.clear();
    return "The gems within the " + name + " are destroyed!";
}

std::vector<ItemStats> take_out_socketed_gems(ItemStats& item, const ItemRollContext& context) {
    // while `+0x39c` > 0, the list's first gem is taken out
    // (the slot it was in gets the last one, the count drops by
    // one); the PC's own loop in case 0x1a writes the same swap by hand.
    std::vector<ItemStats> gems;
    while (!item.socketed.empty()) {
        ItemStats gem = std::move(item.socketed.front());
        item.socketed.front() = std::move(item.socketed.back());
        item.socketed.pop_back();
        if (!item_isa(gem, unit_type::kRandomMagicSocketable, *context.unit_types)) {
            // `CBaseUnit::reapplyEffects(true)`
            // re-creates the `[EFFECT]` blocks the gem lacks
            // (`CEffectManager::createEffect`); the
            // socket took none (`addContainerItem`
            // removes affixes only; `ItemStats::effects` is those blocks).
            // `reapplyAffixes(true)`
            // puts back the affixes
            // `removeAffixesThatDontSupportUnitType` took.
            reapply_affixes(gem, *context.affixes, true, item_effect_rolls(context));
            // `CEquipment::improveHeirloom` for a
            // gem whose heirloom level (`+0x22c`) is above 0.
            if (gem.heirloom_level > 0 && context.effects != nullptr) {
                improve_heirloom(gem, context, *context.effects);
            }
        }
        gems.push_back(std::move(gem));
    }
    return gems;
}

std::string gems_recovered_line() {
    // + "!", the PC's
    // `g_GemsRecovered` the same.
    return "The item is destroyed, recovering the gems within!";
}

bool item_takes_gem(const ItemStats& item, const UnitTypeHierarchy& unit_types) {
    // the branch for a tray whose `+0x180` is not 0: `ISA(item,
    // +0x180)` and, with `+0x184` set, `+0x31c` and `+0x394 != +0x39c`.
    return item_isa(item, unit_type::kItem, unit_types) && item.identified &&
           static_cast<uint32_t>(item.sockets) != static_cast<uint32_t>(item.socketed.size());
}

}  // namespace tl::core
