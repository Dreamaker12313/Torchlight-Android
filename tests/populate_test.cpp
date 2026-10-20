#include "core/populate.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::FormationWorld;
using tl::core::GoldWorld;
using tl::core::PopulatedFacing;
using tl::core::PopulatedKind;
using tl::core::PopulatedUnit;
using tl::core::PopulateSection;
using tl::core::PopulateWorld;
using tl::core::Randomizer;
using tl::core::RunicRandom;
using tl::core::SpawnMarker;
using tl::core::Vec3;

struct Placed {
    std::string unit;
    Vec3 at;
    bool character = false;
    bool champion = false;
    PopulatedFacing facing;
};

/// A level that is open everywhere: an open position one unit along x from
/// the point asked about.
struct Level {
    RunicRandom rolls{1234u};
    std::vector<std::string> roll{"Barrel"};
    std::vector<Placed> placed;
    int open_calls = 0;
    int refuse_first = 0;    ///< `randomOpenPosition` finds nowhere this many times
    int no_spawn_first = 0;  ///< the place found is in a no-spawn region this many times
    bool lines_open = true;
    PopulatedUnit made{PopulatedKind::Item, false, 0};
    int created = 0;
    int discarded = 0;
    std::vector<SpawnMarker> markers;

    PopulateWorld world() {
        PopulateWorld w;
        w.volatile_rolls = &rolls;
        w.roll = [this](const std::string&) { return roll; };
        w.random_open_position = [this](const Vec3& at, float) {
            ++open_calls;
            if (open_calls <= refuse_first) return at;
            return Vec3{at.x + 1.0f, at.y, at.z};
        };
        w.in_no_spawn_region = [this](const Vec3&, float radius) {
            TL_CHECK(radius == 1.0f);
            return open_calls <= refuse_first + no_spawn_first;
        };
        w.passable_between = [this](const Vec3&, const Vec3&) { return lines_open; };
        w.create = [this](const std::string&) {
            ++created;
            return made;
        };
        w.add_character = [this](const PopulatedUnit&, const std::string& unit, const Vec3& at,
                                 const PopulatedFacing& facing, bool champion) {
            placed.push_back({unit, at, true, champion, facing});
            return true;
        };
        w.add_item = [this](const PopulatedUnit&, const std::string& unit, const Vec3& at,
                            const PopulatedFacing& facing) {
            placed.push_back({unit, at, false, false, facing});
            return true;
        };
        w.discard = [this](const PopulatedUnit&) { ++discarded; };
        w.markers = &markers;
        return w;
    }
};

const PopulateSection kSection{Vec3{10.0f, -1000.0f, 20.0f}, Vec3{0.0f, 1000.0f, 0.0f}};

}  // namespace

/// One roll's unit lands inside the section's box (plus what the open-position
/// search moved it), at the draw height, and is counted.
TL_TEST(populate_places_a_rolled_item_inside_the_section) {
    Level level;
    const int made =
        tl::core::populate_section_of_level(kSection, "PROPS", false, false, level.world());
    TL_CHECK(made == 1);
    TL_CHECK(level.placed.size() == 1);
    const Placed& p = level.placed.front();
    TL_CHECK(p.unit == "Barrel");
    TL_CHECK(!p.character);
    TL_CHECK(p.at.x >= 1.0f && p.at.x <= 11.0f);
    TL_CHECK(p.at.z >= 0.0f && p.at.z <= 20.0f);
    TL_CHECK(p.at.y == 27.5f);
}

/// The draws are z, then x, then the turn: three ranges, two steps each.
TL_TEST(populate_draws_z_then_x_then_the_turn) {
    Level level;
    RunicRandom twin(1234u);
    const float z = twin.between(kSection.first.z, kSection.second.z);
    const float x = twin.between(kSection.first.x, kSection.second.x);
    twin.between(0.0f, 360.0f);
    tl::core::populate_section_of_level(kSection, "PROPS", false, false, level.world());
    TL_CHECK(level.placed.size() == 1);
    TL_CHECK(level.placed.front().at.z == z);
    TL_CHECK(level.placed.front().at.x == x + 1.0f);
    TL_CHECK(level.rolls.step() == twin.step());
}

