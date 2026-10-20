#include "core/waypoints.h"

namespace tl::core {

bool add_waypoint(TravelRecord& travel, const std::string& dungeon, int32_t depth) {
    // `CPlayer::addWaypoint(std::wstring, int)`:
    // the first tracker (`+0xa40`) with the
    // name, and nothing when the depth is already listed.
    for (tl::formats::DungeonTrackerSave& tracker : travel.trackers) {
        if (tracker.name != dungeon) continue;
        for (int32_t known : tracker.ints) {
            if (known == depth) return false;
        }
        tracker.ints.push_back(depth);
        return true;
    }
    return false;
}

std::vector<WaypointRow> waypoint_rows(
    const TravelRecord& travel, const std::function<bool(const std::string&)>& dungeon_known) {
    // `CWaypointMenu::setOpen(true)`: the row counter only
    // moves when a row is filled, and a row past the layout's last button is
    // skipped without stopping the walk (`row < 0x31 && slot != null`).
    std::vector<WaypointRow> rows;
    for (const tl::formats::DungeonTrackerSave& tracker : travel.trackers) {
        for (int32_t depth : tracker.ints) {
            if (static_cast<int>(rows.size()) >= kWaypointMenuRows) continue;
            if (dungeon_known && !dungeon_known(tracker.name)) continue;
            rows.push_back({tracker.name, depth});
        }
    }
    return rows;
}

const WaypointRow* waypoint_choice(const std::vector<WaypointRow>& rows, int row) {
    // `CWaypointMenu::onClick`: `row < size()`.
    if (row < 0 || row >= static_cast<int>(rows.size())) return nullptr;
    return &rows[static_cast<size_t>(row)];
}

int warp_entry_type(int32_t level_delta, int32_t level_absolute, bool town_return, bool keep_entry,
                    bool waypoint, const std::string& leaving_dungeon,
                    const std::string& destination) {
    // `CGameClient::performWarp`. `cmpq $0,
    // +0x10a0` tests the delta and the absolute depth together; `setg` then
    // reads the delta alone.
    int entry = 2;
    if (!town_return) {
        entry = 1;
        if (level_delta != 0 || level_absolute != 0) entry = 0 < level_delta ? 1 : 0;
    }
    // The dungeon being left is `+0x38d8`'s `+0x60`, `+0x10a8` the
    // warp's destination.
    if (!keep_entry && leaving_dungeon != destination && !town_return) {
        entry = waypoint ? 6 : 5;
    }
    return entry;
}

}  // namespace tl::core
