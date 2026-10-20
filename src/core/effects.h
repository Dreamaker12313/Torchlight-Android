// Effects and affixes as the description screens read them: what one
// `[EFFECT]` block means, what an affix file authors, and the line each prints.
// `CEffect`, `CAffix`, `CSkillEffectAndAffixes` and the `media/EffectsList.dat`
// loader `parseEffects`. Applying an effect is
// `core/effect_manager.h`'s.
//
// `CStringTranslate::getTranslateString` returns its key when nothing
// translates it, so the keys appear here as literals.
#pragma once

#include "core/stats.h"
#include "core/unit_types.h"
#include "formats/adm.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tl::core {

class RunicRandom;

/// `EEFFECT_STAT_MODIFIERS` (`gEFFECT_STAT_MODIFIER_NAMES`, the function's
/// translation unit): the six stats an effect's `STATMODIFYNAME` can scale it
/// by. `MELEE` is strength and `RANGED` dexterity.
enum class EffectStat : int32_t {
    Melee = 0,
    Ranged = 1,
    Defense = 2,
    Magic = 3,
    Level = 4,
    OwnerLevel = 5,
};
inline constexpr int kEffectStatCount = 6;

/// `EDAMAGE_TYPES` (`gDAMAGE_TYPES`): `Physical Magical Fire Ice Electric
/// Poison All`.
enum class DamageType : int32_t {
    Physical = 0,
    Magical = 1,
    Fire = 2,
    Ice = 3,
    Electric = 4,
    Poison = 5,
    All = 6,
};

/// `CEffect::getDamageTypeDisplayString(EDAMAGE_TYPES)`.
/// Physical prints nothing, which is why the
/// templates carry `[DMGTYPE] ` with a space that has to be swallowed with it.
std::string damage_type_display_string(DamageType type);

/// `EEFFECT_ACTIVATION` (`gEffect_Activation_Names`).
enum class EffectActivation : int32_t {
    Passive = 0,
    Dynamic = 1,
    Transfer = 2,
};

/// The two durations that are words rather than numbers (`CEffect::CEffect`
/// the compares):
/// `INSTANT` is -900, the value `applyEffectOnUnit` and `updateEffects` hand to
/// `applyInstantEffect`; `ALWAYS`, `0`, an absent `DURATION` and any number
/// that is not positive are -1000.
inline constexpr float kDurationInstant = -900.0f;
inline constexpr float kDurationAlways = -1000.0f;

/// How many `VALUE`/`GRAPH` columns an effect type has.
inline constexpr int kEffectValueCount = 5;

/// One row of `media/EffectsList.dat`, which holds every per-type table
/// (`gEFFECT_TYPE_NAMES`, the four description templates,
/// `gEFFECT_TYPE_PROPERTY_TAGS`, `gEFFECT_TYPE_GRAPHS`, `gEFFECT_TYPE_BONUS`, the
/// bounds). An effect's type *is* its row index, so the file's order is part of
/// the format (`parseEffects`).
struct EffectTypeInfo {
    std::string name;     ///< `NAME`, upper-cased when matched
    std::string good;     ///< `GOODDES`
    std::string good_ot;  ///< `GOODDESOT`
    std::string bad;      ///< `BADDES`
    std::string bad_ot;   ///< `BADDESOT`
    /// `VALUE1`..`VALUE5`: the key each of the effect's five numbers is read
    /// under. `NA` names no key, and the reader falls back to `VALUE`,
    /// `VALUE2`.. instead.
    std::array<std::string, kEffectValueCount> value_tags;
    /// `GRAPH1`..`GRAPH5`: the curve each number scales along, by level.
    std::array<std::string, kEffectValueCount> graphs;
    /// `gEFFECT_TYPE_BONUS`: 1 for `TYPE:Percent`, 2 for `TYPE:None`, 0 for
    /// `TYPE:Value`. A `None` row skips the good/bad sign test entirely.
    int bonus = 0;
    int min_value = -1;  ///< `MINVAL`, default -1
    int max_value = 1;   ///< `MAXVAL`, default 1
};