/// A point with nowhere open near it, or one inside a no-spawn region, is
/// drawn again.
TL_TEST(populate_retries_a_refused_place) {
    Level level;
    level.refuse_first = 3;
    level.no_spawn_first = 2;
    const int made =
        tl::core::populate_section_of_level(kSection, "PROPS", false, false, level.world());
    TL_CHECK(made == 1);
    TL_CHECK(level.open_calls == 6);
}

/// Fifty tries and the unit is given up: nothing made, nothing counted.
TL_TEST(populate_gives_a_unit_up_after_fifty_tries) {
    Level level;
    level.refuse_first = 1000;
    const int made =
        tl::core::populate_section_of_level(kSection, "PROPS", false, false, level.world());
    TL_CHECK(made == 0);
    TL_CHECK(level.open_calls == 50);
    TL_CHECK(level.placed.empty());
}

/// A blocking item is only left where all four lines through it are passable;
/// one that does not block is not asked.
TL_TEST(populate_drops_a_blocking_item_from_a_tight_place) {
    Level level;
    level.lines_open = false;
    level.made = PopulatedUnit{PopulatedKind::Item, true, 0};
    TL_CHECK(tl::core::populate_section_of_level(kSection, "PROPS", false, false, level.world()) ==
             0);
    TL_CHECK(level.placed.empty());
    TL_CHECK(level.discarded == 1);

    Level barrels;
    barrels.lines_open = false;
    TL_CHECK(
        tl::core::populate_section_of_level(kSection, "PROPS", false, false, barrels.world()) == 1);
}

/// Characters are added and counted, with the champion flag passed on.
TL_TEST(populate_adds_characters_and_passes_the_champion_flag) {
    Level level;
    level.roll = {"Ratlin", "Ratlin"};
    level.made = PopulatedUnit{PopulatedKind::Character, false, 0};
    TL_CHECK(
        tl::core::populate_section_of_level(kSection, "MONSTERS", true, false, level.world()) == 2);
    TL_CHECK(level.placed.size() == 2);
    TL_CHECK(level.placed[0].character && level.placed[0].champion);
}

/// The props phase counts an empty roll as one; the monster phase only after more than ten
/// empty rolls in a row.
TL_TEST(populate_phase_counts_empty_rolls_by_its_rule) {
    const std::vector<PopulateSection> sections{kSection};
    Level empty;
    empty.roll.clear();
    Randomizer pick;
    pick.add_choice(0, 100);
    RunicRandom g_rand(99u);
    RunicRandom twin(99u);
    TL_CHECK(tl::core::populate_phase(sections, pick, g_rand, "PROPS", 5, false, false, true,
                                      empty.world()) == 0);
    for (int i = 0; i < 5; ++i) twin.between(0.0f, 1.0f);
    TL_CHECK(g_rand.state() == twin.state());

    TL_CHECK(tl::core::populate_phase(sections, pick, g_rand, "MONSTERS", 2, false, false, false,
                                      empty.world()) == 0);
    for (int i = 0; i < 22; ++i) twin.between(0.0f, 1.0f);
    TL_CHECK(g_rand.state() == twin.state());
}

/// A phase keeps rolling sections until the count is in, and an index past the
/// list reads the first section.
TL_TEST(populate_phase_fills_the_count) {
    const std::vector<PopulateSection> sections{kSection};
    Level level;
    Randomizer pick;
    pick.add_choice(7, 100);
    RunicRandom g_rand(5u);
    const int placed = tl::core::populate_phase(sections, pick, g_rand, "PROPS", 4, false, false,
                                                true, level.world());
    TL_CHECK(placed == 4);
    TL_CHECK(level.placed.size() == 4);
}

/// The unit is made before it is turned: the turn is the last draw of the
/// three, and a unit off a marker is not turned at all.
TL_TEST(populate_makes_the_unit_before_the_turn) {
    Level level;
    level.made = PopulatedUnit{PopulatedKind::Character, false, 0};
    PopulateWorld w = level.world();
    RunicRandom twin(1234u);
    twin.between(kSection.first.z, kSection.second.z);
    twin.between(kSection.first.x, kSection.second.x);
    float drawn_at_create = 0.0f;
    w.create = [&](const std::string&) {
        // `setLevel`'s hit points come off the stream here.
        drawn_at_create = level.rolls.between(0.0f, 1.0f);
        return level.made;
    };
    TL_CHECK(tl::core::populate_section_of_level(kSection, "MONSTERS", false, false, w) == 1);
    TL_CHECK(drawn_at_create == twin.between(0.0f, 1.0f));
    TL_CHECK(level.placed.front().facing.yaw == twin.between(0.0f, 360.0f) * 0.017453292f);
}

