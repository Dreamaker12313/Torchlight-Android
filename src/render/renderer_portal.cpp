// The town portal: opening one, the props at either end, using them, and where the warp
// leaves the player. The rules are core/town_portal; this is the level's half.
#include "render/renderer_impl.h"

#include "core/audio.h"
#include "core/text.h"

#include <cmath>

namespace tl::render {

tl::core::PortalLevel Renderer::Impl::portal_level() const {
    tl::core::PortalLevel level;
    level.dungeon = level_dungeon;
    level.depth = level_depth;
    // A layout loaded on its own is no dungeon's level and has no template.
    level.has_template = !level_dungeon.empty();
    level.allow_portals = level_allows_portals;
    level.is_town = level_is_town;
    level.town_portal_spot = {town_portal_spot.x, town_portal_spot.y, town_portal_spot.z};
    level.waypoint_spot = {waypoint_spot.x, waypoint_spot.y, waypoint_spot.z};
    return level;
}

void Renderer::Impl::disable_portal(OpenPortal& portal) {
    destroy_particle_holders(portal.holders);
    portal.holders.clear();
    for (const OpenPortal::HeldSound& held : portal.sounds) tl::core::audio_stop_voice(held.voice);
    portal.sounds.clear();
    portal.enabled = false;
}

void Renderer::Impl::delete_open_portals() {
    // CLevel::deleteOpenPortals: every TOWNPORTAL (0x2b)
    // and WAYPOINTNODE (0xab) item goes (here at once, with its sounds), then
    // updateAutomapIcons. A map portal is neither and stays.
    load_unit_types();
    std::vector<OpenPortal> kept;
    for (OpenPortal& portal : open_portals) {
        const uint32_t type = unit_types.id_of(portal.unittype);
        if (!unit_types.isa(type, tl::core::unit_type::kTownPortal) &&
            !unit_types.isa(type, tl::core::unit_type::kWaypointNode)) {
            kept.push_back(std::move(portal));
            continue;
        }
        disable_portal(portal);
    }
    open_portals = std::move(kept);
    portal_targeted = -1;
    update_automap_icons();
}

void Renderer::Impl::create_portals() {
    // `CPlayer::createPortals`.
    if (config.travel == nullptr) return;
    for (const tl::core::PortalPlacement& placement :
         tl::core::portals_to_create(*config.travel, portal_level())) {
        // `CResourceManager::createUnit(L"PROPS", name)` and `CLevel::addItem`.
        TriggerProp prop;
        trigger_prop(placement.unit, prop);
        if (prop.display_name.empty()) {
            TL_LOG_WARN("portal: no PROPS unit called '%s'", placement.unit.c_str());
            continue;
        }
        OpenPortal portal;
        portal.unit = placement.unit;
        portal.display_name = prop.display_name;
        portal.unittype = prop.unittype;
        portal.at = ogre::Vector3(placement.at.x, placement.at.y, placement.at.z);
        // Drawn by its LAYOUTFILE: its own kind-0 CLayout (CTriggerUnit::unitInit
        // +0x240), not a CParticle.
        portal.holders =
            place_particle_layout(prop_layout_path(placement.unit), to_ogre(portal.at),
                                  Ogre::Quaternion::IDENTITY, nullptr, ParticleStart::Live);
        TL_LOG_INFO("portal: '%s' at (%.2f, %.2f, %.2f)", portal.display_name.c_str(),
                    static_cast<double>(portal.at.x), static_cast<double>(portal.at.y),
                    static_cast<double>(portal.at.z));
        open_portals.push_back(std::move(portal));
        start_portal_sounds(open_portals.back());
    }
    // `CLevel::updateAutomapIcons`, which
    // `createPortals` ends with.
    update_automap_icons();
}

void Renderer::Impl::start_portal_sounds(OpenPortal& portal) {
    // CSoundObjectDescriptor::descriptorSceneActivated:
    // each Sound with START ON ACTIVATE (+0x120; constructor)
    // plays at the node, or unpositioned without ENVIRONMENTAL (+0x123).
    const std::string reference = prop_layout_path(portal.unit);
    std::string path = resolve_archive_path(reference);
    if (path.empty()) path = resolve_archive_path(reference + ".cmp");
    if (path.empty()) return;
    const tl::formats::CmpScene* const parsed = layout_file(path);
    if (parsed == nullptr || !parsed->ok) return;
    const tl::formats::CmpScene& scene = *parsed;
    ensure_sound_index();
    namespace f = tl::formats::cmp_field::sound;
    const auto flag = [](const tl::formats::CmpNode& node, size_t index, bool fallback) {
        const tl::formats::CmpField* field = node.field(index);
        return field != nullptr && field->count() == 1 ? field->as_bool() : fallback;
    };
    const auto number = [](const tl::formats::CmpNode& node, size_t index, float fallback) {
        const tl::formats::CmpField* field = node.field(index);
        return field != nullptr && field->count() == 1 ? field->as_float() : fallback;
    };
    for (const tl::formats::CmpNode& node : scene.nodes) {
        if (node.type != tl::formats::cmp_type::kSound) continue;
        if (!flag(node, f::kStartOnActivate, true)) continue;
        const tl::formats::CmpField* guid = node.field(f::kGuid);
        if (guid == nullptr) continue;
        const SoundDefinition* definition = sound_by_guid(guid->as_text());
        if (definition == nullptr || definition->files.empty()) continue;

        OpenPortal::HeldSound held;
        held.voice = "portal/" + portal.unit + "/" + std::to_string(node.id);
        held.definition = guid->as_text();
        if (node.has_position()) {
            held.at = ogre::Vector3(node.position().x, node.position().y, node.position().z);
        }
        held.at += portal.at;
        held.volume = definition->volume * number(node, f::kVolume, 1.0f);
        // `CSoundBank::addSample`: the
        // definition's radius, or the node's when it authors none; none at all
        // without `ENVIRONMENTAL`.
        held.radius =
            definition->radius != 0.0f ? definition->radius : number(node, f::kRadius, 0.0f);
        if (!flag(node, f::kEnvironmental, true)) held.radius = 0.0f;

        const std::string resolved = resolve_archive_path(definition->files.front());
        if (!audio_enabled || !tl::core::audio_available()) {
            TL_LOG_INFO("portal: sound %s -> %s, %s, radius %.1f (no audio device, not played)",
                        held.definition.c_str(), resolved.empty() ? "(missing)" : resolved.c_str(),
                        definition->loops ? "looping" : "once", static_cast<double>(held.radius));
            continue;
        }
        float gain = 0.0f;
        float pan = 0.0f;
        positional_mix(held.volume, held.radius, to_ogre(held.at), gain, pan);
        tl::core::SamplePtr sample = sample_for(definition->files.front());
        if (!sample) continue;
        // A one-shot plays and is done; a loop is held and re-mixed as the
        // listener moves (`CSoundObject::updateSounds` keeps it up).
        tl::core::audio_play_voice(std::move(sample), held.voice, gain, pan, definition->loops);
        TL_LOG_INFO("portal: sound %s -> %s, %s, gain %.2f", held.definition.c_str(),
                    resolved.c_str(), definition->loops ? "looping" : "once",
                    static_cast<double>(gain));
        if (definition->loops) portal.sounds.push_back(held);
    }
}

void Renderer::Impl::step_portal_sounds() {
    for (const OpenPortal& portal : open_portals) {
        for (const OpenPortal::HeldSound& held : portal.sounds) {
            float gain = 0.0f;
            float pan = 0.0f;
            positional_mix(held.volume, held.radius, to_ogre(held.at), gain, pan);
            tl::core::audio_set_voice_mix(held.voice, gain, pan);
        }
    }
}

bool Renderer::Impl::add_map_portal(const Ogre::Vector3& at, const std::string& warp,
                                    bool enabled) {
    // `CResourceManager::createUnit(L"PROPS", L"MAP PORTAL")` - a `CTriggerUnit`
    // (`CREATEAS TRIGGER`, `UNITTYPE MAPPORTAL`) - then `CLevel::addItem` and
    // `CBaseUnit::addNewEffect` with the `WARP`.
    TriggerProp prop;
    trigger_prop(tl::core::kMapPortalUnit, prop);
    if (prop.display_name.empty()) {
        TL_LOG_WARN("portal: no PROPS unit called '%s'", tl::core::kMapPortalUnit);
        return false;
    }
    OpenPortal portal;
    portal.unit = tl::core::kMapPortalUnit;
    portal.display_name = prop.display_name;
    portal.unittype = prop.unittype;
    portal.at = from_ogre(at);
    portal.warp = warp;
    portal.enabled = enabled;
    if (enabled) {
        // Drawn by its `LAYOUTFILE` (`media/particles/objects/townPortal`), as
        // the town portal's props are (`create_portals`).
        portal.holders =
            place_particle_layout(prop_layout_path(portal.unit), to_ogre(portal.at),
                                  Ogre::Quaternion::IDENTITY, nullptr, ParticleStart::Live);
    }
    TL_LOG_INFO("portal: '%s' to %s at (%.2f, %.2f, %.2f)%s", portal.display_name.c_str(),
                warp.c_str(), static_cast<double>(at.x), static_cast<double>(at.y),
                static_cast<double>(at.z), enabled ? "" : " (used)");
    open_portals.push_back(std::move(portal));
    if (enabled) start_portal_sounds(open_portals.back());
    return true;
}

void Renderer::Impl::open_map_portal(const std::string& dungeon) {
    // `CPlayer::openMapPortal`.
    const tl::core::Vec3 me = player.position();
    const tl::core::TemporaryPortal made = tl::core::open_map_portal(dungeon, [&] {
        // `CLevel::randomOpenPosition(position, 3.0, false)`.
        return tl::core::random_open_position(
            me, 0.0f, 3.0f,
            [this](const tl::core::Vec3& p) {
                return !passability.ready() ||
                       (passability.map_passable(p) && passability.position_passable(p));
            },
            tl::core::volatile_random());
    });
    add_map_portal(Ogre::Vector3(made.at.x, made.at.y, made.at.z), made.warp, true);
}

std::vector<tl::core::TemporaryPortal> Renderer::Impl::temporary_portals() const {
    std::vector<tl::core::TemporaryPortal> out;
    out.reserve(open_portals.size());
    for (const OpenPortal& portal : open_portals) {
        tl::core::TemporaryPortal item;
        item.warp = portal.warp;
        item.at = {portal.at.x, portal.at.y, portal.at.z};
        out.push_back(std::move(item));
    }
    return out;
}

void Renderer::Impl::use_targeted_portal() {
    if (portal_targeted < 0 || static_cast<size_t>(portal_targeted) >= open_portals.size()) return;
    OpenPortal& portal = open_portals[static_cast<size_t>(portal_targeted)];
    // `CTriggerUnit::trigger`, after
    // `setTriggerState` and `CItem::interact`, by the prop's type.
    load_unit_types();
    const uint32_t type = unit_types.id_of(portal.unittype);
    if (unit_types.isa(type, tl::core::unit_type::kTownPortal)) {
        // `warpLevels(L"Town", -99, 0, false, L"")`; the warp is main.cpp's
        // (`Renderer::dungeon_transition`). `performWarp` plays client slot
        // 0x1c, `TOWNPORTALENTER`.
        TL_LOG_INFO("portal: A on '%s'", portal.display_name.c_str());
        play_ui(tl::core::InterfaceSound::TownPortalEnter);
        portal_warp_pending = true;
        dungeon_transition_pending = true;
        return;
    }
    if (unit_types.isa(type, tl::core::unit_type::kMapPortal)) {
        // The name of the prop's first `WARP` (0x71) effect; with one, the
        // prop is turned off (vtable +0x40, `setEnabled(false)`) and
        // `warpLevels(name, 0, 0, false, L"", false)` follows.
        if (portal.warp.empty()) return;
        TL_LOG_INFO("portal: A on '%s' to %s", portal.display_name.c_str(), portal.warp.c_str());
        const std::string name = portal.warp;
        disable_portal(portal);
        portal_targeted = -1;
        const tl::core::TravelRecord none;
        const tl::core::TravelRecord& travel = config.travel != nullptr ? *config.travel : none;
        const tl::core::WarpRequest request = tl::core::warp_levels(name, 0, 0, travel);
        const tl::core::WarpDestination to =
            tl::core::warp_destination(request, warp_origin(), travel);
        if (!to.go) return;
        warp_pending = true;
        warp_dungeon = to.dungeon;
        warp_floor = std::max(0, to.depth) + 1;
        warp_level_delta = request.delta;
        warp_level_absolute = request.absolute;
        warp_waypoint = false;
        dungeon_transition_pending = true;
        return;
    }
    if (unit_types.isa(type, tl::core::unit_type::kWaypointNode)) {
        // `CGameUI::toggleWaypointMenu`.
        TL_LOG_INFO("portal: A on '%s'", portal.display_name.c_str());
        toggle_waypoint_menu();
    }
}

// ---------------------------------------------------------------------------
// Waypoints
// ---------------------------------------------------------------------------

void Renderer::Impl::toggle_waypoint_menu() {
    // CGameUI::toggleWaypointMenu: unPause, closeAll,
    // then setOpen(!open). A reaches a prop only while no menu has the pad.
    if (waypoint_screen.visible()) {
        close_waypoint_menu();
    } else {
        open_waypoint_menu();
    }
}

void Renderer::Impl::open_waypoint_menu() {
    // `CWaypointMenu::setOpen(true)`: the title, every row
    // hidden, then one row per waypoint (`core::waypoint_rows`), shown and
    // captioned.
    if (config.travel == nullptr) return;
    if (!waypoint_screen.ready()) {
        TL_LOG_WARN("waypoints: no waypointmenu_xenon.layout mounted; the menu cannot open");
        return;
    }
    const tl::core::DungeonFiles files = archive_dungeon_files();
    waypoint_rows = tl::core::waypoint_rows(*config.travel, [&files](const std::string& name) {
        return files.dungeon_by_name && files.dungeon_by_name(name) != nullptr;
    });
    // `CDropdownMenu::setTitle(g_Waypoints)`, "Waypoints".
    waypoint_screen.set_caption("Title", "Waypoints");
    for (int i = 0; i < tl::core::kWaypointMenuRows; ++i) {
        const std::string button = "Button" + std::to_string(i + 1);
        const std::string choice = "Choice" + std::to_string(i + 1);
        const bool filled = i < static_cast<int>(waypoint_rows.size());
        waypoint_screen.set_hidden(button, !filled);
        waypoint_screen.set_hidden(choice, !filled);
        if (!filled) continue;
        const tl::core::WaypointRow& row = waypoint_rows[static_cast<size_t>(i)];
        const std::string label = waypoint_label(row, files);
        waypoint_screen.set_caption(choice, label);
        TL_LOG_INFO("waypoints: row %d '%s' (%s depth %d)", i, label.c_str(), row.dungeon.c_str(),
                    row.depth);
    }
    waypoint_selected = 0;
    // A stick already held does not move the highlight the moment it opens.
    waypoint_stick_held = true;
    refresh_waypoint_menu();
    waypoint_screen.show();
    TL_LOG_INFO("waypoints: the menu opens with %zu row(s)", waypoint_rows.size());
}

void Renderer::Impl::close_waypoint_menu() {
    // `CWaypointMenu::setOpen(false)`.
    waypoint_screen.hide();
    TL_LOG_INFO("waypoints: the menu closes");
}

void Renderer::Impl::refresh_waypoint_menu() {
    // The pad's highlight on a row: its `HOVER` art (`GEM_BUTTON_ROLLOVER`).
    for (int i = 0; i < tl::core::kWaypointMenuRows; ++i) {
        waypoint_screen.set_hover("Button" + std::to_string(i + 1), i == waypoint_selected);
    }
    waypoint_screen.refresh();
}

void Renderer::activate_waypoint() { impl_->activate_waypoint(); }

bool Renderer::waypoint_menu_open() const { return impl_->waypoint_screen.visible(); }

bool Renderer::walk_into_waypoint_portal() {
    Impl& m = *impl_;
    m.load_unit_types();
    for (const Impl::OpenPortal& portal : m.open_portals) {
        if (!m.unit_types.isa(m.unit_types.id_of(portal.unittype),
                              tl::core::unit_type::kWaypointNode)) {
            continue;
        }
        place_player_at(portal.at.x, portal.at.z);
        advance_scene(0.25f);
        press_a();
        advance_scene(0.1f);
        return m.waypoint_screen.visible();
    }
    TL_LOG_INFO("--portal waypoints: no waypoint portal on this level");
    return false;
}

// PORT: a headless verification step: a waypoint row chosen by stepping the menu.
bool Renderer::choose_waypoint(int row) {
    Impl& m = *impl_;
    if (!m.waypoint_screen.visible()) return false;
    tl::core::InputState down;
    down.move_y = -1.0f;
    for (int i = 0; i < row; ++i) {
        m.waypoint_stick_held = false;
        m.drive_waypoint_menu(down, false, false);
    }
    m.drive_waypoint_menu(tl::core::InputState{}, true, false);
    return m.dungeon_transition_pending;
}

bool Renderer::place_player_at_waypoint() {
    Impl& m = *impl_;
    // `CLevel::loadRoomLayout`, cases 14 and
    // 15 with entry type 6.
    if (!m.has_waypoint_entry_spot) {
        TL_LOG_INFO("waypoint: no Waypoint node on this level; the player stays at its start");
        return false;
    }
    TL_LOG_INFO("waypoint: the player starts at (%.2f, %.2f, %.2f)",
                static_cast<double>(m.waypoint_entry_spot.x),
                static_cast<double>(m.waypoint_entry_spot.y),
                static_cast<double>(m.waypoint_entry_spot.z));
    return place_player_at(m.waypoint_entry_spot.x, m.waypoint_entry_spot.z);
}

void Renderer::create_portals() {
    Impl& m = *impl_;
    m.delete_open_portals();
    m.create_portals();
}

bool Renderer::place_player_at_town_portal() {
    Impl& m = *impl_;
    // `CLevel::loadRoomLayout`, case 7 with
    // entry type 2.
    if (!m.has_town_portal_spot) return false;
    return place_player_at(m.town_portal_spot.x, m.town_portal_spot.z);
}

void Renderer::arrive_back_at_portal() {
    Impl& m = *impl_;
    if (m.config.travel == nullptr) return;
    // `performWarp`: `deleteOpenPortals`, the placement, then
    // `dropToGround(4.0)` (`place_player_at`'s ground ray).
    m.delete_open_portals();
    const tl::core::Vec3 at =
        tl::core::arrive_back_at_portal(*m.config.travel, [&m](const tl::core::Vec3& portal) {
            return tl::core::random_open_position(
                portal, 0.0f, 1.0f,
                [&m](const tl::core::Vec3& p) {
                    return !m.passability.ready() ||
                           (m.passability.map_passable(p) && m.passability.position_passable(p));
                },
                tl::core::volatile_random());
        });
    place_player_at(at.x, at.z);
    TL_LOG_INFO("portal: back on %s depth %d; the portal is closed", m.level_dungeon.c_str(),
                m.level_depth);
}

// PORT: a headless verification step: a carried item used by name.
bool Renderer::use_bag_item(const std::string& item_name) {
    Impl& m = *impl_;
    const std::string wanted = tl::core::upper_ascii(item_name);
    const std::vector<tl::core::InventorySlot>& slots = m.player_inventory.slots();
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].count <= 0 || tl::core::upper_ascii(slots[i].item.name) != wanted) continue;
        const bool used = m.use_player_item(i, /*from_tray=*/true);
        m.update_menu();
        m.refresh_game_hud();
        return used;
    }
    TL_LOG_INFO("use: the player carries no '%s'", item_name.c_str());
    return false;
}