/// `media/EffectsList.dat`, read whole. 145 rows ship.
class EffectTypeTable {
public:
    /// `parseEffects`: every `[EFFECT]` under
    /// the root, in order.
    void load(const tl::formats::AdmFile& file);

    /// The row index for a `TYPE` value, matched upper-cased against `NAME`, or
    /// -1. `CEffect::CEffect` leaves its default type in place on no match.
    int index_of(const std::string& type) const;

    const EffectTypeInfo* at(int index) const;
    // PORT: an accessor of the port's effect type table.
    size_t size() const { return rows_.size(); }
    bool empty() const { return rows_.empty(); }

private:
    std::vector<EffectTypeInfo> rows_;
};

/// The effect types whose `[NAME]` is a *skill*'s display name rather than the
/// effect's own (`CEffect::getDisplayStats`): `SKILL
/// BONUS`, `LEARN SKILL`, `CAST SKILL`, `CAST SKILL AT TARGET`, `CAST SKILL ON
/// STRUCK`, `CAST SKILL ON DEATH`, `STOP SKILL`.
bool effect_name_is_skill(const EffectTypeTable& table, int type);
/// The three whose `[NAME]` is a monster's: `TRANSFORM`, `TRANSFORM PERMANENT`,
/// `DESUMMON MONSTER`.
bool effect_name_is_monster(const EffectTypeTable& table, int type);
/// The four the visual pass scales by duration: `MANA RECHARGE`, `HP RECHARGE`,
/// `MANA RECHARGE PLAYER`, `HP RECHARGE PLAYER`
/// (`CEffect::getVisualCalculatedValues`).
bool effect_is_per_tick(const EffectTypeTable& table, int type);

/// One `[EFFECT]` block, in the half `CEffect` needs to print itself.
struct Effect {
    int type = -1;                                            ///< +0x1c, an `EffectTypeTable` index
    EffectActivation activation = EffectActivation::Dynamic;  ///< +0x20
    float duration = kDurationInstant;  ///< +0x24, the constructor's -900 until `DURATION` is read
    /// +0x28 (X360 `+0x21`): a `DURATION` key was present.
    /// `CSkillEffectAndAffixes::setDurationOverride` leaves such an effect's
    /// duration alone.
    bool authored_duration = false;
    /// +0x31, `EXCLUSIVE`, default 0 (`CEffect::initValues`),
    /// and forced on for a `DAMAGE` (0x34) effect
    /// with a real duration. Only an exclusive effect retires the same-named
    /// copies already running (`CEffectManager::addNewEffect`);
    /// any other stacks with them.
    bool exclusive = false;
    bool display = true;  ///< +0x35, never cleared by the data
    std::string name;     ///< +0x80, `NAME`
    /// +0x88, `LINK_NAME`: the effect this one takes with it when it goes
    /// (`CEffectManager::deleteDeadEffects`).
    std::string link_name;
    DamageType damage_type = DamageType::Physical;
    /// +0xc4..+0xd4: the five numbers, read under the type's `VALUE` tags.
    std::array<float, kEffectValueCount> values{};
    /// +0xd8..+0xf8 as names: the type's `GRAPH` columns, or `GRAPHOVERRIDE`
    /// for all five, or none at all when `NOGRAPH`.
    std::array<std::string, kEffectValueCount> graphs;
    /// +0xf8 / +0x110: `STATMODIFYNAME` and `STATMODIFYPERCENT`, tokenised on
    /// `,` and walked in step. The percent is stored divided by 100, and
    /// defaults to 1.0 when the name list is the longer of the two.
    std::vector<EffectStat> stat_ids;
    std::vector<float> stat_values;
    uint32_t level = 0;  ///< +0x10, what the affix's level set it to
    /// +0x130, `UNITTHEME`: theme name (upper-cased) granted while this effect is active.
    /// `CEffect::CEffect` looks it up in
    /// `CUnitThemes::getSingleton()`.
    std::string unit_theme;

