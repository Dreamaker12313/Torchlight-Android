#include "core/navigation.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace tl::core {

// A position's cell, `floor((x - origin) * 2.5)`: `CLevel::mapPassable`'s index,
// as `PassabilityMap::cell_x`.
int NavGrid::cell_x(float x) const {
    return static_cast<int>(std::floor((x - origin_x_) * PassabilityMap::kInverseCell));
}

// A position's cell, `floor((z - origin) * 2.5)`: `CLevel::mapPassable`'s index.
//
int NavGrid::cell_z(float z) const {
    return static_cast<int>(std::floor((z - origin_z_) * PassabilityMap::kInverseCell));
}

// A cell's centre as `CCharacter::setDestination` reads a
// route's node back out of the pathfinder (`getNodeX` / `getNodeY`, inline
// there): `(cell + 0.5) * size + origin`, one fused step.
float NavGrid::world_x(int cx) const {
    return std::fma(static_cast<float>(cx) + 0.5f, cell_, origin_x_);
}

// A cell's centre as `CCharacter::setDestination` reads a
// route back: `fma(cell + 0.5, 0.4, origin)`.
float NavGrid::world_z(int cz) const {
    return std::fma(static_cast<float>(cz) + 0.5f, cell_, origin_z_);
}

// `CAstarPathfinder::findPath`: the offset clamped to 0,
// divided by the cell size and floored - a division, where `cell_x` multiplies by 2.5.
int NavGrid::astar_cell(float offset) const {
    if (!(offset >= 0.0f)) offset = 0.0f;
    return static_cast<int>(std::floor(offset / cell_));
}

// The distance `findNearestOpen` / `findNearestWideOpen`
// rank a candidate by: in world units, both cells' corners as
// `cell * size + origin` (fused), then `sqrt(dz*dz + dx*dx)` with the sum
// fused too.
float NavGrid::astar_snap_distance(int cx, int cz, int target_cx, int target_cz) const {
    const float target_x = std::fma(static_cast<float>(target_cx), cell_, origin_x_);
    const float target_z = std::fma(static_cast<float>(target_cz), cell_, origin_z_);
    const float dx = std::fma(static_cast<float>(cx), cell_, origin_x_) - target_x;
    const float dz = std::fma(static_cast<float>(cz), cell_, origin_z_) - target_z;
    return std::sqrt(std::fma(dz, dz, dx * dx));
}

// PORT: the port's pathfinding grid emptied.
void NavGrid::clear() {
    walkable_.clear();
    blockers_.clear();
    ground_.clear();
    width_ = 0;
    depth_ = 0;
    walkable_count_ = 0;
}

// PORT: the port's pathfinding grid made from the level's passability
// (`CAstarPathfinder::tileFree`'s two grids).
void NavGrid::build(const PassabilityMap& grid) {
    clear();
    if (!grid.ready()) return;

    cell_ = PassabilityMap::kCell;
    origin_x_ = grid.origin_x();
    origin_z_ = grid.origin_z();
    width_ = grid.width();
    depth_ = grid.height();

    walkable_.assign(static_cast<size_t>(width_) * static_cast<size_t>(depth_), 0);
    blockers_.assign(walkable_.size(), 0);
    ground_.assign(walkable_.size(), 0.0f);

    for (int cz = 0; cz < depth_; ++cz) {
        for (int cx = 0; cx < width_; ++cx) {
            if (!grid.map_passable(cx, cz)) continue;
            const size_t at = static_cast<size_t>(index_of(cx, cz));
            walkable_[at] = 1;
            ground_[at] = grid.ground_at(cx, cz, 0.0f);
            ++walkable_count_;
        }
    }
}

// PORT: the port's pathfinding grid read again over a box the level's map grid changed in.
void NavGrid::refresh(const PassabilityMap& grid, const Vec3& low, const Vec3& high) {
    if (!ready() || !grid.ready()) return;
    const int x0 = std::max(0, cell_x(std::min(low.x, high.x)) - 1);
    const int x1 = std::min(width_ - 1, cell_x(std::max(low.x, high.x)) + 1);
    const int z0 = std::max(0, cell_z(std::min(low.z, high.z)) - 1);
    const int z1 = std::min(depth_ - 1, cell_z(std::max(low.z, high.z)) + 1);
    for (int cz = z0; cz <= z1; ++cz) {
        for (int cx = x0; cx <= x1; ++cx) {
            const size_t at = static_cast<size_t>(index_of(cx, cz));
            const bool open = grid.map_passable(cx, cz);
            if (open == (walkable_[at] != 0)) continue;
            walkable_[at] = open ? 1 : 0;
            if (open) {
                ground_[at] = grid.ground_at(cx, cz, 0.0f);
                ++walkable_count_;
            } else {
                --walkable_count_;
            }
        }
    }
}

