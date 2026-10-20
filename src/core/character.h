// The player character's progression: attributes, level, the points a level-up
// grants and where they are spent, and the running totals the journal prints.
// Fed from the class's `.DAT` (base attributes, `ARMOR`, the curve names) and the
// curves themselves (`StatBook`, `core/stats.h`).
#pragma once

#include "core/character_effects.h"
#include "core/context_tips.h"
#include "core/journal.h"
#include "core/stats.h"
#include "core/units.h"
#include "formats/adm.h"

#include <cstdint>
#include <string>

namespace tl::core {

class EffectManager;

/// The top of one weapon's damage range: its `MAXDAMAGE` percentage of
/// `BASE_WEAPON_DAMAGE` at the weapon's own level. Zero for anything that is
/// not a weapon; the bottom of the range is
/// `CharacterProgression::display_minimum()` of this.
int item_max_damage(const ItemStats& item, const StatBook& graphs);

/// Experience awarded for defeating a monster: what `CCharacter::setLevel`
/// and `CCharacter::makeChampion`
/// leave. A non-zero authored `XP` only switches it on: it becomes
/// `(int)((curve * curve) * 0.01f)` of `EXPERIENCE_MONSTER` (or
/// `EXPERIENCE_CHAMPIONMONSTER`) with the difficulty's suffix, at the unit's
/// level. `CCharacter::applyDamage` passes it straight to `awardExperience`.
uint64_t monster_experience(int monster_level, int monster_xp_stat, bool is_champion,
                            int difficulty, const StatBook& graphs);

/// An item's quality, which is the first word of its `UNITTYPE` (`NORMAL`,
/// `MAGIC`, `RANDOMMAGIC` or `UNIQUE`).
enum class ItemQuality { Normal, Magic, Unique };
ItemQuality item_quality(const ItemStats& item);

/// What a merchant pays for `count` of these, in gold, with no player bonus:
/// `recalculate_item_price` and `item_sell_price_of` (`CEquipment::recalculatePrice`
/// `sellPrice`). `VALUE` 0
/// is read as 100 (the renderer's loader defaults it to 0, the original to 100).
///
/// INVENTED: without a unit-type hierarchy, `UNIQUE` (0x36) and `MAGIC` (0x37)
/// are read off `UNITTYPE`'s first word, and a copy with affixes counts as magic
/// (`isMagical`).
int item_sell_price(const ItemStats& item, const StatBook& graphs, int count = 1);

enum class StatType { Strength = 0, Dexterity = 1, Magic = 2, Defense = 3 };

/// What a playable class authors about itself, lifted straight out of its
/// `.DAT` chain by whoever owns the archive. `core/` does no file I/O.
struct ClassProfile {
    std::string name;          ///< `NAME` - "Alchemist"
    std::string display_name;  ///< `DISPLAYNAME`

    /// `<INTEGER>STRENGTH` and friends.
    int strength = 0;
    int dexterity = 0;
    int magic = 0;
    int defense = 0;

    /// `ARMOR` on the class unit: a percentage of the monsters' armour curve at
    /// the character's level (`CPlayer::calculateNaturalArmor`).
    /// What it wears is the inventory's (`Inventory::armor_bonus`).
    int armor = 0;

    /// `HEALTH_GRAPH` / `MANA_GRAPH`, the curve names. A player's maxima are the
    /// curves' values at its level, rounded up; `MAXHP` is not read
    /// (`calculateMaxHP`, `calculateMaxMana`).
    ///
    std::string health_graph;
    std::string mana_graph;

