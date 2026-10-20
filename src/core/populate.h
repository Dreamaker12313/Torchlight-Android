#pragma once

#include "core/collision.h"
#include "core/random.h"
#include "core/randomizer.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// One of `CLevel::populate`'s sections:
/// the box of a collision-enabled `Room Piece`. The piece's position plus its collision mesh's
/// bounds, not turned or scaled.
struct PopulateSection {
    Vec3 first;   ///< position + the bounds' minimum, y - 1000
    Vec3 second;  ///< position + the bounds' maximum, y + 1000
};

/// A `Quest Boss` property node of the level (`CLevel::loadRoomLayout`), as the NPC phase
/// takes them.
struct SpawnMarker {
    Vec3 position;  ///< `CPositionableObject::getPosition(true)`
    /// `getOrientationQuaternion()`, w, x, y, z.
    std::array<float, 4> orientation{1.0f, 0.0f, 0.0f, 0.0f};
    int32_t taken = 0;  ///< how many units stand on it
};

/// What `CResourceManager::createUnit` made
/// of a rolled unit, as far as `populateSectionOfLevel` asks.
enum class PopulatedKind {
    None,       ///< nothing by that name
    Character,  ///< a `CCharacter`: monsters, NPCs
    Item,       ///< a `CItem`: breakables, trigger props, equipment
};

struct PopulatedUnit {
    PopulatedKind kind = PopulatedKind::None;
    /// A collideable `INTERACTABLE` item (the chests, not the barrels): it is only left where
    /// the four lines through it are passable.
    bool blocks = false;
    /// The caller's handle on the unit it made.
    int64_t handle = 0;
};

/// How a made unit is turned before it joins the level.
struct PopulatedFacing {
    /// Taken from a marker: `setOrientation(marker's quaternion)`. Otherwise
    /// `setDirection(rotateY((0, 0, 1), yaw))`.
    bool from_marker = false;
    std::array<float, 4> orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;  ///< radians about Y
};

/// What the level answers while a section is populated.
struct PopulateWorld {
    /// `CResourceManager::getSpawnClassByName` then `CSpawnClass::rollSpawnClass`
    /// at the level's monster level, on `g_Rand`: the units
    /// of one roll, in order. Empty for no such class.
    std::function<std::vector<std::string>(const std::string& spawn_class)> roll;
    /// `CLevel::randomOpenPosition(p, 3.0, true)`: `p` itself when
    /// it finds nowhere.
    std::function<Vec3(const Vec3& at, float radius)> random_open_position;
    /// `CLevel::isInNoSpawnRegion(p, radius)`.
    ///
    std::function<bool(const Vec3& at, float radius)> in_no_spawn_region;
    /// `CLevel::passableBetween(from, to, false)`.
    ///
    std::function<bool(const Vec3& from, const Vec3& to)> passable_between;
    /// `CResourceManager::createUnit(unit, monster level, false, false)`:
    /// the unit made, with every draw making it takes
    /// (`CBaseUnit::unitInit`'s scale, a character's `setLevel`), before it is turned. Kind
    /// `None` when nothing could be made.
    std::function<PopulatedUnit(const std::string& unit)> create;
    /// `CLevel::addCharacter(unit, at, false)`,
    /// after `CCharacter::makeChampion` when
    /// `champion`. False when it could not be added.
    std::function<bool(const PopulatedUnit& made, const std::string& unit, const Vec3& at,
                       const PopulatedFacing& facing, bool champion)>
        add_character;
    /// `CLevel::addItem(unit, at, true)`.
    /// False when it could not be added.
    std::function<bool(const PopulatedUnit& made, const std::string& unit, const Vec3& at,
                       const PopulatedFacing& facing)>
        add_item;
    /// A made item thrown away (its destructor).
    std::function<void(const PopulatedUnit& made)> discard;
    /// The level's markers an NPC is put on. Null for none.
    std::vector<SpawnMarker>* markers = nullptr;
    /// `g_RandVolatile`: every position and facing.
    RunicRandom* volatile_rolls = nullptr;
};

/// `CLevel::populateSectionOfLevel`: one roll of
/// `spawn_class`, each unit put on a free marker or somewhere open inside the section. Returns
/// the characters plus the items added.
int populate_section_of_level(const PopulateSection& section, const std::string& spawn_class,
                              bool champion, bool use_markers, const PopulateWorld& world);