// `CAstarPathfinder::tileFree` at a
// position: the cell inside and open.
bool NavGrid::walkable(float x, float z) const {
    const int cx = cell_x(x);
    const int cz = cell_z(z);
    if (!inside(cx, cz)) return false;
    return is_open(static_cast<size_t>(index_of(cx, cz)));
}

// PORT: whether a cell is under one of the port's recorded blockers.
bool NavGrid::blocked(float x, float z) const {
    const int cx = cell_x(x);
    const int cz = cell_z(z);
    if (!inside(cx, cz)) return false;
    return blockers_[static_cast<size_t>(index_of(cx, cz))] != 0;
}

// PORT: the port's blocker record emptied.
void NavGrid::clear_blockers() { std::fill(blockers_.begin(), blockers_.end(), 0); }

// PORT: a diagnostic count of the blocker record.
size_t NavGrid::blocked_count() const {
    size_t n = 0;
    for (size_t at = 0; at < blockers_.size(); ++at) {
        if (blockers_[at] != 0) ++n;
    }
    return n;
}

// The cells `CLevel::incrementMapPassability(AxisAlignedBox, float)`
// takes for a box, in the port's blocker record.
size_t NavGrid::mark_blocker(const Vec3& low, const Vec3& high, bool on) {
    if (!ready()) return 0;
    size_t changed = 0;
    // From the floor()ed low corner up to, not including, the high one.
    const int x0 = cell_x(low.x);
    const int x1 = cell_x(high.x);
    const int z0 = cell_z(low.z);
    const int z1 = cell_z(high.z);
    for (int cz = z0; cz < z1; ++cz) {
        for (int cx = x0; cx < x1; ++cx) {
            if (!inside(cx, cz)) continue;
            const size_t at = static_cast<size_t>(index_of(cx, cz));
            // Every cell of the box, walkable or not.
            if (on) {
                if (blockers_[at] < 255) ++blockers_[at];
                if (blockers_[at] == 1) ++changed;
            } else if (blockers_[at] > 0) {
                --blockers_[at];
                if (blockers_[at] == 0) ++changed;
            }
        }
    }
    return changed;
}

// PORT: the ground the port's grid recorded at a cell.
float NavGrid::height_at(float x, float z, float fallback) const {
    const int cx = cell_x(x);
    const int cz = cell_z(z);
    if (!inside(cx, cz)) return fallback;
    const size_t at = static_cast<size_t>(index_of(cx, cz));
    return walkable_[at] != 0 ? ground_[at] : fallback;
}

// PORT: a cell's centre with the ground the port's grid recorded there.
Vec3 NavGrid::sample_at(float x, float z) const {
    const int cx = cell_x(x);
    const int cz = cell_z(z);
    if (!inside(cx, cz)) return {x, 0.0f, z};
    return {world_x(cx), ground_[static_cast<size_t>(index_of(cx, cz))], world_z(cz)};
}

// `CAstarPathfinder::findNearestOpen` (inline
// in `findPath`): the open cell nearest the target within
// `rings`; 9999 is the console's own "none yet".
template <typename Free>
bool NavGrid::nearest_open(const Free& free, int target_cx, int target_cz, int rings, int& out_cx,
                           int& out_cz) const {
    auto open_cell = [&](int cx, int cz) { return inside(cx, cz) && free(cx, cz); };
    constexpr float kNone = 9999.0f;
    float best_distance = kNone;
    for (int dx = -rings; dx <= rings; ++dx) {
        const int cx = target_cx + dx;
        for (int dz = -rings; dz <= rings; ++dz) {
            const int cz = target_cz + dz;
            if (!open_cell(cx, cz)) continue;
            const float distance = astar_snap_distance(cx, cz, target_cx, target_cz);
            if (distance < best_distance) {
                out_cx = cx;
                out_cz = cz;
                best_distance = distance;
            }
        }
    }
    return best_distance != kNone;
}

