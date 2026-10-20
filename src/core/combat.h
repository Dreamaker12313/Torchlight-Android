// A character's fighting numbers and one blow's rolls, transcribed.
//
// `CCharacter::setLevel` turns a unit's
// authored percentages into hit points, armour and damage off the level curves
// for the game's difficulty. `CCharacter::attack`, `performAttack` and
// `rollAttack`
// decide when a swing lands and what it does: a
// monster's ordinary melee blow, and a character's blow with a weapon, whose
// damage `CEquipment::calculateCombatStats` rolled once for the item. Effects
// and skills are not here. Every roll is on the volatile generator, in the
// original's order.
//
// No OGRE and no file I/O.
#pragma once

#include "core/character_effects.h"
#include "core/collision.h"
#include "core/effect_manager.h"
#include "core/random.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "core/weapons.h"

#include <array>
#include <functional>
#include <string>

namespace tl::core {

/// `CGameClient +0x38ec` values (`CDifficultyMenu`): 0 easy, 1 normal, 2 hard,
/// 3 very hard.
namespace difficulty {
inline constexpr int kEasy = 0;
inline constexpr int kNormal = 1;
inline constexpr int kHard = 2;
inline constexpr int kVeryHard = 3;
}  // namespace difficulty

/// What `setLevel` leaves on a character.
struct UnitCombat {
    int level = 1;              ///< +0x100
    int base_max_hp = 0;        ///< +0x418
    float hp = 0.0f;            ///< +0x414
    int armor = 0;              ///< +0x424
    int xp = 0;                 ///< +0x454
    AttackDescription attack;   ///< the first of +0x3f8
    float damage_scale = 1.0f;  ///< +0x720
    /// +0x620 by `EDAMAGE_TYPES`: `ARMOR_<TYPE>` as a share of the armour.
    /// Physical (0) is folded into `armor` and left 0, as `setLevel` does.
    std::array<int, kDamageTypeCount> damage_defense{};
    /// X360 +0x618: the difficulty's health curve at the level,
    /// truncated, which `setLevel` keeps while working out the attack.
    int level_health = 0;
    /// X360 +0x3C0, the fame the unit awards: what the constructor leaves (0)
    /// until `makeChampion` sets it.
    int fame = 0;
    /// X360 +0x63A, `makeChampion` has run.
    bool champion = false;
};

/// What `CCharacter::setLevel` reads off the
/// unit's data into `stats` (defaults in brackets): `MINHP`/`MAXHP` (1.0),
/// `HEALTH_GRAPH`, `SET_HP_PCT` (1.0), `ARMOR` (0), `ARMOR_GRAPH`, `ARMOR_<type>`
/// (-1), `MINDAMAGE`/`MAXDAMAGE` (0), `DAMAGE_GRAPH`, `NO_UNARMED_ATTACKS`,
/// `DAMAGE_<type>` (-1); and `XP` (0) for `CCharacter::unitInit`.
/// A number is the value word whatever the record's type;
/// a name that is not a string reads empty.
///
void read_set_level_data(const tl::formats::AdmSection& unit, UnitStats& stats);

/// `CCharacter::makeChampion(name, theme)`,
/// the figures: once only (`+0x63a`), the hit points again off the champion
/// curve (`MINHP`/`MAXHP` with 0 for unauthored, `HEALTH_GRAPH` or
/// `HEALTH_CHAMPIONMONSTER_BYLEVEL` and its suffix; one volatile draw, the hit
/// points the maximum itself), `XP` off `EXPERIENCE_CHAMPIONMONSTER` squared,
/// the fame award off `FAME_CHAMPIONMONSTER` squared, the armour off `ARMOR_GRAPH`
/// or `ARMOR_CHAMPIONMONSTER_BYLEVEL` with its `ARMOR_<TYPE>` shares. The damage
/// is worked out off `DAMAGE_CHAMPIONMONSTER` and thrown away (both `ceilf`s'
/// results are never stored): a champion hits
/// as hard as the monster. The scale (+0.2), the bravery (1), the name and the
/// theme are the caller's. Returns false when it already was a champion.
bool make_champion(UnitCombat& combat, const UnitStats& stats, int difficulty,
                   const StatBook& graphs, RunicRandom& rolls);

/// `CCharacter::setLevel(level, true)` for a
/// unit that is not the player. `graphs` are `media/graphs/stats`; one volatile
/// draw for the hit points.
UnitCombat set_level(const UnitStats& stats, int level, int difficulty, const StatBook& graphs,
                     RunicRandom& rolls);

/// `CCharacter::setLevel(level, true)`, its
/// attack half: the character's first own attack (`+0x3f8`), built only while
/// it has none. `MINDAMAGE`/`MAXDAMAGE` are percentages of `DAMAGE_GRAPH`, else
/// the difficulty's `DAMAGE_MONSTER`; a player (`ISA(0x1c)`) reads
/// plain `DAMAGE_MONSTER` whatever either says. No draws.
AttackDescription own_attack(const UnitStats& stats, int level, int difficulty, bool player,
                             const StatBook& graphs, int* level_health = nullptr);

/// `CCharacter::maxHP` with no effects: at
/// least 1.
int max_hp(const UnitCombat& combat);

/// What `CCharacter::attackRange`, `meleeRange`, `rangedRange`, `selectAttack`
/// and `attack` read of one hand (`CInventory::getEquipmentEquippedAt`,
/// location 0 the right, 1 the left): held, `ISA` `Weapon` (8), `Ranged Weapon`
/// (0x23) or `Melee` (0x3c), and the `RANGE` of the item's attack for that hand.
struct HeldHand {
    bool held = false;
    bool weapon = false;
    bool ranged = false;
    bool melee = false;
    float range = 0.0f;
};

/// The two hands, and whether the attack in use is a weapon's (`+0x3ec`).
struct HeldHands {
    bool using_weapon = false;
    HeldHand right;  ///< location 0
    HeldHand left;   ///< location 1
};

/// `CCharacter::attackRange`: the attack's
/// `RANGE` - with a weapon in use and a `Weapon` in the left hand beside
/// something in the right, the shorter of it and the two hands' (the longer
/// when either hand is a `Ranged Weapon`) - then
/// `fma(reach, scale, range) + 0.2`. `hands` null is a unit with
/// no inventory.
float attack_range(const UnitStats& stats, const AttackDescription& attack,
                   const HeldHands* hands = nullptr);
/// The same with the character's own scale (`+0x84`, `SCALE` plus its rolled
/// variation) in place of the data's; this and the forms below read it so.
float attack_range(const UnitStats& stats, float scale, const AttackDescription& attack,
                   const HeldHands* hands = nullptr);
/// `CCharacter::meleeRange`: as
/// `attackRange`, but the attack's `RANGE` lowered by each hand's that `ISA`
/// `Melee`, with no test of the other hand.
float melee_range(const UnitStats& stats, const AttackDescription& attack, const HeldHands& hands);
float melee_range(const UnitStats& stats, float scale, const AttackDescription& attack,
                  const HeldHands& hands);
/// `CCharacter::rangedRange`: raised by each
/// hand's that `ISA` `Ranged Weapon`.
float ranged_range(const UnitStats& stats, const AttackDescription& attack, const HeldHands& hands);
float ranged_range(const UnitStats& stats, float scale, const AttackDescription& attack,
                   const HeldHands& hands);

/// Which attack `CCharacter::selectAttack` takes: the character's own
/// (`+0x368` list, one drawn at random), the right hand's or the left's.
enum class AttackHand { Own, Right, Left };
/// `CCharacter::selectAttack(EATTACK_RANGE_TYPE mode)`,
/// its choice: with the right hand empty,
/// the left hand's attack when it holds a `Weapon`, else the character's own;
/// with both hands full and a `Weapon` in the left, the left's when `mode` is 0
/// and `alternate` (`+0x2b2`, flipped by every `performAttack`)
/// is set, 1 and it `ISA` `Melee`, or 2 and it `ISA` `Ranged Weapon`;
/// otherwise the right's.
AttackHand select_attack_hand(const HeldHands& hands, int mode, bool alternate);

/// What `CCharacter::attack` does
/// about the hands before it swings: `SelectAny` - `selectAttack(0)` - unless
/// both hands are full and the left `ISA` `Weapon`; then `Keep` the attack in
/// use when both or neither is a `Ranged Weapon`, else `Mixed`: the caller
/// compares `rangedRange` with `meleeRange` and, when they differ, looks for a
/// foe in reach of the melee hand.
enum class AttackHandStep { SelectAny, Keep, Mixed };
AttackHandStep attack_hand_step(const HeldHands& hands);
/// The melee hand `attack` searches with when the hands are mixed: the right
/// when it is not a `Ranged Weapon`, else the left.
bool attack_melee_hand_is_right(const HeldHands& hands);

/// `CCharacter::inAttackRange(0)`: within 2.5
/// in height and the gap between the two bodies within `RANGE_MULTIPLIER *
/// attackRange()`. With a `RANGED WEAPON` (ISA 0x23) in either hand (`ranged`)
/// the height is not tested and `ranged_bonus` (effect 0x4e,
/// `getEffectValue(0x4e, 7)`) is added.
bool in_attack_range(const UnitStats& stats, const AttackDescription& attack, const Vec3& from,
                     const Vec3& to, float target_radius, bool ranged = false,
                     float ranged_bonus = 0.0f, const HeldHands* hands = nullptr);
bool in_attack_range(const UnitStats& stats, float scale, const AttackDescription& attack,
                     const Vec3& from, const Vec3& to, float target_radius, bool ranged = false,
                     float ranged_bonus = 0.0f, const HeldHands* hands = nullptr);

/// `CCharacter::inStrikeRange(target)`:
/// within 2.5 in height and the gap between the two bodies within the strike
/// reach. With a `RANGED WEAPON` (ISA 0x23) in use (`ranged`) the height is not
/// tested and `ranged_bonus` (effect 0x4e, `getEffectValue(0x4e, 7)`) is added.
/// The console's adds 0.5 to the reach of a unit that `ISA`
/// the player (`console_player`).
bool in_strike_range(const UnitStats& stats, const AttackDescription& attack, const Vec3& from,
                     const Vec3& to, float target_radius, bool ranged = false,
                     float ranged_bonus = 0.0f, bool console_player = false);
bool in_strike_range(const UnitStats& stats, float scale, const AttackDescription& attack,
                     const Vec3& from, const Vec3& to, float target_radius, bool ranged = false,
                     float ranged_bonus = 0.0f, bool console_player = false);

/// `CCharacter::performAttack`, the wall test
/// of a blow with no ranged weapon: a ray 5 above the
/// attacker's feet towards the target's x and z at the same height (`objects`
/// too, the call's last argument false); if it meets something, again from 1.25
/// back along `getForward` and 0.75 along `getRight` (the target end moved the
/// same 0.75), and if that meets something, both ends 1.25 back along
/// `getRight`. Walled when all three meet something. `at` is the attacker's own
/// place when there is nothing to strike.
bool melee_attack_walled(const CollisionWorld& world, const Vec3& here, float facing,
                         const Vec3& at);

/// Where a ranged weapon's shot ends (`performAttack`).
struct RangedShot {
    Vec3 end;
    bool walled = false;  ///< a wall stopped it: the blow is a miss
};

/// `start` is the weapon's tag point. At a target (`target` non-null, its
/// `getPosition(true)` and collision radius): its x and z, and the start's
/// height raised by the target's height above its radius less the attacker's
/// (`here`, `own_radius`). At none: `reach` (`RANGE_MULTIPLIER * strike range
/// + effect 0x4e`) along `forward`. The rise is held within a fifth of the
/// length of `(end.x, start.y, end.z)` either way - the end's *place*, not the
/// shot's length. Then a ray 0.8 above both ends; a wall it meets is the end,
/// at the start's height.
RangedShot ranged_shot(const CollisionWorld* world, const Vec3& start, const Vec3& here,
                       float own_radius, const Vec3* target, float target_radius,
                       const Vec3& forward, float reach);

/// `CCharacter::facingTarget(CCharacter*, CItem*)`:
/// a target nearer than 1.5 on the ground is faced whatever the
/// heading; otherwise the heading to it, wrapped into -180..180 degrees, within
/// half of `ATTACKANGLE` (`+0x694`) or of twice `weapon_arc`
/// (`console_weapon_arc` of the weapon in use `+0x3ec`, 0 with none),
/// whichever is larger.
bool facing_target(float facing, const Vec3& from, const Vec3& to, float attack_angle,
                   float weapon_arc = 0.0f);

/// of the weapon in use: 50 for a `Sword` (ISA 0xb) or an
/// `Axe` (0x2c), 40 for a `Mace` (0x59), 70 for a `Polearm` (0x69) or a
/// `Staff` (0x3d), 0 for anything else.
float console_weapon_arc(bool sword, bool axe, bool mace, bool polearm, bool staff);

/// `CEquipment::calculateCombatStats`, the
/// attack a weapon swings with (`+0x238` the right hand's, `+0x23c` the
/// left's): the class its type names (`attack_animation_class`), the item's
/// `RANGE`, `STRIKERANGE`, `SPEED` / 100 and `AI_ATTACKCOOLDOWN`, and what
/// `selectAttack`, `attack` and `performAttack` ask of it - its missile,
/// whether it `ISA` a ranged weapon or a melee one, and its arc. Not present
/// for an item that is no weapon.
UnitStats::WeaponAttack weapon_attack_of(const ItemStats& item, Hand hand,
                                         const UnitTypeHierarchy& types);

/// `CCharacter::turnToward(direction, dt)`
/// with no effects: the new yaw. `turned` says the heading was more than 5
/// degrees off (`+0x265`).
float turn_toward(float facing, float dir_x, float dir_z, float turn_rate, float delta,
                  bool* turned = nullptr, float factor = 1.0f);

/// What a defender brings to a blow.
struct Defender {
    int armor = 0;         ///< `CCharacter::AC()`
    int max_hp = 1;        ///< `CCharacter::maxHP()`
    int block_chance = 0;  ///< `CCharacter::getBlockChance()`, capped at 50
    /// `INTERRUPT RESISTANCE` (0x88), which `rollAttack` and an instant
    /// `DAMAGE` effect's critical roll against.
    float interrupt_resistance = 0.0f;
    /// `CCharacter::damageDefense(type)`, by `EDAMAGE_TYPES`.
    std::array<int, kDamageTypeCount> damage_defense{};
    /// `CCharacter::getEffectValue` for `damageDefensePercent`
    /// (`PERCENT DAMAGE TAKEN` of the type and of all): the unit's
    /// own effects and its inventory's. Empty: a unit holding none.
    CharacterEffects effects;
    /// The unit's own manager (`CCharacter +0x17c`, X360), whose `SHIELD
    /// BUFFER`s a hit spends. Null: none to spend.
    EffectManager* shields = nullptr;
    /// A shield took the whole hit: `Absorbed!` over the
    /// unit and its bank's slot 7 (`SHIELDBLOCK`), the caller's to show.
    std::function<void()> on_absorbed;
};

/// `CCharacter::damageDefense(type)`:
/// `+0x620` scaled by `PERCENT ARMOR BONUS` (0x17, of the type and of all),
/// plus the type's defence effect (`gDAMAGE_DEFENSE_EFFECT_TYPES`), raised by
/// `defense()` percent, less `DEGRADE ARMOR EFFECT` (0x42). `effects` null is a
/// unit holding none.
int damage_defense(const UnitCombat& combat, int type, int defense,
                   const EffectManager* effects = nullptr);
/// `CCharacter::AC()` over `armorBonus()`:
/// `+0x424` raised by `PERCENT ARMOR BONUS`
/// (0x17) plus `ARMOR BONUS` (0x8), then by `defense()` percent, less `DEGRADE
/// ARMOR EFFECT`, never below 0. `master` is `+0x640`'s effects, whose `PERCENT
/// PET ARMOR` (0x61) joins the percentage. `worn_armor` is the inventory's
/// armour (X360 `+0x3e8 -> +0x08`, `CInventory`'s armour bonus), added to the
/// natural armour first.
int armor_class(const UnitCombat& combat, int defense, const EffectManager* effects,
                const CharacterEffects& master = {}, int worn_armor = 0);

/// The arithmetic of `CCharacter::armorBonus`: the armour,
/// plus `ceilf(armour * percent)` - the percent `pct * 0.01f` with the master's
/// `PERCENT PET ARMOR` fused on (`pet_pct` null for no master) - plus
/// `ceilf(flat)`.
int armor_bonus_formula(int armor, float pct, const float* pet_pct, float flat);
/// `CCharacter::AC`: `ceilf((defense * 0.01f) * bonus)`
/// plus the bonus, less `ceilf(degrade)`, never below 0.
int ac_formula(int bonus, int defense, float degrade);
/// `CCharacter::damageDefense(type)`: `fma(pct_type +
/// pct_all, 0.01f, 1) * own`, kept above 0, plus the type's defence effect;
/// `ceilf((defense * 0.01f) * that)` plus that, less `ceilf(degrade)`, rounded
/// up.
int damage_defense_formula(int own, float pct_type, float pct_all, float own_effect, int defense,
                           float degrade);

/// `CCharacter +0x430`, the defence every character is constructed with
/// (`CCharacter::CCharacter`). A player's is its
/// unit's `DEFENSE` (`CPlayer::unitInit`); nothing
/// else reads that key.
inline constexpr int kCharacterBaseDefense = 10;
/// `CCharacter::defense()` for a unit that is
/// not a player: the base raised by `PERCENT DEFENSE` (0x11, of any type and of
/// all), plus `DEFENSE` (2).
int unit_defense(const EffectManager* effects);

/// What a character brings to `rollAttack` besides the attack itself.
struct Strike {
    int max_physical = 0;  ///< `maxDamage(attack, target)`
    int damage_type = 0;   ///< the attack's `+0x78`
    /// The extra damage by type the second and third loops walk: with a
    /// weapon, `CEquipment::getDamageBonus(type)`;
    /// without one, the character's own
    /// first attack's `+0x24[type]` from type 1 on.
    std::array<int, kDamageTypeCount> bonus{};
    /// `strength()` (`dexterity()` for a ranged weapon), and with a weapon
    /// `magic()`, which joins every type after physical - the stats as they
    /// are; `raised_bonus` takes each times 0.01f.
    float share = 0.0f;
    float magic_share = 0.0f;
    bool weapon = false;
    /// `rollAttack`'s two floats (its 5th and 6th arguments), 1 for an ordinary
    /// swing. The first scales the maximum after `maxDamage` and each type's
    /// bonus before it is raised; the second is the soak factor every
    /// `modifyDamage` is handed. A skill event's weapon attack passes its
    /// `WEAPONDAMAGEPCT` and `SOAKSCALEPCT` (`CSkillEvent::applyWeaponDamage`).
    ///
    float damage_scale = 1.0f;
    float soak_scale = 1.0f;

