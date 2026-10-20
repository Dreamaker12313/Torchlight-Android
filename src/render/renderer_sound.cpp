// The renderer's implementation: interaction, music, banners, sounds and voices, path blockers, unit index.
// Split from renderer.cpp; the shared
// declarations are in renderer_impl.h.
#include "core/unit_stats_reader.h"
#include "render/renderer_impl.h"

namespace tl::render {
// PORT: the drawn entity of a trigger's node (the port's scene bookkeeping).
Ogre::Entity* Renderer::Impl::trigger_entity(int64_t node, const std::string& unit,
                                             const NodeMap& nodes) const {
    const auto range = nodes.equal_range(node);
    const auto units = scene_units.equal_range(unit);
    for (auto it = units.first; it != units.second; ++it) {
        if (it->second.node != node || it->second.entity == nullptr) continue;
        const Ogre::SceneNode* where = it->second.entity->getParentSceneNode();
        for (auto n = range.first; n != range.second; ++n) {
            if (n->second == where) return it->second.entity;
        }
    }
    return nullptr;
}

int64_t Renderer::Impl::instance_key(int64_t node, uint32_t scene, int64_t hierarchy) {
    if (scene == 0 && hierarchy == 0) return node;
    uint64_t k = static_cast<uint64_t>(hierarchy) * 0x9e3779b97f4a7c15ull;
    k ^= (static_cast<uint64_t>(scene) + 1u) * 0xc2b2ae3d27d4eb4full;
    return static_cast<int64_t>(static_cast<uint64_t>(node) ^ k);
}

int64_t Renderer::Impl::instance_key(int64_t node, const NodeMap& nodes) const {
    for (const NestedScript& machine : nested_scripts) {
        if (&machine.nodes == &nodes) return instance_key(node, machine.scene, machine.hierarchy);
    }
    return node;
}

int64_t Renderer::Impl::instance_key(int64_t node, int machine) const {
    if (machine >= 0 && static_cast<size_t>(machine) < nested_scripts.size()) {
        const NestedScript& m = nested_scripts[static_cast<size_t>(machine)];
        return instance_key(node, m.scene, m.hierarchy);
    }
    return node;
}

// `CCharacter::attack` by the player on a
// mimic holding `MIMICIDLE`, without a ranged weapon: `reactToDamage(player,
// true)` and no blow (`step_mimic_attack` brings the player there).
void Renderer::Impl::disturb_targeted_mimic() {
    tl::core::LevelScript* owner = machine_script(mimic_targeted_machine);
    if (owner == nullptr) return;
    std::vector<tl::core::ScriptEvent> events;
    const bool woke = owner->attack_idle_mimic(mimic_targeted_node, quest_log.state(), events);
    TL_LOG_INFO("pad prop: the attack wakes the mimic %lld%s",
                static_cast<long long>(mimic_targeted_node), woke ? "" : " (not lying in wait)");
    apply_machine_events(mimic_targeted_machine, events);
}

// A on a targeted prop: CCharacter::getItemAI once in
// reach, in sight and faced. ENCHANTSHRINE (ISA 0x9d) puts the player in AI state 0x16;
// STATSHRINE (0x9e) and GEMSHRINE (0x9f) name 0x17 and 0x18; anything else is state 0 and
// CTriggerUnit::interact.
void Renderer::Impl::interact_targeted_trigger() {
    tl::core::LevelScript* owner = machine_script(trigger_targeted_machine);
    if (owner == nullptr) return;
    // A `FISHING HOLE` (ISA 0x7a) is not interacted with either: the
    // character's `startFishing` (vtable +0x338).
    for (const tl::core::LevelScript::UnitTriggerView& view : owner->unit_triggers()) {
        if (view.id != trigger_targeted_node || !is_fishing_hole(view.unit)) continue;
        start_fishing(Ogre::Vector3(view.at.x, view.at.y, view.at.z));
        return;
    }
    for (const tl::core::LevelScript::UnitTriggerView& view : owner->unit_triggers()) {
        if (view.id != trigger_targeted_node || !is_enchant_shrine(view.unit)) continue;
        open_enchant_shrine(trigger_targeted_machine, trigger_targeted_node);
        return;
    }
    std::vector<tl::core::ScriptEvent> events;
    owner->interact_unit_trigger(trigger_targeted_node, quest_log.state(), events);
    TL_LOG_INFO("pad prop: A on unit trigger %lld, %zu event(s)",
                static_cast<long long>(trigger_targeted_node), events.size());
    // A room's machine that had gone quiet has something to run again.
    if (trigger_targeted_machine >= 0) {
        nested_scripts[static_cast<size_t>(trigger_targeted_machine)].running = true;
    }
    apply_machine_events(trigger_targeted_machine, events);
}

/// The other half of `getItemAI`/`getItem`:
/// take whatever `update_item_selection` is pointing at. An item the pack has
/// no room for is left sitting rather than forced somewhere or lost.
void Renderer::Impl::pick_up_targeted_item() {
    if (item_targeted < 0 || static_cast<size_t>(item_targeted) >= dropped_items.size()) return;
    // `getItem`'s gold branch (ISA 0x22).
    if (dropped_items[static_cast<size_t>(item_targeted)].is_gold) {
        take_gold(static_cast<size_t>(item_targeted));
        return;
    }
    load_unit_types();
    DroppedWorldItem dropped = dropped_items[static_cast<size_t>(item_targeted)];
    // The item's quest (`+0x170` / `+0x178`), where a ground item holds it as
    // the tag it was placed with.
    if (dropped.item.quest_guid == -1 && !dropped.quest_tag.empty()) {
        dropped.item.quest_guid = quest_guid_of(dropped.quest_tag);
        dropped.item.quest_unit_id = dropped.quest_tag.objective;
    }
    // `CEquipment::addedToInventory` raises
    // the quest's event 0 from inside the pickup (`bag_item_added`), about the
    // item where it lay.
    bag_event_at = dropped.node->getPosition();
    bag_event_at_set = true;
    const int landed = player_inventory.pickup(dropped.item, dropped.count, unit_types);
    bag_event_at_set = false;
    if (landed < 0) {
        // `getItem`: no room anywhere, for a player.
        queue_tip(tl::core::ContextTip::InventoryFull);
        return;
    }

    TL_LOG_INFO("picked up %d x %s", dropped.count, dropped.item.display_name.c_str());
    // `getItem`: a player's tips for what it now carries.
    {
        const uint32_t type = unit_types.id_of(dropped.item.unittype);
        for (tl::core::ContextTip tip : tl::core::item_picked_up_tips(
                 unit_types.isa(type, tl::core::unit_type::kSocketable),
                 unit_types.isa(type, tl::core::unit_type::kSpell), dropped.item.identified)) {
            queue_tip(tip);
        }
    }
    // getItem: after a pickup, canEquip(item, false) and,
    // with a free allowed location and the requirements met,
    // equipEquipmentIntoFirstFreeLocation. One already in an equip location stays worn.
    if (landed >= static_cast<int>(tl::core::Inventory::kFixedSlotCount)) {
        const tl::core::ItemStats& item =
            player_inventory.slots()[static_cast<size_t>(landed)].item;
        if (player_inventory.can_equip(item, unit_types, &player_character, &player_effects) &&
            player_inventory.equip_into_first_free_location(static_cast<size_t>(landed), unit_types,
                                                            &player_character, &player_effects)) {
            TL_LOG_INFO("picked up %s and put it on", dropped.item.display_name.c_str());
            sync_equipped_weapon();
            refresh_skill_effects();
        }
    }
    // `CEquipment::playTakeSound`: slot 0x12 on the **item's own**
    // bank - its `TAKE_SOUND`, `StaffGet` for a staff - not the interface's.
    play_sound(dropped.item.take_sound, dropped.node->getPosition());
    // CLevel::removeItem(item, true) (inlined into getItem):
    // CBaseUnit::deactivateUnitInLevel and off the level's lists.
    remove_ground_item(dropped_items[static_cast<size_t>(item_targeted)]);
    dropped_items.erase(dropped_items.begin() + item_targeted);
    item_targeted = -1;
    // `CItem`'s unit state 3 reaches the spawner that released it.
    if (dropped.unit_id != 0) spawned_item_state(dropped.unit_id, 3);
    item_highlighted = nullptr;
    refresh_game_hud();
}

/// The `.ANIMATION` manifest beside one character mesh, read once; empty when none ships.
const tl::core::AnimationManifest& Renderer::Impl::manifest_for(const std::string& mesh_path) {
    const std::string dir = dir_of_asset(mesh_path);
    const auto found = manifests.find(dir);
    if (found != manifests.end()) return found->second;

    tl::core::AnimationManifest manifest;
    std::string stem = mesh_path;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem.resize(dot);

    const std::string path = archive_path_in_dir(stem + ".ANIMATION.adm", dir);
    if (!path.empty()) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (!bytes.empty()) {
            const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (file.ok) manifest = tl::core::AnimationManifest::from(file);
        }
    }
    if (manifest.empty()) {
        TL_LOG_INFO("  no animation manifest beside '%s'", mesh_path.c_str());
    } else {
        TL_LOG_INFO("  animation manifest for '%s': %zu tagged animation(s)", mesh_path.c_str(),
                    manifest.size());
    }
    return manifests.emplace(dir, std::move(manifest)).first->second;
}

