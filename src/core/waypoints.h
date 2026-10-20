// Waypoints: `CPlayer::addWaypoint`, `CWaypointMenu`, and the entry type
// `CGameClient::performWarp` gives a warp. A waypoint is a depth in a dungeon tracker's
// list (`DungeonTrackerSave::ints`).
// No OGRE and no file I/O.
#pragma once

#include "core/level_memory.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// `CPlayer::addWaypoint(std::wstring, int)`:
/// false when no tracker is named `dungeon` (upper-cased) or the depth is already
/// there; the caller says "Waypoint Activated" only on true.
bool add_waypoint(TravelRecord& travel, const std::string& dungeon, int32_t depth);

/// The menu's rows (`CWaypointMenu::createMenus`, makes ten).
inline constexpr int kWaypointMenuRows = 10;

/// One row: where choosing it warps.
struct WaypointRow {
    std::string dungeon;  ///< the tracker's name
    int32_t depth = 0;
};

/// `CWaypointMenu::setOpen(true)`: trackers and
/// their waypoints in order; a dungeon `CDungeonManager::getDungeonByName` cannot find
/// takes no row.
std::vector<WaypointRow> waypoint_rows(
    const TravelRecord& travel, const std::function<bool(const std::string&)>& dungeon_known);

/// `CWaypointMenu::onClick`: the layout
/// function `0xe + row` is row `row`; one past the rows filled does nothing.
/// The warp it asks for is `warpLevels(dungeon, 0, depth, true, L"", false)`.
const WaypointRow* waypoint_choice(const std::vector<WaypointRow>& rows, int row);

/// `CGameClient::performWarp`: the entry type the
/// level is loaded with. 2 for the town return; else 1 or 0 by
/// the delta; into another dungeon without `keep_entry`, 6 or 5.
int warp_entry_type(int32_t level_delta, int32_t level_absolute, bool town_return, bool keep_entry,
                    bool waypoint, const std::string& leaving_dungeon,
                    const std::string& destination);

/// Entry type 5: into another dungeon by anything but a waypoint. `performWarp`
/// then looks for the level's temporary portal to the dungeon left.
///
inline constexpr int kEntryOtherDungeon = 5;

/// Entry type 6: `CLevel::loadRoomLayout`
/// starts the player at the last `Property Node` typed `Waypoint` (14) or
/// `Waypoint Start` (15) it met.
inline constexpr int kEntryWaypoint = 6;

}  // namespace tl::core