    /// Fields a save record carries (`CItemSaveState::save`)
    /// that nothing transcribed reads yet. Defaults
    /// are the data constructor's (over
    /// `initValues`).
    bool b30 = true;            ///< +0x30, 1 from the data constructor
    bool second_value = false;  ///< +0x32, set when the second number was authored
    bool save = false;          ///< +0x33, `SAVE`
    /// +0x36 (X360 `+0x2e`), `USEOWNERLEVEL` (0 from `initValues`): the
    /// effect takes its owner's level when `CEffect::setOwner` recalculates it,
    /// and the affix-owner setter leaves its level alone.
    bool use_owner_level = false;
    /// +0x37: set by the data constructor unless `NOGRAPH`, and by `setGraphs`.
    bool has_graphs = false;
    float f38 = 1.0f;  ///< +0x38, 1.0 from the data constructor
    /// +0x98: the direction the effect was applied along, zero from
    /// `initValues`. `CAffix::setDirection`
    /// writes it on every effect of a skill's affix; a `KNOCK BACK
    /// EFFECT` pushes along it.
    std::array<float, 3> direction{};
    int32_t i3c = 100;           ///< +0x3c, 100 from `initValues`
    int32_t i40 = 0;             ///< +0x40
    int32_t i44 = 0;             ///< +0x44
    std::string graph_override;  ///< +0x90, `GRAPHOVERRIDE`
    /// +0xb0, `PARTICLE_FX` (`CEffect::setFX`)
    std::string particle_fx;
    /// X360 +0xdc: what `CEffect::calculateBaseValue` last
    /// left, rolled on `g_RandVolatile` in every constructor, `setOwner`,
    /// `CAffix::setPercent`, `setAffixOwner` and
    /// `CAffix::addEffectsToEffectManager`. Empty where the port does not run
    /// those rolls yet; `EffectManager::add_effect` then works it out.
    std::optional<float> rolled_value;

    float min_value() const { return values[0]; }
    float max_value() const { return values[1]; }
};

/// `CEffect::CEffect(CDataGroup*, CBaseUnit*)`,
/// the read half.
Effect parse_effect(const tl::formats::AdmSection& section, const EffectTypeTable& table);

/// The effects a unit is made with: `CBaseUnit::reapplyEffects`
/// finds the unit's `[EFFECTS]` group, else takes
/// the unit's own record when it has an `[EFFECT]`, and
/// `CEffectManager::createEffects` makes the
/// group itself when it is an `[EFFECT]`, else each `[EFFECT]` child in order.
std::vector<Effect> unit_own_effects(const tl::formats::AdmSection& unit,
                                     const EffectTypeTable& table);

/// `CEffect::CEffect(const CEffect*)`:
/// `initValues` then `CEffect::clone`, the
/// copy every held effect is (`CEffectManager::cloneEffect`, `CAffix::clone`,
/// the save states). The console's clone also works out its timed flag again,
/// which the port does not carry. What `clone` resets is
/// `LiveEffect`'s, which starts empty.
Effect clone_effect(const Effect& source);

/// `CEffect::setOwner(CBaseUnit*, bool)`, the
/// level half: with `USEOWNERLEVEL`, an owner and the recalculation asked for,
/// the effect's level (`+0x10`, X360 `+0x8`) becomes the owner's (X360
/// `+0xf0`) - 0 for -1, and 0 above 1000. `owner_level` empty is no owner.
/// The value is worked out again where the effect is held.
void set_owner_level(Effect& effect, std::optional<int32_t> owner_level);

/// `CEffect::CEffect(EEFFECT_TYPE, float duration, float value, bool, int
/// activation, float, bool)`, the effect
/// the code makes for itself (`rollAttack`'s, `transferEffects`' copies, an
/// item's `BLOCK_CHANCE` and its elemental armour):
/// the type, the duration and the first number (`+0xc4`, X360 `+0xe0`), over
/// `initValues` - whose damage type is 1,
/// not the data constructor's `PHYSICAL`. No `DURATION` was authored (X360
/// `+0x21` is not written). `flag30` is `+0x30` (X360 `+0x28`) and `f38`
/// `+0x38` (X360 `+0x30`).
Effect value_effect(int type, float duration, float value, EffectActivation activation,
                    bool flag30 = false, float f38 = 0.0f);