// What a `Music` node's CMusicObject (core/music_object.h) reaches: CSoundManager's music
// channel and the level's `MUSIC`.
tl::core::MusicHost Renderer::Impl::music_host(int64_t node) {
    tl::core::MusicHost host;
    // `getMusicFilePlaying`: the track on the channel, as `playMusic` names it.
    host.playing = [this]() {
        const bool silent = !audio_enabled || !tl::core::audio_available();
        if (music_track.empty()) return std::string();
        if (!silent && !tl::core::audio_is_channel_playing(tl::core::Channel::Music)) {
            return std::string();
        }
        return tl::core::music_track_key(music_track);
    };
    // playMusic(file, loops): stopMusic, then the track and its length (getSoundLength).
    // INVENTED: an already-reduced name is looked for under ../music/.
    host.play = [this](const std::string& file, bool loops) {
        std::string relative = file;
        if (relative.compare(0, 3, "../") != 0 && relative.find('/') == std::string::npos) {
            relative = "../music/" + relative;
        }
        play_music(std::string(), false, "");
        if (!play_music(relative, loops, "a Music node plays ") && music_track != relative) {
            return 0.0f;
        }
        if (loops) return 0.0f;
        std::string tail = relative;
        while (tail.compare(0, 3, "../") == 0) tail.erase(0, 3);
        if (config.assets != nullptr) {
            const std::string real = config.assets->resolve(tail);
            if (!real.empty()) tail = real;
        }
        std::string full = config.game_data;
        if (!full.empty() && full.back() != '/' && full.back() != '\\') full += '/';
        const std::vector<uint8_t> bytes = read_file(full + tail);
        return bytes.empty() ? 0.0f : tl::formats::ogg_duration(bytes.data(), bytes.size());
    };
    host.stop = [this]() { play_music(std::string(), false, ""); };
    host.level_music = [this]() { return tl::core::music_track_key(level_music); };
    // The node's output, through the machine that holds it.
    host.output = [this, node](uint32_t output) {
        tl::core::LevelScript* owner = script_owning(node);
        if (owner == nullptr) return;
        std::vector<tl::core::ScriptEvent> events;
        owner->raise_output(node, output, quest_log.state(), events);
        if (!events.empty()) apply_script_events(events, scripted_nodes);
    };
    return host;
}

void Renderer::Impl::music_node_input(const tl::core::ScriptEvent& event) {
    // `CMusicObjectDescriptor::CreateObject` and the descriptor's
    // `setMusicFile` / `setLoops`, on the first input the node takes.
    auto made = music_objects.find(event.node_id);
    if (made == music_objects.end()) {
        tl::core::MusicObject music;
        music.set_file(event.text);
        music.loops = event.loops;
        made = music_objects.emplace(event.node_id, std::move(music)).first;
    }
    tl::core::MusicObject& music = made->second;
    const tl::core::MusicHost host = music_host(event.node_id);
    if (event.input == tl::core::logic::kStop) {
        tl::core::music_stop(music, host);
    } else if (event.input == tl::core::logic::kPlayLevelMusic) {
        tl::core::music_play_level(music, host);
    } else {
        tl::core::music_play(music, host);
    }
}

void Renderer::Impl::step_music_objects(float delta) {
    for (auto& [node, music] : music_objects) {
        tl::core::music_update(music, music_host(node), delta);
    }
}

bool Renderer::Impl::play_music(const std::string& relative, bool loop, const char* because) {
    // A run with no audio device still resolves the file (not decode it), so headless runs
    // check the music path.
    const bool silent = !audio_enabled || !tl::core::audio_available();

    if (relative == music_track) return true;
    if (relative.empty()) {
        if (!silent) tl::core::audio_stop(tl::core::Channel::Music);
        music_track.clear();
        return true;
    }

    // `../music/town.ogg` is relative to `media/`, so the leading `../` steps
    // out of the archive and into the install directory.
    std::string tail = relative;
    while (tail.compare(0, 3, "../") == 0) tail.erase(0, 3);

    // Through the case-insensitive index: the data's music names are cased unlike the files.
    if (config.assets != nullptr) {
        const std::string real = config.assets->resolve(tail);
        if (!real.empty()) tail = real;
    }

    std::string full = config.game_data;
    if (!full.empty() && full.back() != '/' && full.back() != '\\') full += '/';
    full += tail;

    if (silent) {
        // A track present counts as playing (getMusicFilePlaying), so a Music node runs its
        // course on a silent run too.
        if (is_file(full)) {
            TL_LOG_INFO("music (no audio device, not played): %s", full.c_str());
            music_track = relative;
        } else {
            TL_LOG_WARN("no music at '%s' (%s%s)", full.c_str(), because, relative.c_str());
        }
        return false;
    }

    std::vector<uint8_t> bytes = read_file(full);
    if (bytes.empty()) {
        // Not an error worth stopping for. The music sits outside the archive,
        // and on the console it is only there if it was deployed - see
        // the deploy step's music option.
        TL_LOG_WARN("no music at '%s' (%s%s)", full.c_str(), because, relative.c_str());
        return false;
    }

    TL_LOG_INFO("music: %s (%zu bytes, %s)", full.c_str(), bytes.size(), loop ? "looping" : "once");
    if (!tl::core::audio_play(tl::core::Channel::Music, std::move(bytes), relative, loop, 1.0f)) {
        return false;
    }
    music_track = relative;
    return true;
}

void Renderer::Impl::set_big_text(const std::string& text, float seconds) {
    // `CGameUI::setBigText`: the text, the
    // hold, and alpha 1.0.
    big_banner.text = text;
    big_banner.left = seconds;
    big_banner.alpha = 1.0f;
}

void Renderer::Impl::set_small_text(const std::string& text, float seconds) {
    // `CGameUI::setSmallText`, the same three
    // at `+0x1990`.
    small_banner.text = text;
    small_banner.left = seconds;
    small_banner.alpha = 1.0f;
}

void Renderer::Impl::raise_arrival_banner() {
    // CGameClient::loadLevel: with a template and entry type other than
    // 4, setBigText(LEVELNAME, 1.0) then setSmallText("Floor N" or "", 1.0).
    // DIVERGENCE: entry type 4 is not told apart; every arrival raises it.
    if (!level_has_rules) return;
    constexpr float kArrivalSeconds = 1.0f;
    set_big_text(level_name_authored, kArrivalSeconds);
    set_small_text(tl::core::level_floor_caption(level_shows_floor, level_depth), kArrivalSeconds);
    TL_LOG_INFO("level banner: '%s' / '%s', level name '%s'", big_banner.text.c_str(),
                small_banner.text.c_str(), level_display_name.c_str());
}

std::string Renderer::Impl::preloaded_particle_file(const std::string& name) {
    if (!preloaded_particles_read) {
        preloaded_particles_read = true;
        // CParticlePreloader::CParticlePreloader(CResourceSettings*):
        // the list is the `Preload Particle` setting, else preload.dat.
        const std::string path = resolve_archive_path("media/particles/preload.dat.adm");
        const std::vector<uint8_t> bytes =
            path.empty() ? std::vector<uint8_t>{} : read_resource(path);
        if (!bytes.empty()) {
            const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
            for (const tl::formats::AdmSection& particle : file.root.children) {
                if (particle.name != "PARTICLE") continue;
                const std::string key = tl::core::upper_ascii(particle.string_or("NAME", ""));
                // The first of a name is kept, as the preloader's own lookup finds it.
                if (!key.empty()) preloaded_particles.emplace(key, particle.string_or("FILE", ""));
            }
        }
        TL_LOG_INFO("particles: %zu named in media/particles/preload.dat",
                    preloaded_particles.size());
    }
    const auto found = preloaded_particles.find(tl::core::upper_ascii(name));
    return found == preloaded_particles.end() ? std::string() : found->second;
}

void Renderer::Impl::clear_level_up_particle() {
    destroy_particle_holders(level_up_particle_nodes);
}