// `CAstarPathfinder::findNearestWideOpen`
// (inline in `findPath`): the same, with its four neighbours
// open too.
template <typename Free>
bool NavGrid::nearest_wide_open(const Free& free, int target_cx, int target_cz, int rings,
                                int& out_cx, int& out_cz) const {
    auto open_cell = [&](int cx, int cz) { return inside(cx, cz) && free(cx, cz); };
    constexpr float kNone = 9999.0f;
    float best_distance = kNone;
    for (int dx = -rings; dx <= rings; ++dx) {
        const int cx = target_cx + dx;
        for (int dz = -rings; dz <= rings; ++dz) {
            const int cz = target_cz + dz;
            if (!open_cell(cx, cz)) continue;
            if (!open_cell(cx, cz - 1) || !open_cell(cx, cz + 1)) continue;
            if (!open_cell(cx - 1, cz) || !open_cell(cx + 1, cz)) continue;
            const float distance = astar_snap_distance(cx, cz, target_cx, target_cz);
            if (distance < best_distance) {
                out_cx = cx;
                out_cz = cz;
                best_distance = distance;
            }
        }
    }
    return best_distance != kNone;
}

bool NavGrid::line_is_clear(float from_x, float from_z, float to_x, float to_z) const {
    if (!ready()) return false;
    const float dx = to_x - from_x;
    const float dz = to_z - from_z;
    const float length = std::sqrt(dx * dx + dz * dz);
    // Sampled at half a cell, so a diagonal cannot slip between two blocked
    // cells that touch only at a corner.
    const int steps = std::max(1, static_cast<int>(length / (cell_ * 0.5f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (!walkable(from_x + dx * t, from_z + dz * t)) return false;
    }
    return true;
}

namespace {

/// `CAstarPathfinder::CAstarNode`. `children` caches up to 8 successors, for `relax_children`.
struct AstarNode {
    float f = 0.0f;
    float h = 0.0f;
    float g = 0.0f;
    int cx = 0;
    int cz = 0;
    int parent = -1;
    std::array<int, 8> children{-1, -1, -1, -1, -1, -1, -1, -1};
};

// `CAstarPathfinder::CAstarNode`'s eight children (`generateSuccessor`):
// the first free slot takes the new one.
void record_child(AstarNode& node, int child_index) {
    for (int& slot : node.children) {
        if (slot < 0) {
            slot = child_index;
            return;
        }
    }
}

/// `CAstarPathfinder::propagateDown`: an improved cost
/// cascaded to the cached children through a LIFO stack (`CAstarPathfinder::push`;
/// `pop`, inline on the console).
void relax_children(std::vector<AstarNode>& pool, int node_index) {
    std::vector<int> stack;
    auto relax_from = [&](int from_index) {
        const float candidate = pool[static_cast<size_t>(from_index)].g + 1.0f;
        for (int child_index : pool[static_cast<size_t>(from_index)].children) {
            if (child_index < 0) break;
            AstarNode& child = pool[static_cast<size_t>(child_index)];
            if (candidate < child.g) {
                child.g = candidate;
                child.f = candidate + child.h;
                child.parent = from_index;
                stack.push_back(child_index);
            }
        }
    };
    relax_from(node_index);
    while (!stack.empty()) {
        const int popped = stack.back();
        stack.pop_back();
        relax_from(popped);
    }
}

}  // namespace

// `CAstarPathfinder::findPath` and
// `createPath`, the search; see the body.
template <typename Free>
bool NavGrid::search(const Vec3& from, const Vec3& to, int rings, const Free& free,
                     std::vector<Vec3>& nodes, int expansion_cap) const {
    nodes.clear();
    if (!ready() || walkable_count_ == 0) return false;
    auto open_cell = [&](int cx, int cz) { return inside(cx, cz) && free(cx, cz); };

    // `CAstarPathfinder::findPath`: the destination is
    // snapped first, then the start (the wide rule).
    const int kRings = rings;
    int start_cx = astar_cell(from.x - origin_x_);
    int start_cz = astar_cell(from.z - origin_z_);
    int goal_cx = astar_cell(to.x - origin_x_);
    int goal_cz = astar_cell(to.z - origin_z_);

    bool goal_snapped = false;
    if (!open_cell(goal_cx, goal_cz)) {
        goal_snapped = true;
        int nx = start_cx;
        int nz = start_cz;
        // With no open cell in the box, the goal becomes the start cell.
        nearest_open(free, goal_cx, goal_cz, kRings, nx, nz);
        goal_cx = nx;
        goal_cz = nz;
    }

    const bool start_needs_wide_snap =
        !open_cell(start_cx, start_cz) || !open_cell(start_cx, start_cz - 1) ||
        !open_cell(start_cx, start_cz + 1) || !open_cell(start_cx - 1, start_cz) ||
        !open_cell(start_cx + 1, start_cz);
    if (start_needs_wide_snap) {
        int nx = goal_cx;
        int nz = goal_cz;
        if (nearest_wide_open(free, start_cx, start_cz, kRings, nx, nz)) {
            start_cx = nx;
            start_cz = nz;
        } else {
            start_cx = goal_cx;
            start_cz = goal_cz;
        }
        if (!open_cell(start_cx, start_cz)) return false;  // still stuck: no route
    }

    if (start_cx == goal_cx && start_cz == goal_cz) return false;  // already there

    // `CAstarPathfinder::createPath`.
    const size_t count = walkable_.size();
    std::vector<int> state(count, -1);  // cell index -> pool index, or -1
    std::vector<bool> closed(count, false);
    std::vector<AstarNode> pool;
    pool.reserve(256);

    AstarNode start_node;
    start_node.cx = start_cx;
    start_node.cz = start_cz;
    {
        const float dx = static_cast<float>(start_cx - goal_cx);
        const float dz = static_cast<float>(start_cz - goal_cz);
        start_node.h = std::fma(dz, dz, dx * dx);
    }
    start_node.g = 0.0f;
    start_node.f = start_node.h;
    pool.push_back(start_node);
    state[static_cast<size_t>(index_of(start_cx, start_cz))] = 0;

    std::vector<int> open_order{0};  // sorted ascending by f, front = lowest
    int best_node = -1;              // the closest approach found, by lowest h
    int expansions = 0;
    constexpr float kCloseEnough = 2.0f;

    // `CAstarPathfinder::generateSuccessor`.
    auto generate_successor = [&](int parent_index, int nx, int nz) {
        const size_t at = static_cast<size_t>(index_of(nx, nz));
        const float tentative_g = pool[static_cast<size_t>(parent_index)].g + 1.0f;
        int existing = state[at];
        if (existing >= 0) {
            record_child(pool[static_cast<size_t>(parent_index)], existing);
            AstarNode& node = pool[static_cast<size_t>(existing)];
            if (tentative_g < node.g) {
                node.g = tentative_g;
                node.f = tentative_g + node.h;
                node.parent = parent_index;
                // Quirk kept: the open list is not re-sorted on an improvement.
                if (closed[at]) relax_children(pool, existing);
            }
            return;
        }
        AstarNode node;
        node.cx = nx;
        node.cz = nz;
        const float dx = static_cast<float>(nx - goal_cx);
        const float dz = static_cast<float>(nz - goal_cz);
        node.h = std::fma(dz, dz, dx * dx);
        node.g = tentative_g;
        node.f = node.g + node.h;
        node.parent = parent_index;
        const int new_index = static_cast<int>(pool.size());
        pool.push_back(node);
        state[at] = new_index;
        record_child(pool[static_cast<size_t>(parent_index)], new_index);
        // Before the first node with f >= its own: a tie pops ahead of what was open.
        const auto insert_at = std::find_if(open_order.begin(), open_order.end(), [&](int idx) {
            return pool[static_cast<size_t>(idx)].f >= node.f;
        });
        open_order.insert(insert_at, new_index);
        if (best_node < 0 || node.h < pool[static_cast<size_t>(best_node)].h) best_node = new_index;
    };

    // `CAstarPathfinder::generateSuccessors`: the eight neighbours clockwise from the
    // north-west (`z - 1` is north). A diagonal needs both flanking orthogonals open, an
    // orthogonal one of its two flanking diagonals, so no route enters a one-cell passage.
    struct Step {
        int dx, dz;
        // Diagonal: both of `a` and `b` open; orthogonal: either.
        int ax, az, bx, bz;
        bool diagonal;
    };
    constexpr Step kSteps[8] = {
        {-1, -1, 0, -1, -1, 0, true},   // north-west: north and west
        {0, -1, -1, -1, 1, -1, false},  // north: north-west or north-east
        {1, -1, 0, -1, 1, 0, true},     // north-east: north and east
        {1, 0, 1, -1, 1, 1, false},     // east: north-east or south-east
        {1, 1, 0, 1, 1, 0, true},       // south-east: south and east
        {0, 1, -1, 1, 1, 1, false},     // south: south-west or south-east
        {-1, 1, 0, 1, -1, 0, true},     // south-west: south and west
        {-1, 0, -1, -1, -1, 1, false},  // west: north-west or south-west
    };
    // `CAstarPathfinder::generateSuccessors`.
    auto generate_successors = [&](int node_index) {
        for (const Step& step : kSteps) {
            // Copied out: `generate_successor` can grow `pool`.
            const int cx = pool[static_cast<size_t>(node_index)].cx;
            const int cz = pool[static_cast<size_t>(node_index)].cz;
            const bool a = open_cell(cx + step.ax, cz + step.az);
            const bool beside = step.diagonal ? a && open_cell(cx + step.bx, cz + step.bz)
                                              : a || open_cell(cx + step.bx, cz + step.bz);
            if (!beside || !open_cell(cx + step.dx, cz + step.dz)) continue;
            generate_successor(node_index, cx + step.dx, cz + step.dz);
        }
    };

    int result_node = -1;
    bool failed = false;
    // `createPath`'s path-valid flag (X360 +0x1C): cleared when a pop finds the open list
    // empty, so running out of nodes answers "no route"; the cap does not.
    bool valid = true;
    while (true) {
        int current = -1;
        if (open_order.empty()) valid = false;
        if (!open_order.empty()) {
            current = open_order.front();
            open_order.erase(open_order.begin());
            const AstarNode& popped = pool[static_cast<size_t>(current)];
            closed[static_cast<size_t>(index_of(popped.cx, popped.cz))] = true;
        }

        if (goal_snapped && best_node >= 0 &&
            pool[static_cast<size_t>(best_node)].h < kCloseEnough) {
            result_node = best_node;
            break;
        }
        if (current < 0 || expansions > expansion_cap) {
            if (best_node < 0) {
                failed = true;
            } else {
                result_node = best_node;
            }
            break;
        }
        const AstarNode& node = pool[static_cast<size_t>(current)];
        if (node.cx == goal_cx && node.cz == goal_cz) {
            result_node = current;
            break;
        }
        ++expansions;
        generate_successors(current);
    }
    if (failed || result_node < 0 || !valid) return false;

    // Walk `parent` back to the start and reverse (`createPath`'s list reversal).
    std::vector<int> cells;
    for (int at = result_node; at != -1; at = pool[static_cast<size_t>(at)].parent) {
        cells.push_back(at);
    }
    std::reverse(cells.begin(), cells.end());

    // Every node at its cell centre, the start's cell first, no string-pulling (`getNodeX/Y`).
    nodes.reserve(cells.size());
    for (int index : cells) {
        const AstarNode& node = pool[static_cast<size_t>(index)];
        const float x = world_x(node.cx);
        const float z = world_z(node.cz);
        nodes.push_back({x, ground_[static_cast<size_t>(index_of(node.cx, node.cz))], z});
    }
    return true;
}

// `CAstarPathfinder::findPath` on the level's
// map grid, the start left off the route.
std::vector<Vec3> NavGrid::find_path(const Vec3& from, const Vec3& to) const {
    std::vector<Vec3> route;
    auto free = [this](int cx, int cz) { return is_open(static_cast<size_t>(index_of(cx, cz))); };
    if (!search(from, to, kNavSnapRings, free, route, kAstarExpansionCap) || route.empty())
        return {};
    // `from` excluded.
    route.erase(route.begin());
    return route;
}

bool NavGrid::find_nodes(const Vec3& from, const Vec3& to, const PassabilityMap& level,
                         bool ignore_objects, std::vector<Vec3>& nodes, int expansion_cap) const {
    // `CAstarPathfinder::tileFree`: the map grid, and the
    // object grid unless `+0x69` is set, both live.
    auto free = [&](int cx, int cz) {
        if (!level.map_passable(cx, cz)) return false;
        return ignore_objects || level.object_passable(cx, cz);
    };
    return search(from, to, kAstarSnapRings, free, nodes, expansion_cap);
}

bool NavGrid::position_passable(const PassabilityMap& level, const Vec3& at) const {
    return level.position_passable(at);
}

// `CLevel::passableBetween`: the level's, its
// grid holding the blockers too.
bool NavGrid::passable_between(const PassabilityMap& level, const Vec3& from, const Vec3& to,
                               bool map_only) const {
    return level.passable_between(from, to, map_only);
}

}  // namespace tl::core