/// Whose stats scale an effect: `CCharacter::strength/dexterity/defense/magic`
/// and the level, as `CEffect::calculateBaseValue` reads them.
struct EffectOwner {
    int strength = 0;
    int dexterity = 0;
    int defense = 0;
    int magic = 0;
    uint32_t level = 1;    ///< `CCharacter +0x100`, for `OWNERLEVEL`
    bool present = false;  ///< false when the effect has no owner yet
};

/// `CEffect::ECALCULATETYPES`: 0 rolls between the two numbers, 1 takes the
/// larger, 2 the smaller.
enum class EffectCalculate : int32_t { Roll = 0, Larger = 1, Smaller = 2 };

/// `CEffect::modifyEffectsByCharacter(CCharacter*)`:
/// what the owner's stats make of an effect,
/// `1 + sum(stat / 100 * weight)` over its `STATMODIFYNAME` list (`LEVEL` and
/// `OWNERLEVEL` add the level itself times the weight). 1 with no list or no
/// owner.
float modify_effects_by_character(const Effect& effect, const EffectOwner& owner);

/// `CEffect::calculateBaseValue(ECALCULATETYPES)`
/// without the roll: the value of `which`,
/// times `modify_effects_by_character`. With no curve that is the number
/// itself; with one it is a percentage of the curve at the effect's level.
float effect_base_value(const Effect& effect, EffectCalculate which, const StatBook& graphs,
                        const EffectOwner& owner, RunicRandom* rolls = nullptr);

/// `CEffect::value(EEFFECT_VALUES)`: the
/// column's curve at the effect's level (`+0x10`) times the number as a
/// percentage, or the number itself when the column has no curve.
float effect_column_value(const Effect& effect, size_t column, const StatBook& graphs);

/// `CEffectDisplayValues`, as `CEffect::getVisualCalculatedValues(level, bool)`
/// fills it. The screen passes level -1,
/// which leaves the effect's own level alone, and `false`, which computes both
/// ends of the range.
struct EffectDisplayValues {
    float duration = 0.0f;   ///< +0x10, the effect's `DURATION` as authored
    float min = 0.0f;        ///< +0x14, `calculateBaseValue(Smaller)`
    float max = 0.0f;        ///< +0x18, `calculateBaseValue(Larger)`
    float force_min = 0.0f;  ///< +0x1c, the effect's third number
    float force_max = 0.0f;  ///< +0x20, its fourth
    bool per_tick = false;   ///< +0x28: the value was scaled by the duration
    int type = -1;           ///< +0x2c
};

EffectDisplayValues effect_display_values(const Effect& effect, const EffectTypeTable& table,
                                          const StatBook& graphs, const EffectOwner& owner);

/// What the description needs besides the effect itself: the tables, the curves,
/// the owner, and the two name lookups the seven skill and three monster effect
/// types make.
struct EffectDisplayContext {
    const EffectTypeTable* types = nullptr;
    const StatBook* graphs = nullptr;
    EffectOwner owner;
    /// `CSkillParser::getSkillData(name)`'s `DISPLAYNAME`, `???` when there is
    /// no such skill.
    std::function<std::string(const std::string&)> skill_display_name;
    /// The same from the master resource list's `MONSTERS` group.
    std::function<std::string(const std::string&)> monster_display_name;
    /// An affix by `NAME` and level, as `CResourceManager::getNewAffixByName`
    /// hands one out.
    std::function<const struct Affix*(const std::string&, uint32_t)> affix;
    /// One named skill's property at a level, as `CExecuteSkillProps::
    /// getSkillStats` reaches it (inlined in `CSkillProperty::getSkillStats`):
    /// `CSkillManager::addSkill(name, false)` on the owner's
    /// manager, `CSkill::_setLevelOfSkillFromSkillManager(level)`, then
    /// `CSkill::getSkillLevelStats`' choice: the level's
    /// property, the first level's past the end, the effective one for 0.
    std::function<const struct SkillProperty*(const std::string&, uint32_t)> skill_level_property;
};