/// One of `CLevel::populate`'s unit
/// phases: a section picked on `g_Rand` until `wanted` units are counted. With `empty_counts`
/// (every phase but the monsters') an empty roll counts as one; without it, more than ten
/// empty rolls in a row count as one.
int populate_phase(const std::vector<PopulateSection>& sections, Randomizer& sections_pick,
                   RunicRandom& g_rand, const std::string& spawn_class, uint32_t wanted,
                   bool champion, bool use_markers, bool empty_counts, const PopulateWorld& world);

/// One `[FORMATION]` of the template's formations file: its `FILE` and
/// `WEIGHT` (`CDataGroup::GetDataValue(L"WEIGHT", 1)`).
struct FormationChoice {
    std::string file;
    int32_t weight = 1;
};

/// What `CLevel::populateFormations` asks of the level.
struct FormationWorld {
    /// `CLevel::randomOpenPosition(p, 3.0, true)`.
    std::function<Vec3(const Vec3& at, float radius)> random_open_position;
    /// `CLevel::isInNoSpawnRegion(p, radius)`.
    std::function<bool(const Vec3& at, float radius)> in_no_spawn_region;
    /// `CFormationNode::CFormationNode`, `setLayoutFile(file)` and `setPosition(at)`, the node
    /// joining the level's formations.
    std::function<void(const std::string& file, const Vec3& at)> add_formation;
    /// `g_RandVolatile`.
    RunicRandom* volatile_rolls = nullptr;
};

/// `CLevel::populateFormations`. `choices` are the template's
/// formations in order; a child with no `FILE` ends the phase with nothing made. `wanted` is
/// `getNumberOfUnitsToCreate(7)`, drawn by the caller in its place. Returns `wanted`, even
/// where a formation found no place.
uint32_t populate_formations(const std::vector<PopulateSection>& sections,
                             Randomizer& sections_pick, const std::vector<FormationChoice>& choices,
                             const std::function<uint32_t()>& wanted, std::vector<Vec3>& existing,
                             RunicRandom& g_rand, const FormationWorld& world);

/// What the gold phase asks of the level.
struct GoldWorld {
    /// `CLevel::randomOpenItemPosition(p, 3.0, false)`.
    ///
    std::function<Vec3(const Vec3& at, float radius)> random_open_item_position;
    /// `CLevel::isInNoSpawnRegion(p, radius)`.
    std::function<bool(const Vec3& at, float radius)> in_no_spawn_region;
    /// `new CItemGold(rm, amount)` and `CLevel::addItem(gold, at, false)`.
    std::function<void(int32_t amount, const Vec3& at)> add_gold;
    /// `g_RandVolatile`.
    RunicRandom* volatile_rolls = nullptr;
};

/// `CLevel::populate`'s gold phase: `wanted` tries at a pile of
/// 2 to 12 gold. Returns the piles made.
int populate_gold(const std::vector<PopulateSection>& sections, Randomizer& sections_pick,
                  uint32_t wanted, RunicRandom& g_rand, const GoldWorld& world);

/// One of the level's room records, one per room layout: the union of its sections, made by
/// `CLevel::populate`.
struct LevelRoomBox {
    /// 0 nothing, 1 the box, 2 everything (the test always passes; `populate` makes none).
    int32_t state = 0;
    Vec3 low{-0.5f, -0.5f, -0.5f};
    Vec3 high{0.5f, 0.5f, 0.5f};

    /// the first section seeds the box, each later one widens it.
    void add(const PopulateSection& section);
};

/// `CLevel::getRoomIndexThatPositionIsIn(const Vector3&)`:
/// the first record whose state is 2, or
/// is 1 with `position` inside its box (ends included); -1 for none.
int32_t room_index_that_position_is_in(const std::vector<LevelRoomBox>& rooms,
                                       const Vec3& position);

/// A unit the level holds, as `CLevel::getItemByOriginalGuid` reads it: its layout GUID, its
/// parents' hierarchy hash (`calculateParentHierarchyHashCode`) and where it stands.
struct OriginalGuidUnit {
    int64_t guid = 0;
    int64_t hierarchy = 0;
    Vec3 position;
};

/// `CLevel::getItemByOriginalGuid`: the first unit of that
/// GUID in that room (any for -1) whose hash matches, or `hierarchy` is 0; else the last of
/// that GUID there. Returns its index, or -1.
int find_item_by_original_guid(const std::vector<OriginalGuidUnit>& items,
                               const std::vector<LevelRoomBox>& rooms, int64_t guid, int32_t room,
                               int64_t hierarchy);

}  // namespace tl::core