void Renderer::Impl::player_levelled_up() {
    // `CCharacter::levelUp` has already run,
    // in `award_experience`.
    character_level = player_character.level();
    // health and mana set to their new maxima.
    refresh_player_maxima();
    player_health.current = player_health.max;
    player_health.dead = false;

    // the particle, made once by `unitInit` and started
    // again on every level-up. This engine places it afresh instead, which
    // leaves the same one running: the previous one is taken down first.
    clear_level_up_particle();
    const std::string file = preloaded_particle_file("PLAYER_LEVELUP");
    if (!file.empty() && subject_node != nullptr) {
        level_up_particle_nodes = place_particle_layout(file, Ogre::Vector3::ZERO,
                                                        Ogre::Quaternion::IDENTITY, subject_node);
        TL_LOG_INFO("level up: %s on the player (%zu holder(s))", file.c_str(),
                    level_up_particle_nodes.size());
    }

    // CPlayer::levelUp plays the chime (slot 25 bound by CPlayer::unitInit)
    // and the narrator, banners between. The chime
    // only while the bank is not playing it (CSoundBank::bankPlaying(25)):
    // two levels at once ring once.
    if (!tl::core::audio_voice_playing(
            lowered(tl::core::interface_sound_name(tl::core::InterfaceSound::LevelUp)))) {
        play_ui(tl::core::InterfaceSound::LevelUp);
    }
    set_big_text(tl::core::level_up_banner(), tl::core::kLevelUpBannerSeconds);
    set_small_text(tl::core::level_up_caption(player_character.level()),
                   tl::core::kLevelUpBannerSeconds);
    play_voice(tl::core::SoundSlot::VoiceLevelUp);
    TL_LOG_INFO("level up: '%s' / '%s'", big_banner.text.c_str(), small_banner.text.c_str());
    // `CInventory::refreshEquipped()` before
    // the tip: every listener's `equipmentEquipped(nullptr)` - the character's
    // returns at once, the menus redraw.
    update_menu();
    //
    queue_tip(tl::core::ContextTip::LevelUp);
    // CInventory::forceRecalculationOfEquipmentStats() (inline in levelUp
    // on the console, after the new maxima).
    player_inventory.calculate_effect_values();
    refresh_game_hud();
}

void Renderer::Impl::refresh_game_hud() {
    if (!game_hud.ready()) return;

    tl::render::HudState state;
    state.health =
        player_health.max > 0.0f ? std::max(0.0f, player_health.current) / player_health.max : 1.0f;
    state.mana = player_character.max_mp() > 0.0f
                     ? std::max(0.0f, player_character.mp()) / player_character.max_mp()
                     : 1.0f;
    state.experience = player_character.experience_progress(stat_graphs);
    // `CLevel::getName`, written into the
    // `LevelName` window every frame by `CGameUI::updateIngameUI`.
    state.level_name = level_display_name;
    if (big_banner.alpha > 0.0f) {
        state.announce = big_banner.text;
        state.announce_alpha = big_banner.alpha;
    }
    if (small_banner.alpha > 0.0f) {
        state.sub_announce = small_banner.text;
        state.sub_announce_alpha = small_banner.alpha;
    }
    state.available_points = available_points.plus;
    state.available_points_glow = available_points.glow;

    // The panel belongs on screen while there is an animal in the level. A
    // character chooses a dog or a cat when it is created and never gives it
    // up, so the only thing that takes the panel away is a trip to town.
    state.pet = pet.alive() && pet.in_world();
    state.pet_health = pet.health_fraction();
    state.pet_fleeing = pet.state() == tl::core::PetState::Fleeing;
    state.pet_stance = pet.stance() == tl::core::PetStance::Aggressive  ? 0
                       : pet.stance() == tl::core::PetStance::Defensive ? 1
                                                                        : 2;

    // `CGameUI::updateSlots`: a slot holding
    // a skill the manager knows, with an icon, shows the skill; any other shows
    // its item half.
    state.skill_set = player_skills.skill_set();
    for (int slot = 0; slot < tl::core::kSkillFunctionSlots; ++slot) {
        const size_t i = static_cast<size_t>(slot);
        const tl::core::KnownSkill* skill = player_skills.find_by_guid(player_skills.slots()[i]);
        if (skill != nullptr && skill->def != nullptr && !skill->def->icon.empty()) {
            state.slots[i] = tl::core::skill_slot_view(
                skill->def->icon, player_skill_runner.cooling_time(skill->entry.name));
            continue;
        }
        const int64_t guid = player_skills.second_slots()[i];
        if (guid == -1) continue;
        const tl::core::ItemStats* item = items.find_by_guid(guid);
        state.slots[i] =
            tl::core::item_slot_view(item != nullptr ? item->icon : std::string(),
                                     tl::core::equipment_count_of_guid(player_inventory, guid));
    }

    // The pad's target in hud_xenon.layout's TargetHealth panel.
    if (!pad_selected.empty()) {
        state.target = true;
        const tl::core::UnitStats* stats = unit_book.find(pad_selected);
        state.target_name = stats != nullptr && !stats->display_name.empty()
                                ? tl::core::strip_markup(stats->display_name)
                                : pad_selected;
        // The character's own name (`+0x4c0`) where it was given one.
        const auto named = unit_names.find(pad_selected_node);
        if (named != unit_names.end()) state.target_name = tl::core::strip_markup(named->second);
        state.target_champion =
            (stats != nullptr && stats->is_champion) || spawn_marked_champion(pad_selected_node);
        state.target_level = effective_monster_level(pad_selected_node);
        if (stats != nullptr) state.target_description = stats->description;
        // A machine's character keeps its hit points in the machine
        // (LevelScript::unit_health); anything else in unit_health here.
        float current = 0.0f;
        float most = 0.0f;
        const tl::core::LevelScript* owner = script_owning(pad_selected_node);
        if (owner == nullptr || !owner->unit_health(pad_selected_node, current, most)) {
            const Health& health = health_of(pad_selected_node, pad_selected);
            current = health.current;
            most = health.max;
        }
        state.target_health = most > 0.0f ? std::max(0.0f, current) / most : 1.0f;
    }

    game_hud.set_state(state);
}

void Renderer::Impl::start_level_audio(const std::string& layout_path) {
    // No early return for a silent run: play_music() still resolves the track.

    // --- Music, from the rules file beside the layout (rules_path_for()). ---------------
    const std::string rules_path = rules_path_for(layout_path);

    std::string music;
    std::string level_name;
    // A level with no template has an empty name (`getName`'s first branch).
    level_has_rules = false;
    level_name_authored.clear();
    level_shows_floor = true;
    level_display_name.clear();
    if (!rules_path.empty()) {
        const std::vector<uint8_t> bytes = read_resource(rules_path);
        if (!bytes.empty()) {
            const tl::formats::AdmFile rules = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (rules.ok) {
                music = rules.string_or("MUSIC", "");
                level_name = rules.string_or("NAME", "");
                // The HUD's level name: CLevel::getName
                // over LEVELNAME (+0x770, default Dungeon) and SHOWFLOORNUMBER (+0x76c).
                level_has_rules = true;
                level_name_authored =
                    tl::core::strip_markup(rules.string_or("LEVELNAME", "Dungeon"));
                level_shows_floor = rules.bool_or("SHOWFLOORNUMBER", true);
                level_display_name = tl::core::level_display_name(level_name_authored,
                                                                  level_shows_floor, level_depth);
            }
        }
    }

    // A main-menu level's music is in the engine: CGameClient::loadMenuLevel calls
    // CSoundManager::stopMusic() then plays `../music/Title.ogg`, looping.
    // Recognised by the rules' NAME (MainMenu*) or a layout under MAINMENUS/.
    const bool menu_level =
        level_name.compare(0, 8, "MainMenu") == 0 ||
        tl::core::upper_ascii(layout_path).find("/MAINMENUS/") != std::string::npos;
    if (menu_level) {
        // `CGameClient::loadMenuLevel` raises
        // no banner, and no HUD is drawn over a backdrop.
        level_has_rules = false;
        level_display_name.clear();
    }
    if (music.empty() && menu_level) {
        music = kMenuMusic;
        TL_LOG_INFO("main-menu level '%s': music is the engine's own '%s'", level_name.c_str(),
                    music.c_str());
    }

    // Remembered whole, so `Play Level Music` on a `Music` node has something
    // to go back to after a level has swapped in a fight track.
    level_music = music;
    music_objects.clear();
    // The rules carry no loop flag because a level's theme always does; a
    // `Music` node's own `LOOPS` is for the scripted tracks, which are a
    // different thing.
    play_music(music, /*loop=*/true, "the level authors MUSIC:");
    // The level's `Sound` nodes are its machines' (`core/sound_object.h`),
    // started by their scene's activation and played through
    // `node_sound_host()`.
}

namespace {

std::string node_channel_key(size_t index) { return "node-channel/" + std::to_string(index); }

}  // namespace

