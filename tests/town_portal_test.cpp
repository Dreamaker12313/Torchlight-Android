// The town portal: `CPlayer::openPortal`, `CPlayer::createPortals` and the -99
// branch of `CGameClient::performWarp`.
#include "core/town_portal.h"
#include "test_support.h"

using tl::core::PortalLevel;
using tl::core::TravelRecord;
using tl::core::Vec3;

namespace {

PortalLevel mines(int depth) {
    PortalLevel level;
    level.dungeon = "MAIN";
    level.depth = depth;
    level.has_template = true;
    level.allow_portals = true;
    return level;
}

PortalLevel town() {
    PortalLevel level;
    level.dungeon = "TOWN";
    level.has_template = true;
    level.allow_portals = false;
    level.is_town = true;
    level.town_portal_spot = {-12.0f, 0.5f, 30.0f};
    return level;
}

}  // namespace

// `CPlayer::openMapPortal`, `CLevel::findTemporaryDungeonPortal` and
// `deleteTemporaryDungeonPortals`.
TL_TEST(a_map_portal_is_allowed_in_a_town_and_wherever_portals_are) {
    TL_CHECK(tl::core::map_portal_allowed(mines(3)));
    TL_CHECK(tl::core::map_portal_allowed(town()));
    PortalLevel sealed = mines(3);
    sealed.allow_portals = false;
    TL_CHECK(!tl::core::map_portal_allowed(sealed));
    PortalLevel bare;
    bare.allow_portals = true;
    bare.is_town = true;
    TL_CHECK(!tl::core::map_portal_allowed(bare));
}

TL_TEST(a_map_portal_is_found_by_its_dungeon_and_takes_the_dungeons_history_with_it) {
    std::vector<tl::core::TemporaryPortal> items;
    // A town portal's prop: no `WARP`.
    tl::core::TemporaryPortal plain;
    plain.at = {1.0f, 0.0f, 1.0f};
    items.push_back(plain);
    int drawn = 0;
    items.push_back(tl::core::open_map_portal("randommapmine1-10", [&] {
        ++drawn;
        return Vec3{4.0f, 0.5f, -6.0f};
    }));
    items.push_back(tl::core::open_map_portal("RandomMapCrypt1-10", [&] {
        ++drawn;
        return Vec3{9.0f, 0.5f, 2.0f};
    }));
    TL_CHECK_EQ(drawn, 2);
    // `STRINGS::StringUpper` on the effect's name.
    TL_CHECK_EQ(items[1].warp, std::string("RANDOMMAPMINE1-10"));

    const Vec3 crypt = tl::core::find_temporary_dungeon_portal(items, "RANDOMMAPCRYPT1-10");
    TL_CHECK_EQ(crypt.x, 9.0f);
    TL_CHECK_EQ(crypt.z, 2.0f);
    const Vec3 none = tl::core::find_temporary_dungeon_portal(items, "MAIN");
    TL_CHECK_EQ(none.x, tl::core::kNoTemporaryPortal);
    TL_CHECK_EQ(none.y, tl::core::kNoTemporaryPortal);
    TL_CHECK_EQ(none.z, tl::core::kNoTemporaryPortal);

    TravelRecord travel;
    tl::formats::LevelSaveState visited;
    visited.dungeon = "RANDOMMAPMINE1-10";
    visited.depth = 0;
    visited.time = 12.0f;
    travel.levels.push_back(visited);
    tl::formats::LevelSaveState kept;
    kept.dungeon = "MAIN";
    kept.time = 30.0f;
    travel.levels.push_back(kept);

    TL_CHECK_EQ(tl::core::delete_temporary_dungeon_portals(items, "RANDOMMAPMINE1-10", &travel),
                static_cast<size_t>(1));
    TL_CHECK(!items[0].marked);
    TL_CHECK(items[1].marked);
    TL_CHECK(!items[2].marked);
    // `CPlayer::clearDungeonHistory`: the dungeon's levels are stale.
    TL_CHECK_EQ(travel.levels[0].time, -1.0f);
    TL_CHECK_EQ(travel.levels[1].time, 30.0f);
    // No portal to the dungeon: nothing marked, no history touched.
    TL_CHECK_EQ(tl::core::delete_temporary_dungeon_portals(items, "MAIN", &travel),
                static_cast<size_t>(0));
    TL_CHECK_EQ(travel.levels[1].time, 30.0f);
}