    /// `STAT_POINTS_PER_LEVEL` - a curve name too, and a mod can point it
    /// somewhere else. Defaulted to the shipped name so a class whose `.DAT`
    /// omits it still levels up.
    std::string stat_points_graph = kStatPointsPerLevelGraph;
    /// `SKILL_POINTS_PER_LEVEL` / `SKILL_POINTS_PER_FAME_LEVEL`, the curves
    /// `CPlayer::getSkillPointsAwardedForLevel` and
    /// `CPlayer::getSkillPointsAwardedForFameLevel` read by the names the
    /// class file gives (`CPlayer +0x8a0`, `+0x8a8`).
    std::string skill_points_graph = kSkillPointsPerLevelGraph;
    std::string fame_skill_points_graph = kSkillPointsPerFameLevelGraph;
};

/// What `CPlayer::unitInit` and
/// `CPlayer::calculateNaturalArmor` read off
/// the class's record (`CPlayer +0x178`, its file over its `BASEFILE`):
/// `STRENGTH`, `DEXTERITY`, `MAGIC` and `DEFENSE` as the value word, 10 when
/// not there; `MANA_GRAPH` and `HEALTH_GRAPH`, "MANA_PLAYER_DESTROYER" and
/// "HEALTH_PLAYER_DESTROYER" when not there; the three point curves, their own
/// names when not there; `ARMOR` as the value word, 0 when not there. A record
/// of another type than a string gives an empty curve name. The name and the
/// display name are the caller's.
ClassProfile read_class_profile(const tl::formats::AdmSection& unit);

/// The four maxima the stats screen prints, each as a `"lo-hi"` range whose low
/// end is `CharacterProgression::display_minimum()` of it.
///
/// `CStatsMenu::update`, from
/// `CCharacter::maximumDamageForDisplay(bool, bool, bool)` and `CCharacter::AC()`;
/// `combat_sheet()` fills it.
struct CombatSheet {
    int melee_max = 0;
    int ranged_max = 0;
    int magic_max = 0;
    /// The magic row's low end: not half of `magic_max` but the smaller non-zero
    /// of its two `minimumDamageForDisplay` calls (`combat_sheet`).
    int magic_min = 0;
    int armour_max = 0;  ///< `CCharacter::AC()`
};

/// `CCharacter::awardExperience`, the add to
/// `+0x448`: a gain saturates at 0x7fffffff, a loss floors at 0.
int32_t experience_after_award(int32_t total, int amount);

/// `CMasterResourceManager::getMaxLevel`: the gate curve's point count.
int max_experience_level(const StatBook& graphs);

/// The arithmetic of `CCharacter::strength`, `dexterity`
/// and `magic`: the stat, plus
/// `ceilf((pct * stat) * 0.01f)`, plus `ceilf(flat)`, each through `fctiwz`.
int stat_with_bonus(int stat, float pct, float flat);
/// `CCharacter::defense`: as `stat_with_bonus`, the percent
/// being `(pct_any + pct_all) * 0.01f`, applied as `stat * that`.
int defense_with_bonus(int stat, float pct_any, float pct_all, float flat);
/// `CCharacter::maxHP`: the base at least 1, the percent
/// `pct * 0.01f` with the master's fused on (`pet_pct` null for no master).
int max_hp_formula(int base, float pct, const float* pet_pct, float flat);
/// `CCharacter::maxMana`: `fctiwz(base + permanent)` at
/// least 1, then `ceilf((base * pct) * 0.01f)` and `ceilf(flat)`.
int max_mana_formula(int base, float permanent, float pct, float flat);

class CharacterProgression {
public:
    CharacterProgression() = default;

    /// Take the class's authored profile and sample every curve it names at
    /// level 1. `graphs` is the `StatBook` the level already loaded; a curve it
    /// has not got leaves the figure at zero rather than at a guess.
    void init(const ClassProfile& profile, const StatBook& graphs,
              const std::string& character_name = "");