// The mixer behind a `Sound` node's bank. The rules - when a channel starts,
// at what volume, when it stops - are the console's, in `core/sound_object`;
// these are its `CSoundManager` calls.
tl::core::SoundBankHost Renderer::Impl::node_sound_host() {
    tl::core::SoundBankHost host;
    // CSoundManager::playSound(file, volume, node):
    // nothing at no volume; else + randomBetweenVolatile(0, VOLUMEVARIATION), held to 1.
    host.play = [this](const std::string& file, float volume, float variation,
                       const tl::core::Vec3* at, bool loop) -> int32_t {
        if (!(volume > 0.0f)) return -1;
        const float gain =
            std::min(1.0f, volume + tl::core::volatile_random().between(0.0f, variation));
        size_t index = 0;
        for (; index < node_channels.size(); ++index) {
            if (!node_channel_sounding(index)) break;
        }
        if (index == node_channels.size()) return -1;
        NodeChannel& channel = node_channels[index];
        const std::string key = node_channel_key(index);
        tl::core::audio_stop_voice(key);
        channel = NodeChannel{};
        channel.positional = at != nullptr;
        if (at != nullptr) channel.at = Ogre::Vector3(at->x, at->y, at->z);
        channel.gain = gain;
        channel.pan = channel.positional ? node_channel_pan(channel.at) : 0.0f;
        if (!audio_enabled || !tl::core::audio_available()) {
            // A headless run resolves the file once and says so, as
            // `play_sound()` does: the only check a silent run can make.
            if (reported_sounds.insert(lowered(file)).second) {
                const std::string resolved = resolve_archive_path(file);
                if (resolved.empty()) {
                    TL_LOG_WARN("sound node (not played): '%s' is not in the archive",
                                file.c_str());
                    return -1;
                }
                TL_LOG_INFO("sound node (no audio device, not played): %s, volume %.2f%s",
                            resolved.c_str(), static_cast<double>(gain), loop ? ", looping" : "");
            }
            // DIVERGENCE (tooling): with no device the channel is held without a voice, so
            // the banks draw their volatile numbers as with sound.
            channel.used = true;
            channel.silent = true;
            channel.silent_loop = loop;
            return static_cast<int32_t>(index);
        }
        tl::core::SamplePtr sample = sample_for(file);
        if (!sample) return -1;
        if (!tl::core::audio_play_voice(std::move(sample), key, gain, channel.pan, loop)) return -1;
        channel.used = true;
        if (reported_sounds.insert(lowered(file)).second) {
            TL_LOG_INFO("sound node: %s on channel %zu (gain %.2f%s)", file.c_str(), index,
                        static_cast<double>(gain), loop ? ", looping" : "");
        }
        return static_cast<int32_t>(index);
    };
    // `CSoundManager::isChannelPlaying`.
    host.playing = [this](int32_t id) {
        if (id < 0 || static_cast<size_t>(id) >= node_channels.size()) return false;
        return node_channel_sounding(static_cast<size_t>(id));
    };
    // `CSoundManager::stopSound`.
    host.stop = [this](int32_t id) {
        if (id < 0 || static_cast<size_t>(id) >= node_channels.size()) return;
        tl::core::audio_stop_voice(node_channel_key(static_cast<size_t>(id)));
        node_channels[static_cast<size_t>(id)].used = false;
    };
    host.set_volume = [this](int32_t id, float volume) {
        if (id < 0 || static_cast<size_t>(id) >= node_channels.size()) return;
        NodeChannel& channel = node_channels[static_cast<size_t>(id)];
        if (!channel.used) return;
        channel.gain = volume;
        tl::core::audio_set_voice_mix(node_channel_key(static_cast<size_t>(id)), channel.gain,
                                      channel.pan);
    };
    host.set_paused = [this](int32_t id, bool paused) {
        if (id < 0 || static_cast<size_t>(id) >= node_channels.size()) return;
        if (!node_channels[static_cast<size_t>(id)].used) return;
        tl::core::audio_set_voice_paused(node_channel_key(static_cast<size_t>(id)), paused);
    };
    return host;
}

bool Renderer::Impl::node_channel_sounding(size_t index) const {
    const NodeChannel& channel = node_channels[index];
    if (!channel.used) return false;
    if (channel.silent) return channel.silent_loop;
    return tl::core::audio_voice_playing(node_channel_key(index));
}

// Which ear: where the node sits across the camera's right from the listener,
// the port's stand-in for FMOD's 3D panning (the mixer is the port's own).
float Renderer::Impl::node_channel_pan(const Ogre::Vector3& at) const {
    if (camera == nullptr) return 0.0f;
    const Ogre::Vector3 listener = subject_node != nullptr && player_control
                                       ? subject_node->getPosition()
                                       : camera->getDerivedPosition();
    Ogre::Vector3 towards = at - listener;
    const float away = towards.normalise();
    if (away <= 0.0f) return 0.0f;
    return std::max(-1.0f, std::min(1.0f, camera->getDerivedRight().dotProduct(towards)));
}

void Renderer::Impl::step_node_channels(float /*delta*/) {
    for (size_t index = 0; index < node_channels.size(); ++index) {
        NodeChannel& channel = node_channels[index];
        if (!channel.used || !channel.positional) continue;
        const float pan = node_channel_pan(channel.at);
        if (std::fabs(pan - channel.pan) < 1e-3f) continue;
        channel.pan = pan;
        tl::core::audio_set_voice_mix(node_channel_key(index), channel.gain, channel.pan);
    }
}

void Renderer::Impl::ensure_sound_index() {
    if (sound_files_loaded) return;
    sound_files_loaded = true;

    std::vector<std::string> names = archive_paths_under("media/sounds/", ".adm");
    // Then the console's own UI.DAT, after the install's (a name both define keeps the
    // install's): it adds Navigate and the rest only the console has.
    for (const std::string& path : archive_paths_under("xbox/media/sounds/", ".adm")) {
        names.push_back(path);
    }
    // `CSoundBankDataInformation::reload`
    // and `parseSoundData`: each listed
    // file's groups, in order.
    for (const std::string& path : names) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) continue;
        const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (!file.ok) continue;
        sound_info.parse_file(file.root);
    }
    TL_LOG_INFO("sounds: %zu definition(s) from %zu table(s)", sound_info.size(), names.size());
}

const Renderer::Impl::SoundDefinition* Renderer::Impl::sound_by_name(const std::string& name) {
    if (name.empty()) return nullptr;
    ensure_sound_index();
    // `getSoundDataObject(name)`; every name is stored upper-cased.
    return sound_info.find(tl::core::upper_ascii(name));
}

const Renderer::Impl::SoundDefinition* Renderer::Impl::sound_by_guid(const std::string& guid) {
    if (guid.empty()) return nullptr;
    ensure_sound_index();
    // `getSoundDataObjectByGuid`. The GUID is an `INTEGER64` in the sound
    // tables and a `STRING` in the layouts that reference it, so it arrives
    // here as its decimal spelling.
    char* end = nullptr;
    const long long value = std::strtoll(guid.c_str(), &end, 10);
    if (end == guid.c_str() || *end != 0) return nullptr;
    return sound_info.find_by_guid(static_cast<int64_t>(value));
}

/// How long a `SOUND` definition plays, in seconds (its first recording): how long the
/// line stays up and the scene waits.
float Renderer::Impl::sound_length(const std::string& name) {
    const SoundDefinition* definition = sound_by_name(name);
    if (definition == nullptr) return 0.0f;

    const std::string path = resolve_archive_path(definition->files.front());
    if (path.empty()) return 0.0f;
    const std::vector<uint8_t> bytes = read_resource(path);
    if (bytes.empty()) return 0.0f;
    return tl::formats::ogg_duration(bytes.data(), bytes.size());
}

tl::core::SamplePtr Renderer::Impl::sample_for(const std::string& archive_path) {
    const auto found = samples.find(archive_path);
    if (found != samples.end()) return found->second;

    tl::core::SamplePtr decoded;
    const std::string resolved = resolve_archive_path(archive_path);
    if (!resolved.empty()) {
        const std::vector<uint8_t> bytes = read_resource(resolved);
        if (!bytes.empty()) {
            // A quarter of the one-shots, every voice line among them, are Ogg: the
            // extension picks the decoder.
            if (lowered(resolved).size() > 4 &&
                lowered(resolved).compare(lowered(resolved).size() - 4, 4, ".ogg") == 0) {
                decoded = tl::core::audio_decode_ogg(bytes, archive_path);
            } else {
                const tl::formats::WavSound wav =
                    tl::formats::parse_wav(bytes.data(), bytes.size());
                if (wav.ok) {
                    auto sample = std::make_shared<tl::core::Sample>();
                    sample->samples = wav.samples;
                    sample->channels = wav.channels;
                    sample->rate = wav.sample_rate;
                    sample->frames = wav.frames;
                    decoded = std::move(sample);
                } else {
                    TL_LOG_WARN("sound: could not decode '%s': %s", archive_path.c_str(),
                                wav.error.c_str());
                }
            }
        }
    }
    // A null is cached too: a file the archive has not got must not be looked
    // for again every time a paw lands.
    samples.emplace(archive_path, decoded);
    return decoded;
}

