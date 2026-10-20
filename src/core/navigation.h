// Where a character can walk, and A* routes across it.
//
// The open cells are `CLevel::calculatePassability`'s; `CAstarPathfinder::tileFree`
// routes over them. `CAstarPathfinder::findPath`/
// `createPath`/`generateSuccessor(s)`:
// every step costs 1.0, the heuristic is the
// squared distance (so a closed node can be reopened), routes are cell-centre staircases.
//
// DIVERGENCE (`find_path` only - the pet and the player's walks): `findNearestOpen`/
// `findNearestWideOpen` scan a 33x33 box; `find_path` keeps
// `kNavSnapRings`, which `snap_distance()`'s callers rely on. `find_nodes` is the original's.
//
// No OGRE and no file I/O.
#pragma once

#include "core/collision.h"
#include "core/passability.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tl::core {

/// How far apart the cells are, in level units: the passability grid's 0.4.
inline constexpr float kNavCell = PassabilityMap::kCell;

/// How many cells either end of a `find_path` route may be snapped by (two units). Part of the
/// contract: how far a completed route may end from the point asked for.
inline constexpr int kNavSnapRings = 4;

/// `CAstarPathfinder::findNearestOpen`/`findNearestWideOpen`'s own box: 16 cells
/// either way. What a
/// character's route search uses.
inline constexpr int kAstarSnapRings = 16;

/// `CAstarPathfinder +0x6c` as `CAstarPathfinder::CAstarPathfinder` leaves it:
/// past this many expansions `createPath` keeps the closest approach.
inline constexpr int kAstarExpansionCap = 150;

/// A walkable grid over one level, and A* across it. One byte a cell.
class NavGrid {
public:
    /// Take the cells a baked passability grid left open
    /// (`CLevel::mapPassable`), with the floor height its cast found in each.
    void build(const PassabilityMap& grid);
    /// Take the cells under `low`..`high` (the ground plane) from the grid again, after a door
    /// changed the map grid (`CBaseUnit::removeFromAvoidanceMap`;
    /// `addToAvoidanceMap`).
    void refresh(const PassabilityMap& grid, const Vec3& low, const Vec3& high);

    void clear();

    // PORT: an accessor of the port's pathfinding grid.
    bool ready() const { return width_ > 0 && depth_ > 0; }
    // PORT: an accessor of the port's pathfinding grid (`CAstarPathfinder`'s view).
    size_t cell_count() const { return walkable_.size(); }
    /// How many cells the level's map grid leaves open, as last read
    /// (`build`, `refresh`): the geometry's, less what blockers and doors close.
    // PORT: an accessor of the port's pathfinding grid (`CAstarPathfinder`'s view).
    size_t walkable_count() const { return walkable_count_; }

    /// How many cells the stick's blocker record covers.
    size_t blocked_count() const;
    float cell_size() const { return cell_; }

    /// How far a completed route may end from the point it was given, in world
    /// units: `kNavSnapRings` cells. See the constant.
    float snap_distance() const { return cell_ * static_cast<float>(kNavSnapRings); }

    /// Whether a character can stand at (x, z).
    bool walkable(float x, float z) const;

    /// Whether an active `Path Node Occupation` blocker covers (x, z). A record for the stick's
    /// gate (`Player::update`) only; routes read the blockers from the map grid
    /// (`CPropertyNode::updatePathNodes`,
    /// `CLevel::incrementMapPassability(AxisAlignedBox, float)`,
    /// `decrementMapPassability`).
    bool blocked(float x, float z) const;

    /// The point the grid actually sampled for (x, z): its cell's centre, at the
    /// ground height found there.
    Vec3 sample_at(float x, float z) const;

    /// The ground height the grid sampled at (x, z); the fallback when the cell
    /// is outside the grid or has no ground.
    float height_at(float x, float z, float fallback) const;

    /// A route from `from` to `to`, cell centres, `from` excluded and the destination last;
    /// empty when there is none. `CAstarPathfinder::findPath` over the snapshot, with
    /// `kNavSnapRings`.
    std::vector<Vec3> find_path(const Vec3& from, const Vec3& to) const;