/// `getRangeOfValues(float, float, bool)`:
/// `a-b` when the two ceilings differ, otherwise the larger or the smaller of
/// them alone.
std::string range_of_values(float a, float b, bool use_max);

/// `CEffect::getDisplayStats(CEffectDisplayValues&)`:
/// the type's template with `[VALUE]`,
/// `[VALUE1]`, `[VALUE2]`, `[MINVAL]`, `[MAXVAL]`, `[VALUE3AND4]`, `[NAME]`,
/// `[DMGTYPE]` and `[DURATION]` filled in. A "bad" effect - one whose value
/// runs against the sign of its type's `MAXVAL` - is wrapped in the red colour
/// run `|cFFff483f`..`|u`.
/// `colour` is `getDisplayStats`' last argument:
/// false, as a skill's block passes it, leaves a bad line unwrapped.
std::string effect_display_stats(const Effect& effect, const EffectDisplayContext& context,
                                 bool colour = true);
/// The same over values the caller worked out - what
/// `CEffectManager::getVisualDescription`
/// hands it after merging like effects.
std::string effect_display_stats(const Effect& effect, const EffectDisplayValues& values,
                                 const EffectDisplayContext& context, bool colour = true);

/// One `[AFFIX]` file: `CAffix`, the read half (`CAffix::parseAffixGroup`
/// defaults from there).
struct Affix {
    /// `NAME`, as authored. The original keeps it upper-cased at +0x50 (default
    /// "NA"); `AffixBook` (`core/affix_book.h`) keys on the upper-cased form.
    std::string name;
    std::string prefix;            ///< +0x58, `PREFIX`, default ""
    std::string suffix;            ///< +0x60, `SUFFIX`, default ""
    int rank = 1;                  ///< +0x74, `RANK`, default 1
    int min_spawn_range = 0;       ///< +0x68, `MIN_SPAWN_RANGE`, default 0
    int max_spawn_range = 999999;  ///< +0x6c, `MAX_SPAWN_RANGE`, default 999999
    /// +0xac, `WEIGHT`, default 1: the odds `CEffectGroupManager::
    /// rollAndCreateRandomAffixes` adds the
    /// affix with, and 0 keeps it out of the random pool altogether.
    int weight = 1;
    /// +0x78, `SLOTS_OCCUPY`, default 1: what one pick costs of the slot count
    /// `CEquipment::enchant` rolled.
    int slots_occupy = 1;
    /// +0xb0 (count +0xb8): the unit types the affix may go on, each name of
    /// the unit-types child resolved through `CHierarchy::getTypeIDByName`,
    /// unknown names dropped, in file order. `CAffix::canBeAppliedToUnitType`
    /// accepts a type that `ISA`s any of
    /// them; an empty list accepts nothing.
    std::vector<uint32_t> unit_types;
    /// The same group as `AFFIXES.RAW` keeps it for the random roll
    /// (`CEffectGroupManager`'s folder load, `+0x68`): every
    /// name's `getTypeIDByName`, an unknown one too (`kUnknown`, which nothing
    /// `ISA`s).
    std::vector<uint32_t> listed_unit_types;
    /// +0xa4, `DURATION`, default -1.0. Only a **positive** value is a real
    /// duration: `parseAffixGroup` turns
    /// anything else back into -1.0, and `CAffix::updateAffixDuration`
    /// counts down whenever the value is not negative - so 0.0 would
    /// expire on the first tick.
    float duration = -1.0f;
    /// +0xa8, set beside +0xa4 when the file authored a positive `DURATION`.
    /// A skill block's own `DURATION` may only override an affix that did not
    /// (`CSkillEffectAndAffixes::applyAffixesAndEffects`).
    ///
    bool authored_duration = false;
    std::vector<Effect> effects;  ///< +0x88, the `[EFFECT]` blocks in file order
    /// X360 +0xa8: the level `setLevel` last left - the one
    /// handed in, held to the spawn range. `CAffix::CAffix(CAffix*, unsigned)`
    /// stores the level it is handed here before `clone`.
    int32_t level = -1;
};