/// Play one authored sound definition, positioned in the level; every number is the
/// definition's. INVENTED: the rolloff shape - linear to silence at RADIUS, panned across
/// the camera's right vector.
bool Renderer::Impl::play_sound(const std::string& name, const Ogre::Vector3& at) {
    return play_sound(name, at, 1.0f, 0.0f, std::string());
}

const std::string& Renderer::Impl::pick_recording(const SoundDefinition& definition) {
    // PORT: the sound system's choice of recording.
    if (definition.files.size() < 2) return definition.files.front();
    std::uniform_int_distribution<size_t> any(0, definition.files.size() - 1);
    return definition.files[any(recording_picker)];
}

bool Renderer::Impl::play_sound_drawn(const std::string& name, const std::string& recording,
                                      float variation, const Ogre::Vector3& at) {
    // PORT: the mix of a sample the machine drew (`play_unit_sample`).
    const SoundDefinition* definition = sound_by_name(name);
    if (definition == nullptr || definition->files.empty()) return false;
    if (!audio_enabled || !tl::core::audio_available()) return false;
    float gain = 0.0f;
    float pan = 0.0f;
    if (!positional_mix(definition->volume, definition->radius, at, gain, pan) || !(gain > 0.0f))
        return false;
    gain = std::min(1.0f, gain + variation);
    // The recording the bank's instance holds, as the machine drew it.
    tl::core::SamplePtr sample =
        sample_for(recording.empty() ? pick_recording(*definition) : recording);
    if (!sample) return false;
    return tl::core::audio_play_voice(std::move(sample), lowered(name), gain, pan);
}

bool Renderer::Impl::play_sound(const std::string& name, const Ogre::Vector3& at,
                                float volume_scale, float radius_if_none,
                                const std::string& voice_key, bool volume_given) {
    if (name.empty()) return false;
    const SoundDefinition* definition = sound_by_name(name);
    if (definition == nullptr) {
        // Worth one line: a tag naming a sound the tables have not got is a
        // misreading of the tag, not a missing file.
        if (reported_sounds.insert(lowered(name)).second) {
            TL_LOG_WARN("sound: nothing in media/sounds/ is called '%s'", name.c_str());
        }
        return false;
    }
    if (definition->files.empty()) return false;

    // CSoundBank::playSample: a chance rolls
    // randomIntegerBetweenVolatile(1, 100) and plays only above the roll (PLAYCHANCE:100 is
    // silent once in a hundred). Rolled with or without a device.
    tl::core::RunicRandom& rolls = tl::core::volatile_random();
    if (definition->play_chance >= 0 && definition->play_chance <= rolls.integer_between(1, 100)) {
        return false;
    }
    // Which of the bank slot's samples: `randomIntegerBetweenVolatile(0, count -
    // 1)` over the one a sound played by name stands in for. Which
    // recording of it sounds is the sound system's choice, not a draw.
    (void)rolls.integer_between(0, 0);
    const std::string& file = pick_recording(*definition);
    // `playSample`: a volume handed in replaces the definition's (`+0xac` x
    // the 3rd argument); none is the definition's (`+0x78`). `CSoundManager::playSound`:
    // nothing at no volume.
    const float volume = volume_given ? volume_scale : definition->volume * volume_scale;
    if (!(volume > 0.0f)) return false;

    // A run with no audio device still resolves the file, once, as play_music() does.
    if (!audio_enabled || !tl::core::audio_available()) {
        if (reported_sounds.insert(lowered(name)).second) {
            const std::string resolved = resolve_archive_path(definition->files.front());
            if (resolved.empty()) {
                TL_LOG_WARN("sound (not played): '%s' names '%s', which is not in the archive",
                            name.c_str(), definition->files.front().c_str());
            } else {
                TL_LOG_INFO(
                    "sound (no audio device, not played): %s -> %s (%zu recording(s), "
                    "volume %.2f, radius %.1f, play chance %d)",
                    name.c_str(), resolved.c_str(), definition->files.size(),
                    static_cast<double>(definition->volume),
                    static_cast<double>(definition->radius), definition->play_chance);
            }
        }
        return false;
    }
    // Started: `randomBetweenVolatile(0, VOLUMEVARIATION)` added
    // (once FMOD's `playSound` has succeeded), the sum held to 1.
    const float variation = rolls.between(0.0f, definition->volume_variation);

    float gain = 0.0f;
    float pan = 0.0f;
    // `CSoundBank::addSample`: the
    // definition's radius, or the bank's when it authors none.
    const float radius = definition->radius != 0.0f ? definition->radius : radius_if_none;
    // PORT: the falling off with distance, which the console's emitter does.
    const bool audible = positional_mix(volume, radius, at, gain, pan);
    if (!audible || !(gain > 0.0f)) return false;
    gain = std::min(1.0f, gain + variation);

    const std::string key = lowered(name);
    tl::core::SamplePtr sample = sample_for(file);
    if (!sample) return false;
    if (!voice_key.empty()) tl::core::audio_stop_voice(voice_key);
    const bool played = tl::core::audio_play_voice(std::move(sample),
                                                   voice_key.empty() ? key : voice_key, gain, pan);
    // Once per definition, so a log carries what a scene actually made a noise
    // about without carrying one line per footfall.
    if (played && reported_sounds.insert(key).second) {
        TL_LOG_INFO("sound: %s -> %s (%zu recording(s), gain %.2f, pan %+.2f)", name.c_str(),
                    file.c_str(), definition->files.size(), static_cast<double>(gain),
                    static_cast<double>(pan));
    }
    return played;
}

bool Renderer::Impl::positional_mix(float volume, float radius, const Ogre::Vector3& at,
                                    float& gain, float& pan) const {
    gain = volume;
    pan = 0.0f;
    if (radius > 0.0f) {
        // The listener is the character, not the camera: authored radii are a few units,
        // and the camera sits far further back.
        const Ogre::Vector3 listener =
            subject_node != nullptr && player_control
                ? subject_node->getPosition()
                : (camera != nullptr ? camera->getDerivedPosition() : Ogre::Vector3::ZERO);
        const float away = listener.distance(at);
        if (away >= radius) {
            gain = 0.0f;
            return false;
        }
        gain *= 1.0f - away / radius;
        // Which ear, though, is the camera's question: left and right are what
        // the viewer sees, not what the character faces.
        if (camera != nullptr) {
            const Ogre::Vector3 right = camera->getDerivedRight();
            pan = std::max(-1.0f, std::min(1.0f, right.dotProduct(at - listener) / radius));
        }
    }
    return gain > 0.0f;
}

bool Renderer::Impl::play_sound_2d(const std::string& name, const char* label) {
    if (name.empty()) return false;
    if (!audio_enabled || !tl::core::audio_available())
        return play_sound(name, Ogre::Vector3::ZERO);
    const SoundDefinition* definition = sound_by_name(name);
    if (definition == nullptr || definition->files.empty()) return false;
    const std::string key = lowered(name);
    // `CSoundBank::playSample` with no scene
    // node, then `CSoundManager::playSound`:
    // the same three rolls as `play_sound()`.
    tl::core::RunicRandom& rolls = tl::core::volatile_random();
    if (definition->play_chance >= 0 && definition->play_chance <= rolls.integer_between(1, 100)) {
        return false;
    }
    const std::string& file = definition->files[static_cast<size_t>(
        rolls.integer_between(0, static_cast<int>(definition->files.size()) - 1))];
    if (!(definition->volume > 0.0f)) return false;
    const float gain =
        std::min(1.0f, definition->volume + rolls.between(0.0f, definition->volume_variation));
    tl::core::SamplePtr sample = sample_for(file);
    if (!sample) return false;
    const size_t frames = sample->frames;
    const int rate = sample->rate;
    const bool played = tl::core::audio_play_voice(std::move(sample), key, gain, 0.0f);
    // Once per definition, like `play_sound()`. A line that does not come out is
    // otherwise invisible, and a voice-over is the one sound where silence looks
    // exactly like the game being quiet on purpose.
    if (played && reported_sounds.insert(key).second) {
        TL_LOG_INFO("%s: %s -> %s (%.2f s, gain %.2f)", label, name.c_str(), file.c_str(),
                    static_cast<double>(frames) / static_cast<double>(rate > 0 ? rate : 44100),
                    static_cast<double>(gain));
    }
    return played;
}

