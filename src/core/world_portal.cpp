// The gameplay half of render/renderer_portal.cpp (core/world.h).
#include "core/audio.h"
#include "core/text.h"
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

// PORT: the level as `WarpOrigin` reads it (`CDungeon +0x80`, the parent dungeon).
tl::core::WarpOrigin World::warp_origin() const {
    tl::core::WarpOrigin from;
    from.dungeon = level_dungeon;
    from.depth = level_depth;
    if (level_dungeon.empty()) return from;
    // `CDungeon +0x80`, the dungeon's `PARENT_DUNGEON`.
    if (const tl::core::Dungeon* dungeon =
            loaded_dungeon_manager().dungeon_by_name(level_dungeon)) {
        from.parent = dungeon->def.parent_dungeon;
    }
    return from;
}

std::string World::waypoint_label(const tl::core::WaypointRow& row,
                                  const tl::core::DungeonFiles& files) {
    // CWaypointMenu::setOpen: LEVELNAME, with
    // " - Floor <depth + 1>" when SHOWFLOORNUMBER - CLevel::getName's formula.
    tl::core::Dungeon* dungeon =
        files.dungeon_by_name ? files.dungeon_by_name(row.dungeon) : nullptr;
    if (dungeon == nullptr) return row.dungeon;
    // The dungeon's kept template for that depth, built on `g_Rand` as it
    // stands if this is the first asking.
    const tl::core::LevelTemplate* found = dungeon->get_level_template_for_depth(
        static_cast<uint32_t>(std::max(0, row.depth)), files, tl::core::game_random());
    if (found == nullptr) return row.dungeon;
    return tl::core::level_display_name(tl::core::strip_markup(found->level_name),
                                        found->show_floor_number, row.depth);
}

void World::drive_waypoint_menu(const tl::core::InputState& in, bool confirm_edge, bool skip_edge) {
    // The console's dropdown: up and down move the highlight over the filled
    // rows, A is `onClick` on it, B is the close button (`handle_CloseButton`).
    const int rows = static_cast<int>(waypoint_rows.size());
    if (std::abs(in.move_y) > 0.5f) {
        if (!waypoint_stick_held && rows > 0) {
            waypoint_selected = (waypoint_selected + (in.move_y > 0.5f ? rows - 1 : 1)) % rows;
            play_ui(tl::core::InterfaceSound::Navigate);
            refresh_waypoint_menu();
        }
        waypoint_stick_held = true;
    } else {
        waypoint_stick_held = false;
    }
    if (skip_edge) {
        close_waypoint_menu();
        return;
    }
    if (!confirm_edge) return;
    // `CWaypointMenu::onClick`: a row past the filled ones
    // does nothing, and `setOpen(false)` follows every click.
    const tl::core::WaypointRow* row = tl::core::waypoint_choice(waypoint_rows, waypoint_selected);
    if (row != nullptr) warp_to_waypoint(*row);
    close_waypoint_menu();
}

void World::warp_to_waypoint(const tl::core::WaypointRow& row) {
    // `CGameClient::warpLevels(dungeon, 0, depth, true, L"", false)`; the warp
    // is main.cpp's (`Renderer::dungeon_transition`). `performWarp` plays client
    // slot 0x1c, `TOWNPORTALENTER`, for a waypoint warp.
    TL_LOG_INFO("waypoints: warp to %s depth %d", row.dungeon.c_str(), row.depth);
    play_ui(tl::core::InterfaceSound::TownPortalEnter);
    warp_pending = true;
    warp_dungeon = row.dungeon;
    warp_floor = row.depth + 1;
    warp_level_delta = 0;
    warp_level_absolute = row.depth;
    warp_waypoint = true;
    dungeon_transition_pending = true;
}

void World::open_town_portal() {
    // `CCharacter::applyInstantEffect`: the character's `openPortal`
    // (vtable +0x3e8) on its own level, `CPlayer::openPortal`.
    //
    if (config.travel == nullptr) return;
    const tl::core::Vec3 me = player.position();
    const bool opened = tl::core::open_portal(*config.travel, portal_level(), [&] {
        // `CLevel::randomOpenPosition(position, 3.0, false)`.
        return tl::core::random_open_position(
            me, 0.0f, 3.0f,
            [this](const tl::core::Vec3& p) {
                return !passability.ready() ||
                       (passability.map_passable(p) && passability.position_passable(p));
            },
            tl::core::volatile_random());
    });
    if (!opened) return;
    delete_open_portals();
    create_portals();
    TL_LOG_INFO("portal: opened on %s depth %d", level_dungeon.c_str(), level_depth);
}

void World::activate_waypoint() {
    // CWaypointActivator::activate: the player adds the
    // level's dungeon and depth; a new one raises g_WaypointActivated as the
    // big text for 1.0.
    if (config.travel == nullptr) return;
    const bool added = tl::core::add_waypoint(*config.travel, level_dungeon, level_depth);
    TL_LOG_INFO("waypoint: %s depth %d %s", level_dungeon.c_str(), level_depth,
                added ? "activated" : "not added (known already, or no tracker)");
    if (!added) return;
    set_big_text("Waypoint Activated", 1.0f);
    set_small_text(std::string(), 1.0f);
}

}  // namespace tl::core