void Renderer::arrive_from_temporary_portal(const std::string& left_dungeon) {
    Impl& m = *impl_;
    // performWarp: arriving from a dungeon,
    // the player stands at the level's portal to it (randomOpenPosition(portal, 1.0),
    // dropToGround(4.0)); then every portal to it goes, with that dungeon's history.
    const std::string left = tl::core::upper_ascii(left_dungeon);
    if (left.empty()) return;
    std::vector<tl::core::TemporaryPortal> items = m.temporary_portals();
    const tl::core::Vec3 portal = tl::core::find_temporary_dungeon_portal(items, left);
    if (portal.y != tl::core::kNoTemporaryPortal) {
        const tl::core::Vec3 at = tl::core::random_open_position(
            portal, 0.0f, 1.0f,
            [&m](const tl::core::Vec3& p) {
                return !m.passability.ready() ||
                       (m.passability.map_passable(p) && m.passability.position_passable(p));
            },
            tl::core::volatile_random());
        place_player_at(at.x, at.z);
    }
    const size_t marked = tl::core::delete_temporary_dungeon_portals(items, left, m.config.travel);
    if (marked == 0) return;
    // The level deletes a marked item on its next update; here at once.
    std::vector<Impl::OpenPortal> kept;
    for (size_t i = 0; i < m.open_portals.size(); ++i) {
        if (items[i].marked) {
            m.disable_portal(m.open_portals[i]);
            continue;
        }
        kept.push_back(std::move(m.open_portals[i]));
    }
    m.open_portals = std::move(kept);
    m.portal_targeted = -1;
    TL_LOG_INFO("portal: back from %s; %zu map portal(s) closed and its levels forgotten",
                left.c_str(), marked);
}