// `CQuestDialog::playDialogSound`: the
// dialog's `SOUND` in slot 0x25 of a bank of its own, played with no position.
// `CQuestDialogMenu::setNPC` stops the
// outgoing dialog's recording before it plays the new one.
void Renderer::Impl::play_dialog_sound(const std::string& name) {
    stop_dialog_sound();
    if (name.empty()) return;
    if (play_sound_2d(name, "dialog")) dialog_sound_key = lowered(name);
}

// `CQuestDialog::stopDialogSound` ->
// `CSoundBank::stop(0x25)`.
void Renderer::Impl::stop_dialog_sound() {
    if (dialog_sound_key.empty()) return;
    tl::core::audio_stop_voice(dialog_sound_key);
    dialog_sound_key.clear();
}

const char* Renderer::Impl::voice_or_none(tl::core::SoundSlot slot) const {
    const std::string& name = player_stats.sounds[slot];
    return name.empty() ? "-" : name.c_str();
}

bool Renderer::Impl::play_ui(tl::core::InterfaceSound sound) {
    const char* name = tl::core::interface_sound_name(sound);
    // Non-positional, like every other interface sound: `CGameUI` and the menus
    // pass a null scene node to `playSample`, so the listener's distance never
    // enters into it.
    return play_sound_2d(name, "ui");
}

bool Renderer::Impl::play_voice(tl::core::SoundSlot slot, float delay, float volume) {
    const std::string& name = player_stats.sounds[slot];
    if (name.empty()) {
        // Logged once: six voice slots are unauthored and VOICESPELLLEARNED_SOUND is
        // misspelled in the data (core/sound_bank.h).
        const int index = static_cast<int>(slot);
        if (reported_voices.insert(index).second) {
            TL_LOG_INFO("voice: the class authors no '%s'; nothing to say",
                        tl::core::kSoundSlotKeys[static_cast<size_t>(index)]);
        }
        return false;
    }
    return queue_global_sample(slot, name, volume, delay);
}

// CSoundBank::queueGlobalSample(slot, volume, delay). The
// gate on the sound system (manager +0x375/+0x374) is the mixer's: a silent run still
// queues.
bool Renderer::Impl::queue_global_sample(tl::core::SoundSlot slot, const std::string& name,
                                         float volume, float delay) {
    if (name.empty()) return false;
    const SoundDefinition* definition = sound_by_name(name);
    if (definition == nullptr) return false;
    tl::core::RunicRandom& rolls = tl::core::volatile_random();
    // The slot's chance (X360 +0x7c, Linux +0x90): none (-1) always plays;
    // else a 1..100 roll must come out below it.
    if (definition->play_chance >= 0 && definition->play_chance <= rolls.integer_between(1, 100)) {
        return false;
    }
    // A volume of 0 is the slot's own (X360 +0x6c, Linux +0x78); any other is
    // scaled by the bank's (X360 +0x90, Linux +0xac; 1).
    const float level = volume == 0.0f ? definition->volume : volume * 1.0f;
    // No samples, nothing; else one drawn on the volatile stream,
    // before `queueSound` decides whether to keep it.
    if (definition->files.empty()) return false;
    const std::string& file = definition->files[static_cast<size_t>(
        rolls.integer_between(0, static_cast<int>(definition->files.size()) - 1))];
    tl::core::VoiceQueue::Entry entry;
    entry.slot = static_cast<int>(slot);
    entry.sample = name;
    entry.file = file;
    entry.volume = level;
    entry.delay = delay;
    const bool playing = tl::core::audio_voice_playing(voice_queue.channel());
    if (!voice_queue.queue(std::move(entry), playing)) {
        TL_LOG_INFO("voice: %s not queued (%s)", name.c_str(),
                    voice_queue.playing_slot() == static_cast<int>(slot) && playing
                        ? "it is the one sounding"
                        : "already queued");
        return false;
    }
    return true;
}

// `CSoundManager::update(listener, position, delta)`,
// its queue half: one line at a time, each once
// the one before has finished and its own delay has run.
void Renderer::Impl::step_voices(float delta) {
    const bool playing = tl::core::audio_voice_playing(voice_queue.channel());
    tl::core::VoiceQueue::Entry entry;
    if (!voice_queue.update(delta, playing, entry)) return;
    // stopSound on the old channel, then playSound(sample, no node, volume),
    // volume variation capped at 1.
    if (!voice_queue.channel().empty()) tl::core::audio_stop_voice(voice_queue.channel());
    voice_queue.set_channel(std::string());
    const SoundDefinition* definition = sound_by_name(entry.sample);
    if (definition == nullptr || !(entry.volume > 0.0f)) return;
    tl::core::RunicRandom& rolls = tl::core::volatile_random();
    const float gain =
        std::min(1.0f, entry.volume + rolls.between(0.0f, definition->volume_variation));
    const std::string key = "voice:" + lowered(entry.sample);
    bool played = false;
    if (audio_enabled && tl::core::audio_available()) {
        tl::core::SamplePtr sample = sample_for(entry.file);
        if (sample) played = tl::core::audio_play_voice(std::move(sample), key, gain, 0.0f);
    }
    if (played) voice_queue.set_channel(key);
    TL_LOG_INFO("voice: %s -> %s (gain %.2f)%s", entry.sample.c_str(), entry.file.c_str(),
                static_cast<double>(gain), played ? "" : ", silent");
}

// CPropertyNode::updatePathNodes, once its cells are in:
// killOrMoveBaseUnits, machine by machine, over the
// node's own box (+0x120).
void Renderer::Impl::clear_path_blocker_area(const PathBlocker& b) {
    tl::core::LevelScript::OccupationArea area;
    area.centre = b.centre;
    area.min = b.low;
    area.max = b.high;
    area.circle = b.circle;
    area.scale_x = b.scale_x;
    // A copy of the list: the events can link a layout, which adds a machine.
    const std::vector<MachineView> views = machines();
    for (const MachineView& view : views) {
        std::vector<tl::core::ScriptEvent> events;
        std::vector<int64_t> killed;
        std::vector<std::pair<int64_t, std::string>> names;
        view.script->kill_or_move_base_units(area, quest_log.state(), events, killed);
        for (const int64_t id : killed) names.emplace_back(id, view.script->unit_of(id));
        if (view.nodes != nullptr && !events.empty()) apply_script_events(events, *view.nodes);
        for (const auto& dead : names) {
            TL_LOG_INFO("script: %s is caught in a pathing blocker and dies", dead.second.c_str());
            kill_unit(dead.first, dead.second, /*fall=*/false, tl::core::LevelScript::kNoAttacker);
        }
    }
}

// A Property Node as updatePathNodes sees it: TYPE 9 (Box) or 8 (Circle), its box from
// updateBoundingBox.
// RADIUS and BOX WIDTH set scale X, BOX LENGTH scale Z (else 1).
bool Renderer::Impl::path_blocker_of(const tl::core::LevelScript::NodeRecord& n,
                                     PathBlocker& b) const {
    const bool circle = n.kind == "Path Node Occupation Circle";
    if (!circle && n.kind != "Path Node Occupation Box") return false;
    b.circle = circle;
    b.centre = n.position;
    b.scale_x = n.box_width != 0.0f ? n.box_width : n.radius;
    const float scale_z = n.box_length != 0.0f ? n.box_length : 1.0f;
    const tl::core::NoSpawnRegion box =
        tl::core::property_node_bounds(n.position, n.right, n.up, n.forward, b.scale_x, scale_z);
    b.low = box.min;
    b.high = box.max;
    return true;
}

// The same for a node of the level's own file, laid before its script stands; world axes
// are the columns of node_orientation.
bool Renderer::Impl::path_blocker_of(const tl::formats::CmpNode& node, PathBlocker& b) const {
    if (node.type != tl::formats::cmp_type::kMarker) return false;
    namespace f = tl::formats::cmp_field::property_node;
    const tl::formats::CmpField* type = node.field(f::kType);
    const tl::formats::CmpField* where = node.field(f::kPosition);
    if (type == nullptr || where == nullptr || where->count() != 3) return false;
    const auto number = [&node](size_t index) {
        const tl::formats::CmpField* field = node.field(index);
        return field != nullptr && field->count() == 1 ? field->as_float() : 0.0f;
    };
    tl::core::LevelScript::NodeRecord n;
    n.node = node.id;
    n.kind = type->as_text();
    n.position = {where->as_float(0), where->as_float(1), where->as_float(2)};
    n.radius = number(f::kRadius);
    n.box_width = number(f::kBoxWidth);
    n.box_length = number(f::kBoxLength);
    const Ogre::Quaternion orientation = node_orientation(node);
    const Ogre::Vector3 x = orientation.xAxis();
    const Ogre::Vector3 y = orientation.yAxis();
    const Ogre::Vector3 z = orientation.zAxis();
    n.right = {x.x, x.y, x.z};
    n.up = {y.x, y.y, y.z};
    n.forward = {z.x, z.y, z.z};
    return path_blocker_of(n, b);
}