    /// A character that is not a player, as `CEquipment::canEquip`
    /// reads it: `CCharacter::CCharacter`
    /// leaves strength and dexterity 0, magic and
    /// defense 10, and the unit's level.
    void init_unit(int level) {
        profile_ = ClassProfile{};
        profile_.magic = 10;
        profile_.defense = 10;
        level_ = level;
    }

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    const std::string& name() const { return name_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    const std::string& title() const { return title_; }
    const std::string& player_class() const { return profile_.name; }

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    void set_name(const std::string& name) { name_ = name; }

    /// `CPlayer +0xa10`: how many retirements came before this character
    /// (`CGameClient::loadLevel`, the heirloom hand-off). Saved in the
    /// file's header (`saveCharacter`).
    int generation() const { return generation_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    void set_generation(int generation) { generation_ = generation; }
    /// `CPlayer +0xa14`: retired (`CEnchantMenu::performInteraction` case 0x1b).
    /// Saved after the generation; a retired file cannot be continued
    /// (`reloadFiles`), and `CPlayer::update` fades it out.
    bool retired() const { return retired_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    void set_retired(bool retired) { retired_ = retired; }
    void set_title(const std::string& title) { title_ = title; }

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int level() const { return level_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    uint64_t experience() const { return static_cast<uint64_t>(std::max(0, experience_)); }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    uint64_t experience_gate() const {
        return static_cast<uint64_t>(std::max(0, experience_gate_));
    }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int fame_level() const { return fame_level_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    uint64_t fame() const { return static_cast<uint64_t>(std::max(0, fame_)); }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    uint64_t fame_gate() const { return static_cast<uint64_t>(std::max(0, fame_gate_)); }

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    float hp() const { return hp_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    float max_hp() const { return max_hp_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    float mp() const { return mp_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    float max_mp() const { return max_mp_; }

    /// The renderer owns the character's actual health - it is what a monster
    /// takes off - so the sheet is told, not asked.
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    void set_hp(float current, float max) {
        hp_ = current;
        max_hp_ = max;
    }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    void set_mp(float current, float max) {
        mp_ = current;
        max_mp_ = max;
    }

    /// The class curve's maximum health and mana at the level, `+0x418` and
    /// `+0x43c` (`setLevel`); `max_hp()`/`max_mp()` are what the renderer last
    /// worked out from them (`CCharacter::maxHP`,
    /// `maxMana`).
    float base_max_hp() const { return base_max_hp_; }
    float base_max_mp() const { return base_max_mp_; }
    /// `+0x41c`: health added for good by `PERMANENT HEALTH`, which `maxHP()`
    /// adds to `+0x418`. Saved at +0xe8.
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int permanent_hp() const { return permanent_hp_; }
    /// `+0x440`, a float: mana added for good by `PERMANENT MANA`, which
    /// `maxMana()` adds to `+0x43c`. Saved as an int at +0xf4.
    float permanent_mp() const { return permanent_mp_; }

    /// `CCharacter::applyInstantEffect`'s permanent cases, on a player only:
    /// the field becomes `(int)(value + (float)field)` - `PERMANENT STRENGTH`
    /// (0x72), `DEXTERITY` (0x73), `MAGIC` (0x74),
    /// `HEALTH` (0x75), `DEFENSE` (0x76), `AWARD STATPOINT`
    /// (0x78); `PERMANENT MANA` (0x77) adds to a float. The
    /// value joins the points spent and is saved with them.
    /// `CCharacter::maxHP`. `master` is the
    /// pet's master's total, null for anyone else.
    int max_hp_of(CharacterEffects effects, CharacterEffects master = nullptr) const;
    /// `CCharacter::maxMana`.
    int max_mp_of(CharacterEffects effects) const;
    void add_permanent_attribute(StatType type, float value);
    void add_permanent_hp(float value);
    void add_permanent_mp(float value);
    void award_stat_points(float value);

    int base_strength() const { return profile_.strength; }
    int base_dexterity() const { return profile_.dexterity; }
    int base_magic() const { return profile_.magic; }
    int base_defense() const { return profile_.defense; }

    /// Total attributes including permanently spent points and effects:
    /// `CCharacter::strength`,
    /// `CCharacter::dexterity`,
    /// `CCharacter::magic` and
    /// `CCharacter::defense`, through
    /// `stat_with_bonus` and `defense_with_bonus`.
    int strength() const { return strength(nullptr); }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int dexterity() const { return dexterity(nullptr); }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int magic() const { return magic(nullptr); }
    int defense() const { return defense(nullptr); }

    /// `effects` is the character's total (`CCharacter::getEffectValue`
    /// `core/character_effects.h`): its own manager
    /// and its inventory's. A bare manager converts to one with no inventory.
    int strength(CharacterEffects effects) const;
    int dexterity(CharacterEffects effects) const;
    int magic(CharacterEffects effects) const;
    int defense(CharacterEffects effects) const;

    int stat(StatType type) const { return stat(type, nullptr); }
    int stat(StatType type, CharacterEffects effects) const;

    /// `CCharacter::armorBonus()`: the
    /// character's own `ARMOR` plus what the inventory carries
    /// (`effects.inventory()`'s `armor_bonus`), scaled by PERCENT ARMOR BONUS
    /// (0x17), plus ARMOR BONUS (0x8).
    int armour_bonus() const { return armour_bonus(nullptr); }
    int armour_bonus(CharacterEffects effects) const;

    /// `CCharacter::AC()`: `armorBonus +
    /// ceil(armorBonus * defense() / 100) - ceil(degrade)`, floored at zero;
    /// degrade is DEGRADE ARMOR EFFECT (0x42).
    int armour_class() const { return armour_class(nullptr); }
    int armour_class(CharacterEffects effects) const;

    /// `CCharacter::baseArmorBonus()`: the
    /// natural armour (`+0x38c`) plus the inventory's, plus `ceilf` of the ARMOR
    /// BONUS (8) of the effects named `INNATEARMOR` (`getEffectValue` by name),
    /// plus the base defense's percentage of that sum, at
    /// least 0. The console's stats page colours the armour row by `AC()`
    /// against it.
    int base_armour_bonus(CharacterEffects effects) const;
    /// `CPlayer +0x38c`: what `calculateNaturalArmor` left.
    int natural_armor() const { return natural_armor_; }

    /// Critical strike chance in percent: `CCharacter::getCriticalChance`,
    /// `ceilf(CRITICAL CHANCE(0x37) + 3)`, at most 100.
    int crit_chance(CharacterEffects effects = nullptr) const;

    /// `CCharacter::damageDefense(EDAMAGE_TYPES)`,
    /// the figure the stats screen's
    /// resistances row prints and `modifyDamage` soaks an elemental blow with:
    ///
    ///     d = +0x620[type] * (1 + (PERCENT ARMOR BONUS(0x17, type) + (0x17, 6)) / 100), >= 0
    ///     e = getEffectValue(gDAMAGE_DEFENSE_EFFECT_TYPES[type], 7)
    ///     ceil((int)ceil(defense / 100 * (d + e)) + d + e - (int)ceil(DEGRADE ARMOR(0x42)))
    ///
    /// The table is 0x23..0x28 for types 0-5, 0 for 6.
    int damage_defense(int type, CharacterEffects effects) const;

    /// Block chance in percent: `CCharacter::getBlockChance`, PERCENT
    /// BLOCK CHANCE BASE (0x3e) plus PERCENT BLOCK CHANCE (0x21), at most 50.
    int block_chance(CharacterEffects effects = nullptr) const;

    /// The low end of every range the stats screen prints:
    /// `CCharacter::minimumAC()` and
    /// `CCharacter::minimumDamageForDisplay()`,
    /// `ceil(maximum * 0.5)`. Not held at 0: a negative maximum shows "-11--23".
    static int display_minimum(int maximum);

    /// `display_minimum(maximum)` and `maximum` as `"lo-hi"`.
    static std::string display_range(int maximum);

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int unspent_stat_points() const { return unspent_stat_points_; }

    /// `CCharacter +0x460` (Xbox `+0x3c8`): skill points not yet spent. The
    /// skill screen spends them through `SkillBook::learn`.
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int unspent_skill_points() const { return unspent_skill_points_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int& unspent_skill_points_ref() { return unspent_skill_points_; }
    /// `CCharacter::giveUnusedSkillPoints`.
    void give_unspent_skill_points(int count) { unspent_skill_points_ += count; }

    /// The attribute before effects: the class's figure plus every point placed
    /// on it.
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    int base_stat(StatType type) const { return base_of(type) + spent_[index(type)]; }

    /// One point from the unspent pool onto `type` (`delta` 1), or back into the
    /// pool (-1). Unchecked: the console's stats page decides when
    /// (`core/stats_page.h`).
    void move_stat_point(StatType type, int delta) {
        spent_[index(type)] += delta;
        unspent_stat_points_ -= delta;
    }

    /// One level: grants `STAT_POINTS_PER_LEVEL` and `SKILL_POINTS_PER_LEVEL`
    /// at the new level, re-samples the health, mana, armour and gate curves,
    /// and heals to `max_hp_of(effects)` and `max_mp_of(effects)`. Needs the
    /// same `StatBook` `init()` was given.
    void level_up(const StatBook& graphs, CharacterEffects effects = nullptr);

    /// Grant points outright - a `--stat-points` run, or a quest reward.
    void give_unspent_stat_points(int count);

    /// `CCharacter::awardExperience`.
    /// `amount` is signed: a death penalty is a negative award, and `XP GAIN
    /// BONUS` applies to it too unless `shared` (the original's last argument).
    /// The total is an `int`, saturating at 0x7fffffff and floored at 0.
    /// **One** level per award: a quest that pays past two gates levels once,
    /// and the next award levels again. At the cap (the gate curve's point
    /// count) the total is clamped to the gate. `dying` refuses a positive
    /// award, as AI states 5 and 6 do. `effects` may be null (no bonus).
    /// Returns true when a level was gained.
    bool award_experience(int amount, const StatBook& graphs, CharacterEffects effects = nullptr,
                          bool shared = false, bool dying = false);

    /// `CCharacter::awardExperience`, what
    /// each unit at `+0x648` (the pets) is awarded ahead of the character's own
    /// add: the amount with the character's `XP GAIN BONUS` on it.
    /// False when the award is refused before the loop (dying or dead, and a
    /// gain).
    bool pet_experience_share(int amount, CharacterEffects effects, bool dying, int& share) const;

    /// `CCharacter::awardFame`: as
    /// `award_experience`, with `FAME GAIN BONUS`, but it loops: one award can
    /// cross several fame levels. Each is `CCharacter::fameUp`,
    /// whose `SKILL_POINTS_PER_FAME_LEVEL` is read at the
    /// *experience* level (kept). True when a fame level was gained.
    bool award_fame(int amount, const StatBook& graphs, CharacterEffects effects = nullptr,
                    bool shared = false);
    /// The amount the last `award_fame` added, after the bonus: what its
    /// text prints (`<amount> Fame`). 0 for an award of nothing.
    int last_fame_award() const { return last_fame_award_; }

    /// Experience progress in [0, 1] within the current level's gate
    /// (`CGameUI::updateIngameUI`).
    float experience_progress(const StatBook& graphs) const;

    /// The fields of `CCharacter` this sheet keeps, as `CCharacter::fillSaveState`
    /// copies them into a `CCharacterSaveState`.
    /// Accessors: `strength()`, `dexterity()`,
    /// `defense()`, `magic()`
    /// `mana()`, `giveUnusedStatPoints`
    /// `addFame`, `awardExperience`.
    struct Saved {
        int level = 1;
        uint64_t experience = 0;
        uint64_t fame = 0;
        int fame_level = 1;
        float hp = 0.0f;
        float mp = 0.0f;
        int strength = 0;
        int dexterity = 0;
        int defense = 0;
        int magic = 0;
        int unspent_stat_points = 0;
        int unspent_skill_points = 0;  ///< `+0x460` (+0x10c)
        int permanent_hp = 0;          ///< `+0x41c` (+0xe8)
        float permanent_mp = 0.0f;     ///< `+0x440` (+0xf4, as an int)
    };
    Saved saved() const;

    /// `CCharacter::applySaveState` on this
    /// sheet: back at its level with its curves re-sampled, points spent as the
    /// attributes say.
    void apply_saved(const Saved& saved, const StatBook& graphs);

    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    JournalStatistics& journal() { return journal_; }
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    const JournalStatistics& journal() const { return journal_; }

    /// `CPlayer +0xa17`: the context tips this character has been shown.
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    TipsSeen& tips_seen() { return tips_seen_; }
    const TipsSeen& tips_seen() const { return tips_seen_; }

private:
    // PORT: an accessor of the port's character sheet (`CCharacter`'s fields it keeps).
    static int index(StatType type) { return static_cast<int>(type); }
    int base_of(StatType type) const;

    /// Re-read every curve at the current level. Called by `init()` and
    /// `level_up()` so the two cannot drift apart.
    void resample(const StatBook& graphs);

    ClassProfile profile_;
    std::string name_;
    int generation_ = 0;
    bool retired_ = false;
    std::string title_ = "Unrecognized";

    int level_ = 1;
    int32_t experience_ = 0;
    int32_t experience_gate_ = 0;
    int fame_level_ = 1;
    int32_t fame_ = 0;
    int last_fame_award_ = 0;
    int32_t fame_gate_ = 0;

    float hp_ = 0.0f;
    float max_hp_ = 0.0f;
    float mp_ = 0.0f;
    float max_mp_ = 0.0f;
    /// `+0x424`, `calculateNaturalArmor`'s.
    int natural_armor_ = 0;
    float base_max_hp_ = 0.0f;
    float base_max_mp_ = 0.0f;
    int permanent_hp_ = 0;
    float permanent_mp_ = 0.0f;

    int spent_[4] = {0, 0, 0, 0};
    int unspent_stat_points_ = 0;
    int unspent_skill_points_ = 0;

    JournalStatistics journal_;
    TipsSeen tips_seen_{};
};

}  // namespace tl::core