    /// `CAstarPathfinder::findPath` on the level's live map
    /// and object grids (`tileFree`); `ignore_objects` is the pathfinder's +0x69. `nodes` runs
    /// start cell to end cell, for `CCharacter::setDestination`. `expansion_cap` is `+0x6c`:
    /// 150, or 800 while the character holds a path (`setPathToFollow`).
    bool find_nodes(const Vec3& from, const Vec3& to, const PassabilityMap& level,
                    bool ignore_objects, std::vector<Vec3>& nodes,
                    int expansion_cap = kAstarExpansionCap) const;

    /// `CLevel::positionPassable(Vector3)`.
    bool position_passable(const PassabilityMap& level, const Vec3& at) const;

    /// `CLevel::passableBetween`.
    bool passable_between(const PassabilityMap& level, const Vec3& from, const Vec3& to,
                          bool map_only) const;

    /// Record, for `blocked()`, that a blocker covers the box `low`..`high`, or no longer does;
    /// returns how many cells changed. Counted, so two that overlap do not lift each other.
    /// The map grid is written apart (`PassabilityMap::increment_map_passability`).
    size_t mark_blocker(const Vec3& low, const Vec3& high, bool on);

    /// Forget the stick's blocker record.
    void clear_blockers();

    /// Whether the straight line between two points stays on walkable ground.
    /// Exposed because it is what a follower uses to decide it can skip ahead.
    bool line_is_clear(float from_x, float from_z, float to_x, float to_z) const;

private:
    // PORT: an accessor of the port's pathfinding grid (`CAstarPathfinder`'s view).
    int index_of(int cx, int cz) const { return cz * width_ + cx; }
    bool inside(int cx, int cz) const { return cx >= 0 && cz >= 0 && cx < width_ && cz < depth_; }
    int cell_x(float x) const;
    int cell_z(float z) const;
    float world_x(int cx) const;
    float world_z(int cz) const;
    /// The pathfinder's own cell for an offset from the grid's origin.
    int astar_cell(float offset) const;
    /// How far a candidate cell is from the snap's centre, as the two scans
    /// below measure it.
    float astar_snap_distance(int cx, int cz, int target_cx, int target_cz) const;

    /// `CAstarPathfinder::findNearestOpen`: the open cell
    /// within `rings` nearest the target, by float corner distance; only a strictly nearer
    /// cell replaces the one held (x outer, z inner, ascending). False when there is none.
    template <typename Free>
    bool nearest_open(const Free& free, int target_cx, int target_cz, int rings, int& out_cx,
                      int& out_cz) const;

    /// `CAstarPathfinder::findNearestWideOpen`: the same,
    /// with the four orthogonal neighbours open too. Snaps the start, never a destination.
    template <typename Free>
    bool nearest_wide_open(const Free& free, int target_cx, int target_cz, int rings, int& out_cx,
                           int& out_cz) const;

    /// `findPath` + `createPath` over cells `free(cx, cz)` says are open; see
    /// `find_nodes`.
    template <typename Free>
    bool search(const Vec3& from, const Vec3& to, int rings, const Free& free,
                std::vector<Vec3>& nodes, int expansion_cap) const;

    /// Whether a cell is open on the level's map grid, as last read (`build`, `refresh`).
    std::vector<uint8_t> walkable_;

    /// How many blockers currently cover each cell, for the stick's gate only.
    std::vector<uint8_t> blockers_;

    std::vector<float> ground_;

    // PORT: an accessor of the port's pathfinding grid (`CAstarPathfinder`'s view).
    bool is_open(size_t at) const { return walkable_[at] != 0; }
    bool open_cell(int cx, int cz) const {
        return inside(cx, cz) && is_open(static_cast<size_t>(index_of(cx, cz)));
    }
    int width_ = 0;
    int depth_ = 0;
    float origin_x_ = 0.0f;
    float origin_z_ = 0.0f;
    float cell_ = kNavCell;

    size_t walkable_count_ = 0;
};

}  // namespace tl::core
