#include "core/character.h"

#include "core/combat.h"
#include "core/death.h"

#include "core/effect_manager.h"
#include "core/inventory.h"
#include "core/item_roll.h"
#include "core/weapons.h"
#include "formats/float_convert.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace tl::core {

namespace {

// PORT: ASCII upper-casing.
std::string upper_of(const std::string& in) {
    std::string out = in;
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

}  // namespace

int item_max_damage(const ItemStats& item, const StatBook& graphs) {
    if (weapon_damage_kind(item.unittype) == WeaponDamageKind::None) return 0;
    const float base = graphs.value(kWeaponDamageGraph, static_cast<float>(item.level), 0.0f);
    if (!(base > 0.0f)) return 0;
    // `CEquipment::setGraphDamage` of the
    // authored top, unrolled: `ceilf((points * curve) * 0.01f)`.
    const int maximum = static_cast<int>(std::ceil((item.max_damage * base) * 0.01f));
    return maximum > 0 ? maximum : 0;
}

uint64_t monster_experience(int monster_level, int monster_xp_stat, bool is_champion,
                            int difficulty, const StatBook& graphs) {
    // `CCharacter::setLevel`,
    // and `CCharacter::makeChampion`:
    // a non-zero `XP` becomes `(int)((curve * curve)
    // * 0.01f)` of the difficulty's curve at the unit's level.
    if (monster_xp_stat == 0) return 0;
    const std::string curve_name =
        std::string(is_champion ? kExperienceChampionMonsterGraph : kExperienceMonsterGraph) +
        difficulty_suffix(difficulty);
    const float curve = graphs.value(curve_name, static_cast<float>(monster_level), 0.0f);
    const int xp = tl::formats::ppc_fctiwz((curve * curve) * 0.01f);
    return xp > 0 ? static_cast<uint64_t>(xp) : 0;
}

// INVENTED: an item's quality by the first word of its `UNITTYPE` (`UNIQUE`,
// `MAGIC`, `RANDOMMAGIC`); the console's own test in
// `CEquipment::recalculatePrice` is not transcribed here.
ItemQuality item_quality(const ItemStats& item) {
    const std::string type = upper_of(item.unittype);
    if (type.rfind("UNIQUE", 0) == 0) return ItemQuality::Unique;
    if (type.rfind("MAGIC", 0) == 0 || type.rfind("RANDOMMAGIC", 0) == 0) {
        return ItemQuality::Magic;
    }
    return ItemQuality::Normal;
}

int item_sell_price(const ItemStats& item, const StatBook& graphs, int count) {
    // `CEquipment::recalculatePrice` then
    // `sellPrice`, with the quality test
    // described in the header.
    ItemStats copy = item;
    if (copy.value == 0) copy.value = 100;
    const ItemQuality quality = item_quality(item);
    set_item_prices(copy, graphs, quality == ItemQuality::Unique,
                    quality == ItemQuality::Magic || !item.affixes.empty(), false);
    return item_sell_price_of(copy, count, nullptr);
}

ClassProfile read_class_profile(const tl::formats::AdmSection& unit) {
    // CPlayer::unitInit: the attributes by
    // `CDataGroup::GetDataValue(group, name)` and the value's
    // word, the curve names by the defaulted getter.
    //
    ClassProfile profile;
    const auto word = [&unit](const char* key, int fallback) {
        const tl::formats::AdmRecord* record = unit.find(key);
        return record != nullptr ? static_cast<int>(record->word_as_integer()) : fallback;
    };
    const auto text = [&unit](const char* key, const char* fallback) {
        const tl::formats::AdmRecord* record = unit.find(key);
        if (record == nullptr) return std::string(fallback);
        return (record->type == tl::formats::AdmType::String ||
                record->type == tl::formats::AdmType::Translate)
                   ? record->text
                   : std::string();
    };
    profile.strength = word("STRENGTH", 10);
    profile.dexterity = word("DEXTERITY", 10);
    profile.magic = word("MAGIC", 10);
    profile.defense = word("DEFENSE", 10);
    profile.mana_graph = text("MANA_GRAPH", "MANA_PLAYER_DESTROYER");
    profile.health_graph = text("HEALTH_GRAPH", "HEALTH_PLAYER_DESTROYER");
    profile.stat_points_graph = text("STAT_POINTS_PER_LEVEL", kStatPointsPerLevelGraph);
    profile.skill_points_graph = text("SKILL_POINTS_PER_LEVEL", kSkillPointsPerLevelGraph);
    profile.fame_skill_points_graph =
        text("SKILL_POINTS_PER_FAME_LEVEL", kSkillPointsPerFameLevelGraph);
    // CPlayer::calculateNaturalArmor: `ARMOR`'s word, 0 when
    // not there.
    profile.armor = word("ARMOR", 0);
    return profile;
}

// INVENTED: a new character's sheet - level and fame 1, nothing spent,
// "Unrecognized" as its title, full health and mana; the console's character
// creation is not transcribed for it.
void CharacterProgression::init(const ClassProfile& profile, const StatBook& graphs,
                                const std::string& character_name) {
    profile_ = profile;
    name_ = character_name.empty() ? profile_.name : character_name;
    title_ = "Unrecognized";

    level_ = 1;
    experience_ = 0;
    fame_level_ = 1;
    fame_ = 0;
    unspent_stat_points_ = 0;
    unspent_skill_points_ = 0;
    for (int i = 0; i < 4; ++i) spent_[i] = 0;
    journal_ = JournalStatistics{};
    tips_seen_ = TipsSeen{};

    resample(graphs);
    hp_ = max_hp_;
    mp_ = max_mp_;
}

// PORT: the sheet's fields gathered for the save file.
CharacterProgression::Saved CharacterProgression::saved() const {
    Saved out;
    out.level = level_;
    out.experience = static_cast<uint64_t>(std::max(0, experience_));
    out.fame = static_cast<uint64_t>(std::max(0, fame_));
    out.fame_level = fame_level_;
    out.hp = hp_;
    out.mp = mp_;
    out.strength = strength();
    out.dexterity = dexterity();
    out.defense = defense();
    out.magic = magic();
    out.unspent_stat_points = unspent_stat_points_;
    out.unspent_skill_points = unspent_skill_points_;
    out.permanent_hp = permanent_hp_;
    out.permanent_mp = permanent_mp_;
    return out;
}

void CharacterProgression::apply_saved(const Saved& saved, const StatBook& graphs) {
    level_ = std::max(1, saved.level);
    experience_ = static_cast<int32_t>(std::min<uint64_t>(saved.experience, 0x7fffffff));
    fame_ = static_cast<int32_t>(std::min<uint64_t>(saved.fame, 0x7fffffff));
    fame_level_ = std::max(1, saved.fame_level);
    unspent_stat_points_ = std::max(0, saved.unspent_stat_points);
    unspent_skill_points_ = std::max(0, saved.unspent_skill_points);
    // `applySaveState`: `+0x41c` as it was,
    // `+0x440` from the int.
    permanent_hp_ = saved.permanent_hp;
    permanent_mp_ = saved.permanent_mp;
    spent_[index(StatType::Strength)] = std::max(0, saved.strength - profile_.strength);
    spent_[index(StatType::Dexterity)] = std::max(0, saved.dexterity - profile_.dexterity);
    spent_[index(StatType::Magic)] = std::max(0, saved.magic - profile_.magic);
    spent_[index(StatType::Defense)] = std::max(0, saved.defense - profile_.defense);
    resample(graphs);
    hp_ = saved.hp;
    mp_ = saved.mp;
}

void CharacterProgression::resample(const StatBook& graphs) {
    const float at = static_cast<float>(level_);

    // `CPlayer::calculateMaxHP()`:
    // `(int)ceilf(graph->getValue(level, 0))`; left as it was without the curve.
    if (!profile_.health_graph.empty()) {
        if (const StatGraph* graph = graphs.find(profile_.health_graph)) {
            max_hp_ = base_max_hp_ = static_cast<float>(static_cast<int>(std::ceil(graph->at(at))));
        }
    }
    // `CPlayer::calculateMaxMana()`: `+0x43c`
    // the same way.
    if (!profile_.mana_graph.empty()) {
        if (const StatGraph* graph = graphs.find(profile_.mana_graph)) {
            max_mp_ = base_max_mp_ = static_cast<float>(static_cast<int>(std::ceil(graph->at(at))));
        }
    }
    // `CPlayer::calculateNaturalArmor()`,
    // which `unitInit`, `applySaveState` and `levelUp` call: `+0x424` is the
    // class's `ARMOR`, and when that is not 0, that percentage of
    // `ARMOR_MONSTER_BYLEVEL` at the level, rounded up.
    natural_armor_ = profile_.armor;
    if (natural_armor_ != 0) {
        if (const StatGraph* graph = graphs.find(kMonsterArmorGraph)) {
            // (armor * curve) * 0.01f, rounded up, `fctiwz`.
            natural_armor_ = tl::formats::ppc_fctiwz(
                std::ceil((static_cast<float>(natural_armor_) * graph->at(at)) * 0.01f));
        }
    }

    experience_gate_ = tl::core::experience_gate(graphs, level_);
    fame_gate_ = tl::core::fame_gate(graphs, fame_level_);
}

namespace {
/// `CCharacter::applyInstantEffect`'s add:
/// `(int)(value + (float)field)`.
int add_truncated(int field, float value) {
    return tl::formats::x86_float_to_int32(value + static_cast<float>(field));
}
}  // namespace

int stat_with_bonus(int stat, float pct, float flat) {
    const int scaled = tl::formats::ppc_fctiwz(std::ceil((pct * static_cast<float>(stat)) * 0.01f));
    return tl::formats::ppc_fctiwz(std::ceil(flat)) + (scaled + stat);
}

int defense_with_bonus(int stat, float pct_any, float pct_all, float flat) {
    const int scaled = tl::formats::ppc_fctiwz(
        std::ceil(static_cast<float>(stat) * ((pct_any + pct_all) * 0.01f)));
    return tl::formats::ppc_fctiwz(std::ceil(flat)) + (scaled + stat);
}

int max_hp_formula(int base, float pct, const float* pet_pct, float flat) {
    float percent = pct * 0.01f;
    if (pet_pct != nullptr) percent = std::fma(*pet_pct, 0.01f, percent);
    if (!(base > 1)) base = 1;
    const float scaled = std::ceil(static_cast<float>(base) * percent);
    return tl::formats::ppc_fctiwz(scaled) + tl::formats::ppc_fctiwz(std::ceil(flat)) + base;
}

int max_mana_formula(int base, float permanent, float pct, float flat) {
    int total = tl::formats::ppc_fctiwz(static_cast<float>(base) + permanent);
    if (!(total > 1)) total = 1;
    const float scaled = std::ceil((static_cast<float>(total) * pct) * 0.01f);
    return tl::formats::ppc_fctiwz(scaled) + tl::formats::ppc_fctiwz(std::ceil(flat)) + total;
}

int CharacterProgression::max_hp_of(CharacterEffects effects, CharacterEffects master) const {
    // `CCharacter::maxHP`.:
    // the percent times 0.01f, the master's share fused on
    // (`fmadds`), `fctiwz`.
    const float pet = master ? master.effect_value(kEffectTypePercentPetHealth) : 0.0f;
    return max_hp_formula(permanent_hp_ + static_cast<int>(base_max_hp_),
                          effects ? effects.effect_value(kEffectTypePercentHp) : 0.0f,
                          master ? &pet : nullptr,
                          effects ? effects.effect_value(kEffectTypeMaxHp) : 0.0f);
}

int CharacterProgression::max_mp_of(CharacterEffects effects) const {
    // `CCharacter::maxMana`.:
    // (base * percent) * 0.01f, `fctiwz`.
    return max_mana_formula(static_cast<int>(base_max_mp_), permanent_mp_,
                            effects ? effects.effect_value(kEffectTypePercentMana) : 0.0f,
                            effects ? effects.effect_value(kEffectTypeMaxMana) : 0.0f);
}

void CharacterProgression::add_permanent_attribute(StatType type, float value) {
    // `CCharacter::applyInstantEffect`, cases
    // 0x72, 0x73, 0x74, 0x76.
    const int i = index(type);
    if (i < 0 || i >= 4) return;
    const int field = base_of(type) + spent_[i];
    spent_[i] = add_truncated(field, value) - base_of(type);
}

void CharacterProgression::add_permanent_hp(float value) {
    // Case 0x75: `+0x41c`.
    permanent_hp_ = add_truncated(permanent_hp_, value);
}

void CharacterProgression::add_permanent_mp(float value) {
    // Case 0x77: `+0x440 = +0x440 + value`, a float.
    permanent_mp_ = value + permanent_mp_;
}

void CharacterProgression::award_stat_points(float value) {
    // Case 0x78: `+0x45c`, the unspent stat points.
    unspent_stat_points_ = add_truncated(unspent_stat_points_, value);
}

// PORT: the class's figure for an attribute.
int CharacterProgression::base_of(StatType type) const {
    switch (type) {
        case StatType::Strength: return profile_.strength;
        case StatType::Dexterity: return profile_.dexterity;
        case StatType::Magic: return profile_.magic;
        case StatType::Defense: return profile_.defense;
    }
    return 0;
}

int CharacterProgression::strength(CharacterEffects effects) const {
    const int base = profile_.strength + spent_[0];
    if (!effects) return base;
    // CCharacter::strength:
    // ceil((getEffectValue(0x47, 7) * float(base)) * 0.01f) + ceil(getEffectValue(0x45, 7)) + base
    const float pct = effects.effect_value(kEffectTypePercentStrengthBonus, kDamageTypeAny);
    const float flat = effects.effect_value(kEffectTypeStrengthBonus, kDamageTypeAny);
    return stat_with_bonus(base, pct, flat);
}

int CharacterProgression::dexterity(CharacterEffects effects) const {
    const int base = profile_.dexterity + spent_[1];
    if (!effects) return base;
    // CCharacter::dexterity:
    // ceil((getEffectValue(0x48, 7) * float(base)) * 0.01f) + ceil(getEffectValue(0x46, 7)) + base
    const float pct = effects.effect_value(kEffectTypePercentDexterityBonus, kDamageTypeAny);
    const float flat = effects.effect_value(kEffectTypeDexterityBonus, kDamageTypeAny);
    return stat_with_bonus(base, pct, flat);
}

int CharacterProgression::magic(CharacterEffects effects) const {
    const int base = profile_.magic + spent_[2];
    if (!effects) return base;
    // CCharacter::magic:
    // ceil((getEffectValue(0x12, 7) * float(base)) * 0.01f) + ceil(getEffectValue(0x3, 7)) + base
    const float pct = effects.effect_value(kEffectTypePercentMagic, kDamageTypeAny);
    const float flat = effects.effect_value(kEffectTypeMagic, kDamageTypeAny);
    return stat_with_bonus(base, pct, flat);
}

int CharacterProgression::defense(CharacterEffects effects) const {
    const int base = profile_.defense + spent_[3];
    if (!effects) return base;
    // CCharacter::defense:
    // ceil(float(base) * ((getEffectValue(0x11, 7) + getEffectValue(0x11, 6)) * 0.01f))
    //   + ceil(getEffectValue(0x2, 7)) + base
    const float pct7 = effects.effect_value(kEffectTypePercentDefense, kDamageTypeAny);
    const float pct6 = effects.effect_value(kEffectTypePercentDefense, 6);
    const float flat = effects.effect_value(kEffectTypeDefense, kDamageTypeAny);
    return defense_with_bonus(base, pct7, pct6, flat);
}

// PORT: one of the four attributes by kind (`strength`, `dexterity`, `magic`, `defense`).
int CharacterProgression::stat(StatType type, CharacterEffects effects) const {
    switch (type) {
        case StatType::Strength: return strength(effects);
        case StatType::Dexterity: return dexterity(effects);
        case StatType::Magic: return magic(effects);
        case StatType::Defense: return defense(effects);
    }
    return 0;
}

int CharacterProgression::armour_bonus(CharacterEffects effects) const {
    // CCharacter::armorBonus: the natural armor
    // plus the inventory's, then `armor_bonus_formula`. The player has no master.
    const Inventory* inventory = effects.inventory();
    const int base = natural_armor_ + (inventory != nullptr ? inventory->armor_bonus() : 0);
    if (!effects) return base;
    const float pct = effects.effect_value(kEffectTypePercentArmorBonus, kDamageTypeAny);
    const float flat = effects.effect_value(kEffectTypeArmorBonus, kDamageTypeAny);
    return armor_bonus_formula(base, pct, nullptr, flat);
}

int CharacterProgression::base_armour_bonus(CharacterEffects effects) const {
    // CCharacter::baseArmorBonus: the
    // inventory's armour, `+0x38c` and `(int)ceilf(getEffectValue(8, 0.0f,
    // L"INNATEARMOR"))`; then the base defense's share of that sum,
    // `(int)ceilf((defense * 0.01f) * sum)`, floored at 0.
    const Inventory* inventory = effects.inventory();
    const float innate =
        effects ? effects.effect_value(kEffectTypeArmorBonus, "INNATEARMOR") : 0.0f;
    const int sum = tl::formats::ppc_fctiwz(std::ceil(innate)) +
                    (inventory != nullptr ? inventory->armor_bonus() : 0) + natural_armor_;
    const int scaled = tl::formats::ppc_fctiwz(std::ceil(
        (static_cast<float>(base_stat(StatType::Defense)) * 0.01f) * static_cast<float>(sum)));
    const int total = scaled + sum;
    return total < 0 ? 0 : total;
}

int CharacterProgression::armour_class(CharacterEffects effects) const {
    // CCharacter::AC: `ac_formula`.
    const int bonus = armour_bonus(effects);
    const int def = defense(effects);
    const float degrade =
        effects ? effects.effect_value(kEffectTypeDegradeArmorEffect, kDamageTypeAny) : 0.0f;
    return ac_formula(bonus, def, degrade);
}

int CharacterProgression::crit_chance(CharacterEffects effects) const {
    // CCharacter::getCriticalChance: `ceilf(getEffectValue(0x37, 7) + 3.0f)`,
    // truncated, at most 100. Dexterity is not read.
    const float val =
        effects ? effects.effect_value(kEffectTypeCriticalChance, kDamageTypeAny) : 0.0f;
    const float total = std::ceil(val + 3.0f);
    int result = static_cast<int>(total);
    if (result >= 100) result = 100;
    return result;
}

int CharacterProgression::damage_defense(int type, CharacterEffects effects) const {
    // CCharacter::damageDefense(EDAMAGE_TYPES):
    // `damage_defense_formula`.
    static constexpr std::array<int, kDamageTypeCount> kDamageDefenseEffectTypes{
        0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x00};  // gDAMAGE_DEFENSE_EFFECT_TYPES
    if (type < 0 || type >= kDamageTypeCount) return 0;
    const int own = 0;  // `+0x620[type]`
    return damage_defense_formula(
        own, effects.effect_value(kEffectTypePercentArmorBonus, type),
        effects.effect_value(kEffectTypePercentArmorBonus, 6),
        effects.effect_value(kDamageDefenseEffectTypes[static_cast<size_t>(type)], kDamageTypeAny),
        defense(effects), effects.effect_value(kEffectTypeDegradeArmorEffect, kDamageTypeAny));
}

int CharacterProgression::block_chance(CharacterEffects effects) const {
    // CCharacter::getBlockChance: getEffectValue(0x3e, 7) +
    // getEffectValue(0x21, 7) + 0, at most 50, truncated toward zero.
    const float val_pct =
        effects ? effects.effect_value(kEffectTypePercentBlockChance, kDamageTypeAny) : 0.0f;
    const float val_base =
        effects ? effects.effect_value(kEffectTypePercentBlockChanceBase, kDamageTypeAny) : 0.0f;
    const float sum = (val_base + val_pct) + 0.0f;
    if (sum > 50.0f) return 50;
    return static_cast<int>(sum);
}

// `CCharacter::minimumAC()` and
// `CCharacter::minimumDamageForDisplay()`:
// `fctiwz(ceilf(maximum * 0.5f))`, a negative maximum included.
int CharacterProgression::display_minimum(int maximum) {
    return static_cast<int>(std::ceil(static_cast<float>(maximum) * 0.5f));
}

// PORT: the stats page's `"lo-hi"`, its low end `display_minimum`.
std::string CharacterProgression::display_range(int maximum) {
    return std::to_string(display_minimum(maximum)) + "-" + std::to_string(maximum);
}

void CharacterProgression::level_up(const StatBook& graphs, CharacterEffects effects) {
    // CPlayer::levelUp over CCharacter::levelUp:
    // `setLevel(level + 1, false)`, then
    // `CPlayer::getStatsPointsAwardedForLevel` and `getSkillPointsAwardedForLevel`
    // at the new level, each `ceilf` of the class's curve.
    // DIVERGENCE: the trial version's level-7 cap
    // is not applied.
    ++level_;
    const float at = static_cast<float>(level_);
    if (!profile_.stat_points_graph.empty() && graphs.find(profile_.stat_points_graph) != nullptr) {
        unspent_stat_points_ +=
            tl::formats::ppc_fctiwz(std::ceil(graphs.find(profile_.stat_points_graph)->at(at)));
    }
    if (!profile_.skill_points_graph.empty() &&
        graphs.find(profile_.skill_points_graph) != nullptr) {
        unspent_skill_points_ +=
            tl::formats::ppc_fctiwz(std::ceil(graphs.find(profile_.skill_points_graph)->at(at)));
    }

    // CPlayer::calculateMaxHP, calculateMaxMana and calculateNaturalArmor,
    // then a full heal.
    resample(graphs);
    hp_ = max_hp_ = static_cast<float>(max_hp_of(effects));
    mp_ = max_mp_ = static_cast<float>(max_mp_of(effects));
}

// PORT: stat points added to the unspent ones.
void CharacterProgression::give_unspent_stat_points(int count) {
    if (count > 0) unspent_stat_points_ += count;
}

namespace {

// The `ceilf(amount * (bonus * 0.01f))` both awards add to a gain, in single
// precision (`awardExperience`, `awardFame`
// only an amount above 0 takes it, times 0.01f).
int with_gain_bonus(int amount, float bonus) {
    if (!(amount > 0)) return amount;
    return amount +
           tl::formats::ppc_fctiwz(std::ceil(static_cast<float>(amount) * (bonus * 0.01f)));
}

// The add both awards share: a positive amount saturates at 0x7fffffff when the
// sum overflows, anything else floors at 0.
//
int32_t add_saturating(int32_t total, int amount) {
    const int64_t sum = static_cast<int64_t>(total) + amount;
    if (amount >= 1) return sum > 0x7fffffff ? 0x7fffffff : static_cast<int32_t>(sum);
    return sum < 0 ? 0 : static_cast<int32_t>(sum);
}

// `CMasterResourceManager::getMaxLevel` / `getMaxFameLevel`:
// the point count of the gate curve itself.
int gate_point_count(const StatBook& graphs, const std::string& gate) {
    const StatGraph* graph = graphs.find(gate);
    return graph != nullptr ? static_cast<int>(graph->size()) : 0;
}

}  // namespace

// CCharacter::awardExperience: refusals, the
// bonus on a gain, the add, then one `levelUp` if the gate is reached below the
// cap (one level per award, however far past), or at the cap the total clamped.
// DIVERGENCE: NOXP is not modelled; "dying or dead" is the caller's `dying`. The
// pets' share is `pet_experience_share`, awarded first by the caller
// (`LevelScript::award_pet_experience`); only the player has a
// `CharacterProgression`, so there is no forward to a master.
bool CharacterProgression::award_experience(int amount, const StatBook& graphs,
                                            CharacterEffects effects, bool shared, bool dying) {
    if (dying && amount >= 1) return false;
    if (!shared) {
        amount = with_gain_bonus(amount, effects.effect_value(kEffectTypeXpGainBonus));
    }
    experience_ = add_saturating(experience_, amount);

    // INVENTED: no gate curve loaded (a unit test) is no gate at all.
    if (graphs.find(kExperienceGateGraph) == nullptr) return false;
    const int32_t gate = tl::core::experience_gate(graphs, level_);
    if (gate <= experience_ && level_ < gate_point_count(graphs, kExperienceGateGraph)) {
        level_up(graphs, effects);
        return true;
    }
    if (gate <= experience_) experience_ = gate;
    return false;
}

// CCharacter::awardExperience, up to its loop
// over +0x648: the refusal, then the bonus the pets' share carries
// too.
bool CharacterProgression::pet_experience_share(int amount, CharacterEffects effects, bool dying,
                                                int& share) const {
    if (dying && amount >= 1) return false;
    share = with_gain_bonus(amount, effects.effect_value(kEffectTypeXpGainBonus));
    return true;
}

// `CCharacter::awardExperience`'s add, saturating.
int32_t experience_after_award(int32_t total, int amount) { return add_saturating(total, amount); }

// CMasterResourceManager::getMaxLevel
int max_experience_level(const StatBook& graphs) {
    return gate_point_count(graphs, kExperienceGateGraph);
}

// CCharacter::awardFame: nothing for 0, the
// bonus on a gain (the last argument is not read), the add, then `fameUp` in a
// loop while the gate is reached below the cap. Quirk kept: reaching the cap in
// the loop leaves the surplus; only an award that *starts* at the cap clamps the
// total.
// DIVERGENCE: the player test always passes; only the player has a
// `CharacterProgression`.
// INVENTED: the forward to a master, as for experience. The "Fame" text
// is the renderer's, from `last_fame_award`.
bool CharacterProgression::award_fame(int amount, const StatBook& graphs, CharacterEffects effects,
                                      bool /*shared*/) {
    last_fame_award_ = 0;
    if (amount == 0) return false;
    amount = with_gain_bonus(amount, effects.effect_value(kEffectTypeFameGainBonus));
    last_fame_award_ = amount;
    fame_ = add_saturating(fame_, amount);

    // INVENTED: no gate curve loaded (a unit test) is no gate at all.
    if (graphs.find(kFameGateGraph) == nullptr) return false;
    const int max_fame_level = gate_point_count(graphs, kFameGateGraph);
    if (tl::core::fame_gate(graphs, fame_level_) <= fame_ && fame_level_ < max_fame_level) {
        while (tl::core::fame_gate(graphs, fame_level_) <= fame_ && fame_level_ < max_fame_level) {
            // CCharacter::fameUp: the fame
            // level, then `SKILL_POINTS_PER_FAME_LEVEL` read at the character's
            // *experience* level (+0xf0), not the fame level - the original's
            // own argument.
            ++fame_level_;
            if (!profile_.fame_skill_points_graph.empty()) {
                if (const StatGraph* graph = graphs.find(profile_.fame_skill_points_graph)) {
                    unspent_skill_points_ +=
                        tl::formats::ppc_fctiwz(std::ceil(graph->at(static_cast<float>(level_))));
                }
            }
        }
        fame_gate_ = tl::core::fame_gate(graphs, fame_level_);
        return true;
    }
    const int32_t gate = tl::core::fame_gate(graphs, fame_level_);
    if (gate <= fame_) fame_ = gate;
    return false;
}

// `CGameUI::updateIngameUI`, the
// experience bar: `(experience - prev_gate) / (curr_gate - prev_gate)`.
float CharacterProgression::experience_progress(const StatBook& graphs) const {
    const int32_t prev_gate =
        level_ > 1
            ? static_cast<int32_t>(std::max(
                  0.0f, graphs.value(kExperienceGateGraph, static_cast<float>(level_ - 1), 0.0f)))
            : 0;
    if (experience_gate_ <= prev_gate) return 0.0f;
    const int32_t current = std::max(experience_, prev_gate);
    const float progress =
        static_cast<float>(current - prev_gate) / static_cast<float>(experience_gate_ - prev_gate);
    return std::clamp(progress, 0.0f, 1.0f);
}

}  // namespace tl::core
