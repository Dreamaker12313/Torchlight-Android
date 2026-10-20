// Where a warp leads: `CGameClient::warpLevels` and `performWarp`, on the shipped Warpers.
// `CWarper::CWarper`'s defaults are delta 1, absolute 0.
#include "core/warp.h"
#include "test_support.h"

using tl::core::TravelRecord;
using tl::core::WarpDestination;
using tl::core::WarpOrigin;

namespace {

WarpOrigin at(const char* dungeon, int depth, const char* parent = "") {
    WarpOrigin from;
    from.dungeon = dungeon;
    from.depth = depth;
    from.parent = parent;
    return from;
}

WarpDestination warp(const char* name, int delta, int absolute, const WarpOrigin& from,
                     const TravelRecord& travel = TravelRecord{}) {
    return tl::core::warp_destination(tl::core::warp_levels(name, delta, absolute, travel), from,
                                      travel);
}

}  // namespace

TL_TEST(a_named_dungeon_lands_on_level_absolute) {
    // The town's `WarperMain`: `Main`, delta 0 - the mine's first floor.
    WarpDestination to = warp("Main", 0, 0, at("TOWN", 0));
    TL_CHECK(to.go);
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 0);
    // `SUNKEN_ENTRANCE_NODE`: `MAIN`, absolute 8.
    to = warp("MAIN", 0, 8, at("SUNKENTEMPLE", 3));
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 8);
    // The waygates: `TOWN`, delta 0.
    to = warp("TOWN", 0, 0, at("MAIN", 3));
    TL_CHECK_EQ(to.dungeon, std::string("TOWN"));
    TL_CHECK_EQ(to.depth, 0);
}

TL_TEST(no_name_or_the_dungeons_own_moves_within_it) {
    // `EXIT_NODE` (nothing authored) and `ENTRANCE_NODE` (delta -1).
    WarpDestination to = warp("", 1, 0, at("MAIN", 2, "Town"));
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 3);
    to = warp("", -1, 0, at("MAIN", 2, "Town"));
    TL_CHECK_EQ(to.depth, 1);
    // The hub's +3 and the glyph room's -2.
    TL_CHECK_EQ(warp("", 3, 0, at("MAIN", 10)).depth, 13);
    TL_CHECK_EQ(warp("", -2, 0, at("MAIN", 13)).depth, 11);
    // The current dungeon named, in any case, is no name: the delta counts.
    to = warp("main", 1, 0, at("MAIN", 2));
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 3);
    // `LEVEL ABSOLUTE` wins with no name too.
    TL_CHECK_EQ(warp("", 1, 5, at("MAIN", 2)).depth, 5);
}

TL_TEST(up_out_of_depth_0_is_the_parent_then_the_last_dungeon) {
    TravelRecord travel;
    travel.last_dungeon = "MAIN";
    travel.last_dungeon_depth = 6;
    // `MAIN`'s parent is the town, at depth 0.
    WarpDestination to = warp("", -1, 0, at("MAIN", 0, "Town"), travel);
    TL_CHECK(to.go);
    TL_CHECK_EQ(to.dungeon, std::string("TOWN"));
    TL_CHECK_EQ(to.depth, 0);
    // No parent: the last-dungeon record, at its depth.
    to = warp("", -1, 0, at("HATCHCRYPT", 0), travel);
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 6);
    // `HATCHPORTAL` (`HatchCrypt`, delta -1) from the town's depth 0 takes the
    // same branch: the named dungeon is replaced.
    to = warp("HatchCrypt", -1, 0, at("TOWN", 0), travel);
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 6);
    // Nowhere to go: no warp.
    TL_CHECK(!warp("", -1, 0, at("HATCHCRYPT", 0)).go);
    // A layout loaded on its own has no dungeon at all.
    TL_CHECK(!warp("", -1, 0, at("", 0), travel).go);
}

TL_TEST(lastdungeon_is_the_record_while_there_is_one) {
    TravelRecord travel;
    travel.last_dungeon = "MAIN";
    travel.last_dungeon_depth = 4;
    // A treasure room's `LASTDUNGEON`, delta 0.
    WarpDestination to = warp("LastDungeon", 0, 0, at("TREASURE", 0), travel);
    TL_CHECK_EQ(to.dungeon, std::string("MAIN"));
    TL_CHECK_EQ(to.depth, 4);
    // Depth 0 of the record is depth 0, not the current depth plus a delta.
    travel.last_dungeon_depth = 0;
    TL_CHECK_EQ(warp("LASTDUNGEON", 1, 0, at("TREASURE", 2), travel).depth, 0);
    // No record: the literal is kept, a dungeon no lookup finds.
    to = warp("LASTDUNGEON", 0, 0, at("TREASURE", 0));
    TL_CHECK_EQ(to.dungeon, std::string("LASTDUNGEON"));
}