// CLevel::incrementMapPassability(AxisAlignedBox, float)
// on, decrementMapPassability off: radius 0 for a box,
// getScaleX() for a circle. The route's grid and NavGrid::blocked are refreshed.
size_t Renderer::Impl::set_path_blocker_cells(const PathBlocker& b, bool on) {
    // `+0x50`/`+0x54` (the grid's size): no grid, no cells.
    if (!passability.ready()) return 0;
    const float radius = b.circle ? b.scale_x : 0.0f;
    if (on) {
        passability.increment_map_passability(b.low, b.high, radius);
    } else {
        passability.decrement_map_passability(b.low, b.high, radius);
    }
    // The record first, while the route's copy still has the cells as they were.
    const size_t changed = nav.mark_blocker(b.low, b.high, on);
    nav.refresh(passability, b.low, b.high);
    return changed;
}

void Renderer::Impl::apply_path_blocker(int64_t node_id, bool on, const NodeMap* nodes, bool live) {
    if (!nav.ready()) return;
    const tl::formats::CmpNode* node = script_layout.node_by_id(node_id);
    if (node == nullptr) {
        // A linked layout's, or a placed room's.
        const auto found =
            machine_path_blockers.find(nodes != nullptr ? instance_key(node_id, *nodes) : node_id);
        if (found == machine_path_blockers.end()) return;
        MachinePathBlocker& b = found->second;
        // `+0x150`: a second `Enable` changes nothing.
        if (b.applied == on) return;
        b.applied = on;
        const size_t changed = set_path_blocker_cells(b.box, on);
        TL_LOG_INFO("script: pathing blocker %lld %s, %zu cell(s) changed",
                    static_cast<long long>(node_id), on ? "on" : "off", changed);
        if (on && live) clear_path_blocker_area(b.box);
        return;
    }
    PathBlocker b;
    if (!path_blocker_of(*node, b)) return;
    // `+0x150`, as for a linked layout's: a timeline's `ENABLED` key sets the
    // flag every frame it plays, and only a change moves cells.
    const bool applied = level_path_blockers_on.count(node_id) != 0;
    if (applied == on) return;
    if (on) {
        level_path_blockers_on.insert(node_id);
    } else {
        level_path_blockers_on.erase(node_id);
    }
    const size_t changed = set_path_blocker_cells(b, on);
    TL_LOG_INFO("script: pathing blocker '%s' %s, %zu cell(s) changed", node->name.c_str(),
                on ? "on" : "off", changed);
    if (on && live) clear_path_blocker_area(b);
}

// updatePathNodes for a linked layout's nodes, placed by their machine's frame
// (NodeRecord::right and the rest are world).
void Renderer::Impl::apply_machine_path_blockers() {
    machine_path_blockers.clear();
    if (!nav.ready()) return;
    size_t blockers = 0;
    for (const MachineView& view : machines()) {
        if (view.script == &script) continue;
        for (const tl::core::LevelScript::NodeRecord& n :
             view.script->nodes_of_type(tl::formats::cmp_type::kMarker)) {
            MachinePathBlocker b;
            if (!path_blocker_of(n, b.box)) continue;
            b.applied = n.enabled;
            machine_path_blockers[instance_key(n.node, view.scene, view.hierarchy)] = b;
            TL_LOG_INFO("  pathing: blocker %lld, box (%.2f, %.2f)-(%.2f, %.2f)%s",
                        static_cast<long long>(n.node), static_cast<double>(b.box.low.x),
                        static_cast<double>(b.box.low.z), static_cast<double>(b.box.high.x),
                        static_cast<double>(b.box.high.z), n.enabled ? "" : " (disabled)");
            if (!n.enabled) continue;
            set_path_blocker_cells(b.box, true);
            ++blockers;
        }
    }
    if (blockers > 0) {
        TL_LOG_INFO(
            "  pathing: %zu blocker(s) of linked layouts on, %zu of %zu walkable cell(s) covered",
            blockers, nav.blocked_count(), nav.walkable_count());
    }
}

// Every blocker that is on comes off the map, its record with it: a level reset
// lays them again from the script (`apply_path_blockers`).
void Renderer::Impl::lift_path_blockers() {
    for (const int64_t id : level_path_blockers_on) {
        const tl::formats::CmpNode* node = script_layout.node_by_id(id);
        PathBlocker b;
        if (node != nullptr && path_blocker_of(*node, b)) set_path_blocker_cells(b, false);
    }
    level_path_blockers_on.clear();
    for (auto& entry : machine_path_blockers) {
        if (!entry.second.applied) continue;
        set_path_blocker_cells(entry.second.box, false);
        entry.second.applied = false;
    }
    nav.clear_blockers();
}

void Renderer::Impl::apply_path_blockers(const tl::formats::CmpScene& layout,
                                         const std::set<int64_t>& gated) {
    if (!nav.ready()) return;
    // Whatever is still on comes off first, so a cell is never counted twice.
    for (const int64_t id : level_path_blockers_on) {
        const tl::formats::CmpNode* node = layout.node_by_id(id);
        PathBlocker b;
        if (node != nullptr && path_blocker_of(*node, b)) set_path_blocker_cells(b, false);
    }
    level_path_blockers_on.clear();
    namespace f = tl::formats::cmp_field::property_node;
    size_t blockers = 0;
    for (const tl::formats::CmpNode& node : layout.nodes) {
        if (node.type != tl::formats::cmp_type::kMarker) continue;
        if (gated.count(node.id) != 0) continue;
        const tl::formats::CmpField* type = node.field(f::kType);
        if (type == nullptr) continue;
        const std::string kind = type->as_text();
        if (kind != "Path Node Occupation Box" && kind != "Path Node Occupation Circle") {
            continue;
        }
        // `ENABLED` defaults to true: 717 of the archive's 1,366 property nodes
        // author it and both of the town's blockers say so outright.
        const tl::formats::CmpField* enabled = node.field(f::kEnabled);
        if (enabled != nullptr && enabled->count() == 1 && !enabled->as_bool()) continue;
        apply_path_blocker(node.id, true);
        ++blockers;
    }
    if (blockers > 0) {
        TL_LOG_INFO("  pathing: %zu blocker(s) on, %zu of %zu walkable cell(s) covered", blockers,
                    nav.blocked_count(), nav.walkable_count());
    }
}

void Renderer::Impl::register_door_avoidance() {
    // CTriggerUnit::setActiveInLevel(true): first time
    // the state set to the first, shown, CItem::snapToGround and
    // CBaseUnit::addToAvoidanceMap, whose INTERACTABLE
    // branch probes the trigger's collision into the map grid. INVENTED: no snap for a
    // layout's prop.
    const std::vector<int64_t> nodes = std::move(door_trigger_nodes);
    door_trigger_nodes.clear();
    if (!passability.ready()) return;

    size_t added = 0;
    for (const int64_t node_id : nodes) {
        // Each placed copy has its own group (instance_key()); a group probed
        // twice would be marked twice, which opening it (a single decrement)
        // never takes back.
        if (door_avoidance_added.count(node_id) != 0) continue;
        if (!collision.has_group(node_id)) continue;
        tl::core::Vec3 low, high;
        if (!collision.group_bounds(node_id, low, high)) continue;
        passability.increment_map_passability_collision(low, high, collision, node_id);
        door_avoidance_added.insert(node_id);
        ++added;
    }
    if (added > 0) {
        TL_LOG_INFO("  passability: %zu door/chest/trigger collision model(s) mark the map grid",
                    added);
    }
}

/// Load the authored numbers (WALKINGSPEED, RUNNINGSPEED and the rest) of every unit this
/// layout places, indexed by NAME from media/units/monsters/ once.
void Renderer::Impl::load_unit_stats(const tl::formats::CmpScene& layout,
                                     const std::set<int64_t>& gated) {
    unit_book.clear();

    std::set<std::string> wanted;
    for (const tl::formats::CmpNode& node : layout.nodes) {
        if (node.type != tl::formats::cmp_type::kMonster) continue;
        if (gated.count(node.id) != 0) continue;
        const tl::formats::CmpField* field = node.field(tl::formats::cmp_field::monster::kMonster);
        if (field == nullptr) continue;
        const std::string unit = field->as_text();
        if (!unit.empty()) wanted.insert(lowered(unit));
    }

    // Before the early return: a level whose monsters all come from a Unit Spawner places
    // none, and still needs the unit book.
    script.set_units(&unit_book);
    wire_script(script);
    if (wanted.empty()) return;

    ensure_monster_index();

    // Each unit is read over its `BASEFILE` chain (`read_unit_file`).
    size_t known = 0, missing = 0;
    for (const std::string& unit : wanted) {
        if (index_unit(unit)) {
            ++known;
        } else {
            ++missing;
        }
    }

    script.set_units(&unit_book);
    wire_script(script);
    TL_LOG_INFO("  units: %zu of %zu placed unit(s) have their own .DAT%s", known, wanted.size(),
                missing > 0 ? " (the rest walk at the player's speed)" : "");
}