/// An NPC takes a free marker, turned as it is; ten picks on, the unit goes in the section.
TL_TEST(populate_puts_an_npc_on_a_free_marker) {
    Level level;
    level.made = PopulatedUnit{PopulatedKind::Character, false, 0};
    SpawnMarker marker;
    marker.position = Vec3{5.0f, 1.0f, 6.0f};
    marker.orientation = {0.0f, 0.0f, 1.0f, 0.0f};
    level.markers.push_back(marker);
    const uint64_t before = level.rolls.state();
    TL_CHECK(tl::core::populate_section_of_level(kSection, "NPCS", false, true, level.world()) ==
             1);
    // One marker: a pick between 1 and 1 does not step the stream, and a
    // unit off a marker draws no turn.
    TL_CHECK(level.rolls.state() == before);
    TL_CHECK(level.placed.front().at.x == 5.0f && level.placed.front().at.z == 6.0f);
    TL_CHECK(level.placed.front().facing.from_marker);
    TL_CHECK(level.markers.front().taken == 1);

    // Taken now: ten picks fail and the section's tries start at ten.
    level.refuse_first = 1000;
    TL_CHECK(tl::core::populate_section_of_level(kSection, "NPCS", false, true, level.world()) ==
             0);
    TL_CHECK(level.open_calls == 40);
}

/// Formations: none at all when a choice names no file; otherwise one node a
/// count, kept twenty apart, the count drawn after the choices are read.
TL_TEST(populate_formations_places_spaced_nodes) {
    const std::vector<PopulateSection> sections{kSection};
    Randomizer pick;
    pick.add_choice(0, 100);
    RunicRandom g_rand(42u);
    RunicRandom rolls(7u);
    std::vector<std::pair<std::string, Vec3>> made;
    FormationWorld world;
    world.volatile_rolls = &rolls;
    world.random_open_position = [](const Vec3& at, float) { return Vec3{at.x, 0.0f, at.z}; };
    world.in_no_spawn_region = [](const Vec3&, float radius) {
        TL_CHECK(radius == 12.0f);
        return false;
    };
    world.add_formation = [&](const std::string& file, const Vec3& at) {
        made.push_back({file, at});
    };
    int asked = 0;
    const auto wanted = [&asked]() {
        ++asked;
        return 3u;
    };
    std::vector<Vec3> existing;
    TL_CHECK(tl::core::populate_formations(sections, pick, {{"A", 1}, {"", 1}}, wanted, existing,
                                           g_rand, world) == 0);
    TL_CHECK(asked == 0);
    TL_CHECK(made.empty());

    TL_CHECK(tl::core::populate_formations(sections, pick, {{"A", 0}, {"B", 3}}, wanted, existing,
                                           g_rand, world) == 3);
    TL_CHECK(asked == 1);
    for (size_t i = 0; i < made.size(); ++i) {
        for (size_t j = i + 1; j < made.size(); ++j) {
            const float dx = made[i].second.x - made[j].second.x;
            const float dz = made[i].second.z - made[j].second.z;
            TL_CHECK(dx * dx + dz * dz >= 400.0f);
        }
    }
}

/// Gold: a point that the open search does not move makes nothing; one it
/// moves makes a pile of 2 to 12.
TL_TEST(populate_gold_makes_piles_where_the_search_moved_the_point) {
    const std::vector<PopulateSection> sections{kSection};
    Randomizer pick;
    pick.add_choice(0, 100);
    RunicRandom g_rand(3u);
    RunicRandom rolls(11u);
    std::vector<int32_t> amounts;
    GoldWorld world;
    world.volatile_rolls = &rolls;
    bool move = false;
    world.random_open_item_position = [&move](const Vec3& at, float radius) {
        TL_CHECK(radius == 3.0f);
        TL_CHECK(at.y == 7.5f);
        return move ? Vec3{at.x + 0.5f, at.y, at.z} : at;
    };
    world.in_no_spawn_region = [](const Vec3&, float radius) {
        TL_CHECK(radius == 0.5f);
        return false;
    };
    world.add_gold = [&](int32_t amount, const Vec3&) { amounts.push_back(amount); };
    TL_CHECK(tl::core::populate_gold(sections, pick, 4, g_rand, world) == 0);
    move = true;
    TL_CHECK(tl::core::populate_gold(sections, pick, 4, g_rand, world) == 4);
    for (int32_t amount : amounts) TL_CHECK(amount >= 2 && amount <= 12);
}