TL_TEST(a_portal_is_not_opened_where_the_template_forbids_it) {
    TravelRecord travel;
    int drawn = 0;
    PortalLevel level = mines(2);
    level.allow_portals = false;
    TL_CHECK(!tl::core::open_portal(travel, level, [&] {
        ++drawn;
        return Vec3{};
    }));
    TL_CHECK(!travel.portal_open);
    // `randomOpenPosition` is not drawn for a refused portal.
    TL_CHECK_EQ(drawn, 0);
    // A plain layout has no template at all.
    PortalLevel bare;
    bare.allow_portals = true;
    TL_CHECK(!tl::core::town_portal_allowed(bare));
}

TL_TEST(an_opened_portal_stands_on_its_level_and_in_the_town) {
    TravelRecord travel;
    TL_CHECK(tl::core::open_portal(travel, mines(2), [] { return Vec3{4.0f, 1.0f, -7.0f}; }));
    TL_CHECK(travel.portal_open);
    TL_CHECK_EQ(travel.portal_dungeon, std::string("MAIN"));
    TL_CHECK_EQ(travel.portal_depth, 2);

    const auto here = tl::core::portals_to_create(travel, mines(2));
    TL_CHECK_EQ(here.size(), 1u);
    TL_CHECK_EQ(here[0].unit, std::string(tl::core::kReturnToTownUnit));
    TL_CHECK(here[0].at.x == 4.0f && here[0].at.y == 1.0f && here[0].at.z == -7.0f);

    // Another floor of the same dungeon gets nothing.
    TL_CHECK(tl::core::portals_to_create(travel, mines(3)).empty());

    // The town gets the way back, at its `Town Portal` node.
    const auto home = tl::core::portals_to_create(travel, town());
    TL_CHECK_EQ(home.size(), 1u);
    TL_CHECK_EQ(home[0].unit, std::string(tl::core::kReturnToDungeonUnit));
    TL_CHECK(home[0].at.x == -12.0f && home[0].at.z == 30.0f);

    travel.portal_open = false;
    TL_CHECK(tl::core::portals_to_create(travel, town()).empty());
}

TL_TEST(a_waypoint_portal_needs_a_waypoint_start_and_a_known_waypoint) {
    TravelRecord travel;
    PortalLevel level = town();
    level.waypoint_spot = {1.0f, 2.0f, 3.0f};
    TL_CHECK(tl::core::portals_to_create(travel, level).empty());
    tl::formats::DungeonTrackerSave tracker;
    tracker.name = "MAIN";
    travel.trackers.push_back(tracker);
    TL_CHECK(tl::core::portals_to_create(travel, level).empty());
    tracker.ints = {3};
    travel.trackers.push_back(tracker);
    travel.trackers.push_back(tracker);
    const auto out = tl::core::portals_to_create(travel, level);
    // One portal however many trackers have one.
    TL_CHECK_EQ(out.size(), 1u);
    TL_CHECK_EQ(out[0].unit, std::string(tl::core::kWaypointPortalUnit));
    // (-1, -1, -1) is no `Waypoint Start`.
    level.waypoint_spot = {-1.0f, -1.0f, -1.0f};
    TL_CHECK(tl::core::portals_to_create(travel, level).empty());
}

TL_TEST(the_portal_warp_goes_home_and_back_to_where_the_portal_stands) {
    TravelRecord travel;
    tl::core::open_portal(travel, mines(4), [] { return Vec3{9.0f, 0.0f, 9.0f}; });

    const tl::core::TownPortalWarp out = tl::core::town_portal_destination(travel, mines(4));
    TL_CHECK_EQ(out.dungeon, std::string("TOWN"));
    TL_CHECK_EQ(out.depth, 0);
    TL_CHECK(!out.back_to_portal);

    const tl::core::TownPortalWarp back = tl::core::town_portal_destination(travel, town());
    TL_CHECK_EQ(back.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(back.depth, 4);
    TL_CHECK(back.back_to_portal);

    Vec3 asked;
    const Vec3 at = tl::core::arrive_back_at_portal(travel, [&](const Vec3& p) {
        asked = p;
        return Vec3{p.x + 0.5f, p.y, p.z};
    });
    TL_CHECK(asked.x == 9.0f && asked.z == 9.0f);
    TL_CHECK(at.x == 9.5f);
    // Going back closes it.
    TL_CHECK(!travel.portal_open);
}
