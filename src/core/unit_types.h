// The unit type hierarchy (`media/UNITTYPES.HIE`) and the ids `CBaseUnit::ISA` is asked about.
// The ids are the file's own `ID`s.
#pragma once

#include "formats/adm.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// The ids the transcribed call sites pass to `CBaseUnit::ISA`.
namespace unit_type {
inline constexpr uint32_t kAny = 0;
inline constexpr uint32_t kCharacter = 25;
inline constexpr uint32_t kMerchant = 41;               ///< 0x29
inline constexpr uint32_t kTownPortal = 43;             ///< 0x2b
inline constexpr uint32_t kNpc = 104;                   ///< 0x68
inline constexpr uint32_t kGambler = 127;               ///< 0x7f
inline constexpr uint32_t kStash = 128;                 ///< 0x80
inline constexpr uint32_t kEnchanter = 130;             ///< 0x82
inline constexpr uint32_t kInteractiveCharacter = 131;  ///< 0x83
inline constexpr uint32_t kSocketableRecoverer = 132;   ///< 0x84
inline constexpr uint32_t kSocketableDestroyer = 133;   ///< 0x85
inline constexpr uint32_t kCombiner = 134;              ///< 0x86
/// `CCharacter::getItemAI`: a targeted prop
/// of these types is not interacted with but puts the player in AI state 0x16,
/// 0x17 or 0x18.
inline constexpr uint32_t kEnchantShrine = 157;  ///< 0x9d, `ENCHANTSHRINE`
inline constexpr uint32_t kStatShrine = 158;     ///< 0x9e, `STATSHRINE`
inline constexpr uint32_t kGemShrine = 159;      ///< 0x9f, `GEMSHRINE`
/// `CCharacter::getItemAI`: a targeted prop
/// of this type starts fishing (`startFishing`, vtable +0x338) rather than
/// being interacted with.
inline constexpr uint32_t kFishingHole = 122;   ///< 0x7a, `FISHING HOLE`
inline constexpr uint32_t kSharedStash = 170;   ///< 0xaa
inline constexpr uint32_t kWaypointNode = 171;  ///< 0xab
/// `CTriggerUnit::trigger` and the level's
/// temporary-portal walks: a map scroll's portal,
/// and a portal a skill opens (the phase beast's, the Mystery Portal).
inline constexpr uint32_t kTempPortal = 165;           ///< 0xa5, `TEMPPORTAL`
inline constexpr uint32_t kMapPortal = 173;            ///< 0xad, `MAPPORTAL`
inline constexpr uint32_t kRetirementCharacter = 175;  ///< 0xaf

// `CInventory`/`CEquipment`'s pane and equip-location gate types, and those
// `CCharacter::unitInit` branches on;
// `gEQUIP_UNITTYPES_PANE`/`_ALLOWED`/`_DISALLOWED`.
inline constexpr uint32_t kWeapon = 8;
inline constexpr uint32_t kOneHanded = 9;  ///< `[One Handed]`, `CInventory`'s hand rules
inline constexpr uint32_t kTwoHanded = 10;
inline constexpr uint32_t kHelmet = 12;
inline constexpr uint32_t kArmor = 13;
inline constexpr uint32_t kRing = 17;
inline constexpr uint32_t kBoots = 16;
inline constexpr uint32_t kBelt = 18;
inline constexpr uint32_t kShoulderArmor = 20;
inline constexpr uint32_t kShield = 21;
inline constexpr uint32_t kNone = 22;  ///< the gate tables' "no second type" sentinel
inline constexpr uint32_t kGloves = 23;
inline constexpr uint32_t kNecklace = 24;
inline constexpr uint32_t kPlayer = 28;   ///< 0x1c
inline constexpr uint32_t kTrinket = 39;  ///< 0x27, a pet's own equip type
inline constexpr uint32_t kBow = 36;
inline constexpr uint32_t kCrossbow = 110;  ///< 0x6e
inline constexpr uint32_t kRifle = 116;     ///< 0x74
/// `NO TRAIL`: a weapon `CCharacter` draws no weapon trail for
/// (`selectAttack`, `updateAnimation`).
/// Every bow is one.
inline constexpr uint32_t kNoTrail = 102;  ///< 0x66
/// 0x5a, `createParticles`' `_PISTOL` particles
inline constexpr uint32_t kPistol = 90;
/// 0x23, dexterity instead of strength in `maxDamage`/`rollAttack`
inline constexpr uint32_t kRangedWeapon = 35;
/// `Melee`: what `CCharacter::meleeRange` and `selectAttack(1)`
/// look for in a hand.
inline constexpr uint32_t kMelee = 60;  ///< 0x3c
/// The weapon kinds the console's facing arc is read off.
inline constexpr uint32_t kSword = 11;     ///< 0xb
inline constexpr uint32_t kAxe = 44;       ///< 0x2c
inline constexpr uint32_t kStaff = 61;     ///< 0x3d
inline constexpr uint32_t kMace = 89;      ///< 0x59
inline constexpr uint32_t kPolearm = 105;  ///< 0x69
/// The console aims a `Wand`'s missile at an item in view.
inline constexpr uint32_t kWand = 98;  ///< 0x62
inline constexpr uint32_t kChestArmor = 15;
/// `CCharacter::maximumDamageForDisplay` adds
/// the matching `PERCENT <KIND> ITEM DAMAGE BONUS` for a weapon of each
/// category.
inline constexpr uint32_t kItemCategoryMartial = 162;  ///< 0xa2
inline constexpr uint32_t kItemCategoryRanged = 163;   ///< 0xa3
inline constexpr uint32_t kItemCategoryMagic = 164;    ///< 0xa4
inline constexpr uint32_t kBreakable = 29;             ///< 0x1d, no floor label by distance
inline constexpr uint32_t kContainer = 30;  ///< 0x1e, no floor label for being highlighted
inline constexpr uint32_t kTakeable = 31;   ///< 0x1f, the general backpack pane
inline constexpr uint32_t kGold = 34;       ///< 0x22, `CItemGold` (`UNITTYPE:GOLD`)
inline constexpr uint32_t kMimic = 167;     ///< 0xa7
/// `PETNOFOLLOW`: a pet that keeps no station by its master (`idleAI`
/// `fleeAI`, `updateAnimation`'s speed match).
inline constexpr uint32_t kPetNoFollow = 169;  ///< 0xa9
inline constexpr uint32_t kPet = 87;           ///< 0x57
inline constexpr uint32_t kFish = 124;
inline constexpr uint32_t kQuestItem = 103;  ///< 0x67, `[QuestItem]`
/// `CItem::isUseable`
inline constexpr uint32_t kConsumable = 1;
inline constexpr uint32_t kInteractable = 32;  ///< 0x20, the other half of it
inline constexpr uint32_t kPotion = 33;        ///< 0x21
inline constexpr uint32_t kPetOnly = 125;      ///< 0x7d
inline constexpr uint32_t kNoPets = 126;       ///< 0x7e
inline constexpr uint32_t kSpell = 129;        ///< 0x81
/// `[item]`, the target-type gate in
/// `CSkill::requirementsMet`
inline constexpr uint32_t kItem = 2;
inline constexpr uint32_t kLeftHanded = 38;  ///< 0x26, `[Left Handed]`
/// Item quality: every concrete item type carries one of these.
/// `CEquipment::enchant` picks the affix-slot range by them.
inline constexpr uint32_t kUnique = 54;  ///< 0x36, `UNIQUE`
inline constexpr uint32_t kMagic = 55;   ///< 0x37, `MAGIC`
/// `CEquipment::isMagical`: never magical;
/// `CEquipment::unitInit`: identified after
/// the roll.
inline constexpr uint32_t kSocketable = 120;  ///< 0x78, `SOCKETABLE`
/// `CEquipment::enchant`: forces the random roll, as `enchant(true)` does.
inline constexpr uint32_t kRandomMagic = 135;  ///< 0x87, `RANDOMMAGIC`
/// `enchant`/`canEnchant`: the fifth enchantable kind beside
/// Weapon, Armor, Ring and Necklace; also in `CSpawnClassData`'s equipment test.
inline constexpr uint32_t kRandomMagicSocketable = 160;  ///< 0xa0, `RANDOMMAGIC SOCKETABLE`
/// 0x1b, `[Monster]`; picks `MONSTERCOOLDOWNMS` in
/// `CSkillProperty::getCoolDown`
inline constexpr uint32_t kMonster = 27;
}  // namespace unit_type

class UnitTypeHierarchy {
public:
    static constexpr uint32_t kUnknown = 0xffffffffu;

    /// `CHierarchy::parseHierachy(CDataGroup*)`:
    /// one group per type under `UNITTYPES`, keyed by `ID`; every other value (the
    /// `child...` lines, which are the type's ancestors) is appended in file order.
    void parse(const tl::formats::AdmSection& root);

    bool empty() const { return by_name_.empty(); }

    /// `CHierarchy::getTypeIDByName`, matched
    /// upper-cased.
    uint32_t id_of(const std::string& name) const;

    /// Reverse lookup: type name from ID, or empty string if not found.
    std::string name_of(uint32_t id) const;

    /// `CHierarchy::ISA(unsigned, unsigned)`:
    /// the type itself, or any id in its list.
    bool isa(uint32_t type, uint32_t want) const;
    bool isa(const std::string& type_name, uint32_t want) const {
        return isa(id_of(type_name), want);
    }

private:
    std::map<std::string, uint32_t> by_name_;
    std::map<uint32_t, std::vector<uint32_t>> ancestors_;
};

}  // namespace tl::core