    // What `rollAttack` asks the attacker's
    // `getEffectValue` for, filled by `strike_effects`; all 0 is a character
    // holding no effects.
    /// `ceilf(getEffectValue(DAMAGE BONUS 10, type))`: flat damage of a type,
    /// which opens that type's loop even when the weapon carries none.
    std::array<int, kDamageTypeCount> flat_bonus{};
    /// `PERCENT DAMAGE BONUS` (0x19) of the type, and of all (6). The percents
    /// here are the effect values as they are, not over 100: `raised_bonus`
    /// fuses each on times 0.01f, as the console does.
    std::array<float, kDamageTypeCount> percent_bonus{};
    float percent_all = 0.0f;
    /// `PERCENT MELEE DAMAGE` (0xf), or `PERCENT RANGED DAMAGE` (0x10) for a
    /// ranged weapon (ISA 0x23).
    float attack_percent = 0.0f;
    /// The weapon's kind: `PERCENT MARTIAL ITEM DAMAGE BONUS` (99, ISA 0xa2),
    /// `PERCENT RANGED ITEM DAMAGE BONUS` (0x66, ISA 0xa3) and `PERCENT MAGIC
    /// ITEM DAMAGE BONUS` (0x67, ISA 0xa4), in that order, each only when the
    /// weapon is of that kind (`item_kind`).
    std::array<float, 3> item_percent{};
    std::array<bool, 3> item_kind{};
    /// `DUAL WIELDING BONUS` (0x58), with both hands full and location 1 not a
    /// shield (ISA 0x15; `dual`).
    float dual_percent = 0.0f;
    bool dual = false;
    /// `CRITICAL CHANCE` (0x37), added to `rollCritical`'s draw.
    float critical_chance = 0.0f;
    /// `PERCENT CRITICAL DAMAGE` (0x59): that percentage of a critical's half.
    float critical_percent = 0.0f;
    /// `INTERRUPT CHANCE` (0x38): a draw of 1..100 under it interrupts; below
    /// 0, a draw of 0..100 under its negation takes the interrupt away.
    float interrupt_chance = 0.0f;
};

/// What `maxDamage` and `rollAttack` test on the weapon in use and the hands.
struct WeaponTraits {
    bool held = false;         ///< a weapon at all (the 3rd argument / `+0x498`)
    bool ranged = false;       ///< ISA 0x23
    bool martial = false;      ///< ISA 0xa2
    bool ranged_item = false;  ///< ISA 0xa3
    bool magic_item = false;   ///< ISA 0xa4
    /// Locations 0 and 1 both hold something and location 1's is no shield.
    bool dual = false;
    /// `maxDamage`'s `EEQUIP_LOCATIONS` argument: 0 when the weapon in use is
    /// the one at location 1, 1 otherwise.
    int minus_location = 1;
};

/// `CCharacter::rollAttack`, the attacker's
/// effect reads: fills the strike's effect fields from `effects` for an attack
/// with `weapon`.
void strike_effects(Strike& strike, const CharacterEffects& effects, const WeaponTraits& weapon);

/// What `maxDamage` and `rollAttack` test on the weapon in
/// use and the two hands: ISA 0x23 (ranged), 0xa2, 0xa3, 0xa4 (the item
/// categories), both hands full with no shield in the left (dual), and which
/// location's effects `maxDamage` leaves out (the weapon in use is at location
/// 1 only when location 0 is empty).
WeaponTraits held_weapon_traits(const ItemStats& weapon, const UnitTypeHierarchy& types,
                                bool right_held, bool left_held, bool left_is_shield);

/// `CCharacter::rollAttack(level, target, weapon, ...)` for a
/// character with `weapon` in use: `maxDamage` on the weapon's attack (its
/// rolled `damage`, type 0) with `strength` (`dexterity` for a ranged weapon),
/// the attacker's effects (`strike_effects`), each type's
/// `CEquipment::getDamageBonus` (`weapon_damage_bonus`), and
/// `magic` for every type after physical.
Strike armed_strike(const ItemStats& weapon, const WeaponTraits& traits, int strength,
                    int dexterity, int magic, const CharacterEffects& effects,
                    const EffectManager::Context& item_effects, float damage_scale = 1.0f,
                    float soak_scale = 1.0f);

/// The same with no weapon in use: the character's own attack (`+0x2c0`),
/// `strength` as the share, and the attack's own damage of each type past
/// physical as the bonus.
Strike unarmed_strike(const AttackDescription& attack, int strength,
                      const CharacterEffects& effects, float damage_scale = 1.0f,
                      float soak_scale = 1.0f);

/// `CCharacter::maxDamage(CAttackDescription*, CCharacter*)`,
/// whole: `base` is the attack's `+0x24`,
/// `type` its `+0x78`, `stat` `strength()` or `dexterity()` by the weapon,
/// `master` the effects of `+0x640` (its `PERCENT PET DAMAGE` 0x5b), `scale`
/// `+0x720`.
int max_damage(int base, int type, int stat, const CharacterEffects& effects,
               const WeaponTraits& weapon, const CharacterEffects& master, float scale);

/// `CEquipment::calculateCombatStats`, the
/// damage half, for one copy of an item: one volatile `MINDAMAGE..MAXDAMAGE`
/// roll (the top of it for an heirloom), scaled by the two damage mods, through
/// `setGraphDamage`, then
/// `split_weapon_damage`. Sets `combat_rolled`, `damage`, `kept_damage` and
/// `inherent_damage`. `heirloom` is `+0x28c`.
void roll_weapon_damage(ItemStats& item, bool is_weapon, const StatBook& graphs, RunicRandom& rolls,
                        int heirloom = 0);

/// `CEquipment::calculateInherentDamages`,
/// what `calculateCombatStats(bool)`
/// ends on and a loaded copy's `applySaveState` ends on: the inherent damage
/// emptied, then for a weapon `damage` split by `DAMAGE_<TYPE>` - each
/// elemental share into `inherent_damage`, `kept_damage` the figure before the
/// physical share, `damage` that share (all of it when `DAMAGE_PHYSICAL` is
/// -1).
void split_weapon_damage(ItemStats& item, bool is_weapon);

/// `CEquipment::calculateCombatStats`, the
/// armour half, which runs before the damage half and draws first. `graph_ac`
/// takes `ARMOR`; when that is 0 and `ARMORMIN` and `ARMORMAX` both are not,
/// one volatile `randomIntegerBetween(min, max(min, max))`; either figure,
/// scaled by `RARITY_AMR_MOD` and `SPECIAL_AMR_MOD`, goes through `setGraphAC`
/// (the `ARMOR_PLAYER_BYLEVEL_FORSET` curve at
/// the item level, at least 1). Then, for an `Armor` (`is_armor`, ISA 0xd):
/// unless `from_save`, each elemental `ARMOR_*` above 0 adds a passive effect
/// of that percentage of the points to `combat_effects`; and an authored
/// `ARMOR_PHYSICAL` replaces the points with its percentage of them, at least
/// 1. `heirloom` is `+0x28c`.
void roll_item_armour(ItemStats& item, bool is_armor, bool from_save, const StatBook& graphs,
                      RunicRandom& rolls, int heirloom = 0);

/// `CEquipment::calculateCombatStats(bool from_save)`,
/// both halves in the original's order:
/// `roll_item_armour`, then `roll_weapon_damage`.
void calculate_combat_stats(ItemStats& item, bool is_weapon, bool is_armor, bool from_save,
                            const StatBook& graphs, RunicRandom& rolls, int heirloom = 0);

/// `CEquipment::addInherentDamage(EDAMAGE_TYPES, int)`
/// and `addDamageBonus(EDAMAGE_TYPES, int)`:
/// the amount onto the type's entry of
/// `+0x380` or `+0x368`, the type appended to `+0x350` when it is new (with a 0
/// in the other list).
void add_inherent_damage(ItemStats& item, int type, int amount);
void add_damage_bonus(ItemStats& item, int type, int amount);

/// One blow, as `rollAttack` resolves it before `applyDamage`.
struct Blow {
    int damage = 0;  ///< after armour; 0 when blocked
    bool critical = false;
    bool blocked = false;
    bool glancing = false;  ///< armour took it down to 1
    bool interrupts = false;
    float knockback = 0.0f;
    float direction_jitter = 0.0f;  ///< degrees, `randomBetweenVolatile(-10, 10)`
    /// What each damage type dealt after the defender's defence, as the pairs
    /// `rollAttack` hands `applyDamageEffects`: the physical roll under the
    /// attack's own type, each bonus type under its own.
    std::array<int, kDamageTypeCount> by_type{};
    int parts = 0;  ///< how many pairs
};

/// `CCharacter::getBlockChance()` for a character holding only
/// `effects`: `PERCENT BLOCK CHANCE BASE` (0x3e) plus `PERCENT BLOCK CHANCE`
/// (0x21), at most 50.
int block_chance(const CharacterEffects& effects);

/// An instant `DAMAGE` effect resolved, before `applyDamage`.
struct InstantDamage {
    /// False for a value that is not below 0 once made negative (0): the
    /// console hands it to `modifyHP` (a heal of 0)
    /// and nothing else of the case runs - no block, no roll.
    bool applied = true;
    bool blocked = false;
    bool critical = false;
    bool interrupts = false;  ///< a critical the target's resistance did not stop
    int damage = 0;           ///< after the target's defence, at least 1
    float force = 0.0f;       ///< the knock-back handed to `applyDamage`
};

/// `CCharacter::applyInstantEffect(CEffect*)`,
/// case `DAMAGE` (0x34), from `blocked()` to
/// the arguments of `applyDamage`: `value` is the effect's base value (`+0xc0`)
/// with a sign that does not matter, `type` its damage type (`+0x14`),
/// `force_min` and `force_max` its values 2 and 3. `attacker` is the effect's
/// owner (`+0x48`) when it is a character, null for none: its `PERCENT DAMAGE
/// BONUS` (0x19) of the type and of all, its `rollCritical` and `PERCENT
/// CRITICAL DAMAGE` (0x59).
InstantDamage instant_damage(float value, int type, float force_min, float force_max,
                             const Defender& target, const CharacterEffects* attacker,
                             RunicRandom& rolls);

/// The affix a blow's elemental part leaves on its target.
struct DamageStatus {
    int type = -1;                ///< 2 fire, 3 ice, 4 electric, 5 poison; -1 none
    const char* affix = nullptr;  ///< `DAMAGEEFFECT*`, the `PLAYER` twin on a player
    float duration_min = 0.0f;
    float duration_max = 0.0f;
};

/// `CCharacter::applyDamageEffects(CCharacter*, float, int (*)[2], unsigned,
/// bool)`, up to the affix: each type's
/// damage (physical left out) against the target's own `+0x620` of that type -
/// `damage / ((defence + damage) * 5)` on one volatile draw, no draw when
/// `forced` (a critical) - and the first type that passes names the affix. A
/// type with no affix (magical, all) ends the walk with nothing. `amount` is
/// the blow, `parts` the pair count: a single physical pair does nothing at
/// all. The duration is drawn by the caller between the two bounds once the
/// affix is found or added.
DamageStatus roll_damage_status(float amount, const std::array<int, kDamageTypeCount>& by_type,
                                int parts, int first_type,
                                const std::array<int, kDamageTypeCount>& own_defense, bool forced,
                                bool target_is_player, RunicRandom& rolls);

/// `CCharacter::rollCritical(target)`:
/// `chance` is the attacker's `CRITICAL CHANCE` (0x37).
bool roll_critical(RunicRandom& rolls, float chance = 0.0f);

/// A weaponless unit's `maxDamage`: the share
/// is `PERCENT MELEE DAMAGE` (0xf) and `PERCENT DAMAGE BONUS` (0x19, of the
/// attack's type and of all) over 100 - its `strength()` is 0 - with the
/// master's (`+0x640`) `PERCENT PET DAMAGE` (0x5b), and the flat
/// `MELEEDAMAGEBONUS` (0) and `DAMAGE BONUS` (10) of the attack's type.
int max_damage(const UnitCombat& attacker, const EffectManager* effects = nullptr,
               const CharacterEffects& master = {});

/// `CCharacter::modifyDamage(damage, type, maximum, factor)`:
/// the armour or the type's defence rolled
/// and taken off in proportion, `damageDefensePercent` (`PERCENT DAMAGE
/// TAKEN`) applied, the defender's `SHIELD BUFFER`s spent
/// (`EffectManager::spend_shield_buffers`) and `on_absorbed` called when they
/// took it all.
int modify_damage(const Defender& defender, int damage, int type, int maximum, float factor,
                  RunicRandom& rolls);

/// `CCharacter::blocked()`.
bool blocked(const Defender& defender, RunicRandom& rolls);

/// `CCharacter::rollAttack(level, target, weapon, 0, 1, 1, 7)`.
///
Blow roll_attack(const Strike& strike, const Defender& defender, RunicRandom& rolls);

/// The same for a monster's own melee blow: no weapon, no strength, and the
/// attack's own damage of each type past physical as the bonus. A skill
/// event's weapon attack passes its `WEAPONDAMAGEPCT` and `SOAKSCALEPCT` as the
/// two scales (`CSkillEvent::applyWeaponDamage`).
Blow roll_attack(const UnitCombat& attacker, const Defender& defender, RunicRandom& rolls,
                 const EffectManager* effects = nullptr, float damage_scale = 1.0f,
                 float soak_scale = 1.0f, const CharacterEffects& master = {});

// What `CCharacter::rollAttack` puts on the
// one it struck once `applyDamageEffects` has run and before `applyDamage`:
// a knock-back, a degraded armour, and the
// attacker's `TRANSFER` effects.

/// `KNOCK BACK` (0xc) and `KNOCK BACK EFFECT` (0x2b).
inline constexpr int kEffectTypeKnockBack = 0xc;
inline constexpr int kEffectTypeKnockBackEffect = 0x2b;
/// `DEGRADE ARMOR` (0x36), whose holder's blows leave `DEGRADE ARMOR EFFECT`
/// (0x42, `kEffectTypeDegradeArmorEffect`).
inline constexpr int kEffectTypeDegradeArmor = 0x36;
/// `rollAttack`'s transfer loop runs the effect types 0 to 0x91.
inline constexpr int kTransferEffectTypes = 0x92;

/// The `KNOCKBACK` effect: `knock_back` is the attacker's
/// `KNOCK BACK` with the weapon's own and its socketed items' added (each
/// `getEffectValue(DYNAMIC, 0xc, 0, 7)`, vtable +0x120). A `DYNAMIC` `KNOCK
/// BACK EFFECT` lasting `(int)(knock_back * 0.5 + 2)` seconds with the force
/// `knock_back * 0.1 + 2` (both fused), named through `setName` (inlined:
/// upper-cased), pointing from the attacker to the target on the ground -
/// normalised only when longer than 1e-8. Made only when `knock_back` is above
/// 0; `addNewEffect` holds it on the target as it is, with no
/// `applyEffectOnUnit`.
Effect knock_back_effect(float knock_back, const Vec3& attacker, const Vec3& target);

/// The `DEGRADEARMOR` effect for the attacker's `DEGRADE ARMOR`
/// above 0: a `DYNAMIC` `DEGRADE ARMOR EFFECT` of 10 that lasts `(int)degrade`
/// seconds, held through `addNewEffect`. Not exclusive: each blow adds one.
Effect degrade_armor_effect(float degrade);

/// `MISSILE REFLECT` (0x3c) and `MISSILE REFLECT VISUAL` (0x3d).
inline constexpr int kEffectTypeMissileReflect = 0x3c;
inline constexpr int kEffectTypeMissileReflectVisual = 0x3d;

/// `CCharacter::getDmgToReflectFromMissile()`:
/// the share of a missile's damage a character
/// sends back, 0 for none. `chance` is its `MISSILE REFLECT`
/// (`getEffectValue(0x3c, 7)`, its inventory's included): above 0, a volatile
/// roll of 0..100 at most `chance` reflects. The share's two ends are the third
/// and fourth numbers of the character's own first `MISSILE REFLECT`
/// (`getEffect`), each raised to a worn item's larger (the 12 equip locations,
/// when it has an inventory); 1.0 when neither end is above 0, else a volatile
/// roll between them - or the low end when the high one is not above 0 - times
/// 0.01.
float dmg_to_reflect_from_missile(float chance, const EffectManager* own, bool has_inventory,
                                  const std::array<const EffectManager*, 12>& worn,
                                  RunicRandom& rolls);

/// The `MISSILE REFLECT VISUAL` effect `rollAttack` hands an attacker whose
/// ranged weapon struck a reflecting character: 0.3 s, a value
/// of 1, `PASSIVE`, the constructor's `bool` set, and the reflected share as
/// its third number (`+0xe8`, X360). When it runs out, the reflector strikes
/// back with the attacker's weapon at that share (`updateEffects`).
Effect missile_reflect_visual_effect(float share);

/// `rollAttack`'s transfer loop: for each effect type from 0 to
/// 0x91, `transferEffects` from the attacker's own manager (X360 `+0x17c`),
/// then - when it has an inventory (`+0x3e8`) - the inventory's (`+0x10`, the
/// set bonuses) and the item at each of the 12 equip locations' own
/// (`CInventory::transferEffects`, inlined on the console). A source
/// left null is skipped, as the original skips an absent manager.
struct AttackEffectSources {
    const EffectManager* own = nullptr;
    bool has_inventory = false;
    const EffectManager* inventory = nullptr;
    std::array<const EffectManager*, 12> worn{};
};
void transfer_attack_effects(const AttackEffectSources& sources,
                             const std::function<bool(const Effect&)>& valid,
                             const std::function<void(Effect&)>& apply);

}  // namespace tl::core
