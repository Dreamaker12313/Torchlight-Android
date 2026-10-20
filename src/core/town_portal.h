// The town portal (`CPlayer::openPortal`, `CPlayer::createPortals`, the -99 branch of
// `CGameClient::performWarp`) and map-scroll portals. The portal lives only in
// `TravelRecord::portal_*`; its props are made again every time a level is entered.
#pragma once

#include "core/collision.h"
#include "core/level_memory.h"

#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// What the portal code reads off a `CLevel`.
struct PortalLevel {
    std::string dungeon;  ///< +0x280, the dungeon's upper-cased `NAME`
    int32_t depth = 0;    ///< +0x1a4
    /// +0x1d8, the level template; a plain layout no dungeon owns has none.
    bool has_template = false;
    bool allow_portals = false;  ///< template +0x84, the stratum's `ALLOW_PORTALS`
    bool is_town = false;        ///< template +0x86, the stratum's `IS_TOWN`
    /// +0x14c, the last `Town Portal` property node `CLevel::loadRoomLayout`
    /// met.
    Vec3 town_portal_spot;
    /// +0x158, likewise for `Waypoint Start`; (-1, -1, -1) when the level has none.
    Vec3 waypoint_spot{-1.0f, -1.0f, -1.0f};
};

/// `CCharacter::isEffectValidForUnit` for
/// `OPEN TOWN PORTAL` (0x2d).
bool town_portal_allowed(const PortalLevel& level);

/// `CPlayer::openPortal`. `open_position` is
/// `CLevel::randomOpenPosition(player position, 3.0, false)`, drawn only when allowed. On true
/// the caller runs `CLevel::deleteOpenPortals` and
/// `portals_to_create`.
bool open_portal(TravelRecord& travel, const PortalLevel& level,
                 const std::function<Vec3()>& open_position);

/// One prop `createPortals` makes: the unit's `NAME` under `PROPS`, and where.
struct PortalPlacement {
    std::string unit;
    Vec3 at;
};

/// The `PROPS` units `createPortals` creates.
inline constexpr const char* kWaypointPortalUnit = "WAYPOINT PORTAL";
inline constexpr const char* kReturnToTownUnit = "RETURN TO TOWN";
inline constexpr const char* kReturnToDungeonUnit = "RETURN TO DUNGEON";

/// `CPlayer::createPortals`: the props a level gets as it
/// is entered.
std::vector<PortalPlacement> portals_to_create(const TravelRecord& travel,
                                               const PortalLevel& level);

/// `CGameClient::performWarp` for `warpLevels("Town", -99)`:
/// to the town, or from a town back to the portal's level.
struct TownPortalWarp {
    std::string dungeon;          ///< `TOWN`, or `+0x7b8`
    int32_t depth = 0;            ///< 0, or `+0x7b4`
    bool back_to_portal = false;  ///< the branch's `bVar8`
};
TownPortalWarp town_portal_destination(const TravelRecord& travel, const PortalLevel& level);

/// `performWarp`'s arrival for `back_to_portal`: stand the player by the portal
/// (`randomOpenPosition(portal, 1.0, false)`) and close it. No props are made on this arrival.
Vec3 arrive_back_at_portal(TravelRecord& travel,
                           const std::function<Vec3(const Vec3&)>& open_position);

// ---------------------------------------------------------------------------
// Temporary dungeon portals: a map scroll's `MAP PORTAL` prop, warping to its dungeon
// ---------------------------------------------------------------------------

/// `CCharacter::isEffectValidForUnit` for `OPEN PORTAL`
/// (0x8d): portals allowed, or a town.
bool map_portal_allowed(const PortalLevel& level);

/// The `PROPS` unit `CPlayer::openMapPortal` creates.
inline constexpr const char* kMapPortalUnit = "MAP PORTAL";

/// One item of a level, as the two walks below read it.
struct TemporaryPortal {
    /// A map portal's `WARP` effect name; empty for any other item.
    std::string warp;
    Vec3 at;  ///< `getPosition(true)`
    /// The level deletes it on its next update.
    bool marked = false;
};

/// What `CPlayer::openMapPortal` makes: the prop, with a
/// saved always-on `WARP` effect named `STRINGS::StringUpper(dungeon)`.
TemporaryPortal open_map_portal(const std::string& dungeon,
                                const std::function<Vec3()>& open_position);

/// `findTemporaryDungeonPortal`'s answer for none; `performWarp` tests the Y alone.
///
inline constexpr float kNoTemporaryPortal = -999.0f;

/// `CLevel::findTemporaryDungeonPortal(std::wstring)`. Not yet
/// transcribed: its `TEMPPORTAL` half (`CSkillEffectAndAffixes::getPortalSkillName`).
///
Vec3 find_temporary_dungeon_portal(const std::vector<TemporaryPortal>& items,
                                   const std::string& dungeon);

/// `CLevel::deleteTemporaryDungeonPortals(std::wstring)`: marks
/// them, with `CPlayer::clearDungeonHistory` for each. Returns the number marked.
size_t delete_temporary_dungeon_portals(std::vector<TemporaryPortal>& items,
                                        const std::string& dungeon, TravelRecord* travel);

}  // namespace tl::core
