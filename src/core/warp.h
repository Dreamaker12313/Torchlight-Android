// Where a warp leads: `CGameClient::warpLevels` and the part of
// `CGameClient::performWarp` that picks a dungeon and depth. No OGRE and no file I/O.
#pragma once

#include "core/level_memory.h"

#include <cstdint>
#include <string>

namespace tl::core {

/// What `warpLevels` stores: the dungeon name (`+0x10a8`), `LEVEL DELTA`
/// (`+0x10a0`) and `LEVEL ABSOLUTE` (`+0x10a4`).
struct WarpRequest {
    std::string dungeon;
    int32_t delta = 1;
    int32_t absolute = 0;
};

/// `CGameClient::warpLevels`: `LASTDUNGEON`
/// is the last-dungeon record at its depth, while that is not empty.
WarpRequest warp_levels(const std::string& dungeon, int32_t delta, int32_t absolute,
                        const TravelRecord& travel);

/// The level a warp leaves: its dungeon (`CGameClient +0x38d8`; empty for a
/// layout loaded on its own), that dungeon's `PARENT_DUNGEON` (`+0x80`) and
/// the level's depth (`CLevel +0x1a4`).
struct WarpOrigin {
    std::string dungeon;
    std::string parent;
    int32_t depth = 0;
};

struct WarpDestination {
    /// False when `performWarp` returns without warping: up out of depth 0
    /// with nowhere to go.
    bool go = false;
    std::string dungeon;  ///< upper-cased
    int32_t depth = 0;
};

/// `CGameClient::performWarp`, every warp but the -99
/// one. Up out of depth 0 goes to
/// `PARENT_DUNGEON`, else the last-dungeon record, else nowhere;
/// the delta counts only when the absolute is 0 and the name was emptied.
WarpDestination warp_destination(const WarpRequest& request, const WarpOrigin& from,
                                 const TravelRecord& travel);

}  // namespace tl::core