// `CLevel::populate`'s room record: the union of a room layout's sections,
// the first corners taken as the minimum and the second as the maximum.
TL_TEST(a_room_box_is_the_union_of_its_sections) {
    tl::core::LevelRoomBox room;
    TL_CHECK_EQ(room.state, 0);
    PopulateSection a;
    a.first = {0.0f, -1000.0f, 0.0f};
    a.second = {10.0f, 1000.0f, 10.0f};
    PopulateSection b;
    b.first = {10.0f, -1002.0f, -5.0f};
    b.second = {20.0f, 1003.0f, 5.0f};
    room.add(a);
    room.add(b);
    TL_CHECK_EQ(room.state, 1);
    TL_CHECK_EQ(room.low.x, 0.0f);
    TL_CHECK_EQ(room.low.y, -1002.0f);
    TL_CHECK_EQ(room.low.z, -5.0f);
    TL_CHECK_EQ(room.high.x, 20.0f);
    TL_CHECK_EQ(room.high.y, 1003.0f);
    TL_CHECK_EQ(room.high.z, 10.0f);
}

// `CLevel::getRoomIndexThatPositionIsIn`: the first record
// holding the point, ends included; an empty record is passed over.
TL_TEST(the_room_index_is_the_first_box_holding_the_point) {
    std::vector<tl::core::LevelRoomBox> rooms(3);
    PopulateSection s;
    s.first = {0.0f, -1000.0f, 0.0f};
    s.second = {10.0f, 1000.0f, 10.0f};
    rooms[1].add(s);
    s.first = {10.0f, -1000.0f, 0.0f};
    s.second = {20.0f, 1000.0f, 10.0f};
    rooms[2].add(s);
    TL_CHECK_EQ(tl::core::room_index_that_position_is_in(rooms, {5.0f, 0.0f, 5.0f}), 1);
    // On the shared edge: the first.
    TL_CHECK_EQ(tl::core::room_index_that_position_is_in(rooms, {10.0f, 0.0f, 5.0f}), 1);
    TL_CHECK_EQ(tl::core::room_index_that_position_is_in(rooms, {15.0f, 0.0f, 5.0f}), 2);
    TL_CHECK_EQ(tl::core::room_index_that_position_is_in(rooms, {25.0f, 0.0f, 5.0f}), -1);
}

// `CLevel::getItemByOriginalGuid`: the GUID in the saved
// room; the copy with the saved hierarchy hash, else the last one in the room.
TL_TEST(an_original_guid_is_found_by_room_then_hierarchy) {
    std::vector<tl::core::LevelRoomBox> rooms(2);
    PopulateSection s;
    s.first = {0.0f, -1000.0f, 0.0f};
    s.second = {10.0f, 1000.0f, 10.0f};
    rooms[0].add(s);
    s.first = {10.5f, -1000.0f, 0.0f};
    s.second = {20.0f, 1000.0f, 10.0f};
    rooms[1].add(s);
    std::vector<tl::core::OriginalGuidUnit> items = {
        {7, 100, {5.0f, 0.0f, 5.0f}},
        {7, 200, {15.0f, 0.0f, 5.0f}},
        {7, 300, {16.0f, 0.0f, 5.0f}},
        {8, 100, {5.0f, 0.0f, 5.0f}},
    };
    using tl::core::find_item_by_original_guid;
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 7, 1, 200), 1);
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 7, 1, 300), 2);
    // No copy in the room has the hash: the last one in it.
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 7, 1, 999), 2);
    // Any room, no hash: the first.
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 7, -1, 0), 0);
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 7, 0, 999), 0);
    TL_CHECK_EQ(find_item_by_original_guid(items, rooms, 9, -1, 0), -1);
}