/// Parse an affix file's root `[AFFIX]`. Without `unit_types` the unit-type
/// list stays empty (the description paths never read it).
Affix parse_affix(const tl::formats::AdmFile& file, const EffectTypeTable& table,
                  const UnitTypeHierarchy* unit_types = nullptr);

/// `CAffix::setLevel(unsigned int)` and
/// `setPercent`: clamp the level into the
/// spawn range, turn it into a percentage of that range, interpolate every
/// ungraphed effect's single value along it, and give every effect the clamped
/// level as `CEffect::setAffixOwner` does -
/// all but a `USEOWNERLEVEL` one, which keeps its owner's. Returns the affix
/// ready to display.
Affix affix_at_level(const Affix& source, uint32_t level);

/// `CEffect::calculateBaseValue(0)` on the
/// console's own object: `effect_base_value(Roll)` drawn on `rng`
/// (`g_RandVolatile`) and kept (`Effect::rolled_value`).
void roll_effect_value(Effect& effect, const StatBook& graphs, const EffectOwner& owner,
                       RunicRandom& rng);

/// What an affix's `calculateBaseValue(0)` calls roll with: the curves,
/// `g_RandVolatile`, and the character the value is taken for (`CEffect
/// +0x48`; none on an item). With no generator nothing is drawn or kept - the
/// display paths, which only want the numbers.
struct EffectRolls {
    const StatBook* graphs = nullptr;
    RunicRandom* rng = nullptr;
    EffectOwner owner;
    // PORT: whether a roll stream is set.
    bool on() const { return graphs != nullptr && rng != nullptr; }
    void roll(Effect& effect) const {
        if (on()) roll_effect_value(effect, *graphs, owner, *rng);
    }
};

/// `CAffix::setLevel(unsigned int)` then
/// `setPercent` on `affix`, whose template
/// (`+0x08`) is `source`: the level held to the spawn range (`Affix::level`),
/// the percent of the range it reaches, and on each effect - while both have
/// one at that place - an ungraphed range set to the point along it (first the
/// low end, a roll, then the high end, a roll), then the level, a roll
/// (`rolls`).
void affix_set_level(Affix& affix, const Affix& source, uint32_t level,
                     const EffectRolls& rolls = EffectRolls{});

/// `CAffix::CAffix(CAffix*, unsigned int)`:
/// `level` at `+0xa8`, then `clone`, which
/// copies the affix and each effect through `CEffect::CEffect(const CEffect*)`
/// - two rolls each - and ends in `setLevel(+0xa8)`; then
/// `setLevel(level)` again unless it is -1.
Affix copy_affix(const Affix& source, uint32_t level, const EffectRolls& rolls = EffectRolls{});

/// `CAffix::setOwner(CBaseUnit*)`:
/// `CEffect::setOwner(owner, true)` on every effect - a
/// `USEOWNERLEVEL` effect takes the owner's level (`+0xf0`; -1 and above 1000
/// are 0) when there is an owner - and a roll each.
void affix_set_owner(Affix& affix, std::optional<uint32_t> owner_level, const EffectRolls& rolls);

/// `CAffix::addEffectsToEffectManager(CEffectManager*)`
/// for a manager whose unit is not a
/// character (an item's, `+0x48` null): each effect but a `USEOWNERLEVEL` one
/// takes the affix's level (`+0xa8`; -1 and above 1000 are 0), then a roll; the
/// effect itself goes onto the manager (`cloneEffect` makes no
/// copy of an effect whose affix already belongs to it).
void affix_add_effects_rolls(Affix& affix, const EffectRolls& rolls);

