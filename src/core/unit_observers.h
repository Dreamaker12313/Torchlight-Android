// Who is watching a unit: the level's map from a unit GUID to its observers (`CLevel
// +0x200` on the console) and the four methods over it. Listeners: spawners
// (`CUnitSpawner::addSpawnedUnit`) and skills with a timed effect on
// the unit (`CSkillEffectAndAffixes::applyAffixesAndEffects`;
// `CSkill::unitStateChange`). Broadcasters: `CBaseUnit::~CBaseUnit`,
// `CCharacter::die`, `CItem::interact`, `CEquipment::addedToInventory`,
// `CCharacter::updateHP`.
// No OGRE and no file I/O.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace tl::core {

/// `EUNIT_STATES`, the values broadcast.
namespace unit_state {
inline constexpr int kDestroyed = 0;
inline constexpr int kDied = 1;
inline constexpr int kInteracted = 2;
inline constexpr int kPickedUp = 3;
inline constexpr int kDiedOfEffect = 4;
}  // namespace unit_state

/// `iUnitObserver`: what the map holds. `unitStateChange` is its one method
/// (vtable +0x4 on the console).
class UnitObserver {
public:
    virtual ~UnitObserver() = default;
    /// `unit` is the one that broadcast, as the map knows it.
    virtual void unit_state_change(const void* machine, int64_t unit, int state) = 0;
};

/// The map itself. A unit is named by the machine that holds it and its node,
/// the port's stand-in for the original's GUID (a null machine is the player).
class UnitObservers {
public:
    struct Unit {
        const void* machine = nullptr;
        int64_t node = 0;
        bool operator<(const Unit& other) const {
            if (machine != other.machine) return machine < other.machine;
            return node < other.node;
        }
    };

    /// `CLevel::addListenerToUnit(long long, iUnitObserver*)`
    /// (the `CBaseUnit*` overload, is inline on the
    /// console). GUID -1 is refused; no duplicates.
    void add(const Unit& unit, UnitObserver* observer);

    /// `CLevel::removeListenerFromUnit(CBaseUnit*, iUnitObserver*)`
    /// (the `long long` overload, is
    /// inline on the console). `TArrayList::remove` moves the last
    /// entry into the gap; an emptied list stays in the map.
    void remove(const Unit& unit, UnitObserver* observer);

    /// `CLevel::removeListenerFromUnits(iUnitObserver*)`:
    /// from every list (`CSkill::~CSkill`, `CUnitSpawner::~CUnitSpawner`).
    void remove_all(UnitObserver* observer);

    /// `CLevel::unitBroadcastMessage(CBaseUnit*, EUNIT_STATES)` (inline
    /// on the console). In list order, the count reread each
    /// time; an observer removing itself makes the walk skip the one swapped in - kept.
    /// State 0 erases the entry afterwards.
    void broadcast(const Unit& unit, int state);

    /// Everything forgotten: the level is going.
    // PORT: an accessor of the port's own bookkeeping.
    void clear() { lists_.clear(); }

    /// The unit's list, as it stands (for tests and the log).
    const std::vector<UnitObserver*>* observers_of(const Unit& unit) const;

private:
    std::map<Unit, std::vector<UnitObserver*>> lists_;
};

}  // namespace tl::core
