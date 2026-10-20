// Waypoints: `CPlayer::addWaypoint`, `CWaypointMenu::setOpen`/`onClick` and the
// entry type `CGameClient::performWarp` gives a warp.
#include "core/waypoints.h"
#include "test_support.h"

using tl::core::TravelRecord;

namespace {

TravelRecord with_trackers(std::initializer_list<const char*> names) {
    TravelRecord travel;
    for (const char* name : names) {
        tl::formats::DungeonTrackerSave tracker;
        tracker.name = name;
        travel.trackers.push_back(tracker);
    }
    return travel;
}

}  // namespace

TL_TEST(a_waypoint_is_added_once_and_only_to_a_known_dungeon) {
    TravelRecord travel = with_trackers({"MAIN"});
    TL_CHECK(tl::core::add_waypoint(travel, "MAIN", 4));
    // The same depth again is not added, and says so.
    TL_CHECK(!tl::core::add_waypoint(travel, "MAIN", 4));
    // No tracker of that name: nothing.
    TL_CHECK(!tl::core::add_waypoint(travel, "SIDE", 2));
    TL_CHECK_EQ(travel.trackers[0].ints.size(), 1u);
    TL_CHECK_EQ(travel.trackers[0].ints[0], 4);
}

TL_TEST(the_menu_lists_trackers_in_order_and_skips_unknown_dungeons) {
    TravelRecord travel = with_trackers({"MAIN", "GONE", "SIDE"});
    travel.trackers[0].ints = {4, 9};
    travel.trackers[1].ints = {1};
    travel.trackers[2].ints = {2};
    const auto rows =
        tl::core::waypoint_rows(travel, [](const std::string& name) { return name != "GONE"; });
    TL_CHECK_EQ(rows.size(), 3u);
    TL_CHECK_EQ(rows[0].dungeon, std::string("MAIN"));
    TL_CHECK_EQ(rows[0].depth, 4);
    TL_CHECK_EQ(rows[1].depth, 9);
    TL_CHECK_EQ(rows[2].dungeon, std::string("SIDE"));
}

TL_TEST(the_menu_stops_filling_at_its_last_row) {
    TravelRecord travel = with_trackers({"MAIN"});
    for (int depth = 0; depth < 14; ++depth) travel.trackers[0].ints.push_back(depth);
    const auto rows = tl::core::waypoint_rows(travel, nullptr);
    TL_CHECK_EQ(rows.size(), static_cast<size_t>(tl::core::kWaypointMenuRows));
    TL_CHECK_EQ(rows.back().depth, tl::core::kWaypointMenuRows - 1);
    // A click past the filled rows does nothing.
    TL_CHECK(tl::core::waypoint_choice(rows, 9) != nullptr);
    TL_CHECK(tl::core::waypoint_choice(rows, 10) == nullptr);
    TL_CHECK(tl::core::waypoint_choice({}, 0) == nullptr);
}

TL_TEST(the_entry_type_follows_perform_warp) {
    using tl::core::warp_entry_type;
    // -99: the town portal's 2, whatever else.
    TL_CHECK_EQ(warp_entry_type(-99, 0, true, false, false, "MAIN", "TOWN"), 2);
    // Within a dungeon: 1 down, 0 up, 1 with neither number set.
    TL_CHECK_EQ(warp_entry_type(1, 0, false, false, false, "MAIN", "MAIN"), 1);
    TL_CHECK_EQ(warp_entry_type(-1, 0, false, false, false, "MAIN", "MAIN"), 0);
    TL_CHECK_EQ(warp_entry_type(0, 0, false, false, false, "MAIN", "MAIN"), 1);
    // The 8-byte compare: an absolute depth alone, with no delta, is not
    // positive and so gives 0.
    TL_CHECK_EQ(warp_entry_type(0, 5, false, false, false, "MAIN", "MAIN"), 0);
    // Into another dungeon: 6 for a waypoint warp, 5 for any other...
    TL_CHECK_EQ(warp_entry_type(0, 4, false, false, true, "TOWN", "MAIN"),
                tl::core::kEntryWaypoint);
    TL_CHECK_EQ(warp_entry_type(0, 0, false, false, false, "TOWN", "MAIN"), 5);
    // ... unless the warp keeps its entry.
    TL_CHECK_EQ(warp_entry_type(1, 0, false, true, true, "TOWN", "MAIN"), 1);
}