/// `CAffix::getDisplayStats(unsigned int)`:
/// its effects' lines, one per line, empty lines dropped.
std::string affix_display_stats(const Affix& affix, const EffectDisplayContext& context,
                                bool colour = true);

/// `CAffix::fillOutStatBonuses(float (&)[6], bool)`.
void affix_fill_out_stat_bonuses(const Affix& affix, std::array<float, kEffectStatCount>& out,
                                 bool take_max);

/// `TARGET`, the filter `CSkillEffectAndAffixes::applyAffixesAndEffects`
/// applies before it puts anything on a unit;
/// any other value is no filter. `gSKILL_EFFECT_AND_AFFIXES_TARGET_NAMES`:
/// `ENEMY SELF PET EVERYBODY FRIEND`, authored as the word and upper-cased.
enum class SkillTargetFilter : int32_t {
    /// `ENEMY`: skipped when the caster and the target share an alignment.
    Enemy = 0,
    /// `SELF`: applied only when the target *is* the caster.
    Self = 1,
    /// `PET`: applied only to the caster's pet (`CCharacter +0x640`).
    Pet = 2,
    /// `EVERYBODY`: no filter. The apply path's test has no arm for 3, so it
    /// falls through and applies, which is what the name says.
    Everybody = 3,
    /// `FRIEND`: skipped for an enemy and skipped for the caster itself.
    Friend = 4,
};

/// A skill level's or event's `[AFFIXES]`, `[AFFIXESREMOVE]`, `[EFFECTS]`,
/// `[EFFECT]` and `[EFFECTSREMOVE]` children: `CSkillEffectAndAffixes`
/// (0x78 bytes on the console). Offsets are the
/// console's.
struct SkillEffectAndAffixes {
    /// One `AFFIX` of an `[AFFIXES]` block (0x34 bytes, made),
    /// with the block's `AFFIXLEVEL`, `TARGETTYPE`, `TARGET` and
    /// `DURATION`.
    struct AffixSpec {
        std::string name;    ///< +0x08
        uint32_t level = 1;  ///< +0x24, `AFFIXLEVEL`, default 1
        /// +0x28, `TARGETTYPE` through `CHierarchy::getTypeIDByName`,
        /// 0 when unauthored: the type
        /// `applyAffixesAndEffects` asks the unit to `ISA`.
        int32_t target_type = 0;
        /// +0x2c, `TARGET`: 0 (`ENEMY`) unless the block authors one of the
        /// five names - not the skill's default, which only the root's `TARGET`
        /// and `[EFFECTS]` take.
        SkillTargetFilter target = SkillTargetFilter::Enemy;
        float duration = -1.0f;  ///< +0x30, `DURATION`, default -1
    };
    /// +0x0c: every `[AFFIXES]` block's `AFFIX` names in file order, which
    /// `applyAffixesAndEffects` puts on and `getDisplayStats`
    /// prints.
    std::vector<AffixSpec> affix_specs;
    /// `CSkillEffectData` (0x24 bytes, vtable): +0x0c the effects,
    /// +0x1c the `TARGETTYPE`, +0x20 the `TARGET`.
    struct EffectGroup {
        std::vector<Effect> effects;
        int32_t target_type = 0;
        SkillTargetFilter target = SkillTargetFilter::Enemy;
    };
    /// +0x1c: at most one group - the `[EFFECT]` blocks of the first
    /// `[EFFECTS]` child, or of the section itself when it has none - made only
    /// when there is at least one.
    std::vector<EffectGroup> effect_groups;
    /// +0x3c: every string value of the first `[AFFIXESREMOVE]`, whatever its
    /// key (`TARGET:SELF`'s `SELF` among them), which
    /// `removeAffixesAndEffects` hands to `deleteAffix` one
    /// by one. Never printed.
    std::vector<std::string> remove_affix_names;
    /// +0x4c: every string value of the first `[EFFECTSREMOVE]`, each handed to
    /// `CEffectManager::removeEffect` - names, not `[EFFECT]` blocks.
    std::vector<std::string> remove_effect_names;
    /// +0x5c, `ADDITIONALDESCRIPTION`: the `[EFFECTS]` child's, then each
    /// `[AFFIXES]`', the `[AFFIXESREMOVE]`'s and the `[EFFECTSREMOVE]`'s in
    /// turn, a block without one keeping the last; then every `\n` written out
    /// (a backslash and an `n`) made a line break (`STRINGS::replaceWString`).
    std::string additional_description;
};

