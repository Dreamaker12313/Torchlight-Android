#include "core/town_portal.h"

namespace tl::core {

namespace {

// PORT: the save format's vector.
Vec3 from_save(const tl::formats::SaveVec3& v) { return {v.x, v.y, v.z}; }

// PORT: the save format's vector.
tl::formats::SaveVec3 to_save(const Vec3& v) {
    tl::formats::SaveVec3 out;
    out.x = v.x;
    out.y = v.y;
    out.z = v.z;
    return out;
}

}  // namespace

bool town_portal_allowed(const PortalLevel& level) {
    // `CCharacter::isEffectValidForUnit`,
    // type 0x2d.
    return level.has_template && level.allow_portals;
}

bool open_portal(TravelRecord& travel, const PortalLevel& level,
                 const std::function<Vec3()>& open_position) {
    // `CPlayer::openPortal`
    if (!town_portal_allowed(level)) return false;
    travel.portal_open = true;
    travel.portal_depth = level.depth;
    travel.portal_dungeon = level.dungeon;
    travel.portal_position = to_save(open_position());
    return true;
}

std::vector<PortalPlacement> portals_to_create(const TravelRecord& travel,
                                               const PortalLevel& level) {
    // `CPlayer::createPortals`.
    std::vector<PortalPlacement> out;
    const Vec3& waypoint = level.waypoint_spot;
    const bool no_waypoint = waypoint.x == -1.0f && waypoint.y == -1.0f && waypoint.z == -1.0f;
    if (!no_waypoint) {
        // The first tracker (`+0xa40`) with a waypoint (`+0x18`) makes one portal.
        for (const tl::formats::DungeonTrackerSave& tracker : travel.trackers) {
            if (tracker.ints.empty()) continue;
            out.push_back({kWaypointPortalUnit, waypoint});
            break;
        }
    }
    if (!travel.portal_open) return out;
    const bool portal_level =
        travel.portal_depth == level.depth && travel.portal_dungeon == level.dungeon;
    // Away from its own level a portal shows only in a town.
    if (!portal_level && !level.is_town) return out;
    if (level.is_town) {
        out.push_back({kReturnToDungeonUnit, level.town_portal_spot});
    } else {
        out.push_back({kReturnToTownUnit, from_save(travel.portal_position)});
    }
    return out;
}

TownPortalWarp town_portal_destination(const TravelRecord& travel, const PortalLevel& level) {
    // `CGameClient::performWarp`.
    TownPortalWarp out;
    if (!level.has_template || !level.is_town) {
        out.dungeon = "TOWN";
        out.depth = 0;
        out.back_to_portal = false;
    } else {
        out.dungeon = travel.portal_dungeon;
        out.depth = travel.portal_depth;
        out.back_to_portal = true;
    }
    return out;
}

Vec3 arrive_back_at_portal(TravelRecord& travel,
                           const std::function<Vec3(const Vec3&)>& open_position) {
    // `performWarp`; the caller does `dropToGround(4.0)`.
    const Vec3 at = open_position(from_save(travel.portal_position));
    travel.portal_open = false;
    return at;
}

bool map_portal_allowed(const PortalLevel& level) {
    // `CCharacter::isEffectValidForUnit`,
    // type 0x8d.
    return level.has_template && (level.allow_portals || level.is_town);
}

TemporaryPortal open_map_portal(const std::string& dungeon,
                                const std::function<Vec3()>& open_position) {
    // `CPlayer::openMapPortal`: the open
    // position is drawn before the unit is created.
    TemporaryPortal portal;
    portal.at = open_position();
    portal.warp = upper_ascii(dungeon);
    return portal;
}

Vec3 find_temporary_dungeon_portal(const std::vector<TemporaryPortal>& items,
                                   const std::string& dungeon) {
    // `CLevel::findTemporaryDungeonPortal`.
    // A marked item is still in the list and still answers.
    for (const TemporaryPortal& item : items) {
        if (!item.warp.empty() && item.warp == dungeon) return item.at;
    }
    return {kNoTemporaryPortal, kNoTemporaryPortal, kNoTemporaryPortal};
}

size_t delete_temporary_dungeon_portals(std::vector<TemporaryPortal>& items,
                                        const std::string& dungeon, TravelRecord* travel) {
    // `CLevel::deleteTemporaryDungeonPortals`.
    size_t marked = 0;
    for (TemporaryPortal& item : items) {
        if (item.warp.empty() || item.warp != dungeon) continue;
        item.marked = true;
        ++marked;
        if (travel != nullptr) clear_dungeon_history(*travel, dungeon);
    }
    return marked;
}

}  // namespace tl::core
