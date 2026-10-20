// Procs: the skills a unit's effects make it cast (`MIN` the chance in percent), and a weapon's
// own skills (`ItemStats::skills`). Run around a blow by `CCharacter::performAttack`.
// Pure: no OGRE.
#pragma once

#include "core/collision.h"
#include "core/effect_manager.h"
#include "core/effects.h"
#include "core/random.h"
#include "core/units.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tl::core {

class StatBook;

inline constexpr int kEffectCastSkill = 0x4a;          ///< `CAST SKILL`
inline constexpr int kEffectCastSkillOnTarget = 0x4b;  ///< `CAST SKILL ON TARGET`
inline constexpr int kEffectCastSkillAtTarget = 0x4c;  ///< `CAST SKILL AT TARGET`
inline constexpr int kEffectCastSkillOnStruck = 0x51;  ///< `CAST SKILL ON STRUCK`
inline constexpr int kEffectCastSkillOnDeath = 0x69;   ///< `CAST SKILL ON DEATH`

/// `CBaseUnit::activateEffect`'s test: `type - 0x4a` below 0x20 and
/// a bit of 0x80000087 - the five types above.
bool effect_teaches_skill(int type);

/// `CSkill::_setLevelOfSkillFromSkillManager(unsigned int)`:
/// -1 is 1; anything else is held to the
/// skill's level count (`+0xa8`, 1 when the file has none). 0 stays 0.
uint32_t skill_level_from_manager(uint32_t level, uint32_t level_count);

/// What the skill list needs of a skill file: `CSkillParser::getSkill`'s
/// template (`CSkill::CSkill`).
struct SkillFileFacts {
    std::string name;          ///< `NAME`, as the file spells it
    uint32_t level_count = 0;  ///< `+0xa8`, the `[LEVELn]` blocks
    bool hidden = false;       ///< `+0x6b`, `HIDDEN`
    int32_t charges = -1;      ///< `+0x124`, `CHARGES`
};
/// The skill file by name, matched without case; nothing when none is called
/// that.
using SkillFileLookup = std::function<std::optional<SkillFileFacts>(const std::string& name)>;

/// One skill a unit's skill manager knows (`CSkillManager +0x60`), as far as
/// the procs ask about it.
struct SkillCopy {
    std::string name;     ///< the file's `NAME`
    uint32_t level = 0;   ///< `+0xdc`
    bool enabled = true;  ///< `+0x6d`
    bool hidden = false;  ///< `+0x6b`
    /// `+0x124`: `CHARGES`, overwritten with the effect's `MAX` by `activateEffect`. Not read
    /// here; `CSkill::requirementsMet` refuses at 0 and
    /// `decrementSkillCharges` counts it down.
    int32_t charges = -1;
};

/// A unit's known list (`CSkillManager +0x60`), for procs.
class SkillCopies {
public:
    /// `CSkillManager::CSkillManager(rm, owner)`. `one_per_name`:
    /// a second `add` of a name re-levels the copy held instead of adding another (the player's
    /// list, `Renderer::Impl::player_proc_skills`).
    explicit SkillCopies(bool one_per_name = false) : one_per_name_(one_per_name) {}

    /// `CBaseUnit::addSkillByName(name, true)` ->
    /// `CSkillManager::addSkill`: always a new copy, then
    /// `setSkillLevel` re-levels every copy of the name.
    /// Null when no file is called that.
    SkillCopy* add(const std::string& name, uint32_t level, const SkillFileLookup& files);

    /// `CSkillManager::getSkill(const std::wstring&, int)`:
    /// the first copy of the name (case-blind) at `level`.
    const SkillCopy* find(const std::string& name, uint32_t level) const;

    /// A copy as a list the caller keeps already holds it (a placed unit's
    /// known list, `LevelScript::UnitCasting::skills`), at the end.
    // PORT: an accessor of a list of skill copies.
    void push_back(const SkillCopy& copy) { copies_.push_back(copy); }

    // PORT: an accessor of a list of skill copies.
    const std::vector<SkillCopy>& copies() const { return copies_; }
    bool empty() const { return copies_.empty(); }

private:
    bool one_per_name_ = false;
    std::vector<SkillCopy> copies_;
};

/// `CBaseUnit::activateEffect(CEffect*)`: a cast-skill effect
/// teaches its skill at the effect's level, the copy's charges its `MAX` (-1 when 0), and takes
/// the copy's level back (0 above 1000).
void activate_effect(Effect& effect, SkillCopies& skills, const StatBook& graphs,
                     const SkillFileLookup& files);

/// The effects and the skills of one unit that holds procs: the character, or
/// a worn item. `skills` null is a unit with no skill manager (`+0x1c8`).
struct ProcHolder {
    const EffectManager* effects = nullptr;
    const SkillCopies* skills = nullptr;
};

/// One cast `executeProcs` makes, handed to the caller as it is made.
struct ProcCast {
    const SkillCopy* skill = nullptr;  ///< what `getSkill(NAME, +0x10)` found
    /// -1: the character's own manager runs it; 0..11, the manager of the item
    /// worn at that equip location.
    int location = -1;
    /// The character's own `CAST SKILL AT TARGET`: the skill starts at the
    /// target's place, and aims there (both the character's with no target).
    /// Otherwise it starts at the character and aims at the target, or at the
    /// character with none.
    bool from_target = false;
    /// The character's own casts but `AT TARGET`: `+0x708 = 1.0` first, the second
    /// `applyDamage` waits out before its next `CAST SKILL ON STRUCK` roll.
    bool lockout = false;
};

/// `CCharacter::executeProcs(EEFFECT_TYPE, CBaseUnit*)`, then
/// `CInventory::executeProcs` over the 12 equip locations. `cast` runs
/// at once for each skill found, so its own draws come between the rolls as the original's do.
void execute_procs(int type, const ProcHolder& character, const std::array<ProcHolder, 12>& worn,
                   const StatBook& graphs, RunicRandom& rolls,
                   const std::function<void(const ProcCast&)>& cast);

/// The skills a weapon copy knows, in the order its `unitInit`
/// and `applySaveState` teach them: its own `[EFFECT]`s, its
/// `[SKILL]` blocks, then the rest of its effects. `context` is the item's effect context.
SkillCopies weapon_known_skills(const ItemStats& item, const EffectManager::Context& context,
                                const StatBook& graphs, const SkillFileLookup& files);

/// The same copy's `CEffectManager`, its cast-skill effects levelled as
/// `activateEffect` leaves them: what `CInventory::executeProcs` walks.
EffectManager weapon_proc_effects(const ItemStats& item, const EffectManager::Context& context,
                                  const StatBook& graphs, const SkillFileLookup& files,
                                  SkillCopies* skills = nullptr);

/// `CCharacter::doWeaponSkill(CEquipment*)`'s two points,
/// every skill's the same. `hand` is the hand's tag point: the
/// left's for a weapon worn at location 1.
struct WeaponSkillPoints {
    Vec3 start;
    Vec3 aim;
};
WeaponSkillPoints weapon_skill_points(const Vec3& hand, const Vec3& position, float radius,
                                      const Vec3& forward);

/// `doWeaponSkill`'s pick: a copy that is enabled (`+0x6d`) and not hidden
/// (`+0x6b`) is run.
bool weapon_skill_runs(const SkillCopy& skill);

/// `CCharacter::doWeaponProcs(CEquipment*, CBaseUnit*)`'s one
/// point, where every skill the weapon knows starts and aims.
Vec3 weapon_proc_point(const Vec3& attacker, const Vec3& target, float target_radius);

}  // namespace tl::core