// CEquipment::calculateCombatStats: the right hand's
// weapon swings with its own attack (class, RANGE, STRIKERANGE, SPEED / 100,
// AI_ATTACKCOOLDOWN), which CCharacter::selectAttack takes over the unit's ATTACK.
tl::core::UnitStats::WeaponAttack Renderer::Impl::weapon_attack_of(const std::string& item_name,
                                                                   tl::core::Hand hand) {
    if (item_name.empty()) return {};
    ensure_item_index();
    const tl::core::ItemStats* item = items.find(item_name);
    if (item == nullptr) return {};
    load_unit_types();
    return tl::core::weapon_attack_of(*item, hand, unit_types);
}

// CCharacter::createDefaultEquipment:
// loadAndEquipSpawnClass for SPAWNRIGHTHAND (0) then
// SPAWNLEFTHAND (1): one rollSpawnClass at level -1, flags 0x10, seed 0xffffffff (g_Rand
// as it stands), the first unit equipped.
void Renderer::Impl::roll_unit_equipment(const std::string& unit,
                                         tl::core::LevelScript::UnitCreation& made) {
    const tl::core::UnitStats stats = stats_for_unit(unit);
    if (!stats.spawn_right_hand.empty()) {
        const std::string rolled =
            resolve_spawn_resource("Spawn Class", stats.spawn_right_hand,
                                   /*depth=*/0, /*level=*/-1, /*flags=*/0x10);
        if (!rolled.empty()) {
            made.right_hand = rolled;
            made.weapon_attack = weapon_attack_of(rolled);
        }
    }
    if (!stats.spawn_left_hand.empty()) {
        const std::string rolled =
            resolve_spawn_resource("Spawn Class", stats.spawn_left_hand,
                                   /*depth=*/0, /*level=*/-1, /*flags=*/0x10);
        if (!rolled.empty()) {
            made.left_hand = rolled;
            made.left_weapon_attack = weapon_attack_of(rolled, tl::core::Hand::Left);
        }
    }
}

tl::core::LevelScript::UnitEquipment Renderer::Impl::unit_equipment_hooks() {
    ensure_item_index();
    load_unit_types();
    tl::core::LevelScript::UnitEquipment hooks;
    hooks.items = &items;
    // `CResourceManager::createEquipment(name, false)`:
    // `createItem(name, false)`.
    hooks.create = [this](const tl::core::ItemStats& data, bool random_enchant) {
        return create_item(data, false, random_enchant);
    };
    // `loadAndEquipSpawnClass`: level -1, seed -1, flags
    // 0x10, on `g_Rand`.
    hooks.roll = [this](const std::string& spawn_class, std::vector<std::string>& units,
                        std::vector<bool>& enchant) {
        SpawnTables tables;
        build_spawn_context(tables, /*items_by_type=*/false);
        const std::optional<tl::core::SpawnClass> loaded =
            load_spawn_class(spawn_class, tables.context);
        if (!loaded) return;
        loaded->roll_spawn_class(units, &enchant, nullptr, nullptr, /*level=*/-1, 0xffffffffu,
                                 /*level_bonus=*/0, /*rarity_override=*/-1, /*magic_find=*/0,
                                 /*flags=*/0x10, tl::core::game_random(), tables.context);
    };
    hooks.enchant = [this](tl::core::ItemStats& item) {
        tl::core::enchant_item(item, true, item_roll_context());
    };
    hooks.weapon_attack = [this](const std::string& item, tl::core::Hand hand) {
        return weapon_attack_of(item, hand);
    };
    return hooks;
}

bool Renderer::Impl::index_unit(const std::string& name) {
    const std::string unit = lowered(name);
    if (unit_book.find(unit) != nullptr) return true;

    ensure_monster_index();
    {
        const auto found = monster_files.find(unit);
        if (found == monster_files.end()) return false;

        // The unit's own data, merged over its `BASEFILE` chain
        // (`CUnitResourceList::parseChildFile`).
        const tl::formats::AdmFile file = read_unit_file(found->second);
        tl::core::UnitStatsSources sources;
        sources.unit_types = &unit_types;
        ensure_effect_data();
        sources.effect_types = &effect_types;
        sources.skill = [this](const std::string& skill) -> const tl::core::AttackSkill* {
            ensure_skill_index();
            const auto entry = skills.find(skill);
            return entry == skills.end() ? nullptr : &entry->second;
        };
        sources.item_unittype = [this](const std::string& item) -> std::optional<std::string> {
            ensure_item_index();
            const tl::core::ItemStats* found_item = items.find(item);
            if (found_item == nullptr) return std::nullopt;
            return found_item->unittype;
        };
        sources.has_animation = [](const std::string& dir, const std::string& animation,
                                   bool armed) {
            return !archive_animation_in_dir(dir, animation, armed, 0).empty();
        };
        sources.weapon_attack = [this](const std::string& item, tl::core::Hand hand) {
            return weapon_attack_of(item, hand);
        };
        tl::core::UnitStats stats = tl::core::read_unit_stats(file, unit, sources);
        // PORT: the player's pet is the renderer's, not a level machine's
        // character, so `setMaster`'s side for it (the
        // master's alignment, `GOOD`) is carried on its unit data: a `PET`
        // with no `ALIGNMENT` of its own stands on the player's side.
        if (file.ok && file.root.find("ALIGNMENT") == nullptr && lowered(stats.unittype) == "pet") {
            stats.alignment = tl::core::UnitStats::Alignment::Friendly;
        }
        unit_book.add(unit, stats);
        std::string swings;
        for (const tl::core::AttackSkill& attack : stats.attacks) {
            swings += swings.empty() ? ", attacks with " : "/";
            swings += attack.animation;
        }
        TL_LOG_INFO(
            "    %-24s walk %.2f run %.2f, radius %.2f, sight %.1f, reach %.2f, "
            "hp %.0f-%.0f%%, dmg %.0f-%.0f%%, %s%s%s%s",
            unit.c_str(), static_cast<double>(stats.movement.walking_speed),
            static_cast<double>(stats.movement.running_speed), static_cast<double>(stats.radius),
            static_cast<double>(stats.sight_radius), static_cast<double>(stats.reach),
            static_cast<double>(stats.min_health), static_cast<double>(stats.max_health),
            static_cast<double>(stats.min_damage), static_cast<double>(stats.max_damage),
            stats.hostile() ? "hostile" : (stats.friendly() ? "friendly" : "neutral"),
            stats.armed ? ", armed" : "", stats.collideable ? "" : ", walk through",
            swings.c_str());
    }
    return true;
}

// PORT: a unit's position by name, from its OGRE bounding box (headless runs).
bool Renderer::Impl::unit_position(const std::string& name, Ogre::Vector3& at,
                                   std::string& found) const {
    const Ogre::Entity* unit = find_unit_entity(name, found);
    if (unit == nullptr) return false;
    at = unit->getWorldBoundingBox(true).getCenter();
    return true;
}

// PORT: a drawn unit by (part of) its name, for the headless switches.
Ogre::Entity* Renderer::Impl::find_unit_entity(const std::string& name, std::string& found) const {
    const std::string want = lowered(name);
    Ogre::Entity* best = nullptr;
    const std::string* best_name = nullptr;
    bool best_exact = false;
    // The first unit whose name contains `name`, unless one is called exactly
    // that ("stash" is the Stash, not the Shared Stash placed before it).
    auto consider = [&](Ogre::Entity* entity, const std::string& unit) {
        if (best_exact || entity == nullptr) return;
        const std::string have = lowered(unit);
        if (have.find(want) == std::string::npos) return;
        const bool exact = have == want;
        if (best != nullptr && !exact) return;
        best = entity;
        best_name = &unit;
        best_exact = exact;
    };
    for (const auto& entry : scene_units) consider(entry.second.entity, entry.first);
    for (const auto& owner : spawned) {
        for (const SpawnedUnit& made : owner.second) consider(made.entity, made.unit);
    }
    if (best == nullptr) return nullptr;
    found = *best_name;
    return best;
}

}  // namespace tl::render