bool Renderer::walk_into_stairs(bool up) {
    Impl& m = *impl_;
    if (!m.dungeon_active) {
        TL_LOG_INFO("--portal %s: not on a generated floor", up ? "up" : "down");
        return false;
    }
    const Ogre::Vector3 at = to_ogre(up ? m.dungeon_entrance_point : m.dungeon_exit_point);
    place_player_at(at.x, at.z);
    advance_scene(0.25f);
    press_a();
    advance_scene(0.25f);
    return m.dungeon_transition_pending;
}

bool Renderer::walk_into_portal() {
    Impl& m = *impl_;
    // The first prop that is still on; a map portal, opened after the level's
    // own, when there is one.
    const Impl::OpenPortal* chosen = nullptr;
    for (const Impl::OpenPortal& portal : m.open_portals) {
        if (!portal.enabled) continue;
        if (chosen == nullptr || (chosen->warp.empty() && !portal.warp.empty())) chosen = &portal;
    }
    if (chosen == nullptr) {
        TL_LOG_INFO("--portal enter: no portal on this level");
        return false;
    }
    const Ogre::Vector3 at = to_ogre(chosen->at);
    place_player_at(at.x, at.z);
    advance_scene(0.25f);
    return press_a() && m.dungeon_transition_pending;
}

}  // namespace tl::render