/// Whether `CSkillProperty`'s and
/// `CSkillEvent`'s constructors make a
/// `CSkillEffectAndAffixes` for `section` at all: a child named `EFFECT`,
/// `EFFECTS`, `AFFIXES` or `AFFIXESREMOVE` (not `EFFECTSREMOVE`). The callers
/// that fall back from an event's block to its property's ask whether the
/// event *has* one (`CSkillEvent +0x54` not null), however empty.
bool has_skill_effect_and_affixes(const tl::formats::AdmSection& section);

/// `CSkillEffectAndAffixes::setDurationOverride(float)`:
/// every affix spec's `DURATION` becomes
/// `seconds`; then, for each effect group, as many times as the group has
/// effects, the effect at **the group's own index** in it (its first when the
/// group is shorter) takes `seconds` unless it authored a `DURATION`. So only
/// one effect of a group is ever reached; kept.
void set_duration_override(SkillEffectAndAffixes& block, float seconds);

/// `CSkillEffectAndAffixes::CSkillEffectAndAffixes`,
/// the read half: everything but the display
/// affixes (+0x2c, `createAffixesForDisplay`).
/// `unauthored` is the root `TARGET`'s default, from the skill
/// (`default_effect_target`); `unit_types` resolves the `TARGETTYPE`s, which
/// stay 0 without it.
SkillEffectAndAffixes parse_skill_effect_and_affixes(
    const tl::formats::AdmSection& section, const EffectTypeTable& table,
    SkillTargetFilter unauthored = SkillTargetFilter::Enemy,
    const UnitTypeHierarchy* unit_types = nullptr);

/// The `TARGET` a block gets when it authors none (`CSkillEffectAndAffixes`'s
/// constructor): from the skill's target type (`SkillTargetType`) and
/// `TARGET_ALIGNMENT` - `SELF` for a self or self-and-pets skill, `PET` for a
/// pets one, `EVERYBODY` for an everybody one, `FRIEND` for a unit-targeted
/// skill aimed at `GOOD`, and `ENEMY` for everything else.
SkillTargetFilter default_effect_target(int32_t target_type, int32_t target_alignment);

/// The root `TARGET` default of every block a skill's constructor makes
/// (`CSkill::CSkill` -> `CSkillProperty`, `CSkillEvent`): the
/// property in use is still null then, so it reads target type 0: `ENEMY`. Only
/// an event cloned while the skill runs (`CSkill::triggerEvent`,
/// once `CLONECOUNT`'s clones are busy) gets `default_effect_target`
/// of the property then in use.
inline constexpr SkillTargetFilter kSkillBlockTargetDefault = SkillTargetFilter::Enemy;

/// `CSkillEffectAndAffixes::getDisplayStats()`:
/// the `ADDITIONALDESCRIPTION`, then every
/// affix's lines, then every effect group's, joined with newlines that are only
/// inserted between two non-empty parts.
std::string skill_effect_and_affixes_display_stats(const SkillEffectAndAffixes& block,
                                                   const EffectDisplayContext& context);

/// `CSkillEffectAndAffixes::fillOutStatBonuses(float (&)[6], bool)`:
/// every affix through `CAffix::fillOutStatBonuses`, then every effect of every
/// group. `take_max` keeps the larger of what is there and what is found;
/// without it a later effect overwrites an earlier one, zero included.
void skill_effect_and_affixes_fill_out_stat_bonuses(const SkillEffectAndAffixes& block,
                                                    const EffectDisplayContext& context,
                                                    std::array<float, kEffectStatCount>& out,
                                                    bool take_max);

}  // namespace tl::core
