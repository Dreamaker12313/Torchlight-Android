// The player's pet: what outlives a level (the unit it is, the stance, the bag, the spells, the
// effects) and a mirror of what the level script last said of it. Its AI runs in the level
// script like any monster's: `CMonster::idleAI`,
// `idleAINormal`, `huntAI`,
// `fleeAI`, `CCharacter::isPetNearDeath`,
// `CCharacter::sendToTown`,
// `returnToTownAI`, `updateInTownTimer`.
//
// No OGRE and no file I/O.
#pragma once

#include "core/collision.h"
#include "core/effect_manager.h"
#include "core/inventory.h"
#include "core/skills.h"
#include "core/units.h"

#include <memory>
#include <string>

namespace tl::core {

/// The pet HUD's stance tray: `PetAggressive`, `PetDefensive`, `PetPassive`. The original's
/// numbers are `pet_stance_number()`'s.
enum class PetStance { Aggressive, Defensive, Passive };

/// What the pet is doing, as the HUD, the menus and the pet commands ask it: the level script's
/// AI state folded into the cases they tell apart.
enum class PetState {
    Following,  ///< idle, or any state not below
    Attacking,  ///< hunting, swinging, or in a skill state
    Fleeing,    ///< state 10
    Departing,  ///< state 0x29, `returnToTownAI`
    InTown,     ///< state 0x2a, `updateInTownTimer`
};

/// The fold.
PetState pet_state_of_ai(int32_t ai_state);

/// The stance a new character's pet takes: `CGameClient::setCreationPet`
/// writes 2.
inline constexpr PetStance kDefaultStance = PetStance::Defensive;

/// What the level script last said about the animal, copied each frame.
struct PetMirror {
    Vec3 position;
    float facing = 0.0f;
    float health = 1.0f;
    float max_health = 1.0f;
    int32_t ai_state = 2;
    bool running = false;
    bool pathing = false;
    bool near_death = false;
    float town_timer = 0.0f;
    /// What a fish's transformation has left (-999 for a permanent one, 0 for none) and the
    /// unit the pet first was.
    float transform_left = 0.0f;
    std::string transformed_from;
};

class Pet {
public:
    /// The bag is the player's pet's: `canEquip` lets it wear trinkets only.
    Pet() : effects_(std::make_shared<EffectManager>()) { bag_.set_players_pet(true); }

    /// A pet of this unit, standing at `at`. The bag, the stance, the spells
    /// and the effects are kept: a level change is not a new animal.
    void spawn(const std::string& unit, const UnitStats& stats, const Vec3& at, float facing);

    /// The same pet as another unit: `CCharacter::convertCharacter`
    /// re-makes the character where it
    /// stands, and everything but what the unit's record says is kept.
    void become(const std::string& unit, const UnitStats& stats) {
        unit_ = unit;
        stats_ = stats;
    }

    /// Whether there is a pet at all. False before `spawn()`.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    bool alive() const { return !unit_.empty(); }

    /// Forget everything. A new game, or a level that has no pet in it.
    void clear();

    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const std::string& unit() const { return unit_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const UnitStats& stats() const { return stats_; }

    /// Copy what the level script last said about it.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    void mirror(const PetMirror& from) { mirror_ = from; }

    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const Vec3& position() const { return mirror_.position; }
    float facing() const { return mirror_.facing; }
    /// Running rather than walking.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    bool running() const { return mirror_.running; }
    /// Somewhere to be.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    bool has_destination() const { return mirror_.pathing; }

    /// Whether the pet is in the world right now. False while it is in town.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    bool in_world() const { return state() != PetState::InTown; }

    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    PetState state() const { return pet_state_of_ai(mirror_.ai_state); }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    int32_t ai_state() const { return mirror_.ai_state; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    PetStance stance() const { return stance_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    void set_stance(PetStance stance) { stance_ = stance; }
    void cycle_stance();

    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    float health() const { return mirror_.health; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    float max_health() const { return mirror_.max_health; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    float health_fraction() const {
        return mirror_.max_health > 0.0f ? mirror_.health / mirror_.max_health : 0.0f;
    }

    /// `isPetNearDeath()`, as the script last answered it. The HUD's
    /// `FleeingText` is driven by exactly this.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    bool near_death() const { return mirror_.near_death; }

    /// What the pet holds: a potion's effects and its spells' buffs. Shared with the level
    /// script's character; survives a fish's turn and a level change, as the bag does.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    EffectManager& effects() { return *effects_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const EffectManager& effects() const { return *effects_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const std::shared_ptr<EffectManager>& shared_effects() const { return effects_; }
    /// A new, empty manager: a warp's, whose carried one points into the
    /// tables of the renderer that was left.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    void reset_effects() { effects_ = std::make_shared<EffectManager>(); }

    /// The two spells a pet learns from scrolls (`learnSpell`,
    /// `canLearnSpell`); its own `[SKILL]` attacks are not in it.
    SkillBook& skills() { return skills_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const SkillBook& skills() const { return skills_; }

    /// The pet's own level and experience; its master's awards add to them
    /// (`CCharacter::awardExperience`). A fish's turn keeps them.
    int level() const { return level_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    int32_t experience() const { return experience_; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    void set_progress(int level, int32_t experience) {
        level_ = level;
        experience_ = experience;
    }

    /// Seconds until it is back from town. Zero when it is not away.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    float town_seconds_left() const { return mirror_.town_timer; }

    /// The transformation, as the script last answered it.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    float transform_left() const { return mirror_.transform_left; }
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    const std::string& transformed_from() const { return mirror_.transformed_from; }
    /// Both, for a pet not yet placed: a saved pet's
    /// (`CCharacter::applySaveState`), or none for a
    /// different animal.
    void set_transformation(float left, const std::string& from) {
        mirror_.transform_left = left;
        mirror_.transformed_from = from;
    }

    /// `0:00:00`, the format `petreturntime2` is authored with.
    std::string town_clock() const;

    /// The pet's own bag. What the town trip sells.
    // PORT: an accessor of the port's pet record (what the level machine last said of it).
    Inventory& bag() { return bag_; }
    const Inventory& bag() const { return bag_; }

    /// The bag half of `CCharacter::updateInTownTimer`:
    /// sells every copy past the worn slots (0 to 0xb) that is not a quest unit (`getIsQuestUnit`)
    /// for `CEquipment::sellPrice()` (`CCharacter::giveGold`).
    /// Returns the gold; `sold` is the copies that went.
    template <typename PriceOf, typename QuestUnit>
    int sell_bag(const PriceOf& price, const QuestUnit& quest_unit, int& sold) {
        int gold = 0;
        sold = 0;
        for (size_t slot = 0; slot < bag_.slots().size(); ++slot) {
            const InventorySlot& held = bag_.slots()[slot];
            if (held.count <= 0) continue;
            if (quest_unit(held.item)) continue;
            if (!(0xb < slot)) continue;
            const int count = held.count;
            gold += price(held.item, count);
            ++sold;
            bag_.remove_item(slot, count);
        }
        return gold;
    }

private:
    std::string unit_;
    UnitStats stats_;
    PetStance stance_ = kDefaultStance;
    PetMirror mirror_;
    Inventory bag_;
    std::shared_ptr<EffectManager> effects_;
    SkillBook skills_;
    /// The level it was made at (`unitInit`; no pet authors a `LEVEL`):
    /// `CResourceManager::createMonster` (from
    /// `setCreationPet` and `CGameClient::loadLevel`) gives the level's monster level, 1 in town.
    int level_ = 1;
    int32_t experience_ = 0;
};

}  // namespace tl::core
