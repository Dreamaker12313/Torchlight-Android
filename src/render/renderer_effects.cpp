// The renderer's implementation: floaties, selection, skills and effects, missiles,
// spawning.
#include "render/renderer_impl.h"
#include "render/unit_opacity.h"

namespace tl::render {
namespace {
/// The voice key of the player's `LOWMANA_SOUND`, which `bankPlaying` asks after.
const std::string kLowManaVoice = "player/lowmana";
}  // namespace

/// One frame of a `FLOATYTEXT` line, over the unit's own position plus `kFloatyHeight`
/// (as `CQuestDialogMenu::setNPC`); off screen or behind the camera it is hidden.
// `CGameUI::addTextEvent`.
void Renderer::Impl::add_text_event(const tl::core::TextEvent& event) {
    if (!tl::core::text_events_shown(game_settings().floaty_numbers)) return;
    // `Ogre::Frustum::isVisible(Sphere(position, 0.5))` on the game camera.
    //
    if (camera != nullptr &&
        !camera->isVisible(
            Ogre::Sphere(Ogre::Vector3(event.x, event.y, event.z), tl::core::kTextEventRadius))) {
        return;
    }
    TL_LOG_INFO("text event: '%s' at (%.2f, %.2f, %.2f)", event.text.c_str(),
                static_cast<double>(event.x), static_cast<double>(event.y),
                static_cast<double>(event.z));
    text_events.push_back(event);
}

// PORT: a combat text (`make_combat_text`) added to the port's text events.
void Renderer::Impl::add_combat_text(tl::core::CombatText what, const Ogre::Vector3& unit_at,
                                     int amount) {
    add_text_event(tl::core::make_combat_text(what, unit_at.x, unit_at.y, unit_at.z, amount));
}

// `CCharacter::modifyDamage`: `Absorbed` and `!` over the
// character, then `CSoundBank::playSample(7)`; the combat-log line is unused.
void Renderer::Impl::show_absorbed(const Ogre::Vector3& at, const tl::core::UnitStats& stats) {
    add_combat_text(tl::core::CombatText::Absorbed, at);
    play_sound(stats.sounds[tl::core::SoundSlot::ShieldBlock], at);
}

// PORT: presentation of an absorbed hit at the unit (its text and sound).
void Renderer::Impl::show_unit_absorbed(tl::core::LevelScript* machine, int64_t node) {
    if (machine == nullptr) return;
    tl::core::Vec3 at;
    if (!machine->unit_position(node, at)) return;
    show_absorbed(Ogre::Vector3(at.x, at.y, at.z), stats_for_unit(machine->unit_of(node)));
}

bool Renderer::Impl::unit_node_position(int64_t node, Ogre::Vector3& at) {
    if (node == 0) {
        if (subject_node == nullptr) return false;
        at = subject_node->getPosition();
        return true;
    }
    for (const auto& entry : scene_units) {
        if (entry.second.node != node || entry.second.entity == nullptr) continue;
        const Ogre::SceneNode* sn = entry.second.entity->getParentSceneNode();
        if (sn == nullptr) continue;
        at = sn->_getDerivedPosition();
        return true;
    }
    for (const auto& owner : spawned) {
        for (const SpawnedUnit& made : owner.second) {
            if (made.entity == nullptr || made.id != node) continue;
            const Ogre::SceneNode* sn =
                made.node != nullptr ? made.node : made.entity->getParentSceneNode();
            if (sn == nullptr) continue;
            at = sn->_getDerivedPosition();
            return true;
        }
    }
    return false;
}

// `CCharacter::loadModel` makes the slot's `CParticle` once; each use calls
// `CParticle::Start`. DIVERGENCE: the layout is placed
// anew per use (no restart of a placed layout here), so two quick blows show two.
void Renderer::Impl::start_character_particle(const tl::core::UnitStats& unit, int slot,
                                              const Ogre::Vector3& at) {
    if (slot < 0 || tl::core::character_particle::kCount <= slot) return;
    const std::string name =
        tl::core::character_particle_names(unit.particles)[static_cast<size_t>(slot)];
    if (name.empty()) return;
    // `CResourceManager::createParticle`: the name's file
    // in `media/particles/preload.dat`; an unlisted name makes nothing.
    const std::string file = preloaded_particle_file(name);
    if (file.empty()) return;
    std::vector<Ogre::SceneNode*> holders =
        place_particle_layout(file, at, Ogre::Quaternion::IDENTITY, nullptr);
    if (holders.empty()) return;
    TL_LOG_INFO("character particle: %s (slot %d) at (%.2f, %.2f, %.2f)", name.c_str(), slot,
                static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z));
    TransientParticles transient;
    transient.holders = std::move(holders);
    transient.self_timed = true;
    transient_particles.push_back(std::move(transient));
}

// `CCharacter::updateEffects`, `fxShouldPlay` / `playFX`:
// each held effect's particle, made once 0.1 above the centre.
void Renderer::Impl::play_effect_fx(tl::core::EffectManager& effects, const Ogre::Vector3& centre,
                                    const std::string& who) {
    for (const std::string& name : effects.take_effect_fx()) {
        // `CResourceManager::createParticle`; an unlisted
        // name makes nothing. DIVERGENCE: asked once, not every update; nothing shows either way.
        const std::string file = preloaded_particle_file(name);
        if (file.empty()) continue;
        const Ogre::Vector3 at = centre + Ogre::Vector3::UNIT_Y * 0.1f;
        std::vector<Ogre::SceneNode*> holders =
            place_particle_layout(file, at, Ogre::Quaternion::IDENTITY, nullptr);
        if (holders.empty()) continue;
        TL_LOG_INFO("effect particle: %s on %s at (%.2f, %.2f, %.2f)", name.c_str(), who.c_str(),
                    static_cast<double>(at.x), static_cast<double>(at.y),
                    static_cast<double>(at.z));
        TransientParticles transient;
        transient.holders = std::move(holders);
        transient.self_timed = true;
        transient_particles.push_back(std::move(transient));
    }
}

// `CCharacter::playHitParticle`.
void Renderer::Impl::play_hit_particle(const tl::core::UnitStats& victim, const Ogre::Vector3& from,
                                       const Ogre::Vector3& at) {
    const tl::core::ParticlePoint p = tl::core::blow_particle_position(
        {from.x, from.y, from.z}, {at.x, at.y, at.z}, victim.radius);
    start_character_particle(victim, tl::core::hit_particle_slot(game_settings().show_blood),
                             Ogre::Vector3(p.x, p.y, p.z));
}

// `CCharacter::playStrikeParticle`.
void Renderer::Impl::play_strike_particle(const tl::core::UnitStats& attacker,
                                          const Ogre::Vector3& from, const Ogre::Vector3& at,
                                          float radius, bool critical, bool blocked) {
    const tl::core::ParticlePoint p =
        tl::core::blow_particle_position({from.x, from.y, from.z}, {at.x, at.y, at.z}, radius);
    start_character_particle(attacker, tl::core::strike_particle_slot(critical, blocked),
                             Ogre::Vector3(p.x, p.y, p.z));
}

// `CCharacter::die`. INVENTED: the point
// (`+0x210`, from the object at `+0x200`, unread); the unit's position stands in.
void Renderer::Impl::play_death_particle(const tl::core::UnitStats& unit, const Ogre::Vector3& at) {
    start_character_particle(unit, tl::core::death_particle_slot(game_settings().show_blood), at);
}

// `CCharacter::rollAttack` for the player's blow, what it
// shows: "Blocked!", `applyDamage`'s hit particle and "Glancing Blow!"
// (not for 0 damage), and the strike particle.
// INVENTED: the `+N`/`-N` lines are not drawn; flag 0x800 is not
// handed over.
void Renderer::Impl::show_player_blow(int64_t target, const std::string& unit, float damage) {
    Ogre::Vector3 at = Ogre::Vector3::ZERO;
    if (subject_node == nullptr || !unit_node_position(target, at)) return;
    const Ogre::Vector3 from = subject_node->getPosition();
    const tl::core::UnitStats victim = stats_for_unit(unit);
    if (last_blow_blocked) add_combat_text(tl::core::CombatText::Blocked, at);
    if (damage != 0.0f) {
        play_hit_particle(victim, from, at);
        // The victim is the player's own when its master is one (`+0x640`).
        if (last_blow_glancing) add_combat_text(tl::core::CombatText::GlancingBlow, at);
    }
    play_strike_particle(player_stats, from, at, victim.radius, last_blow_critical,
                         last_blow_blocked);
}

// `CCharacter::awardFame`, the text.
bool Renderer::Impl::award_player_fame(int amount) {
    const bool fame_up = player_character.award_fame(amount, stat_graphs, player_total());
    const int awarded = player_character.last_fame_award();
    if (awarded != 0 && subject_node != nullptr) {
        add_combat_text(tl::core::CombatText::Fame, subject_node->getPosition(), awarded);
    }
    return fame_up;
}

/// Advance and render all floating text items: dialog lines and combat events.
void Renderer::Impl::step_floaties(float delta) {
    std::vector<FloatyItem> items;

    // A floaty talk line: `CQuestDialogMenu::update`
    // closes the menu with `OK` on its next pass.
    if (talk_floaty_close.pending) {
        if (talk_floaty_close.fresh) {
            talk_floaty_close.fresh = false;
        } else {
            const TalkFloatyClose done = talk_floaty_close;
            talk_floaty_close = TalkFloatyClose{};
            talk_menu_closed(done.machine, done.node, 2, done.complete, done.quest);
        }
    }

    // 1. Dialog floaty line (if active)
    if (!floaty_line.empty()) {
        floaty_remaining -= delta;
        if (floaty_remaining <= 0.0f) {
            floaty_line.clear();
            floaty_unit.clear();
            floaty_remaining = 0.0f;
        } else {
            const std::string want = lowered(floaty_unit);
            const Ogre::Entity* speaker = nullptr;
            auto consider = [&](const Ogre::Entity* entity, const std::string& unit) {
                if (speaker != nullptr || entity == nullptr) return;
                if (lowered(unit).find(want) == std::string::npos) return;
                speaker = entity;
            };
            for (const auto& entry : scene_units) consider(entry.second.entity, entry.first);
            for (const auto& owner : spawned) {
                for (const SpawnedUnit& made : owner.second) consider(made.entity, made.unit);
            }
            const Ogre::SceneNode* node =
                speaker != nullptr ? speaker->getParentSceneNode() : nullptr;
            if (node != nullptr && camera != nullptr) {
                const Ogre::Vector3 head = node->_getDerivedPosition() +
                                           Ogre::Vector3(0.0f, tl::core::kFloatyHeight, 0.0f);
                const Ogre::Vector3 view = camera->getViewMatrix() * head;
                if (view.z < 0.0f) {
                    const Ogre::Vector3 clip = camera->getProjectionMatrix() * view;
                    const float at_x = clip.x * 0.5f + 0.5f;
                    const float at_y = -clip.y * 0.5f + 0.5f;
                    const float alpha = floaty_remaining / tl::core::kFloatySeconds;
                    FloatyItem item;
                    item.text = floaty_line;
                    item.x = at_x;
                    item.y = at_y;
                    item.alpha = alpha > 1.0f ? 1.0f : (alpha < 0.0f ? 0.0f : alpha);
                    item.colour = Ogre::ColourValue::White;
                    item.char_height = 0.030f * 1.25f;
                    items.push_back(item);
                }
            }
        }
    }

    // 2. `CGameUI::updateTextEvents`: spent events go back
    // to the pool.
    for (auto it = text_events.begin(); it != text_events.end();) {
        if (!tl::core::update_text_event(*it, delta)) {
            it = text_events.erase(it);
            continue;
        }
        if (camera != nullptr) {
            const Ogre::Vector3 view = camera->getViewMatrix() * Ogre::Vector3(it->x, it->y, it->z);
            if (view.z < 0.0f) {
                const Ogre::Vector3 clip = camera->getProjectionMatrix() * view;
                FloatyItem item;
                item.text = it->text;
                item.x = clip.x * 0.5f + 0.5f;
                item.y = -clip.y * 0.5f + 0.5f;
                item.alpha = it->opacity;
                item.colour = Ogre::ColourValue(it->red, it->green, it->blue, 1.0f);
                item.char_height = 0.030f * it->scale;
                items.push_back(item);
            }
        }
        ++it;
    }

    hud.set_floaties(items);
}

// CGameClient's per-frame target pick. Candidates:
// visible units on nodes the script still calls targetable; the rest is `core/targeting.h`.
bool Renderer::Impl::selected_unit(std::string& unit, bool& hostile, int64_t& node,
                                   Ogre::Entity*& picked_entity) {
    load_unit_types();
    std::vector<tl::core::TargetCandidate> candidates;
    struct Identity {
        const std::string* name;
        int64_t node;
        bool hostile;
        Ogre::Entity* entity;
    };
    std::vector<Identity> identities;

    const tl::core::Vec3 from = player.position();
    const float sight = tl::core::kTargetSightHeight;

    // `max(7.0, weapon range + 2.0)`.
    const float weapon_range =
        player_stats.weapon_attack.present ? player_stats.weapon_attack.attack.range : 0.0f;
    const float range = tl::core::target_range(weapon_range);
    // Nothing beyond this can be chosen, so no unit beyond it gets a line-of-sight ray
    // (the per-frame cost on a crowded floor).
    const float reach = std::max(range, tl::core::kTargetFallbackRadius);

    auto consider = [&](const Ogre::Entity* entity, const std::string& name, int64_t node_id) {
        if (entity == nullptr || !entity->isVisible() || name.empty()) return;
        // Asked of the machine that runs it (a room's boss is the room's).
        if (!node_targetable(node_id, entity->getParentSceneNode())) return;
        // Never a character whose master is the player (checks `+0x5A4`).
        if (node_is_player_pet(node_id)) return;
        const Ogre::SceneNode* at = entity->getParentSceneNode();
        if (at == nullptr) return;
        const Ogre::Vector3 where = at->_getDerivedPosition();
        const float ox = where.x - from.x;
        const float oz = where.z - from.z;
        if (ox * ox + oz * oz > (reach + 0.01f) * (reach + 0.01f)) return;

        tl::core::TargetCandidate candidate;
        candidate.x = where.x;
        candidate.y = where.y;
        candidate.z = where.z;
        // Not in the book: no readable `.DAT`, so no enemy.
        const tl::core::UnitStats* stats = unit_book.find(name);
        candidate.hostile = stats != nullptr && unit_hostile(node_id, *stats);

        // INVENTED: which harmless units may be pointed at; the original's team and type test
        // ('s, with 5) is not decoded. Someone to talk to counts,
        // a companion does not.
        if (!candidate.hostile) {
            namespace ut = tl::core::unit_type;
            const uint32_t type = unit_types.id_of(stats != nullptr ? stats->unittype : "");
            if (!unit_types.isa(type, ut::kInteractiveCharacter)) return;
        }
        // The ray has both ends lifted, over the floor rather than along it.
        tl::core::CollisionHit hit;
        candidate.in_sight =
            collision.empty() ||
            !collision.ray_collision(tl::core::Vec3{from.x, from.y + sight, from.z},
                                     tl::core::Vec3{where.x, where.y + sight, where.z}, true, hit);
        candidate.id = identities.size();
        candidates.push_back(candidate);
        identities.push_back(
            {&name, node_id, candidate.hostile, const_cast<Ogre::Entity*>(entity)});
    };

    for (const auto& entry : scene_units) {
        consider(entry.second.entity, entry.first, entry.second.node);
    }
    for (const auto& owner : spawned) {
        for (const SpawnedUnit& made : owner.second) consider(made.entity, made.unit, made.id);
    }
    if (candidates.empty()) return false;

    std::uint64_t chosen = 0;
    if (!tl::core::select_target(candidates.data(), candidates.size(), from.x, from.z,
                                 player.facing(), range, &chosen)) {
        return false;
    }
    const Identity& picked = identities[static_cast<std::size_t>(chosen)];
    unit = *picked.name;
    hostile = picked.hostile;
    node = picked.node;
    picked_entity = picked.entity;
    return true;
}

bool Renderer::Impl::entity_still_placed(const Ogre::Entity* entity) const {
    if (entity == nullptr) return false;
    for (const auto& entry : scene_units) {
        if (entry.second.entity == entity) return true;
    }
    for (const auto& owner : spawned) {
        for (const SpawnedUnit& made : owner.second) {
            if (made.entity == entity) return true;
        }
    }
    return false;
}

// Run the console's target pick for this frame and say what it chose; apart from
// drive_player() so headless runs (`--pad`) make the same decision.
bool Renderer::Impl::update_pad_selection(bool active) {
    std::string selected;
    bool hostile = false;
    int64_t node = 0;
    Ogre::Entity* entity = nullptr;
    const bool have = active && !script_dialog_open() && !script_player_disabled &&
                      selected_unit(selected, hostile, node, entity);

    // Light it up, and put the last one back (`CGameClient::update`,
    // `setHighlighted` on the pick and the dropped one).
    if (entity != pad_highlighted) {
        if (pad_highlighted != nullptr && entity_still_placed(pad_highlighted)) {
            highlight_entity(pad_highlighted, false, pad_highlighted_hostile, rim_light_texture);
        }
        // The red glow for a hostile one, a mimic's blue (outline_hostile()).
        const tl::core::UnitStats* picked = have ? unit_book.find(selected) : nullptr;
        const bool red = hostile && (picked == nullptr || outline_hostile(*picked));
        if (entity != nullptr) highlight_entity(entity, true, red, rim_light_texture);
        pad_highlighted = entity;
        pad_highlighted_hostile = red;
    }

    // Only when it changes, to keep the log to the decisions.
    if (selected != pad_selected) {
        if (have) {
            TL_LOG_INFO("pad target: '%s'%s", selected.c_str(), hostile ? " (hostile)" : "");
        } else if (!pad_selected.empty()) {
            TL_LOG_INFO("pad target: none");
        }
    }
    pad_selected = selected;
    pad_selected_hostile = hostile;
    pad_selected_node = node;
    return have;
}

// PORT: the tables an item roll reads, gathered once.
tl::core::ItemRollContext Renderer::Impl::item_roll_context() {
    ensure_effect_data();
    load_stat_graphs();
    load_unit_types();
    if (!item_roll_globals_loaded) {
        item_roll_globals_loaded = true;
        const std::vector<uint8_t> bytes = read_resource("media/GLOBALS.DAT.adm");
        const tl::formats::AdmFile globals =
            bytes.empty() ? tl::formats::AdmFile{}
                          : tl::formats::parse_adm(bytes.data(), bytes.size());
        if (globals.ok) {
            item_roll_globals = tl::core::read_item_roll_globals(globals);
            item_label_colours = tl::core::read_item_label_colours(globals);
        } else {
            TL_LOG_WARN("items: media/GLOBALS.DAT did not read; items roll no enchants or sockets");
        }
    }
    tl::core::ItemRollContext context;
    context.globals = &item_roll_globals;
    context.affixes = &affixes;
    context.unit_types = &unit_types;
    context.graphs = &stat_graphs;
    context.volatile_rng = &tl::core::volatile_random();
    context.game_rng = &tl::core::game_random();
    context.effects = &item_effect_context();
    // Filling a new item's sound bank draws on `g_Rand` (`init_item_copy`), device or not.
    ensure_sound_index();
    context.sounds = &sound_info;
    return context;
}

// PORT: the tables an item's effects read, gathered once.
const tl::core::EffectManager::Context& Renderer::Impl::item_effect_context() {
    ensure_effect_data();
    load_stat_graphs();
    load_unit_themes();
    item_effect_context_.types = &effect_types;
    item_effect_context_.graphs = &stat_graphs;
    item_effect_context_.owner.present = false;
    item_effect_context_.theme_book = &unit_themes;
    return item_effect_context_;
}

tl::core::ItemStats Renderer::Impl::create_item(const tl::core::ItemStats& data, bool from_save,
                                                bool random_enchant_allowed, bool force_enchant) {
    // `CResourceManager::createItem` (by GUID,
    // by name): `createNewItem`, `+0x20a`, `unitInit(data, from_save)`,
    // then (by GUID only) the forced `enchant(true)`.
    tl::core::ItemStats copy = data;
    if (copy.is_gold()) {
        // `createNewItem` makes a `CItemGold`
        // (`CItemGold::unitInit`); `enchant` is `CEquipment`'s.
        load_stat_graphs();
        tl::core::init_gold_copy(copy, from_save, level_monster_level, config.difficulty,
                                 stat_graphs, tl::core::volatile_random());
        return copy;
    }
    tl::core::init_item_copy(copy, from_save, random_enchant_allowed, force_enchant,
                             item_roll_context());
    return copy;
}

// PORT: the inventories handed the effect tables.
void Renderer::Impl::refresh_inventory_effects() {
    ensure_effect_data();
    load_stat_graphs();
    const tl::core::InventoryEffectContext context{&effect_types, &stat_graphs, &item_sets,
                                                   &affixes};
    player_inventory.set_effect_context(context);
    pet.bag().set_effect_context(context);
    refresh_pet_effects();
}

void Renderer::Impl::refresh_skill_effects() {
    ensure_effect_data();
    load_stat_graphs();
    skill_effects.types = &effect_types;
    skill_effects.graphs = &stat_graphs;
    // `CEffect::calculateBaseValue` scales by the owner's four stats: who reads matters.
    const tl::core::CharacterEffects total = player_total();
    skill_effects.owner.strength = player_character.strength(total);
    skill_effects.owner.dexterity = player_character.dexterity(total);
    skill_effects.owner.magic = player_character.magic(total);
    skill_effects.owner.defense = player_character.defense(total);
    skill_effects.owner.level = static_cast<uint32_t>(std::max(1, player_character.level()));
    skill_effects.owner.present = true;
    skill_effects.affix = [this](const std::string& name, uint32_t) -> const tl::core::Affix* {
        return affixes.get_affix(name);
    };
    // What `CExecuteSkillProps::getSkillStats` reaches through the owner's
    // skill manager (`EffectDisplayContext::skill_level_property`).
    skill_effects.skill_level_property = [this](const std::string& name,
                                                uint32_t level) -> const tl::core::SkillProperty* {
        ensure_player_skill_files();
        const auto found = player_skill_files.find(tl::core::upper_ascii(name));
        if (found == player_skill_files.end()) return nullptr;
        tl::core::KnownSkill described;
        described.def = &found->second->def;
        described.effective = level == 0 ? 1 : level;
        return described.property(level);
    };
    skill_effects.skill_display_name = [this](const std::string& name) -> std::string {
        ensure_player_skill_files();
        const auto found = player_skill_files.find(tl::core::upper_ascii(name));
        if (found == player_skill_files.end()) return "???";
        const std::string display = found->second->def.display_name;
        return display.empty() ? "???" : display;
    };
    skill_effects.monster_display_name = [this](const std::string& name) -> std::string {
        ensure_monster_index();
        const auto found = monster_files.find(lowered(name));
        if (found == monster_files.end()) return "???";
        const std::vector<uint8_t> bytes = read_resource(found->second);
        if (bytes.empty()) return "???";
        const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        const std::string display = file.string_or("DISPLAYNAME", "");
        return display.empty() ? "???" : display;
    };
    inventory_menu.set_skill_effects(&skill_effects);

    // The applied manager reads the same tables and owner stats (`CEffect::calculateBaseValue`).
    load_unit_themes();
    tl::core::EffectManager::Context applied;
    applied.types = &effect_types;
    applied.graphs = &stat_graphs;
    applied.owner = skill_effects.owner;
    applied.theme_book = &unit_themes;
    // `CBaseUnit::activateEffect` on the
    // player: a cast-skill effect teaches the skill it names.
    applied.on_activate = [this](tl::core::Effect& effect) {
        tl::core::activate_effect(effect, player_proc_skills, stat_graphs, skill_file_lookup());
    };
    player_effects.set_context(applied);
    // `CSkill::calculateEffectiveSkillLevel` asks the
    // character's manager and its pet's (not held yet: null) for `+N to <skill>`.
    player_skills.set_effect_managers(player_total(), nullptr);
}

tl::core::SkillCastContext Renderer::Impl::player_cast_context() {
    // Self-targeted or passive: caster and target are the same character.
    tl::core::SkillCastContext context;
    context.target = &player_effects;
    context.types = &effect_types;
    context.unit_types = &unit_types;
    context.affix = [this](const std::string& name) -> const tl::core::Affix* {
        return affixes.get_affix(name);
    };
    context.target_is_caster = true;
    context.target_is_enemy = false;
    context.target_is_casters_pet = false;
    context.source = tl::core::EffectSource{true, nullptr, 0};
    context.owner_level = player_character.level();
    // Re-read the caster every time (mana, weapons).
    cast_caster = player_caster();
    context.caster = &cast_caster;
    // `CSkillEvent::startEvent`'s layout, for every block a group fires.
    context.on_event = [this](const tl::core::KnownSkill& skill, uint32_t level, int group,
                              const tl::formats::AdmSection& event,
                              const tl::core::SkillEventTarget& target) {
        start_skill_event(skill, level, group, event, target);
    };
    context.events_alive = [this](const std::string& skill, uint32_t run) {
        return skill_events_alive(skill, run, nullptr, 0);
    };
    context.events_can_stop = [this](const std::string& skill, uint32_t run) {
        return skill_events_can_stop(skill, run, nullptr, 0);
    };
    context.reset_events = [this](const std::string& skill, uint32_t run) {
        reset_skill_events(skill, run, nullptr, 0);
    };
    context.reset_event = [this](const std::string& skill, uint32_t run,
                                 const tl::formats::AdmSection& block) {
        detach_event_missiles(skill, run, block, nullptr, 0);
    };
    // `applyEffectOnUnit` on the player:
    // `isEffectValidForUnit` (vtable +0x238), then `applyInstantEffect`:
    // `OPEN TOWN PORTAL` is `openPortal`; `DESUMMON MONSTER`
    // (0x43) is `CCharacter::removePet(NAME)`.
    // INVENTED: other instant types apply nothing.
    context.instant_effect = [this](const tl::core::Effect& effect) {
        if (effect.type >= 0 && effect.type == effect_types.index_of("DESUMMON MONSTER")) {
            int removed = 0;
            for (const MachineView& view : machines()) {
                std::vector<tl::core::ScriptEvent> events;
                removed +=
                    view.script->desummon_player_pets(effect.name, quest_log.state(), events);
                if (!events.empty()) apply_script_events(events, *view.nodes);
            }
            TL_LOG_INFO("skills: %d pet(s) called '%s' desummoned", removed, effect.name.c_str());
            return;
        }
        if (effect.type == tl::core::kEffectTypeDamage) {
            player_instant_damage(effect);
            return;
        }
        // Case 0x71, `WARP`, the player's only:
        // `CGameClient::warpLevels(NAME, 0, (int)value(), false, L"", false)`.
        constexpr int kEffectTypeWarp = 0x71;
        if (effect.type == kEffectTypeWarp) {
            const float value = tl::core::effect_column_value(effect, 0, stat_graphs);
            warp_by_effect(effect.name, static_cast<int32_t>(value));
            return;
        }
        if (effect.type != tl::core::kEffectTypeOpenTownPortal &&
            effect.type != tl::core::kEffectTypeOpenPortal) {
            return;
        }
        load_unit_types();
        const tl::core::ItemUseTarget target = player_use_target();
        if (!tl::core::effect_valid_for_unit(target, nullptr, effect, 0.0f, unit_types)) return;
        if (effect.type == tl::core::kEffectTypeOpenPortal) {
            // Case 0x8d: `openMapPortal(NAME, level)` (vtable +0x1c4).
            open_map_portal(effect.name);
            return;
        }
        // Case 0x2d: `openPortal(level)` (vtable +0x1c0),
        // then `incrementJournalStatistic(0xd, 1)`.
        open_town_portal();
        count_journal(tl::core::JournalStatistic::PortalsUsed);
    };
    return context;
}

// `CCharacter::applyInstantEffect`, case
// 0x34, with the player as the character struck.
void Renderer::Impl::player_instant_damage(const tl::core::Effect& effect) {
    if (player_health.dead) return;
    const float value =
        tl::core::effect_base_value(effect, tl::core::EffectCalculate::Roll, stat_graphs,
                                    skill_effects.owner, &tl::core::volatile_random());
    const float v = 0.0f < value ? -value : value;
    if (!(v < 0.0f)) return;
    // The effect's owner: the unit whose skill event this is, or the player.
    const bool by_unit = instant_caster_machine != nullptr;
    const tl::core::CharacterEffects attacker =
        by_unit
            ? tl::core::CharacterEffects(instant_caster_machine->unit_effects(instant_caster_node))
            : player_total();
    const int type = static_cast<int>(effect.damage_type);
    const tl::core::InstantDamage hit =
        tl::core::instant_damage(value, type, effect.values[2], effect.values[3], player_defender(),
                                 &attacker, tl::core::volatile_random());
    if (!hit.applied) return;
    const std::string from = by_unit ? instant_caster_machine->unit_of(instant_caster_node)
                                     : std::string("its own skill");
    if (hit.blocked) {
        // "Blocked!" over the character.
        if (subject_node != nullptr) {
            add_combat_text(tl::core::CombatText::Blocked, subject_node->getPosition());
        }
        TL_LOG_INFO("combat: player blocked '%s' from %s", effect.name.c_str(), from.c_str());
        return;
    }
    // INVENTED: a critical's interrupt and the knock-back are not applied.
    hurt_player(static_cast<float>(hit.damage), from,
                by_unit ? instant_caster_node : tl::core::LevelScript::kNoAttacker,
                /*struck=*/true);
    if (!player_health.dead) {
        std::array<int, tl::core::kDamageTypeCount> by_type{};
        if (0 <= type && type < tl::core::kDamageTypeCount) {
            by_type[static_cast<size_t>(type)] = hit.damage;
        }
        // `applyDamageEffects(this, this, ...)`: the character is its own attacker.
        apply_damage_status(nullptr, 0, static_cast<float>(hit.damage), by_type, 1, type, false,
                            player_character.level(), tl::core::EffectSource{true, nullptr, 0});
    }
}

void Renderer::Impl::start_player_passives() {
    // `CSkillManager::startPassiveSkills` stops every
    // running passive first, so re-running is not additive.
    refresh_skill_effects();
    tl::core::SkillCastContext context = player_cast_context();
    context.on_skill = [](const std::string& name, uint32_t level, bool ok) {
        if (ok) return;
        // Usually its `EVENT_START` names an affix no file answers to.
        TL_LOG_WARN("skills: passive '%s' at level %u applied nothing", name.c_str(), level);
    };
    const int started = tl::core::start_passive_skills(player_skills, context);
    // A passive's affix can carry `+N to <skill>`: recompute the ranks
    // (`calculateEffectiveSkillLevel` inside the loop).
    player_skills.set_effect_managers(player_total(), nullptr);
    TL_LOG_INFO("skills: %d passive(s) started, %zu affix(es) held, %zu effect(s)", started,
                player_effects.affixes().size(), player_effects.all_effects().size());
}

void Renderer::Impl::step_player_camera_opacity(float delta) {
    // `CGameClient::updateIngame`: `updateCameraOpacity` with the frame's
    // delta, or 0 when paused.
    if (subject_node == nullptr || camera == nullptr) return;
    const Ogre::Vector3 at = subject_node->_getDerivedPosition();
    const Ogre::Vector3 eye = camera->getDerivedPosition();
    const tl::core::OpacityStep step =
        tl::core::update_camera_opacity(player_opacity, eye.x - at.x, eye.y - at.y, eye.z - at.z,
                                        /*want=*/true, /*in_near_range=*/true, /*visible=*/true,
                                        /*death_flight=*/false, 0.0f, delta);
    player_opacity = step.opacity;
    if (step.written) tl::render::set_node_opacity(subject_node, player_opacity);
}

void Renderer::Impl::update_player_effects(float dt) {
    // A fresh or loaded inventory has not been handed its tables yet.
    if (player_inventory.effect_context().types == nullptr) refresh_inventory_effects();
    // `CCharacter::update`: `updateEffects`, then
    // `calculateEffectValues`, `deleteDeadEffects`; `updateAffixes`
    // from `CBaseUnit::update`. All take dt.
    if (dt <= 0.0f) return;
    player_effects.update_effects(dt);
    // `updateEffects`' `MISSILE REFLECT VISUAL` (0x3d) case: the reflector
    // strikes back with the holder's weapon (`+0x720` = share). INVENTED: the counter-blow
    // is not struck. Knock-back on the player: the push held to 10.
    player.set_push(tl::core::bound_outside_push(
        knock_back_push(player_effects, player_total().effect_value(0x35, 7), dt, "the player"),
        true));
    if (subject_node != nullptr) {
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        play_effect_fx(player_effects, subject_node->getPosition() + Ogre::Vector3::UNIT_Y * body,
                       "the player");
    }
    player_effects.update_affixes(dt);
    player_effects.calculate_effect_values();
    player_effects.delete_dead_effects();
    // Then `updateHP` and `updateMana`, off the totals just worked out.
    update_player_meters(dt);

    // `CCharacter::updateActiveSkill`, vtable +0x380,
    // right after `updateEffects`: upkeep and hold time.
    step_active_skill(dt);

    // `CSkillManager::update(float)`: cooldowns, and a
    // run whose minimum time is up fires `EVENT_END`.
    update_live_skill_events(dt);
    player_skill_runner.update(dt, player_cast_context(),
                               [this](const std::string& name) -> const tl::core::KnownSkill* {
                                   return player_skill(name);
                               });
    // CCharacter::update: `+0x708`, the procs' lockout.
    if (0.0f < player_proc_lockout) player_proc_lockout -= dt;
}

void Renderer::Impl::load_recharge_rates() {
    if (recharge_rates_loaded) return;
    recharge_rates_loaded = true;
    // `CGameGlobals::reload`: a missing key keeps the
    // constructor's value.
    const std::vector<uint8_t> bytes = read_resource("media/GLOBALS.DAT.adm");
    if (bytes.empty()) return;
    const tl::formats::AdmFile globals = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!globals.ok) return;
    recharge_rates.hp = globals.float_or("HP_RECHARGE_RATE", recharge_rates.hp);
    recharge_rates.pet_hp = globals.float_or("PET_HP_RECHARGE_RATE", recharge_rates.pet_hp);
    recharge_rates.mana = globals.float_or("MANA_RECHARGE_RATE", recharge_rates.mana);
}

void Renderer::Impl::refresh_player_maxima() {
    // `CCharacter::maxHP` and `maxMana`:
    // the curve's base, permanent bonuses, `MAX`/`PERCENT` effects.
    const tl::core::CharacterEffects total = player_total();
    player_health.max = static_cast<float>(player_character.max_hp_of(total));
    player_character.set_mp(player_character.mp(),
                            static_cast<float>(player_character.max_mp_of(total)));
}

void Renderer::Impl::respec_player() {
    // `CPlayer::resetSkills`: `stopAllSkills(true, false,
    // true)`, the passives too, restarted at the new levels as `setSkillLevel`
    // does.
    player_skill_runner.stop_all(
        player_cast_context(),
        [this](const std::string& name) -> const tl::core::KnownSkill* {
            return player_skill(name);
        },
        /*force=*/true, /*passives=*/true);
    const int refunded = player_skills.reset_skills();
    player_character.give_unspent_skill_points(refunded);
    start_player_passives();
    TL_LOG_INFO(
        "respec: skills back to the class's starting levels, %d point(s) refunded (%d "
        "unspent)",
        refunded, player_character.unspent_skill_points());
}

void Renderer::Impl::update_player_meters(float dt) {
    refresh_player_maxima();
    // `CCharacter::updateHP`/`updateMana` return at once in state 5 or 6.
    if (player_health.dead || !(player_health.current > 0.0f)) return;
    load_recharge_rates();

    const float hp = tl::core::hp_change(player_health.max, recharge_rates, player_total(), dt);
    if (hp > 0.0f) {
        // `modifyHP`: up to `maxHP()`, an
        // integer; in god mode straight to `maxHP()`.
        const float ceiling = static_cast<float>(static_cast<int>(player_health.max));
        player_health.current = god_mode ? ceiling : std::min(ceiling, player_health.current + hp);
    } else if (hp < 0.0f) {
        // The effect owner taking the credit, for `kill_player`.
        const tl::core::EffectSource credit = player_effects.death_credit();
        player_effect_killer = !credit.set       ? tl::core::LevelScript::kNoAttacker
                               : credit.player() ? 0
                                                 : credit.node;
        player_hurt_by_effects = true;
        hurt_player(-hp, "an effect", tl::core::LevelScript::kNoAttacker, false, /*shown=*/false);
        player_hurt_by_effects = false;
        // HP under 1, `updateHP` broadcasts death by effect (inline
        // e14) before `die`'s broadcast, which `kill_player` held back.
        if (static_cast<int>(std::floor(player_health.current)) < 1) {
            player_effects.take_death_credit();
            unit_observers.broadcast({nullptr, 0}, tl::core::unit_state::kDiedOfEffect);
            if (player_health.dead) {
                unit_observers.broadcast({nullptr, 0}, tl::core::unit_state::kDied);
            }
        }
    }
    // `updateHP`: the effects' damage goes onto the number.
    const float effect_damage = tl::core::effect_values_of_damage(player_total()) * dt;
    if (effect_damage < 0.0f) {
        player_damage_shown.pending = player_damage_shown.pending - effect_damage;
    }

    const float mana =
        tl::core::mana_change(player_character.max_mp(), recharge_rates, player_total(), dt);
    if (mana != 0.0f) modify_player_mana(mana);
    // `displayDamageAbsorbed(delta)`, after `updateMana`.
    display_player_damage(dt);
}

// `CCharacter::displayDamageAbsorbed(float)` on the
// player: red, one above the character.
void Renderer::Impl::display_player_damage(float delta) {
    int amount = 0;
    if (!tl::core::display_damage(player_damage_shown, player_stats.display_dmg, delta, amount)) {
        return;
    }
    if (subject_node == nullptr) return;
    const Ogre::Vector3 over = subject_node->getPosition();
    add_text_event(tl::core::make_damage_text(over.x, over.y, over.z, amount, true));
}

// `CCharacter::modifyMana`.
void Renderer::Impl::modify_player_mana(float delta) {
    const float ceiling = static_cast<float>(static_cast<int>(player_character.max_mp()));
    // `getIsInGodMode` (vtable +0x290, `CPlayer`): the mana is `maxMana()`.
    //
    if (god_mode) {
        player_character.set_mp(ceiling, player_character.max_mp());
        return;
    }
    const float next = std::max(0.0f, std::min(ceiling, player_character.mp() + delta));
    player_character.set_mp(next, player_character.max_mp());
}

tl::core::ItemUseTarget Renderer::Impl::player_use_target() {
    tl::core::ItemUseTarget target;
    target.hp = player_health.current;
    target.max_hp = player_health.max;
    target.mana = player_character.mp();
    target.max_mana = player_character.max_mp();
    target.damageable = true;
    target.unittype = "PLAYER";
    target.level = player_character.level();
    target.effects = &player_effects;
    target.inventory = &player_inventory;
    target.level_allows_portals = tl::core::town_portal_allowed(portal_level());
    target.level_allows_map_portals = tl::core::map_portal_allowed(portal_level());
    target.stunned = player_themes.themes.has_unit_theme("STUNNED", &unit_themes);
    target.dying_or_dead = player_health.dead;
    // `LEARN SKILL`: the player's skill manager and spell slots.
    target.skills = &player_skills;
    target.pet = false;
    target.character = &player_character;
    target.skill_holds = tl::core::skill_holds_character(player_active_skill);
    target.effect_types = &effect_types;
    target.skill_def = [this](const std::string& name) -> const tl::core::SkillDef* {
        const auto it = player_skill_files.find(tl::core::upper_ascii(name));
        if (it != player_skill_files.end()) return &it->second->def;
        // `learnSpell`'s "Skill not found : <name>".
        TL_LOG_WARN("Skill not found : %s", name.c_str());
        return nullptr;
    };
    return target;
}

bool Renderer::Impl::use_player_item(size_t slot_index, bool from_tray) {
    // `CGameUI::useItem`: a living user and a
    // useable item.
    if (player_health.dead || !(player_health.current > 0.0f)) return false;
    if (slot_index >= player_inventory.slots().size()) return false;
    const tl::core::InventorySlot& slot = player_inventory.slots()[slot_index];
    if (slot.count <= 0) return false;
    load_unit_types();
    ensure_effect_data();
    const tl::core::ItemStats item = slot.item;
    const int stack = slot.count;
    if (!tl::core::item_is_useable(item, unit_types)) return false;
    // The Xbox's `useItem` sends a `PETONLY` item to the pet. INVENTED: not
    // modelled.
    if (unit_types.isa(item.unittype, tl::core::unit_type::kPetOnly) && pet.alive()) return false;
    // `CGameUI::performItemUse`: `TARGET_TYPE` 1 picks a
    // target with the cursor. INVENTED: a pad has no cursor; nothing happens.
    if (item.target_type != 0) return false;

    tl::core::ItemUseTarget target = player_use_target();
    if (!tl::core::can_use_on_target(item, target, unit_types, stat_graphs)) {
        if (from_tray && item.name == "Town Portal Scroll") {
            // The bag's press: an unusable `Town Portal
            // Scroll` queues `VOICECANTCASTHERE_SOUND`, 0.1 s on.
            play_voice(tl::core::SoundSlot::VoiceCantCastHere, 0.1f);
        } else {
            // `ERROR_SOUND` on the game UI's bank.
            play_ui(tl::core::InterfaceSound::Error);
        }
        TL_LOG_INFO("use: %s refused", item.display_name.c_str());
        return false;
    }
    int uses = item.uses;
    const tl::core::ItemUseResult used =
        tl::core::use_on_target(item, stack, uses, target, unit_types, stat_graphs);
    // CEquipment::useOnTarget: incrementJournalStatistic(12, 1)
    if (used.potion_counted) count_journal(tl::core::JournalStatistic::PotionsUsed);
    if (used.play_use_sound && subject_node != nullptr) {
        play_sound(item.use_sound, subject_node->getPosition());
    }
    // `incrementStackBy(-1)`, `removeEquipment` on the last use. INVENTED: a copy's
    // `+0x248` is not kept between uses (every shipped item has `USES:1`).
    if (used.remove_item) {
        player_inventory.remove_item(slot_index, stack);
    } else if (stack > 1) {
        player_inventory.remove_item(slot_index, 1);
    }
    TL_LOG_INFO("use: %s, %d effect(s) on the player, %d left", item.display_name.c_str(),
                used.applied, used.remove_item ? 0 : stack - 1);
    // `applyInstantEffect`'s `OPEN TOWN PORTAL` (before the stack loses its copy), then
    // `incrementJournalStatistic(0xd, 1)`.
    if (used.open_town_portal) {
        open_town_portal();
        count_journal(tl::core::JournalStatistic::PortalsUsed);
    }
    // ... and its `OPEN PORTAL`: a map scroll's portal.
    for (const std::string& dungeon : used.open_map_portals) open_map_portal(dungeon);
    if (used.maxima_changed) {
        refresh_player_maxima();
        TL_LOG_INFO("use: maximum health %.0f, mana %.0f (permanent +%d / +%.1f)",
                    static_cast<double>(player_health.max),
                    static_cast<double>(player_character.max_mp()), player_character.permanent_hp(),
                    static_cast<double>(player_character.permanent_mp()));
    }
    if (used.respec) respec_player();
    if (!used.skill.empty()) perform_unknown_skill(used.skill, used.skill_level);
    if (!used.learned_spell.empty()) {
        // `applyInstantEffect`: global slot 0x41, `VOICESPELLLEARNED_SOUND`, which
        // no class file authors (they spell it with two Ls): silent, as in the original.
        play_voice(tl::core::SoundSlot::VoiceSpellLearned, 0.1f);
        // `applyInstantEffect`.
        queue_tip(tl::core::ContextTip::Spell);
        player_skills.set_effect_managers(player_total(), nullptr);
        TL_LOG_INFO("use: learned the spell '%s'", used.learned_spell.c_str());
    }
    return true;
}

// PORT: the player's inventory handed to `identify_item_in`.
bool Renderer::Impl::identify_player_item(size_t slot_index) {
    return identify_item_in(player_inventory, slot_index);
}

bool Renderer::Impl::identify_item_in(tl::core::Inventory& bag, size_t slot_index) {
    // the `Identify Scroll` copies (`CInventory::getEquipmentsOfUnitGuid`)
    // in the bag, then the pet's unless gone to town (0x29, 0x2a). INVENTED: the original
    // spends the first and clears the last; one stack is both here.
    tl::core::ItemStats* target = bag.item_in_slot(slot_index);
    if (target == nullptr) return false;
    tl::core::Inventory* holder = &player_inventory;
    int scroll = player_inventory.find_by_name("Identify Scroll");
    if (scroll < 0 && pet.alive() && pet.in_world()) {
        holder = &pet.bag();
        scroll = holder->find_by_name("Identify Scroll");
    }
    if (scroll < 0) {
        TL_LOG_INFO("identify: no Identify Scroll for '%s'", tl::core::item_name(*target).c_str());
        return false;
    }
    ensure_effect_data();
    const tl::core::ItemStats used_item = holder->slots()[static_cast<size_t>(scroll)].item;
    const int stack = holder->slots()[static_cast<size_t>(scroll)].count;
    int uses = used_item.uses;
    const tl::core::ItemUseResult used = tl::core::use_on_item(used_item, stack, uses, *target);
    if (used.applied == 0) {
        TL_LOG_INFO("identify: the scroll does nothing to '%s'",
                    tl::core::item_name(*target).c_str());
        return false;
    }
    // (`CItem::destroyItemText`: the copy reads under its full name from here.)
    if (used.play_use_sound && subject_node != nullptr) {
        play_sound(used_item.use_sound, subject_node->getPosition());
    }
    holder->remove_item(static_cast<size_t>(scroll), used.remove_item ? stack : 1);
    // The copy's effects count from now on, wherever it sits.
    refresh_inventory_effects();
    TL_LOG_INFO("identify: '%s' identified, %d scroll(s) left",
                tl::core::item_name(*target).c_str(), holder->count_by_name("Identify Scroll"));
    return true;
}

bool Renderer::Impl::wear_player_item(size_t slot_index) {
    // past the useable items.
    if (slot_index >= player_inventory.slots().size()) return false;
    const tl::core::InventorySlot& slot = player_inventory.slots()[slot_index];
    if (slot.count <= 0) return false;
    load_unit_types();
    if (!slot.item.identified) return identify_player_item(slot_index);

    const std::string name = tl::core::item_name(slot.item);
    const tl::core::RequirementReductions no_reductions{};
    const int level_required =
        tl::core::item_level_requirement(slot.item, &no_reductions, unit_types);
    const bool worn = slot_index < 12;
    const tl::core::Inventory::WearOutcome out =
        worn ? player_inventory.take_off_item(slot_index, unit_types)
             : player_inventory.wear_item(slot_index, unit_types, &player_character,
                                          &player_effects);
    // Sample 0x11 (`LAND_SOUND`) of each item's bank, at the player.
    if (subject_node != nullptr) {
        for (const tl::core::ItemStats& item : out.sounded) {
            play_sound(item.land_sound, subject_node->getPosition());
        }
    }
    // `CLevel::addItem` at the character, and the item's drop.
    for (const tl::core::InventorySlot& dropped : out.dropped) {
        drop_item_near_player(dropped.item, dropped.count);
    }
    if (out.equipped) {
        TL_LOG_INFO("equip: '%s' worn at location %d (%zu sound(s), %zu dropped)", name.c_str(),
                    out.location, out.sounded.size(), out.dropped.size());
    } else if (out.taken_off) {
        TL_LOG_INFO("equip: '%s' taken off%s", name.c_str(),
                    out.dropped.empty() ? "" : ", onto the floor: the bag is full");
    } else {
        TL_LOG_INFO("equip: '%s' is not worn (the character is level %d, the item asks for %d)",
                    name.c_str(), player_character.level(), level_required);
    }
    return out.equipped || out.taken_off;
}

void Renderer::Impl::bag_default_action(int slot_idx) {
    if (slot_idx < 0 || static_cast<size_t>(slot_idx) >= player_inventory.slots().size() ||
        player_inventory.slots()[static_cast<size_t>(slot_idx)].count <= 0) {
        return;
    }
    const auto& slot = player_inventory.slots()[static_cast<size_t>(slot_idx)];
    load_unit_types();
    if (unit_types.isa(slot.item.unittype, tl::core::unit_type::kSocketable)) {
        // a gem (ISA 0x78) on an ordinary tray goes to
        // the item-select dialog.
        open_gem_socketing();
    } else if (unit_types.isa(slot.item.unittype, tl::core::unit_type::kFish)) {
        // a fish (ISA 0x7c) is used on the player's first pet.
        feed_pet_from(player_inventory, static_cast<size_t>(slot_idx));
    } else if (unit_types.isa(slot.item.unittype, tl::core::unit_type::kSpell)) {
        // a spell goes to the learn test without the empty slot,
        // then (the learn, or the replace dialog).
        if (tl::core::spell_learnable(slot.item, slot.item.uses, player_use_target(), unit_types,
                                      stat_graphs)) {
            learn_spell_from_tray(/*pet_owner=*/false, static_cast<size_t>(slot_idx));
        }
    } else if (tl::core::item_is_useable(slot.item, unit_types)) {
        // `processIngameInput`'s slot use: a useable item
        // (`CItem::isUseable`) goes to `useItem`.
        use_player_item(static_cast<size_t>(slot_idx), /*from_tray=*/true);
    } else {
        // any other is identified, worn or taken off; a quest item
        // stays.
        wear_player_item(static_cast<size_t>(slot_idx));
    }
    // Every branch above can change slot 0/1: keep the held-item mesh in sync.
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
}

void Renderer::Impl::activate_item_slot(int index) {
    // `CGameUI::activateItemSlot`: the slot's item GUID (`CPlayer +0x900`), the
    // bag's copy, and its slot as the one to use (`CGameUI +0x1678`, X360 `+0x1498`).
    if (index < 0 || index >= tl::core::kSkillFunctionSlots) return;
    const int64_t guid = player_skills.second_slots()[static_cast<size_t>(index)];
    if (guid == -1) return;
    const int slot = tl::core::equipment_of_guid(player_inventory, guid);
    if (slot < 0) return;
    load_unit_types();
    // `CGameUI::processIngameInput`, the requested slot:
    // a useable item (`CItem::isUseable`) goes to `useItem`.
    if (tl::core::item_is_useable(player_inventory.slots()[static_cast<size_t>(slot)].item,
                                  unit_types)) {
        if (use_player_item(static_cast<size_t>(slot))) {
            sync_equipped_weapon();
            update_menu();
            refresh_game_hud();
        }
        return;
    }
    // Any other: `equipEquipmentIntoFirstFreeLocation`, then if wearable (`canEquip`) what
    // `getComparisonItems` finds is taken out and the item put on (`Inventory::wear_item`,
    // the tray's). INVENTED: an item already worn is left
    // (`equipEquipmentIntoFirstFreeLocation`, unread).
    tl::core::Inventory::WearOutcome out;
    if (slot >= 12) {
        out = player_inventory.wear_item(static_cast<size_t>(slot), unit_types, &player_character,
                                         &player_effects);
    }
    if (subject_node != nullptr) {
        for (const tl::core::ItemStats& item : out.sounded) {
            play_sound(item.land_sound, subject_node->getPosition());
        }
    }
    for (const tl::core::InventorySlot& dropped : out.dropped) {
        drop_item_near_player(dropped.item, dropped.count);
    }
    if (!out.equipped) {
        // Nothing put on: sample 0x18 and `VOICEIMPOSSIBLE` (0x31) after 0.1 s.
        play_ui(tl::core::InterfaceSound::Error);
        play_voice(tl::core::SoundSlot::VoiceImpossible, 0.1f);
    }
    sync_equipped_weapon();
    update_menu();
    refresh_game_hud();
}

bool Renderer::Impl::cast_player_skill(const tl::core::KnownSkill& skill, bool report) {
    // `CCharacter::castSkill`, its entry tests: a swing
    // with more than 0.2 s left, or a cast still holding: true, nothing done.
    if (swing.active && (swing.length - swing.elapsed) / swing_rate() > 0.2f) return true;
    if (!player_active_skill.empty()) {
        const std::string& name = player_active_skill.skill;
        const tl::core::SkillRunState* active = player_skill_runner.state(name);
        const tl::core::SkillAnimations animations =
            active != nullptr ? active->animations : tl::core::SkillAnimations{};
        if (tl::core::still_casting(player_active_skill, animations,
                                    player_skill_runner.is_running(name))) {
            return true;
        }
    }

    build_player_catalog();
    tl::core::SkillCastContext context = player_cast_context();
    tl::core::SkillCaster caster = player_caster();
    // With no target and no target item, one in the skill's cone; the turn is the
    // character's `update`'s (`step_player_skill_turn`).
    find_player_skill_target(skill);
    aim_position_skill(skill, caster);
    // `castSkill` hands `startSkill` the target (kept, `CSkill +0x40`); with none,
    // `performSkill` hands the target item.
    if (player_target_node != 0) {
        for (tl::core::LevelScript* machine : level_machines) {
            tl::core::Vec3 at;
            if (!machine->unit_position(player_target_node, at)) continue;
            context.event_target.has_unit = true;
            context.event_target.unit = player_target_node;
            context.event_target.machine = machine;
            break;
        }
    } else if (player_skill_item != 0) {
        const int index = breakable_index(player_skill_item);
        if (index >= 0) {
            const BreakableWorld& b = breakables[static_cast<size_t>(index)];
            context.event_target.item = b.id;
            // `getSkillCanBeExecuted(skill, this, 3, +0x228, +0x350)` tests the
            // item as the target, at its node (no `+0x194` read for a breakable).
            tl::core::SkillTargetUnit item;
            const Ogre::Vector3 p = b.node->getPosition();
            item.position = {p.x, p.y, p.z};
            item.is_item = unit_types.isa(unit_types.id_of(stats_for_unit(b.unit).unittype), 2);
            caster.target_unit = item;
        }
    }
    float spent = 0.0f;
    const tl::core::SkillCastResult result =
        player_skill_runner.cast(skill, caster, context, &spent);
    if (result != tl::core::SkillCastResult::Cast) {
        // `castSkill` (`canCastCurrentSkill`):
        // short of mana, `LOWMANA_SOUND` unless already playing (`CSoundBank::bankPlaying`);
        // a player then queues the tip and `VOICEMANALOW_SOUND` 0.1 s on.
        if (result == tl::core::SkillCastResult::NotEnoughMana) {
            if (subject_node != nullptr && !tl::core::audio_voice_playing(kLowManaVoice)) {
                play_sound(player_stats.sounds[tl::core::SoundSlot::LowMana],
                           subject_node->getPosition(), 1.0f, 0.0f, kLowManaVoice);
            }
            queue_tip(tl::core::ContextTip::ManaLow);
            play_voice(tl::core::SoundSlot::VoiceManaLow, 0.1f);
        }
        if (report) {
            TL_LOG_WARN("skills: '%s' not cast: %s (level %u, %.0f mana)", skill.entry.name.c_str(),
                        tl::core::skill_cast_result_name(result), skill.effective,
                        static_cast<double>(caster.mana));
        }
        return false;
    }
    // CCharacter::castSkill: incrementJournalStatistic(8, 1)
    count_journal(tl::core::JournalStatistic::SkillsCast);
    // `CCharacter::modifyMana(-getManaCost())`.
    if (spent > 0.0f) modify_player_mana(-spent);
    // `castSkill`'s animation half: flags reset, `assignSkillAnimations`, rate, cue.
    tl::core::SkillRunState* running = player_skill_runner.state(skill.entry.name);
    if (running != nullptr && skill.def != nullptr) {
        const tl::core::SkillAnimationCue cue = tl::core::begin_skill_animation(
            player_active_skill, skill.entry.name, *skill.current_property(), caster, *running);
        play_skill_cue(cue);
    }
    TL_LOG_INFO(
        "skills: cast '%s' at level %u, %.0f mana spent (%.0f left), cooldown %.2fs, "
        "%zu affix(es) held, %zu effect(s)",
        skill.entry.name.c_str(), skill.effective, static_cast<double>(spent),
        static_cast<double>(player_character.mp()),
        static_cast<double>(running != nullptr ? running->cooldown_remaining : 0.0f),
        player_effects.affixes().size(), player_effects.all_effects().size());
    return true;
}

tl::core::Vec3 Renderer::Impl::pad_skill_point(const tl::core::KnownSkill& skill, float stick_x,
                                               float stick_y, float left_trigger,
                                               float right_trigger) const {
    const tl::core::Vec3 me = player.position();
    // a stick past 0.1 aims along itself at twice its deflection, turned by the
    // camera's heading, height 0; at rest takes position plus forward.
    tl::core::Vec3 point;
    if (0.1f < std::sqrt(stick_x * stick_x + stick_y * stick_y)) {
        const float sin_yaw = std::sin(camera_yaw);
        const float cos_yaw = std::cos(camera_yaw);
        const float sx = stick_x * 2.0f;
        const float sy = stick_y * 2.0f;
        point = {me.x + (sy * sin_yaw - sx * cos_yaw), 0.0f, me.z + (sy * cos_yaw + sx * sin_yaw)};
    } else {
        const float yaw = player.facing();
        point = {me.x + std::sin(yaw), me.y, me.z + std::cos(yaw)};
    }
    // the distance along that line, between `RANGEMIN` (+0x58) and `RANGE`
    // (+0x54) less 0.1 (`CSkill +0x5c`): the LT slot (5 or 8, `CGameUI +0xf88`) by the left
    // trigger's pull, the RT slot (6 or 9) by the right's; else
    // the middle.
    const tl::core::SkillProperty* property = skill.property(skill.effective);
    const float far_end = property != nullptr ? property->range : 1.0f;
    const float near_end = property != nullptr ? property->range_min : 0.0f;
    const size_t set_offset = player_skills.skill_set() != 0 ? 3 : 0;
    const auto in_slot = [&](int slot) {
        return player_skills.slots()[static_cast<size_t>(slot) + set_offset] == skill.guid();
    };
    float distance = (far_end + near_end) * 0.5f;
    if (0.0f < left_trigger && in_slot(5)) {
        distance = (far_end - 0.1f - near_end) * left_trigger + near_end;
    } else if (0.0f < right_trigger && in_slot(6)) {
        distance = (far_end - 0.1f - near_end) * right_trigger + near_end;
    }
    float dx = point.x - me.x;
    float dz = point.z - me.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (1e-8f < length) {
        dx /= length;
        dz /= length;
    }
    return {me.x + dx * distance, me.y, me.z + dz * distance};
}

// `CCharacter::setTargetDirection`.
void Renderer::Impl::set_player_target_direction(const tl::core::Vec3& point) {
    player_turn_point = {point.x, 0.0f, point.z};
    const tl::core::Vec3 me = player.position();
    float dx = player_turn_point.x - me.x;
    float dz = player_turn_point.z - me.z;
    const float length = std::sqrt(dx * dx + 0.0f + dz * dz);
    if (1e-08f < length) {
        dx = dx * (1.0f / length);
        dz = dz * (1.0f / length);
    }
    player_turn_direction = {dx, 0.0f, dz};
}

// `CCharacter::turnTowardPosition` and
// `turnToward`.
void Renderer::Impl::turn_player_toward(const tl::core::Vec3& at, float dt) {
    player_turn_point = {at.x, 0.0f, at.z};
    const tl::core::Vec3 me = player.position();
    float dx = at.x - me.x;
    float dy = at.y - me.y;
    float dz = at.z - me.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (1e-08f < length) {
        dx = dx * (1.0f / length);
        dy = dy * (1.0f / length);
        dz = dz * (1.0f / length);
    }
    // `turnToward` stores nothing in states 5 and 6 or with no turn rate.
    if (player_death.dying || player_stats.turn_rate == 0.0f) return;
    player_turn_direction = {dx, dy, dz};
    player.face(tl::core::turn_toward(player.facing(), dx, dz, player_stats.turn_rate, dt));
}

void Renderer::Impl::step_player_skill_turn(float dt) {
    // taken holding a skill or with the hold position flag (`+0x1fe`).
    if (player_active_skill.empty() && !attack_button.hold) return;
    const std::string& name = player_active_skill.skill;
    const tl::core::KnownSkill* skill = player_skills.find_by_name(name);
    const tl::core::SkillRunState* state = player_skill_runner.state(name);
    const tl::core::SkillProperty* property =
        skill != nullptr ? skill->property(state != nullptr ? state->level : skill->effective)
                         : nullptr;
    // The target (`+0x284`), else the target item (`+0x28c`).
    std::optional<tl::core::Vec3> target;
    if (player_target_node != 0) {
        for (tl::core::LevelScript* machine : level_machines) {
            tl::core::Vec3 at;
            if (!machine->unit_position(player_target_node, at)) continue;
            target = at;
            break;
        }
    } else if (player_skill_item != 0) {
        const int index = breakable_index(player_skill_item);
        if (index >= 0) {
            const Ogre::Vector3 p = breakables[static_cast<size_t>(index)].node->getPosition();
            target = tl::core::Vec3{p.x, p.y, p.z};
        }
    }
    const bool has_def = skill != nullptr && skill->def != nullptr;
    const tl::core::PlayerSkillTurn turn = tl::core::player_skill_turn(
        player_active_skill, state != nullptr ? state->animations : tl::core::SkillAnimations{},
        player_skill_runner.is_running(name), !has_def || skill->def->allows_turning,
        has_def && skill->def->moves_to_target,
        property != nullptr ? property->find_target_angle : 0.0f, target.has_value(),
        attack_button.hold);
    switch (turn) {
        case tl::core::PlayerSkillTurn::None: break;
        case tl::core::PlayerSkillTurn::Aim: {
            const tl::core::Vec3 me = player.position();
            turn_player_toward({me.x + player_turn_direction.x, me.y + player_turn_direction.y,
                                me.z + player_turn_direction.z},
                               dt);
            break;
        }
        case tl::core::PlayerSkillTurn::Target:
            // `dt * 30`. Only ever chosen with a target.
            if (target.has_value()) turn_player_toward(*target, dt * 30.0f);
            break;
        case tl::core::PlayerSkillTurn::Point: turn_player_toward(player_turn_point, dt); break;
    }
}

void Renderer::Impl::aim_position_skill(const tl::core::KnownSkill& skill,
                                        tl::core::SkillCaster& caster) {
    const tl::core::SkillProperty* property = skill.property(skill.effective);
    const int32_t type = property != nullptr
                             ? property->target_type
                             : (skill.def != nullptr ? skill.def->base.target_type : 0);
    // Target types 1 `POSITION`, 11, 5 `POSITIONRANDOM` and 6 `POSITIONFLEE`.
    if (type != 1 && type != 0xb && type != 5 && type != 6) return;
    const tl::core::Vec3 me = player.position();
    // The point stored (x and z, height 0); `--cast` aims at rest.
    tl::core::Vec3 point = pad_skill_target.has_value()
                               ? *pad_skill_target
                               : pad_skill_point(skill, 0.0f, 0.0f, 0.0f, 0.0f);
    point.y = 0.0f;
    // (not Linux's `findWorldLocation`): a ray from 100 above to 100 below;
    // a hit puts the point 0.1 over the ground.
    tl::core::CollisionHit hit;
    if (collision.ray_collision({point.x, point.y + 100.0f, point.z},
                                {point.x, point.y - 100.0f, point.z}, true, hit)) {
        point = hit.point;
        point.y += 0.1f;
    }
    // `setTarget(0)` and the target item let go.
    set_player_target(0);
    const float ground = point.y;
    // Within `RANGE` (`CSkill::getRange`) of the caster on the ground plane.
    tl::core::Vec3 offset{point.x - me.x, 0.0f, point.z - me.z};
    const float range = property != nullptr ? property->range : 1.0f;
    float length = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    if (range < length) {
        offset.x = offset.x / length * range;
        offset.z = offset.z / length * range;
        length = range;
    }
    point = {me.x + offset.x, me.y, me.z + offset.z};
    if (skill.def != nullptr && skill.def->requires_pathable && property != nullptr) {
        // Walked back 0.4 at a time, `length / 0.4 - 2` times, until `CCharacter::canPathTo`
        // answers; then the route's last node, else the
        // caster's position.
        tl::core::Vec3 step{0.0f, 0.0f, 0.0f};
        if (1e-8f < length) step = {offset.x / length * -0.4f, 0.0f, offset.z / length * -0.4f};
        const int tries = static_cast<int>(length / 0.4f) + -2;
        bool found = false;
        for (int i = 0; i < tries; ++i) {
            std::vector<tl::core::Vec3> nodes;
            if (passability.ready() &&
                nav.find_nodes(me, point, passability, /*ignore_objects=*/true, nodes) &&
                !nodes.empty()) {
                const tl::core::Vec3 goal = nav.sample_at(point.x, point.z);
                const tl::core::Vec3 last = nav.sample_at(nodes.back().x, nodes.back().z);
                if (goal.x == last.x && goal.z == last.z) {
                    point.x = nodes.back().x;
                    point.z = nodes.back().z;
                    found = true;
                    break;
                }
            }
            point = {point.x + step.x, point.y + step.y, point.z + step.z};
        }
        if (!found) point = me;
    } else {
        point.y = ground;
    }
    caster.target_position = point;
    // `castSkill` keeps the point in `+0x228` for the skill's turn.
    player_turn_point = point;
    TL_LOG_INFO("skills: '%s' aimed at (%.2f, %.2f, %.2f), %.2f away", skill.entry.name.c_str(),
                static_cast<double>(point.x), static_cast<double>(point.y),
                static_cast<double>(point.z),
                static_cast<double>(std::sqrt((point.x - me.x) * (point.x - me.x) +
                                              (point.z - me.z) * (point.z - me.z))));
}

void Renderer::Impl::step_hotbar(const std::array<bool, 7>& pressed,
                                 const std::array<bool, 7>& held, float stick_x, float stick_y) {
    const auto at = [](const std::array<bool, 7>& state, tl::core::HotbarButton button) {
        return state[static_cast<size_t>(button)];
    };
    const tl::core::HotbarPress press = tl::core::hotbar_press(
        player_skills.slots(), player_skills.second_slots(),
        [this](int64_t guid) { return player_skills.find_by_guid(guid) != nullptr; },
        player_skills.skill_set(),
        [&](tl::core::HotbarButton button) { return at(pressed, button); },
        [&](tl::core::HotbarButton button) { return at(held, button); });
    if (press.slot < 0) return;
    if (!press.skill) {
        // The pending use (`CGameUI +0x149c`), for `CGameUI::activateItemSlot`.
        activate_item_slot(press.slot);
        return;
    }
    const tl::core::KnownSkill* skill =
        player_skills.find_by_guid(player_skills.slots()[static_cast<size_t>(press.slot)]);
    if (skill == nullptr) return;
    // The skill-use call ends, past, in `CCharacter::castSkill`
    // twice, the first result dropped. Every path first runs the stick's aim
    // and `setTarget(0)` (clearing `+0x28c`), so the
    // target set
    // never reaches the cast: `find_player_skill_target` picks in the cone.
    set_player_target(0);
    // The point it aims.
    // DIVERGENCE (platform): ZL and ZR are buttons, so a held trigger is a full pull.
    const auto down = [&](tl::core::HotbarButton button) {
        return pressed[static_cast<size_t>(button)] || held[static_cast<size_t>(button)];
    };
    pad_skill_target =
        pad_skill_point(*skill, stick_x, stick_y, down(tl::core::HotbarButton::LT) ? 1.0f : 0.0f,
                        down(tl::core::HotbarButton::RT) ? 1.0f : 0.0f);
    set_player_target_direction(*pad_skill_target);
    cast_player_skill(*skill, !press.continuing);
    cast_player_skill(*skill, false);
    pad_skill_target.reset();
}

void Renderer::Impl::release_held_skill(const std::array<bool, 7>& pressed,
                                        const std::array<bool, 7>& held, bool face_pressed) {
    // The Xbox in-game dispatch: a face button press skips the test for the
    // frame. `CCharacter::performingSkill`, the dispatch's.
    if (face_pressed || !player_performing_skill()) return;
    const tl::core::KnownSkill* active = player_skills.find_by_name(player_active_skill.skill);
    const int64_t guid = active != nullptr ? active->guid() : -1;
    // The X button's own skill (`CPlayer +0x2d0`) is never active: the port has none on X.
    const auto at = [](const std::array<bool, 7>& state, tl::core::HotbarButton button) {
        return state[static_cast<size_t>(button)];
    };
    if (!tl::core::hotbar_lets_go(
            player_skills.slots(), guid, player_skills.skill_set(),
            [&](tl::core::HotbarButton button) { return at(pressed, button); },
            [&](tl::core::HotbarButton button) { return at(held, button); })) {
        return;
    }
    // The second argument is `+0x55 == 0`: X not down at the last dispatch.
    attempt_to_stop_player_skill(!attack_button.was_down);
}

// PORT: the player's skill asked of `still_casting` (core/skill_cast).
bool Renderer::Impl::player_performing_skill() {
    const std::string& name = player_active_skill.skill;
    const tl::core::SkillRunState* state = player_skill_runner.state(name);
    return tl::core::still_casting(
        player_active_skill, state != nullptr ? state->animations : tl::core::SkillAnimations{},
        player_skill_runner.is_running(name));
}

void Renderer::Impl::attempt_to_stop_player_skill(bool stop_path) {
    // `CPlayer::attemptToStopPlayerSkill(bool)`.
    if (player_active_skill.empty()) return;
    const std::string name = player_active_skill.skill;
    const tl::core::KnownSkill* skill = player_skills.find_by_name(name);
    const tl::core::SkillRunState* state = player_skill_runner.state(name);
    const tl::core::SkillAnimations* animations = state != nullptr ? &state->animations : nullptr;
    const int loop_end = animations != nullptr ? animations->loop_end : -1;
    // `CSkill::getCanStop`: no property, or the run has lasted `MINIMUMTIMEMS`.
    bool can_stop = true;
    if (skill != nullptr && state != nullptr) {
        if (const tl::core::SkillProperty* property = skill->property(state->level)) {
            can_stop = property->minimum_time <= state->elapsed;
        }
    }
    // `CCharacter::attemptStopOfActiveSkill`.
    const tl::core::SkillStop stop =
        tl::core::attempt_stop_of_active_skill(player_active_skill, animations, can_stop);
    const auto lookup = [this](const std::string& n) -> const tl::core::KnownSkill* {
        return player_skill(n);
    };
    if (stop == tl::core::SkillStop::StopByName) {
        const float elapsed = state != nullptr ? state->elapsed : 0.0f;
        player_skill_runner.stop_by_name(name, player_cast_context(), lookup);
        TL_LOG_INFO("skills: '%s' let go after %.2fs", name.c_str(), static_cast<double>(elapsed));
    } else if (stop == tl::core::SkillStop::StopAll) {
        // `stopAllSkills(false, false, false)`.
        player_skill_runner.stop_all(player_cast_context(), lookup, /*force=*/false,
                                     /*passives=*/false);
    }
    if (player_performing_skill()) return;
    // `blendAnimation(loopEnd, 0.2, 1.0, -1)` with `IDLE` queued behind it.
    if (loop_end >= 0 && static_cast<size_t>(loop_end) < player_catalog.size()) {
        tl::core::SkillAnimationCue end;
        end.animation = loop_end;
        end.duration = player_catalog[static_cast<size_t>(loop_end)].second;
        end.then_idle = true;
        play_skill_cue(end);
    }
    // `MOVES_TO_TARGET` (`CSkill +0x68`): `stopPathing`.
    if (stop_path && skill != nullptr && skill->def != nullptr && skill->def->moves_to_target) {
        player.stop_following();
    }
}

void Renderer::Impl::bind_default_item_slots() {
    // `CPlayer::unitInit`: `Health Potion` and `Mana
    // Potion` GUIDs into item slots 0 and 1 (+0x890/+0x898; skill map +0x840/+0x848
    // cleared), the bumpers. The console puts nothing on slot 9; the link map (+0x6a8,
    // +0x6b0) is not kept.
    ensure_item_index();
    const std::pair<int, const char*> defaults[] = {{0, "Health Potion"}, {1, "Mana Potion"}};
    for (const auto& [index, name] : defaults) {
        const tl::core::ItemStats* item = items.find(name);
        if (item != nullptr && item->unit_guid != -1)
            player_skills.set_item_slot(index, item->unit_guid);
    }
}

tl::core::SkillCaster Renderer::Impl::player_caster() const {
    tl::core::SkillCaster caster;
    caster.mana = player_character.mp();
    caster.unit_types = &unit_types;
    caster.effects = &player_effects;
    caster.inventory = &player_inventory;
    // `THEMEREQUIRED` / `NOTTHEMED` ask the caster's themes.
    caster.theme_book = &unit_themes;
    caster.themes = &player_themes.themes;
    // `CCharacter::getWeaponInRightHand` and `InLeftHand`: the two
    // equip locations.
    const std::vector<tl::core::InventorySlot>& slots = player_inventory.slots();
    const auto hand = [&slots](int location) -> std::string {
        if (location < 0 || static_cast<size_t>(location) >= slots.size()) return std::string();
        if (slots[static_cast<size_t>(location)].count <= 0) return std::string();
        return slots[static_cast<size_t>(location)].item.unittype;
    };
    caster.right_hand_type = hand(tl::core::equip_location::kRightHand);
    caster.left_hand_type = hand(tl::core::equip_location::kLeftHand);
    caster.model = player_catalog.empty() ? nullptr : &player_catalog;
    caster.is_character = true;
    // `CPositionableObject::getPosition(true)`: the radius
    // (`+0x194`) above the feet.
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    caster.position = {player.position().x, player.position().y + body, player.position().z};
    // `CAttackDescription +0x10` of each hand's attack (`CEquipment +0x238`, `+0x23c`): the
    // prefix `calculateCombatStats` gives the item.
    if (!caster.right_hand_type.empty()) {
        caster.right_hand_attack_animation =
            tl::core::attack_animation_class(caster.right_hand_type, tl::core::Hand::Right);
    }
    if (!caster.left_hand_type.empty()) {
        caster.left_hand_attack_animation =
            tl::core::attack_animation_class(caster.left_hand_type, tl::core::Hand::Left);
    }
    return caster;
}

// PORT: the class file found by NAME and handed to `parse_class_skills` and
// `SkillBook::set_class`.
void Renderer::Impl::build_player_skills() {
    player_skills = tl::core::SkillBook{};
    player_skills_source.clear();
    if (player_class.empty()) return;
    ensure_player_skill_files();
    const std::string want = tl::core::upper_ascii(player_class);
    tl::formats::AdmFile class_file;
    for (const char* root : {"xbox/media/units/players/", "media/units/players/"}) {
        for (const std::string& path : archive_paths_under(root, ".dat.adm")) {
            const std::vector<uint8_t> bytes = read_resource(path);
            if (bytes.empty()) continue;
            tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (!file.ok || tl::core::upper_ascii(file.string_or("NAME", "")) != want) continue;
            class_file = std::move(file);
            player_skills_source = path;
            break;
        }
        if (!player_skills_source.empty()) break;
    }
    if (player_skills_source.empty()) {
        TL_LOG_WARN("player skills: no class file names '%s'", player_class.c_str());
        return;
    }
    const std::vector<tl::core::ClassSkill> entries = tl::core::parse_class_skills(class_file);
    const std::array<std::string, 3> tab_names = tl::core::parse_skill_tab_names(class_file);
    size_t missing = 0;
    player_skills.set_class(entries, [&](const std::string& name) -> const tl::core::SkillDef* {
        const auto it = player_skill_files.find(tl::core::upper_ascii(name));
        if (it != player_skill_files.end()) return &it->second->def;
        ++missing;
        TL_LOG_WARN("player skills: no skill file is called '%s'", name.c_str());
        return nullptr;
    });
    player_skills.tab_names = tab_names;
    TL_LOG_INFO("player skills: %zu from %s (%zu without a file)", entries.size(),
                player_skills_source.c_str(), missing);
}

void Renderer::Impl::handle_skills_tab(const tl::core::InputState& in, const SkillsTabPad& pad) {
    using tl::core::PadButton;
    using tl::core::SkillsTabCommand;
    const bool a = in.confirm && !confirm_held;
    const bool b = in.skip && !skip_held;
    const bool b_up = !in.skip && skip_held;
    bool changed = false;

    // The presses, as `GamepadButtonDown` hands them; the sticks' four
    // directions are buttons (codes 17-24), one per push.
    std::vector<PadButton> presses;
    if (pad.dpad_up) presses.push_back(PadButton::DPadUp);
    if (pad.dpad_down) presses.push_back(PadButton::DPadDown);
    if (a) presses.push_back(PadButton::A);
    if (b) presses.push_back(PadButton::B);
    if (pad.x) presses.push_back(PadButton::X);
    if (pad.y) presses.push_back(PadButton::Y);
    if (pad.rt) presses.push_back(PadButton::RightTrigger);
    if (pad.lt) presses.push_back(PadButton::LeftTrigger);
    if (pad.start) presses.push_back(PadButton::Start);
    {
        float sx = in.stick_x;
        float sy = in.stick_y;
        bool right_stick = false;
        if (std::abs(sx) < 0.5f && std::abs(sy) < 0.5f) {
            sx = in.look_x;
            sy = in.zoom;
            right_stick = true;
        }
        const bool pushed = std::abs(sx) > 0.5f || std::abs(sy) > 0.5f;
        if (pushed && !skill_stick_held) {
            PadButton direction;
            if (std::abs(sy) >= std::abs(sx)) {
                direction = sy > 0.0f ? PadButton::LeftStickUp : PadButton::LeftStickDown;
            } else {
                direction = sx > 0.0f ? PadButton::LeftStickRight : PadButton::LeftStickLeft;
            }
            if (right_stick) {
                direction = static_cast<PadButton>(static_cast<int>(direction) + 4);
            }
            presses.push_back(direction);
        }
        skill_stick_held = pushed;
    }

    for (PadButton button : presses) {
        if (!inventory_menu.visible() || inventory_menu.menu_mode() != MenuMode::Skills) break;
        const bool sheet = inventory_menu.skill_map_open();
        tl::core::KnownSkill* skill = inventory_menu.selected_skill(player_skills);
        const bool has_sheet = skill != nullptr && skill->effective != 0 && !skill->passive();
        const tl::core::SkillsTabStep step = tl::core::skills_tab_button_down(
            button, sheet, inventory_menu.skill_cursor().in_spells, has_sheet);
        switch (step.command) {
            case SkillsTabCommand::None: break;
            case SkillsTabCommand::SkillSet:
                // with the skills menu open: no gate.
                if (tl::core::switch_skill_set(player_skills, step.value, true) ==
                    tl::core::SkillSetSwitch::Switched) {
                    play_ui(tl::core::InterfaceSound::SkillAssign);
                    refresh_game_hud();
                }
                changed = true;
                break;
            case SkillsTabCommand::Learn:
                if (skill != nullptr &&
                    player_skills.learn(*skill, static_cast<uint32_t>(player_character.level()),
                                        player_character.unspent_skill_points_ref())) {
                    // Slot 0x1b, `POINTASSIGN`.
                    play_ui(tl::core::InterfaceSound::PointAssign);
                    TL_LOG_INFO("skills: '%s' to level %u, %d point(s) left",
                                skill->entry.name.c_str(), skill->invested,
                                player_character.unspent_skill_points());
                    // `CSkillManager::setSkillLevel` ends
                    // in `startPassiveSkills`: a passive
                    // point takes effect at once.
                    start_player_passives();
                    refresh_game_hud();
                    changed = true;
                }
                break;
            case SkillsTabCommand::OpenSheet:
                inventory_menu.set_skill_map_open(true);
                changed = true;
                break;
            case SkillsTabCommand::CloseSheet:
                inventory_menu.set_skill_map_open(false);
                changed = true;
                break;
            case SkillsTabCommand::SheetPress:
                // dims press frames the console sheet lacks: just the legend.
                changed = true;
                break;
            case SkillsTabCommand::CloseMenu: close_master_menu(); return;
            case SkillsTabCommand::NextTab: step_master_menu_tab(1); return;
            case SkillsTabCommand::PrevTab: step_master_menu_tab(-1); return;
            case SkillsTabCommand::Cursor:
                // with `NAVIGATE` (slot 0x43) on every step.
                switch (step.value) {
                    case 3: inventory_menu.skill_cursor_up(player_skills); break;
                    case 4: inventory_menu.skill_cursor_down(player_skills); break;
                    case 5: inventory_menu.skill_cursor_right(player_skills); break;
                    default: inventory_menu.skill_cursor_left(player_skills); break;
                }
                play_ui(tl::core::InterfaceSound::Navigate);
                changed = true;
                break;
        }
    }

    // The releases, as `GamepadButtonUp` hands them, in its order.
    const std::pair<bool, PadButton> releases[] = {
        {pad.y_up, PadButton::Y},
        {pad.rt_up, PadButton::RightTrigger},
        {b_up, PadButton::B},
        {pad.lt_up, PadButton::LeftTrigger},
    };
    for (const auto& [released, code] : releases) {
        if (!released) continue;
        if (!inventory_menu.visible() || inventory_menu.menu_mode() != MenuMode::Skills) break;
        const std::optional<tl::core::SkillButton> button =
            tl::core::skills_sheet_button_up(code, inventory_menu.skill_map_open());
        if (!button.has_value()) continue;
        tl::core::KnownSkill* skill = inventory_menu.selected_skill(player_skills);
        if (skill != nullptr && player_skills.map(*skill, *button, player_skills.skill_set())) {
            // slot 0x1e, `ASSIGNSKILL`; then the sheet closes.
            play_ui(tl::core::InterfaceSound::SkillAssign);
            TL_LOG_INFO("skills: '%s' mapped to function slot %d", skill->entry.name.c_str(),
                        tl::core::skill_button_slot(*button, player_skills.skill_set()));
            inventory_menu.set_skill_map_open(false);
            refresh_game_hud();
        }
        // Refused, the sheet stays and the legend is refreshed.
        changed = true;
    }
    if (changed) update_menu();
}

// PORT: the port's index of the monster files, by NAME (archive lookup).
void Renderer::Impl::ensure_monster_index() {
    if (monster_files_loaded) return;
    monster_files_loaded = true;
    for (const std::string& path : archive_paths_under("media/units/monsters/", ".dat.adm")) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) continue;
        const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (!file.ok || file.section() != "UNIT") continue;
        const std::string name = file.string_or("NAME", "");
        if (!name.empty()) monster_files.emplace(lowered(name), path);
    }
    TL_LOG_INFO("unit index: %zu monster unit(s)", monster_files.size());
}

void Renderer::Impl::ensure_prop_index() {
    if (prop_files_loaded) return;
    prop_files_loaded = true;
    for (const std::string& path : archive_paths_under("media/units/props/", ".dat.adm")) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) continue;
        const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (!file.ok || file.section() != "UNIT") continue;
        const std::string name = file.string_or("NAME", "");
        if (!name.empty()) prop_files.emplace(lowered(name), path);
    }
    TL_LOG_INFO("prop index: %zu prop unit(s)", prop_files.size());
}

/// Resolve a `Unit Trigger`'s `MODEL` to the mesh, collision and states its unit file
/// authors: a `CTriggerUnit` (`getUnitModel()` / `getUnitCollisionModel()`;
/// `unitInit`, reads `MAXSTATES`
/// and `ANIMATION_STATE_n`). A unit with no `MESHFILE` draws nothing; `BASEFILE` is not
/// followed (props' base authors no mesh). Cached per unit name.
/// `CUnitTriggerDescriptor::GetTriggerModelStringByID`.
void Renderer::Impl::finish_trigger_unit(int64_t node, const std::string& unit,
                                         const TriggerProp& prop, bool by_player,
                                         const std::vector<const Ogre::SceneNode*>& mine,
                                         bool treasure, bool skill) {
    // Where the prop stands: the firing machine's copy of it, else nothing.
    Ogre::Vector3 at = Ogre::Vector3::ZERO;
    Ogre::Quaternion facing = Ogre::Quaternion::IDENTITY;
    bool found = false;
    const auto units = scene_units.equal_range(unit);
    for (auto it = units.first; it != units.second && !found; ++it) {
        if (it->second.node != node || it->second.entity == nullptr) continue;
        if (const Ogre::SceneNode* sn = it->second.entity->getParentSceneNode()) {
            if (!mine.empty() && std::find(mine.begin(), mine.end(), sn) == mine.end()) continue;
            at = sn->_getDerivedPosition();
            facing = sn->_getDerivedOrientation();
            found = true;
        }
    }
    if (!found) return;

    // `CTriggerUnit::trigger`, a one-shot reaching
    // `MAXSTATES`: `+0x1f0` cleared; a `[TREASURE]` class rolls
    // `randomIntegerBetweenVolatile(MINCOUNT, MAXCOUNT)` through `spawnTreasureClass`.
    //
    if (treasure && !prop.treasure_class.empty()) {
        // CTriggerUnit::trigger: incrementJournalStatistic(9, 1)
        if (by_player) count_journal(tl::core::JournalStatistic::ChestsOpened);
        const int32_t count =
            tl::core::volatile_random().integer_between(prop.treasure_min, prop.treasure_max);
        if (!loot_tables_built) {
            loot_tables_built = true;
            build_spawn_context(loot_tables, /*items_by_type=*/true);
        }
        const tl::core::SpawnContext& context = loot_tables.context;
        const tl::core::SpawnClass* treasure =
            context.get_spawn_class ? context.get_spawn_class(prop.treasure_class) : nullptr;
        if (treasure == nullptr) {
            TL_LOG_WARN("prop: %s names no spawn class '%s'", unit.c_str(),
                        prop.treasure_class.c_str());
        } else {
            tl::core::LootCharacter opener;
            const tl::core::LootCharacter* who = by_player ? loot_character(0, opener) : nullptr;
            const tl::core::LootPlacement place = loot_placement_at(at);
            const std::vector<tl::core::ItemStats> made = tl::core::create_units_at_position(
                *treasure, static_cast<uint32_t>(std::max(0, count)),
                who != nullptr ? &who->spawn : nullptr, nullptr, level_monster_level,
                tl::core::volatile_random(), tl::core::game_random(), context, loot_unit_maker(),
                &place);
            std::string names;
            for (const tl::core::ItemStats& item : made) {
                names += (names.empty() ? "" : ", ") + item.name;
            }
            TL_LOG_INFO("prop: %s opens on %zu item(s) from '%s' [%s]", unit.c_str(), made.size(),
                        prop.treasure_class.c_str(), names.c_str());
        }
    }

    // Then `CBaseUnit::selectRandomSkill`, executed at
    // the prop's position lowered by `+0x194`.
    if (!skill) return;
    const int32_t pick = tl::core::select_random_skill(static_cast<uint32_t>(prop.skills.size()),
                                                       tl::core::game_random());
    if (pick >= 0 && static_cast<size_t>(pick) < prop.skills.size()) {
        cast_prop_trap(unit, prop.skills[static_cast<size_t>(pick)],
                       prop.skill_levels[static_cast<size_t>(pick)],
                       at - Ogre::Vector3(0.0f, prop.collision_radius, 0.0f), facing, by_player);
    }
}

bool Renderer::Impl::trigger_prop(const std::string& unit, TriggerProp& out) {
    const std::string key = lowered(unit);
    const auto cached = trigger_props.find(key);
    if (cached != trigger_props.end()) {
        out = cached->second;
        return !out.mesh.empty();
    }

    TriggerProp prop;
    ensure_prop_index();
    const auto found = prop_files.find(key);
    if (found != prop_files.end()) {
        const std::vector<uint8_t> bytes = read_resource(found->second);
        if (!bytes.empty()) {
            const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (file.ok) {
                std::string directory = file.string_or("RESOURCEDIRECTORY", "");
                const std::string mesh_file = file.string_or("MESHFILE", "");
                const std::string collision_file = file.string_or("COLLISIONFILE", "");
                for (char& c : directory) {
                    if (c == 92) c = '/';
                }
                if (!directory.empty() && directory.back() != '/') directory.push_back('/');
                if (!directory.empty() && !mesh_file.empty()) {
                    prop.mesh = directory + mesh_file + ".MESH";
                }
                // A collision model needs no drawn one (`generic_trigger_collision_01`).
                if (!directory.empty() && !collision_file.empty()) {
                    prop.collision = directory + collision_file + ".MESH";
                }
                // `CTriggerUnit::addAnimations()`: a
                // state's `ANIMATION_STATE_LOOP_<n>` (looping, `+0x280`), else
                // `ANIMATION_STATE_<n>`; the transition only when that gave one.
                // INVENTED: names, looked up when played; the original keeps indices
                // (`+0x268`, `+0x298`), exact case, index 0 for a missing one.
                prop.resting = file.string_or("ANIMATION_STATE_1", "");
                prop.resting_loop = file.string_or("ANIMATION_STATE_LOOP_1", "");
                prop.activated = file.string_or("ANIMATION_STATE_2", "");
                prop.activated_loop = file.string_or("ANIMATION_STATE_LOOP_2", "");
                if (!prop.activated.empty() || !prop.activated_loop.empty()) {
                    prop.opening = file.string_or("ANIMATION_TRANSITION_STATE_2", "");
                }
                if (!prop.resting.empty() || !prop.resting_loop.empty()) {
                    prop.closing = file.string_or("ANIMATION_TRANSITION_STATE_1", "");
                }
                prop.found = true;
                prop.max_states = static_cast<int32_t>(file.integer_or("MAXSTATES", 2));
                {
                    const std::string loop = tl::core::upper_ascii(file.string_or("LOOPTYPE", ""));
                    static const char* const kLoopTypes[] = {"NO LOOP", "CYCLE", "BACK AND FORTH"};
                    for (int32_t i = 0; i < 3; ++i) {
                        if (loop == kLoopTypes[i]) {
                            prop.loop_type = i;
                            break;
                        }
                    }
                }
                {
                    // `CTriggerUnit::addSoundsToState(state, key)`:
                    // the key's sound, else `INTERACT_SOUND`.
                    const std::string interact = file.string_or("INTERACT_SOUND", "");
                    prop.state_sound[0] = file.string_or("SOUND_STATE_ONE", "");
                    prop.state_sound[1] = file.string_or("SOUND_STATE_TWO", "");
                    for (std::string& sound : prop.state_sound) {
                        if (sound.empty()) sound = interact;
                    }
                }
                prop.collideable = file.bool_or("COLLIDEABLE", true);
                prop.block = file.bool_or("BLOCK", false);
                prop.occupies_nodes = file.bool_or("OCCUPIESNODES", true);
                prop.shadows = file.bool_or("SHADOWS", true);
                prop.state_layout[0] = file.string_or("LAYOUT_STATE_1", "");
                prop.state_layout[1] = file.string_or("LAYOUT_STATE_2", "");
                prop.display_name = tl::core::strip_markup(file.string_or("DISPLAYNAME", ""));
                if (prop.display_name.empty()) prop.display_name = file.string_or("NAME", "ITEM");
                prop.unittype = file.string_or("UNITTYPE", "");
                prop.collision_radius = file.float_or("COLLISION_RADIUS", 0.3f);
                prop.scale = file.float_or("SCALE", 1.0f);
                prop.scale_variation = file.float_or("SCALE_VARIATION", 0.0f);
                for (const tl::formats::AdmSection& group : file.root.children) {
                    if (group.name == "TREASURE") {
                        prop.treasure_class = group.string_or("SPAWNCLASS", "");
                        prop.treasure_min = static_cast<int32_t>(group.integer_or("MINCOUNT", 1));
                        prop.treasure_max = static_cast<int32_t>(group.integer_or("MAXCOUNT", 1));
                    } else if (group.name == "SKILL") {
                        const std::string skill = group.string_or("NAME", "");
                        if (skill.empty()) continue;
                        prop.skills.push_back(skill);
                        prop.skill_levels.push_back(
                            static_cast<uint32_t>(group.integer_or("LEVEL", 1)));
                    }
                }
            }
        }
    }
    trigger_props.emplace(key, prop);
    out = prop;
    return !prop.mesh.empty();
}

std::string Renderer::Impl::prop_layout_path(const std::string& unit) {
    ensure_prop_index();
    const auto found = prop_files.find(lowered(unit));
    if (found == prop_files.end()) return std::string();

    const std::vector<uint8_t> bytes = read_resource(found->second);
    if (bytes.empty()) return std::string();
    const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!file.ok) return std::string();

    std::string directory = file.string_or("RESOURCEDIRECTORY", "");
    const std::string layout_file = file.string_or("LAYOUTFILE", "");
    if (directory.empty() || layout_file.empty()) return std::string();
    for (char& c : directory) {
        if (c == '\\') c = '/';
    }
    if (directory.back() != '/') directory.push_back('/');
    return directory + layout_file + ".LAYOUT";
}

// PORT: a unit whose body is a layout placed through `place_layout`.
bool Renderer::Impl::place_layout_bodied_unit(const std::string& unit, const Ogre::Vector3& at) {
    const std::string path = prop_layout_path(unit);
    if (path.empty()) return false;

    // Depth 1: its own node map, and with no owning link it arms its own machine.
    if (!place_layout(path, at, Ogre::Quaternion::IDENTITY, /*depth=*/1)) {
        return false;
    }
    TL_LOG_INFO("script: '%s' placed as a layout at (%.2f, %.2f, %.2f)", unit.c_str(),
                static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z));
    return true;
}

std::vector<Ogre::SceneNode*> Renderer::Impl::place_particle_layout(const std::string& reference,
                                                                    const Ogre::Vector3& at,
                                                                    const Ogre::Quaternion& facing,
                                                                    Ogre::SceneNode* parent,
                                                                    ParticleStart start) {
    std::vector<Ogre::SceneNode*> holders;
    if (reference.empty()) return holders;
    std::string path = reference;
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    const size_t before = particle_systems.size();
    const bool riding = parent != nullptr;
    // Depth 1: a particle file, not a level.
    const bool outer_particle_file = placing_particle_file;
    placing_particle_file = true;
    cparticle_places_next_layout = start != ParticleStart::Live;
    const bool placed = place_layout(path, riding ? Ogre::Vector3::ZERO : at,
                                     riding ? Ogre::Quaternion::IDENTITY : facing, 1);
    cparticle_places_next_layout = false;
    placing_particle_file = outer_particle_file;
    if (!placed) return holders;
    arm_particle_timer(before, particle_systems.size());
    for (size_t i = before; i < particle_systems.size(); ++i) {
        ParticleSystem& system = particle_systems[i];
        Ogre::SceneNode* holder = system.holder();
        if (holder == nullptr || system.claimed) continue;
        system.claimed = true;
        if (std::find(holders.begin(), holders.end(), holder) != holders.end()) continue;
        if (riding) {
            if (Ogre::Node* up = holder->getParent()) up->removeChild(holder);
            parent->addChild(holder);
        }
        holders.push_back(holder);
    }
    if (start != ParticleStart::Live) {
        // The `CParticle`'s node: the parent it rides, or `at`.
        const Ogre::Vector3 origin = riding ? parent->_getDerivedPosition() : at;
        const uint32_t id =
            register_cparticle(path, before, origin, [&holders](const ParticleSystem& system) {
                return std::find(holders.begin(), holders.end(), system.holder()) != holders.end();
            });
        if (start == ParticleStart::Now) {
            if (id != 0) {
                cparticle_started(id);
            } else if (const ParticleFileExtras* extras = particle_extras(path)) {
                play_particle_extras(*extras, origin, path);
            }
        }
    }
    return holders;
}

const Renderer::Impl::ParticleFileExtras* Renderer::Impl::particle_extras(
    const std::string& reference) {
    std::string path = resolve_archive_path(reference);
    if (path.empty()) path = resolve_archive_path(reference + ".cmp");
    if (path.empty()) return nullptr;
    const auto found = particle_file_extras.find(path);
    if (found != particle_file_extras.end()) return &found->second;
    ParticleFileExtras& extras = particle_file_extras[path];
    const tl::formats::CmpScene* const layout = layout_file(path);
    if (layout == nullptr || !layout->ok) return &extras;

    // `CParticlePreloader::LoadParticle`: the file's
    // `Sound` objects whose GUID names a definition, each at its node's `VOLUME`
    // (`CSoundObjectDescriptor::Set_setVolume`) or 1. A looping one is refused.
    ensure_sound_index();
    namespace type = tl::formats::cmp_type;
    for (const tl::formats::CmpNode& node : layout->nodes) {
        if (node.type != type::kSound) continue;
        namespace f = tl::formats::cmp_field::sound;
        const tl::formats::CmpField* guid = node.field(f::kGuid);
        if (guid == nullptr || guid->as_text().empty() || guid->as_text() == "-1") continue;
        const SoundDefinition* definition = sound_by_guid(guid->as_text());
        if (definition == nullptr) continue;
        if (definition->loops) {
            // The original's own line (`logMessage(..., 3)`), once per cached file; the
            // shipped data has such files.
            TL_LOG_INFO("Particles can't have sounds that loop. : %s", path.c_str());
            continue;
        }
        ParticleFileExtras::Sound sound;
        sound.guid = guid->as_text();
        if (const tl::formats::CmpField* volume = node.field(f::kVolume);
            volume != nullptr && volume->count() == 1) {
            sound.volume = volume->as_float();
        }
        extras.sounds.push_back(sound);
    }

    // Its `Camera Shake` objects, cloned (`CCameraShake::clone`):
    // `CCameraShakeDescriptor`'s setters over the constructor's
    // defaults.
    for (const tl::formats::CmpNode& node : layout->nodes) {
        if (node.type != type::kCameraShake) continue;
        load_camera_shake_graphs();
        extras.shakes.push_back(tl::core::camera_shake_from_node(node, camera_shake_graphs));
    }
    return &extras;
}

uint32_t Renderer::Impl::register_cparticle(
    const std::string& reference, size_t first, const Ogre::Vector3& origin,
    const std::function<bool(const ParticleSystem&)>& take) {
    uint32_t id = 0;
    for (size_t i = first; i < particle_systems.size(); ++i) {
        ParticleSystem& system = particle_systems[i];
        if (system.cparticle != 0 || !take(system)) continue;
        Ogre::SceneNode* holder = system.holder();
        if (holder == nullptr) continue;
        if (id == 0) {
            if (++next_placed_particle == 0) ++next_placed_particle;
            id = next_placed_particle;
            PlacedParticle placed;
            placed.extras = particle_extras(reference);
            placed.reference = reference;
            placed.from_holder = holder->_getFullTransform().inverse() * origin;
            placed_particles[id] = placed;
        }
        system.cparticle = id;
    }
    return id;
}

// `CLevel::updateVisibleParticles(Ogre::Camera*, const Ogre::Vector3&, bool)`,
// every frame with the camera's focus and `getIsPaused()`.
void Renderer::Impl::update_visible_particles(bool paused) {
    if (listed_particles == 0 || camera == nullptr) return;
    const Ogre::Viewport* const view = camera->getViewport();
    if (view == nullptr) return;
    const Ogre::Vector3 focus = game_camera.focus;
    // With a player, a pass only once the focus has moved 0.25, or on a paused frame.
    if (player_control) {
        if (!paused) {
            const float dx = focus.x - visible_particles_last.x;
            const float dy = focus.y - visible_particles_last.y;
            const float dz = focus.z - visible_particles_last.z;
            if (std::sqrt(std::fma(dx, dx, std::fma(dz, dz, dy * dy))) < 0.25f) return;
        }
        visible_particles_last = focus;
    }
    // Projection times view, and the camera's up, which `getScreenPosition` adds.
    const Ogre::Matrix4 clip = camera->getProjectionMatrix() * camera->getViewMatrix();
    const Ogre::Vector3 up = camera->getDerivedOrientation().yAxis();
    // `CGameUI::getWindowWidth` and `getAspectRatio`.
    const float width = static_cast<float>(view->getActualWidth());
    const float aspect = width / static_cast<float>(view->getActualHeight());
    // 45 with `NETBOOK MODE`, which the port's settings do not hold.
    const float depth_limit = 90.0f;

    size_t shown = 0;
    size_t hidden = 0;
    for (auto& [id, placed] : placed_particles) {
        if (!placed.listed) continue;
        // The link's `CParticle` (`+0x1f0`); a `NO CULLING` one (`+0x127`) never joined.
        // INVENTED: links are all enabled (the original skips disabled ones, vtable +0x48).
        const ParticleSystem* first = nullptr;
        for (const ParticleSystem& system : particle_systems) {
            if (system.cparticle == id) {
                first = &system;
                break;
            }
        }
        if (first == nullptr) continue;
        const Ogre::Vector3 at = first->link_position;

        // `CGameUI::getScreenPosition(position, up, matrix)`:
        // one `up` above, through the matrix with its divide by w, X and Y over Z,
        // X to the window times the share no menu covers (`+0x14a0`).
        Ogre::Vector3 screen = clip * (at + up);
        if (screen.z != 0.0f) {
            const float inverse = 1.0f / screen.z;
            screen.x = screen.x * inverse;
            screen.y = screen.y * inverse;
            screen.z = screen.z * inverse;
        }
        const float half = width * 0.5f;
        const float across = std::fma(width, 0.5f, half * screen.x);

        const float dx = at.x - focus.x;
        const float dz = at.z - focus.z;
        const float away = std::sqrt(std::fma(dz, dz, dx * dx));

        const bool on_screen = !(across < width * -0.5f) && !(across > width * 1.5f) &&
                               !(screen.z < 0.0f) && screen.z <= depth_limit;
        const bool show = on_screen || away < aspect * 15.0f || paused;
        if (show == placed.visible) continue;
        placed.visible = show;
        ++(show ? shown : hidden);
        // `CLayout::setVisible`: the flag changed, so
        // `Start` or `Stop(false)`.
        for (ParticleSystem& system : particle_systems) {
            if (system.cparticle != id) continue;
            if (show) {
                restart_particle_system(system);
                // `CParticle::forceParticleUpdate(1.0)`, on a paused pass.
                if (paused) force_torchlight_particle_update(system.pu, 1.0f);
            } else {
                stop_particle_emission(system);
            }
        }
        if (show) cparticle_started(id);
    }
    if (shown + hidden > 0) {
        TL_LOG_INFO("particles: %zu link(s) shown, %zu hidden, of %zu the level lists", shown,
                    hidden, listed_particles);
    }
}

void Renderer::Impl::cparticle_started(uint32_t id) {
    const auto found = placed_particles.find(id);
    if (found == placed_particles.end() || found->second.extras == nullptr) return;
    const PlacedParticle& placed = found->second;
    for (const ParticleSystem& system : particle_systems) {
        if (system.cparticle != id) continue;
        Ogre::SceneNode* holder = system.holder();
        if (holder == nullptr) continue;
        play_particle_extras(*placed.extras, holder->_getFullTransform() * placed.from_holder,
                             placed.reference);
        return;
    }
}

void Renderer::Impl::play_particle_extras(const ParticleFileExtras& extras, const Ogre::Vector3& at,
                                          const std::string& reference) {
    // `CParticleCache::playSounds`: each sound in order on the preloader's bank,
    // radius the definition's or the bank's 10 (`CSoundBank`).
    for (const ParticleFileExtras::Sound& sound : extras.sounds) {
        // `getSoundDataObjectByGuid`, played by its name.
        const SoundDefinition* named = sound_by_guid(sound.guid);
        if (named == nullptr) continue;
        play_sound(named->name, at, sound.volume, 10.0f, std::string(),
                   /*volume_given=*/sound.volume != 0.0f);
    }
    // `CParticleCache::playCameraShakes`: `startCameraShake(position, false)` on
    // each. DIVERGENCE: the camera owns a copy where the original lends the cache's object
    // (plumbing: no shake here outlives the call).
    if (!extras.shakes.empty()) load_camera_shake_graphs();
    for (const tl::core::CameraShake& authored : extras.shakes) {
        tl::core::CameraShake shake;
        shake.clone_from(authored, camera_shake_graphs);
        shake.start(tl::core::Vec3f{at.x, at.y, at.z}, /*owned=*/true, camera_shakes);
        TL_LOG_INFO("camera: %s shakes '%s'%s", reference.c_str(), shake.name.c_str(),
                    shake.curve != nullptr ? "" : " (no curve; nothing moves)");
    }
}

void Renderer::Impl::forget_dead_cparticles() {
    // A placement leaves no trace of its own going: its systems do.
    if (placed_particles.size() < 256) return;
    std::set<uint32_t> live;
    for (const ParticleSystem& system : particle_systems) {
        if (system.cparticle != 0) live.insert(system.cparticle);
    }
    for (auto it = placed_particles.begin(); it != placed_particles.end();) {
        if (live.count(it->first) != 0) {
            ++it;
            continue;
        }
        if (it->second.listed && listed_particles > 0) --listed_particles;
        it = placed_particles.erase(it);
    }
}

// `CTriggerUnit::setTriggerState`, its walk of `+0x2b0`:
// the current state's layout shown and started, the others hidden and stopped; `unitInit`
// parents each to the unit's node.
void Renderer::Impl::set_trigger_state_layout(Ogre::Entity* entity, const TriggerProp& prop,
                                              int state) {
    if (entity == nullptr || state < 1 || state > 2) return;
    Ogre::SceneNode* node = entity->getParentSceneNode();
    if (node == nullptr) return;
    TriggerStateLayout& held = trigger_state_layouts[entity];
    if (held.state == state) return;
    destroy_particle_holders(held.holders);
    held.holders.clear();
    held.state = state;
    const std::string& file = prop.state_layout[state - 1];
    if (file.empty()) return;
    // `unitInit` loads each as a kind-2 `CLayout` and, with no `Timeline`, as kind 1.
    const tl::formats::CmpScene* const parsed = cached_layout(file);
    const bool timed = parsed != nullptr && parsed->ok && !parsed->timelines.empty();
    const ParticleStart start =
        !timed && renderer_detail::caching_particles() ? ParticleStart::Now : ParticleStart::Live;
    held.holders =
        place_particle_layout(file, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY, node, start);
    // A prop the level has hidden shows its layout when it is shown itself.
    if (!entity->getVisible()) {
        for (Ogre::SceneNode* holder : held.holders) holder->setVisible(false);
    }
    TL_LOG_INFO("unit trigger: state %d layout %s, %zu system node(s)", state, file.c_str(),
                held.holders.size());
}

void Renderer::Impl::destroy_particle_holders(std::vector<Ogre::SceneNode*>& holders) {
    for (Ogre::SceneNode* holder : holders) {
        if (holder == nullptr) continue;
        for (size_t i = 0; i < particle_systems.size();) {
            ParticleSystem& system = particle_systems[i];
            if (system.pu.system != nullptr && system.holder() == holder) {
                // The holder is the system's own node, destroyed below.
                system.pu.node = nullptr;
                destroy_torchlight_particle(scene, system.pu);
                particle_systems.erase(particle_systems.begin() + static_cast<long>(i));
            } else {
                ++i;
            }
        }
        // No node map may keep a placed node once it is gone (`node_sink`).
        const auto forget = [holder](NodeMap& nodes) {
            for (auto it = nodes.begin(); it != nodes.end();) {
                it = it->second == holder ? nodes.erase(it) : std::next(it);
            }
        };
        forget(scripted_nodes);
        for (NestedScript& machine : nested_scripts) forget(machine.nodes);
        scripted_scene_nodes.erase(holder);
        scene->destroySceneNode(holder);
    }
    holders.clear();
}

const tl::formats::CmpScene* Renderer::Impl::cached_layout(const std::string& reference) {
    std::string key = tl::core::upper_ascii(reference);
    for (char& c : key) {
        if (c == '\\') c = '/';
    }
    const auto found = cached_layouts.find(key);
    if (found != cached_layouts.end()) return found->second;
    std::string path = resolve_archive_path(key);
    if (path.empty()) path = resolve_archive_path(key + ".cmp");
    const tl::formats::CmpScene* out = path.empty() ? nullptr : layout_file(path);
    cached_layouts.emplace(key, out);
    return out;
}

void Renderer::Impl::start_skill_event(const tl::core::KnownSkill& skill, uint32_t level, int group,
                                       const tl::formats::AdmSection& event,
                                       const tl::core::SkillEventTarget& target) {
    const tl::core::SkillEventSpec spec = tl::core::read_skill_event(event);
    // The event's caster: the player, an `EXECUTE_SKILL`'s (`event_caster_override`), or a
    // placed unit casting its own skill (`unit_event_caster`).
    const bool unit_cast = unit_event_caster.machine != nullptr;
    Ogre::SceneNode* const caster_node = unit_cast ? unit_event_caster.scene
                                         : event_caster_override != nullptr ? event_caster_override
                                                                            : subject_node;
    if (caster_node == nullptr || skill.def == nullptr) return;
    // The caster's body radius (`+0x194`).
    const float caster_body = unit_cast                    ? unit_event_caster.radius
                              : player_stats.radius > 0.0f ? player_stats.radius
                                                           : tl::core::kDefaultCollisionRadius;
    const float player_body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    std::string reference = spec.file;
    for (char& c : reference) {
        if (c == '\\') c = '/';
    }

    SkillEventSource source;
    source.skill = skill.entry.name;
    source.level = level;
    source.event = &event;
    const tl::core::SkillProperty* property = skill.property(level);
    // The event's property (`CSkillEvent +0x4c`); an inherited event's is the parent's.
    //
    source.property =
        property != nullptr && property->section != nullptr ? &property->event_section() : nullptr;
    source.duration_override = property != nullptr ? property->events_duration_override : 0.0f;
    source.target_type = property != nullptr ? property->target_type : skill.def->base.target_type;
    // The blocks were made with the skill (`kSkillBlockTargetDefault`).
    source.unauthored = tl::core::kSkillBlockTargetDefault;
    source.target_alignment = skill.def->target_alignment;
    source.spec = spec;
    source.group = group;
    source.caster_machine = unit_event_caster.machine;
    source.caster_node = unit_event_caster.node;
    source.run = target.run;
    // `startEvent`: `+0x15c = +0x158`, the block's
    // `DURATIONOVERRIDEMS` in seconds.
    const int hold_ms = static_cast<int>(event.integer_or("DURATIONOVERRIDEMS", 0));
    if (0 < hold_ms) {
        SkillEventHold hold;
        hold.skill = source.skill;
        hold.run = source.run;
        hold.caster_machine = source.caster_machine;
        hold.caster_node = source.caster_node;
        hold.left = static_cast<float>(hold_ms) * 0.001f;  // `fmuls` by 0.001f
        skill_event_holds.push_back(hold);
    }

    // `CSkillEvent::startEvent`, where the layout goes:
    // no target unit for target type 11 (`TARGET_POS`); the origin is `triggerEvent`'s.
    const Ogre::Vector3 caster_feet = caster_node->getPosition();
    Ogre::Vector3 at = caster_feet;
    if (target.origin.has_value())
        at = Ogre::Vector3(target.origin->x, target.origin->y, target.origin->z);
    Ogre::Quaternion facing = caster_node->getOrientation();
    if (target.yaw.has_value())
        facing = Ogre::Quaternion(Ogre::Radian(*target.yaw), Ogre::Vector3::UNIT_Y);
    const bool has_unit = target.has_unit && source.target_type != 0xb;
    // A target item (a breakable) takes the unit's place: radius 0, centre at its node.
    const int item_index = !has_unit && target.item != 0 && source.target_type != 0xb
                               ? breakable_index(target.item)
                               : -1;
    Ogre::SceneNode* item_node =
        item_index >= 0 ? breakables[static_cast<size_t>(item_index)].node : nullptr;
    // `startEvent`'s `+0x160` and `+0x1fc`: the skill's range along the forward; unless
    // `PLACEONTARGET`, to the target (less both radii) or the point; never past the range.
    const tl::core::SkillProperty* ranged = skill.property(level);
    const float skill_range = ranged != nullptr ? ranged->range : 1.0f;
    const Ogre::Vector3 caster_forward = caster_node->getOrientation() * Ogre::Vector3::UNIT_Z;
    float to_target_distance = skill_range;
    tl::core::Vec3 to_target{caster_forward.x * skill_range, 0.0f, caster_forward.z * skill_range};
    if (!spec.place_on_target) {
        const float caster_radius = caster_body;
        const auto* aimed = static_cast<const tl::core::LevelScript*>(target.machine);
        tl::core::Vec3 feet;
        if (has_unit && aimed == nullptr && unit_cast && subject_node != nullptr) {
            // The player, as a unit's target.
            const Ogre::Vector3 you = subject_node->getPosition();
            const float dx = you.x - at.x;
            const float dz = you.z - at.z;
            to_target_distance =
                std::max(0.0f, std::sqrt(dx * dx + 0.0f + dz * dz) - (player_body + caster_radius));
            to_target = {dx, 0.0f, dz};
        } else if (has_unit && aimed != nullptr && aimed->unit_position(target.unit, feet)) {
            const float dx = feet.x - at.x;
            const float dz = feet.z - at.z;
            to_target_distance =
                std::max(0.0f, std::sqrt(dx * dx + 0.0f + dz * dz) -
                                   (aimed->unit_radius(target.unit) + caster_radius));
            to_target = {dx, 0.0f, dz};
        } else if (item_node != nullptr) {
            const Ogre::Vector3 there = item_node->getPosition();
            const float dx = there.x - at.x;
            const float dz = there.z - at.z;
            to_target_distance =
                std::max(0.0f, std::sqrt(dx * dx + 0.0f + dz * dz) - caster_radius);
            to_target = {dx, 0.0f, dz};
        } else if (target.position.has_value()) {
            const float dx = target.position->x - at.x;
            const float dz = target.position->z - at.z;
            to_target_distance =
                std::max(0.0f, std::sqrt(dx * dx + 0.0f + dz * dz) - caster_radius);
            to_target = {dx, 0.0f, dz};
        }
    }
    to_target_distance = std::min(to_target_distance, skill_range);
    if (spec.place_on_target) {
        // `PLACEONTARGET`: the unit's feet, else the point, else the origin.
        bool placed_on_unit = false;
        if (has_unit) {
            if (target.machine == nullptr) {
                // A null machine is the player (the caster, for its own skills).
                at = subject_node != nullptr ? subject_node->getPosition() : caster_feet;
                placed_on_unit = true;
            } else {
                const auto* machine = static_cast<const tl::core::LevelScript*>(target.machine);
                tl::core::Vec3 feet;
                if (machine->unit_position(target.unit, feet)) {
                    at = Ogre::Vector3(feet.x, feet.y, feet.z);
                    placed_on_unit = true;
                }
            }
        } else if (item_node != nullptr) {
            at = item_node->getPosition();
            placed_on_unit = true;
        }
        if (!placed_on_unit && target.position.has_value()) {
            at = Ogre::Vector3(target.position->x, target.position->y, target.position->z);
        }
    }
    // A position skill's layout faces away from the caster once more than 0.5 off it.
    const int32_t skill_type = skill.def->base.target_type;
    if (skill_type == 1 || skill_type == 0xb || skill_type == 6 || skill_type == 5) {
        float dx = at.x - caster_feet.x;
        float dz = at.z - caster_feet.z;
        const float away = std::sqrt(dx * dx + 0.0f + dz * dz);
        if (0.5f < away) {
            dx /= away;
            dz /= away;
            Ogre::Quaternion q;
            q.FromAxes(Ogre::Vector3(dz, 0.0f, -dx), Ogre::Vector3::UNIT_Y,
                       Ogre::Vector3(dx, 0.0f, dz));
            facing = q;
        }
    }

    // `ATTACHTOTARGET` (`updateEvent`, +0x13e): with a target and
    // no `ATTACH_LINE_EMITTER_TO_MAX_DISTANCE`, on the target's `FIREATBONE`; otherwise
    // the range along the forward, cut 0.1 before a `LINE_EMITTER_COLLIDES` hit.
    // INVENTED: bones are body centres, no re-aim each frame, no lead (`+0x164`).
    Ogre::SceneNode* ride = spec.attaches ? caster_node : nullptr;
    if (spec.attach_to_target) {
        auto* followed = static_cast<tl::core::LevelScript*>(const_cast<void*>(target.machine));
        bool on_unit = false;
        if (has_unit && !spec.attach_to_max_distance) {
            if (followed == nullptr) {
                const float body = player_stats.radius > 0.0f ? player_stats.radius
                                                              : tl::core::kDefaultCollisionRadius;
                at = subject_node->getPosition() + Ogre::Vector3(0.0f, body, 0.0f);
                ride = subject_node;
                on_unit = true;
            } else if (tl::core::Vec3 feet; followed->unit_position(target.unit, feet)) {
                at = Ogre::Vector3(feet.x, feet.y + followed->unit_radius(target.unit), feet.z);
                ride = nullptr;
                if (NodeMap* nodes = nodes_of(followed)) {
                    const auto found = nodes->find(target.unit);
                    if (found != nodes->end()) ride = found->second;
                }
                on_unit = true;
            }
        } else if (item_node != nullptr && !spec.attach_to_max_distance) {
            at = item_node->getPosition();
            ride = item_node;
            on_unit = true;
        }
        if (!on_unit) {
            const Ogre::Vector3 from =
                caster_node->getPosition() + Ogre::Vector3(0.0f, caster_body, 0.0f);
            const Ogre::Vector3 forward = caster_node->getOrientation() * Ogre::Vector3::UNIT_Z;
            const tl::core::SkillProperty* range_of = skill.property(level);
            Ogre::Vector3 to = from + forward * (range_of != nullptr ? range_of->range : 1.0f);
            tl::core::CollisionHit hit;
            if (spec.line_emitter_collides &&
                collision.ray_collision({from.x, from.y, from.z}, {to.x, to.y, to.z}, true, hit)) {
                to = Ogre::Vector3(hit.point.x, hit.point.y, hit.point.z) -
                     Ogre::Vector3(hit.normal.x, hit.normal.y, hit.normal.z) * 0.1f;
            }
            at = to;
            ride = nullptr;
        }
    }

    // The layout, when the block names one (`FILE`); an event without one
    // still strikes its target and runs its skills.
    if (!reference.empty())
        [&] {
            // The spawners the layout carries, for the `Spawn Units` its timeline sends.
            if (const tl::formats::CmpScene* layout = cached_layout(reference)) {
                for (const tl::formats::CmpNode& node : layout->nodes) {
                    if (node.type != tl::formats::cmp_type::kUnitSpawner) continue;
                    skill_spawners[node.id] =
                        SkillSpawner{source, tl::core::read_spawner_shape(node)};
                }
            }

            const size_t before = nested_scripts.size();
            const size_t systems_before = particle_systems.size();
            if (!place_layout(reference, at, facing, /*depth=*/1)) {
                TL_LOG_WARN("skills: '%s' could not place '%s'", source.skill.c_str(),
                            reference.c_str());
                return;
            }
            // `ATTACHES` rides the caster, `ATTACHTOTARGET` the target; a machine it owns
            // keeps its placed point.
            if (ride != nullptr) {
                const Ogre::Quaternion inverse = ride->_getDerivedOrientation().Inverse();
                const Ogre::Vector3 origin = ride->_getDerivedPosition();
                const Ogre::Vector3 scale = ride->_getDerivedScale();
                for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                    if (particle_systems[i].claimed) continue;
                    Ogre::SceneNode* holder = particle_systems[i].holder();
                    if (holder == nullptr || holder->getParent() == ride) continue;
                    const Ogre::Vector3 world = holder->_getDerivedPosition();
                    const Ogre::Quaternion turned = holder->_getDerivedOrientation();
                    if (Ogre::Node* up = holder->getParent()) up->removeChild(holder);
                    ride->addChild(holder);
                    holder->setPosition((inverse * (world - origin)) / scale);
                    holder->setOrientation(inverse * turned);
                    holder->setScale(Ogre::Vector3::UNIT_SCALE / scale);
                }
            }
            // The event's listeners (`addListeners`) before the layout runs.
            hang_player_skill_event(source, before);
            LiveSkillEvent live;
            live.skill = source.skill;
            live.run = source.run;
            live.first = before;
            live.end = nested_scripts.size();
            live.caster_machine = source.caster_machine;
            live.caster_node = source.caster_node;
            {
                // `CSkill +0xec`, the run's loop animation, off the caster's runner.
                const tl::core::SkillRunner* runner = &player_skill_runner;
                if (source.caster_machine != nullptr) {
                    const tl::core::LevelScript::UnitCasting* k =
                        source.caster_machine->unit_casting(source.caster_node);
                    runner = k != nullptr ? &k->runner : nullptr;
                }
                const tl::core::SkillRunState* run =
                    runner != nullptr ? runner->state(skill.entry.name) : nullptr;
                live.loop = run != nullptr && run->animations.loop_into != -1;
            }
            for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                // Not what a script of the layout placed meanwhile (`claimed`).
                if (particle_systems[i].claimed) continue;
                Ogre::SceneNode* holder = particle_systems[i].holder();
                if (holder != nullptr) particle_systems[i].claimed = true;
                if (holder != nullptr && std::find(live.holders.begin(), live.holders.end(),
                                                   holder) == live.holders.end()) {
                    live.holders.push_back(holder);
                }
                // `startEvent`'s `CLineEmitterWrapper`s: each system of the layout
                // holding a `Line` emitter.
                if (holder != nullptr &&
                    torchlight_particle_line_emitters(particle_systems[i].pu) > 0 &&
                    std::find(live.line_holders.begin(), live.line_holders.end(), holder) ==
                        live.line_holders.end()) {
                    live.line_holders.push_back(holder);
                }
            }
            // The owner whose bones `updateEvent` reads (`getOwnerCharacter`) and the target.
            if (unit_cast) {
                live.owner = ViewUnit{false, unit_event_caster.machine, unit_event_caster.node};
            } else if (event_caster_override != nullptr &&
                       event_caster_override_machine != nullptr) {
                live.owner =
                    ViewUnit{false, event_caster_override_machine, event_caster_override_node};
            } else {
                live.owner = ViewUnit{true, nullptr, 0};
            }
            if (has_unit) {
                live.has_target = true;
                live.target =
                    ViewUnit{target.machine == nullptr,
                             static_cast<tl::core::LevelScript*>(const_cast<void*>(target.machine)),
                             target.unit};
            } else if (item_node != nullptr) {
                live.has_target = true;
                live.target = ViewUnit{};
                live.target.item = target.item;
            }
            live.attach_line_emitter = spec.attach_line_emitter;
            live.to_max_distance = spec.attach_to_max_distance;
            live.collides = spec.line_emitter_collides;
            live.far_target = spec.attach_to_target &&
                              ((!has_unit && item_node == nullptr) || spec.attach_to_max_distance);
            live.far_point = at;
            live.fire_from = spec.fire_from_bone;
            live.fire_at = spec.fire_at_bone;
            live.range = skill_range;
            live.target_ahead = spec.target_ahead;
            if (spec.attach_owner) {
                for (size_t i = before; i < nested_scripts.size() && live.shape == 0; ++i) {
                    const std::vector<int64_t> shapes = nested_scripts[i].script.damage_shape_ids();
                    if (shapes.empty()) continue;
                    live.shape_machine = &nested_scripts[i].script;
                    live.shape = shapes.front();
                }
                live.attach_owner = live.shape != 0;
                live.ignores_units = spec.attach_ignores_units;
                live.ignores_obstructions = spec.attach_ignores_obstructions;
                live.scale_distance = spec.scale_attach_distance;
                live.at = at;
                live.facing = facing;
                live.to_target = to_target;
                live.distance = to_target_distance;
                live.skill_target_type = skill.def->base.target_type;
            }
            live_skill_events.push_back(live);
            // `startEvent` ends in `updateEvent`.
            update_skill_event_lines(live_skill_events.back());
            // Casting the skill is the `Play` its `LAYOUT CONTROL` timelines wait for.
            for (size_t i = before; i < nested_scripts.size(); ++i) {
                NestedScript& machine = nested_scripts[i];
                std::vector<tl::core::ScriptEvent> events;
                machine.running = true;
                machine.script.play_layout_timelines(quest_log.state(), events,
                                                     /*backwards=*/false);
                if (!events.empty()) apply_script_events(events, machine.nodes);
            }
            TL_LOG_INFO("skills: '%s' places %s at (%.2f, %.2f, %.2f)%s", source.skill.c_str(),
                        reference.c_str(), static_cast<double>(at.x), static_cast<double>(at.y),
                        static_cast<double>(at.z), spec.attaches ? " (attached)" : "");
        }();

    // `APPLYEFFECTS` (+0x138) with a target: `applyEffects`, `applyWeaponDamage`,
    // `invokeHitSkills`; without, a unit group's event (4, 5, 6, 8) still lays its effects.
    // A breakable (`ISA(0x1d)`) is broken when `effectsTargetEnemy`
    // answers (vtable +0x280). INVENTED: item target types 7 and 8 and the game-speed
    // modifier (+0xa8) are not carried.
    auto* unit_machine = static_cast<tl::core::LevelScript*>(const_cast<void*>(target.machine));
    const bool unit_is_character =
        has_unit && (unit_machine == nullptr || !unit_machine->unit_of(target.unit).empty());
    bool struck = false;
    if (spec.apply_effects && unit_is_character) {
        skill_event_strike(source, unit_machine, target.unit);
        struck = true;
    }
    if (!struck && unit_is_character && (group == 4 || group == 5 || group == 6 || group == 8)) {
        skill_event_effects_on(source, unit_machine, target.unit);
    }
    if (spec.apply_effects && item_node != nullptr) {
        tl::core::SkillCastContext asked;
        asked.types = &effect_types;
        asked.unit_types = &unit_types;
        if (tl::core::event_effects_target_enemy(event, asked, source.property,
                                                 source.unauthored)) {
            // Queued for the frame's item pass (the break may place layouts); a unit's
            // skill breaks it with no breaker.
            const bool by_player = source.caster_machine == nullptr;
            if (queue_item_interact(target.item, by_player)) {
                TL_LOG_INFO("skills: '%s' breaks %s", source.skill.c_str(),
                            breakables[static_cast<size_t>(item_index)].unit.c_str());
            }
        }
    }

    // The executed skills' point: the point given, else the target's centre, else origin.
    tl::core::Vec3 point{at.x, at.y, at.z};
    ViewUnit event_unit;
    if (has_unit) {
        event_unit.player = unit_machine == nullptr;
        event_unit.machine = unit_machine;
        event_unit.node = target.unit;
    }
    if (target.position.has_value()) {
        point = *target.position;
    } else if (has_unit && unit_machine != nullptr) {
        tl::core::Vec3 feet;
        if (unit_machine->unit_position(target.unit, feet)) {
            point = {feet.x, feet.y + unit_machine->unit_radius(target.unit), feet.z};
        }
    } else if (has_unit) {
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        point = {player.position().x, player.position().y + body, player.position().z};
    } else if (item_node != nullptr) {
        const Ogre::Vector3 there = item_node->getPosition();
        point = {there.x, there.y, there.z};
    }
    execute_event_skills(source, spec, has_unit ? &event_unit : nullptr, {at.x, at.y, at.z}, facing,
                         point);
}

void Renderer::Impl::update_live_skill_events(float delta) {
    // `updateEvent`: the event's time runs down.
    for (size_t k = 0; k < skill_event_holds.size();) {
        skill_event_holds[k].left -= delta;
        if (0.0f < skill_event_holds[k].left) {
            ++k;
        } else {
            skill_event_holds.erase(skill_event_holds.begin() + static_cast<long>(k));
        }
    }
    // `updateEvent`: while a timeline updates, an
    // `ATTACHOWNER` event moves its owner to the damage shape (`SCALEATTACHDISTANCE`);
    // `moveToPosition` refusing stops it. The owner is
    // `getOwnerCharacter`'s. INVENTED: `stopEvent` is the event leaving the list.
    for (size_t k = 0; k < live_skill_events.size();) {
        LiveSkillEvent& event = live_skill_events[k];
        if (event.listed) update_skill_event_lines(event);
        if (!event.attach_owner || event.shape_machine == nullptr) {
            ++k;
            continue;
        }
        bool playing = false;
        for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
            if (nested_scripts[i].script.timelines_playing()) playing = true;
        }
        tl::core::Vec3 local;
        if (!playing || !event.shape_machine->node_local_position(event.shape, local)) {
            ++k;
            continue;
        }
        Ogre::Vector3 to;
        if (event.skill_target_type != 1) {
            Ogre::Vector3 offset(local.x, local.y, local.z);
            if (event.scale_distance) {
                offset.x *= event.distance;
                offset.z *= event.distance;
            }
            to = event.facing * offset + event.at;
        } else {
            to = Ogre::Vector3(event.to_target.x * local.z, event.to_target.y * local.z,
                               event.to_target.z * local.z) +
                 event.at;
        }
        const bool moved = event.owner.player
                               ? move_player_to(to, event.ignores_units, event.ignores_obstructions)
                               : move_unit_to(event.owner.machine, event.owner.node, to,
                                              event.ignores_units, event.ignores_obstructions);
        if (!moved) {
            TL_LOG_INFO("skills: '%s' cannot carry the character on; its event stops",
                        event.skill.c_str());
            live_skill_events.erase(live_skill_events.begin() + static_cast<long>(k));
            continue;
        }
        ++k;
    }
}

namespace {

/// `CCharacter::getBones`: the bone each cached node
/// follows, by `gSKILL_BONE_ATTACHMENT_NAMES` index, each name tried in this order.
const Ogre::Bone* skill_attachment_bone(Ogre::Entity* entity, int attachment) {
    if (entity == nullptr || !entity->hasSkeleton()) return nullptr;
    std::vector<const char*> names;
    switch (attachment) {
        case tl::core::kSkillBoneHead: names = {"Bip01 Head"}; break;
        case tl::core::kSkillBoneRightHand: names = {"tag_righthand", "Bip01 R Hand"}; break;
        case tl::core::kSkillBoneLeftHand: names = {"tag_lefthand", "Bip01 L Hand"}; break;
        case tl::core::kSkillBoneRightShoulder: names = {"Bip01 R Clavicle"}; break;
        case tl::core::kSkillBoneLeftShoulder: names = {"Bip01 L Clavicle"}; break;
        default: return nullptr;
    }
    Ogre::SkeletonInstance* skeleton = entity->getSkeleton();
    for (const char* name : names) {
        if (skeleton->hasBone(name)) return skeleton->getBone(name);
    }
    return nullptr;
}

}  // namespace

bool Renderer::Impl::skill_bone_world(const ViewUnit& unit, int attachment, Ogre::Vector3& out) {
    // `CSkillEvent::getBonePosition(unit, attachment, true)`.
    //
    Ogre::Entity* entity = nullptr;
    Ogre::Vector3 feet;
    if (unit.item != 0) {
        // A target item: its model and its node.
        const int index = breakable_index(unit.item);
        if (index < 0) return false;
        entity = breakables[static_cast<size_t>(index)].entity;
        feet = breakables[static_cast<size_t>(index)].node->getPosition();
    } else if (unit.player) {
        if (subject_node == nullptr) return false;
        entity = subject;
        feet = subject_node->getPosition();
    } else {
        if (unit.machine == nullptr) return false;
        tl::core::Vec3 at;
        if (!unit.machine->unit_position(unit.node, at)) return false;
        feet = Ogre::Vector3(at.x, at.y, at.z);
        if (SceneUnit* drawn = scene_unit_of(unit.machine, unit.node)) entity = drawn->entity;
    }
    // `CENTER`: half the mesh's height, at its scale, above the feet.
    if (attachment == tl::core::kSkillBoneCenter) {
        if (entity != nullptr && entity->getParentSceneNode() != nullptr) {
            const Ogre::AxisAlignedBox& box = authored_mesh_bounds(entity);
            const float scale = entity->getParentSceneNode()->_getDerivedScale().x;
            out = feet +
                  Ogre::Vector3(0.0f,
                                std::fabs((box.getMaximum().y - box.getMinimum().y) * 0.5f) * scale,
                                0.0f);
        } else {
            out = feet;
        }
        return true;
    }
    // A hand, the head or a shoulder: the bone's world place (`updateBonePositions`).
    // No such bone: the zero vector, not the feet.
    if (attachment >= tl::core::kSkillBoneHead && attachment <= tl::core::kSkillBoneLeftShoulder) {
        const Ogre::Bone* bone = skill_attachment_bone(entity, attachment);
        if (bone == nullptr || entity == nullptr || entity->getParentNode() == nullptr) {
            out = Ogre::Vector3::ZERO;
            return true;
        }
        out = entity->getParentNode()->_getFullTransform() * bone->_getDerivedPosition();
        return true;
    }
    // None, or `POSITION`: the feet.
    out = feet;
    return true;
}

// PORT: a drawn unit's facing read off its OGRE node.
bool Renderer::Impl::skill_unit_forward(const ViewUnit& unit, Ogre::Vector3& out) {
    const Ogre::Node* node = nullptr;
    if (unit.player) {
        node = subject_node;
    } else if (unit.machine != nullptr) {
        if (SceneUnit* drawn = scene_unit_of(unit.machine, unit.node)) {
            if (drawn->entity != nullptr) node = drawn->entity->getParentSceneNode();
        }
    }
    if (node == nullptr) return false;
    out = node->_getDerivedOrientation() * Ogre::Vector3::UNIT_Z;
    return true;
}

void Renderer::Impl::update_skill_event_lines(LiveSkillEvent& event) {
    // `CSkillEvent::updateEvent`. INVENTED: the target's
    // lead (`TARGET_AHEAD_OF_UNIT`, +0x164) is not added.
    const bool want_lines =
        (event.attach_line_emitter || event.to_max_distance) && !event.line_holders.empty();
    if (!event.far_target && !want_lines) return;
    Ogre::Vector3 start;
    Ogre::Vector3 forward;
    if (!skill_bone_world(event.owner, event.fire_from, start) ||
        !skill_unit_forward(event.owner, forward)) {
        return;
    }
    // The far end: the range along the owner's facing, cut 0.1 short of a
    // `LINE_EMITTER_COLLIDES` hit (`CLevel::rayCollision`).
    const auto cut = [this, &event](const Ogre::Vector3& from, Ogre::Vector3 to) {
        tl::core::CollisionHit hit;
        if (event.collides &&
            collision.ray_collision({from.x, from.y, from.z}, {to.x, to.y, to.z}, true, hit)) {
            to = Ogre::Vector3(hit.point.x, hit.point.y, hit.point.z) -
                 Ogre::Vector3(hit.normal.x, hit.normal.y, hit.normal.z) * 0.1f;
        }
        return to;
    };
    const Ogre::Vector3 far = cut(start, start + forward * event.range);

    // `ATTACHTOTARGET` with no target, or +0x148: at the far end. INVENTED: moved, not
    // turned (vtable +0x118); its machine's nodes stay.
    if (event.far_target) {
        const Ogre::Vector3 by = far - event.far_point;
        if (by.squaredLength() > 0.0f) {
            for (Ogre::SceneNode* holder : event.holders) {
                if (holder != nullptr) holder->translate(by, Ogre::Node::TS_WORLD);
            }
        }
        event.far_point = far;
    }

    if (!want_lines) return;
    // The line: `FIREFROMBONE` to `FIREATBONE`, or to the far end (no target, or +0x148).
    Ogre::Vector3 end = far;
    if (event.has_target && !event.to_max_distance) {
        Ogre::Vector3 at;
        if (skill_bone_world(event.target, event.fire_at, at)) end = cut(start, at);
    }
    for (ParticleSystem& system : particle_systems) {
        Ogre::SceneNode* holder = system.holder();
        if (holder == nullptr || std::find(event.line_holders.begin(), event.line_holders.end(),
                                           holder) == event.line_holders.end()) {
            continue;
        }
        // The wrapper's particle at the bone, `LineEmitter::setEnd` to the end.
        holder->_setDerivedPosition(start);
        set_torchlight_particle_line_end(system.pu, end - start);
    }
    if (!event.line_reported) {
        event.line_reported = true;
        TL_LOG_INFO(
            "skills: '%s' stretches %zu line(s) from (%.2f, %.2f, %.2f) to "
            "(%.2f, %.2f, %.2f)",
            event.skill.c_str(), event.line_holders.size(), static_cast<double>(start.x),
            static_cast<double>(start.y), static_cast<double>(start.z), static_cast<double>(end.x),
            static_cast<double>(end.y), static_cast<double>(end.z));
    }
}

bool Renderer::Impl::move_player_to(const Ogre::Vector3& to, bool ignore_units,
                                    bool ignore_obstructions) {
    // `CCharacter::moveToPosition`: unless obstructions
    // are ignored, the way (or 0.4 along it) must be passable; then `dropToGround(4.0)`.
    // INVENTED: occupancy (vtables +0x268/+0x270), `passableBetween`'s units, `+0x228`.
    (void)ignore_units;
    const tl::core::Vec3 from = player.position();
    if (!ignore_obstructions && passability.ready()) {
        if (!passability.passable_between(from, {to.x, to.y, to.z}, true)) {
            Ogre::Vector3 step(to.x - from.x, 0.0f, to.z - from.z);
            const float length = step.length();
            if (1e-8f < length) step /= length;
            const tl::core::Vec3 nearer{from.x + step.x * 0.4f, from.y, from.z + step.z * 0.4f};
            if (!passability.passable_between(nearer, {to.x, to.y, to.z}, true)) return false;
        }
    }
    const Ogre::Vector3 ground = dropped_to_ground(to);
    player.place({ground.x, ground.y, ground.z}, player.facing());
    if (subject_node != nullptr) subject_node->setPosition(ground);
    return true;
}

bool Renderer::Impl::move_unit_to(tl::core::LevelScript* machine, int64_t node,
                                  const Ogre::Vector3& to, bool ignore_units,
                                  bool ignore_obstructions) {
    // `CCharacter::moveToPosition` on a placed unit, from
    // its feet. INVENTED: as `move_player_to`.
    (void)ignore_units;
    if (machine == nullptr) return false;
    tl::core::Vec3 from;
    if (!machine->unit_position(node, from)) return false;
    if (!ignore_obstructions && passability.ready()) {
        if (!passability.passable_between(from, {to.x, to.y, to.z}, true)) {
            Ogre::Vector3 step(to.x - from.x, 0.0f, to.z - from.z);
            const float length = step.length();
            if (1e-8f < length) step /= length;
            const tl::core::Vec3 nearer{from.x + step.x * 0.4f, from.y, from.z + step.z * 0.4f};
            if (!passability.passable_between(nearer, {to.x, to.y, to.z}, true)) return false;
        }
    }
    const Ogre::Vector3 ground = dropped_to_ground(to);
    machine->move_unit(node, {ground.x, ground.y, ground.z});
    return true;
}

bool Renderer::Impl::skill_events_alive(const std::string& skill, uint32_t run,
                                        tl::core::LevelScript* caster_machine,
                                        int64_t caster_node) {
    // `CSkillEvent::shouldStop`: alive while a missile it
    // fired flies (`+0xf8`; `CSkillEvent::missileBeingFired`, `missileDieing`),
    // its `DURATIONOVERRIDEMS` lasts, a timeline of its layout updates, its
    // particles live (`CLayout::getNumberOfParticlesUpdating`
    // -> `CEditorScene::getNumberOfParticlesUpdating`,
    // `CParticleWrapper::isPlaying`), or its
    // skill loops. INVENTED: `isPlaying`'s second half and state `+0x60 == 4`; a missile is
    // matched by skill, run and caster.
    const auto missile_flies = [this](const LiveSkillEvent& event) {
        for (const FlyingMissile& flying : missiles) {
            if (flying.level_placed || flying.by_weapon || flying.detached ||
                flying.missile.dying) {
                continue;
            }
            if (flying.source.skill == event.skill && flying.source.run == event.run &&
                flying.source.caster_machine == event.caster_machine &&
                flying.source.caster_node == event.caster_node) {
                return true;
            }
        }
        return false;
    };
    bool alive = false;
    for (const SkillEventHold& hold : skill_event_holds) {
        if (hold.skill == skill && hold.run == run && hold.caster_machine == caster_machine &&
            hold.caster_node == caster_node) {
            alive = true;
        }
    }
    for (LiveSkillEvent& event : live_skill_events) {
        if (!event.listed) continue;
        bool playing = false;
        for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
            if (nested_scripts[i].script.timelines_playing()) playing = true;
        }
        const bool has_layout = event.first < event.end;
        if (has_layout && (event.loop || skill_event_particles_alive(event))) playing = true;
        if (!playing && !missile_flies(event)) {
            // `updateEvent` returned 0: the event leaves the skill's list.
            event.listed = false;
            continue;
        }
        if (event.skill == skill && event.run == run && event.caster_machine == caster_machine &&
            event.caster_node == caster_node) {
            alive = true;
        }
    }
    return alive;
}

bool Renderer::Impl::skill_events_can_stop(const std::string& skill, uint32_t run,
                                           tl::core::LevelScript* caster_machine,
                                           int64_t caster_node) {
    // `CSkillEvent::canStop`, as `CSkill::attemptToStopSkill`
    // asks it: its time spent, then no layout, a looping
    // skill, or no timeline updating; then no particles (+0x220, `getParticlesStillVisible`).
    for (const SkillEventHold& hold : skill_event_holds) {
        if (hold.skill == skill && hold.run == run && hold.caster_machine == caster_machine &&
            hold.caster_node == caster_node && 0.0f < hold.left) {
            return false;
        }
    }
    for (const LiveSkillEvent& event : live_skill_events) {
        if (!event.listed || event.loop) continue;
        if (event.skill != skill || event.run != run || event.caster_machine != caster_machine ||
            event.caster_node != caster_node) {
            continue;
        }
        for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
            if (nested_scripts[i].script.timelines_playing()) return false;
        }
        if (event.first < event.end && skill_event_particles_alive(event)) return false;
    }
    return true;
}

// `CLayout::getNumberOfParticlesUpdating` for an event's
// layout: a placed system whose technique 0 has particles (`CParticleWrapper::isPlaying`'s
// first test); then `CUnitSpawner::getParticlesStillVisible()`
// on each spawner of the layout (`+0xc0`).
bool Renderer::Impl::skill_event_particles_alive(const LiveSkillEvent& event) const {
    const auto alive_in = [this](const std::vector<Ogre::SceneNode*>& holders) {
        for (const ParticleSystem& system : particle_systems) {
            Ogre::SceneNode* holder = system.holder();
            if (holder == nullptr ||
                std::find(holders.begin(), holders.end(), holder) == holders.end()) {
                continue;
            }
            if (torchlight_particle_has_particles(system.pu)) return true;
        }
        return false;
    };
    if (alive_in(event.holders)) return true;
    for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
        for (const tl::core::LevelScript::NodeRecord& n :
             nested_scripts[i].script.nodes_of_type(tl::formats::cmp_type::kUnitSpawner)) {
            const auto placed = spawner_particles.find(n.node);
            if (placed == spawner_particles.end()) continue;
            for (const auto& layout : placed->second) {
                if (alive_in(layout.second)) return true;
            }
        }
    }
    return false;
}

// `CSkillEvent::reset`: the missile list half (vtable
// +0x24 and safe pointers are the port's lifetime). Matched by skill, copy, caster and
// block, so a clone's missiles go too.
void Renderer::Impl::detach_event_missiles(const std::string& skill, uint32_t run,
                                           const tl::formats::AdmSection& block,
                                           tl::core::LevelScript* caster_machine,
                                           int64_t caster_node) {
    for (FlyingMissile& flying : missiles) {
        if (flying.level_placed || flying.by_weapon || flying.detached) continue;
        if (flying.source.event != &block || flying.source.skill != skill ||
            flying.source.run != run || flying.source.caster_machine != caster_machine ||
            flying.source.caster_node != caster_node) {
            continue;
        }
        flying.detached = true;
        TL_LOG_INFO("missiles: '%s' %s left without its event (restarted)", skill.c_str(),
                    flying.missile.spec.name.c_str());
    }
}

void Renderer::Impl::reset_skill_events(const std::string& skill, uint32_t run,
                                        tl::core::LevelScript* caster_machine,
                                        int64_t caster_node) {
    skill_event_holds.erase(std::remove_if(skill_event_holds.begin(), skill_event_holds.end(),
                                           [&](const SkillEventHold& hold) {
                                               return hold.skill == skill && hold.run == run &&
                                                      hold.caster_machine == caster_machine &&
                                                      hold.caster_node == caster_node;
                                           }),
                            skill_event_holds.end());
    for (size_t k = 0; k < live_skill_events.size();) {
        LiveSkillEvent& event = live_skill_events[k];
        if (event.skill != skill || event.run != run || event.caster_machine != caster_machine ||
            event.caster_node != caster_node) {
            ++k;
            continue;
        }
        // `CSkillEvent::stopEvent`: `CLayout::stop(false)`,
        // `stopAllParticles(loop)` and `disableAllDamageShapes`.
        // INVENTED: looping sounds (`CSoundObject::stop`) are not stopped.
        for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
            NestedScript& machine = nested_scripts[i];
            std::vector<tl::core::ScriptEvent> events;
            machine.script.stop_layout_timelines(quest_log.state(), events);
            machine.script.disable_damage_shapes(quest_log.state(), events);
            if (!events.empty()) apply_script_events(events, machine.nodes);
        }
        // `CSkillEvent::stopAllParticles`: the layout's
        // own `Particle` is `CParticleWrapper::stop` at once; a `Layout Link
        // Particle`'s `CParticle::Stop(loop)`. Holders go
        // once nothing in them lives.
        for (ParticleSystem& system : particle_systems) {
            Ogre::SceneNode* holder = system.holder();
            if (holder == nullptr || std::find(event.holders.begin(), event.holders.end(),
                                               holder) == event.holders.end()) {
                continue;
            }
            if (system.owner == 0 || event.loop) {
                if (!system.finished) {
                    stop_torchlight_particle(system.pu);
                    system.pu_dying = false;
                    system.finished = true;
                }
            } else {
                stop_particle_emission(system);
            }
        }
        // and `CUnitSpawner::stopAllParticles()` on each spawner
        // (`+0xc0`): its particle layouts `CLayout::stop(false)`.
        for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
            for (const tl::core::LevelScript::NodeRecord& n :
                 nested_scripts[i].script.nodes_of_type(tl::formats::cmp_type::kUnitSpawner)) {
                const auto placed = spawner_particles.find(n.node);
                if (placed == spawner_particles.end()) continue;
                for (const auto& layout : placed->second) {
                    for (ParticleSystem& system : particle_systems) {
                        Ogre::SceneNode* holder = system.holder();
                        if (holder == nullptr || system.finished ||
                            std::find(layout.second.begin(), layout.second.end(), holder) ==
                                layout.second.end()) {
                            continue;
                        }
                        stop_particle_emission(system);
                    }
                }
            }
        }
        if (!event.holders.empty()) {
            transient_particles.push_back(TransientParticles{event.holders, 0.0f, true});
        }
        live_skill_events.erase(live_skill_events.begin() + static_cast<long>(k));
    }
}

void Renderer::Impl::hang_player_skill_event(const SkillEventSource& source, size_t first) {
    if (first >= nested_scripts.size()) return;
    // The shapes this event's layout placed, by machine and node.
    std::vector<std::pair<tl::core::LevelScript*, int64_t>> shapes;
    for (size_t i = first; i < nested_scripts.size(); ++i) {
        for (int64_t id : nested_scripts[i].script.damage_shape_ids()) {
            shapes.emplace_back(&nested_scripts[i].script, id);
        }
    }
    if (shapes.empty()) return;

    // `TARGET_SPECIFIC_UNITS` (+0xd8): `UNIT_GUID`s in the original, unit names here.
    std::vector<std::string> specific;
    for (const tl::formats::AdmSection& part : source.event->children) {
        if (tl::core::upper_ascii(part.name) != "TARGET_SPECIFIC_UNITS") continue;
        for (const tl::formats::AdmRecord& record : part.records) {
            if (tl::core::upper_ascii(record.key) == "UNIT") {
                specific.push_back(tl::core::upper_ascii(record.text));
            }
        }
    }

    // The shape's position for `canEffectPosObject`:
    // within 3.0 of the unit's centre in height.
    const auto shape_height = [shapes](const tl::core::DamageShape& shape, float& y) {
        for (const auto& entry : shapes) {
            if (entry.first->damage_shape(entry.second) != &shape) continue;
            tl::core::Vec3 p;
            if (!entry.first->node_position(entry.second, p)) return false;
            y = p.y;
            return true;
        }
        return false;
    };
    // The character's centre, HP and side; false for an unrun unit; null machine: player.
    struct Who {
        float centre_y = 0.0f;
        float hp = 0.0f;
        tl::core::UnitStats::Alignment side = tl::core::UnitStats::Alignment::Neutral;
        std::string unit;
        bool pet = false;
    };
    const auto describe = [this](const tl::core::DamageShapeUnit& who, Who& out) {
        if (who.machine == nullptr) {
            const float body = player_stats.radius > 0.0f ? player_stats.radius
                                                          : tl::core::kDefaultCollisionRadius;
            out.centre_y = player.position().y + body;
            out.hp = player_health.dead ? 0.0f : player_health.current;
            out.side = tl::core::UnitStats::Alignment::Friendly;
            return true;
        }
        const auto* machine = static_cast<const tl::core::LevelScript*>(who.machine);
        tl::core::Vec3 feet;
        float max = 0.0f;
        if (!machine->unit_position(who.node, feet) ||
            !machine->unit_health(who.node, out.hp, max)) {
            return false;
        }
        out.centre_y = feet.y + machine->unit_radius(who.node);
        out.unit = machine->unit_of(who.node);
        out.side = machine->unit_side(who.node).value_or(tl::core::UnitStats::Alignment::Neutral);
        out.pet = machine->unit_is_player_pet(who.node);
        return true;
    };

    auto listener = std::make_unique<SkillEventListener>();
    const SkillEventSource src = source;
    // `CSkillEvent::getCharacterCanBeAffectedByDamageShape`,
    // with the player as the skill's master owner.
    listener->can = [src, specific, shape_height, describe](const tl::core::DamageShape& shape,
                                                            const tl::core::DamageShapeUnit& who) {
        Who w;
        if (!describe(who, w)) return false;
        // The dead only for a corpse shape or `TARGET_CORPSES`; else the living.
        if (!shape.target_corpse && !src.spec.target_corpses) {
            if (w.hp < 1.0f) return false;
        } else if (0.0f < w.hp) {
            return false;
        }
        // INVENTED: `SINGLETARGET` (+0xb4) and `TARGET_UNITTYPE` (+0x74) are not read.
        float shape_y = 0.0f;
        if (!shape_height(shape, shape_y) || !(std::fabs(shape_y - w.centre_y) <= 3.0f))
            return false;
        if (!specific.empty() && std::find(specific.begin(), specific.end(),
                                           tl::core::upper_ascii(w.unit)) == specific.end()) {
            return false;
        }
        if (src.target_type == 4) return true;  // `EVERYBODY`
        // `SELFANDPETS` (10) and `PETS` (9). INVENTED: a placed unit's pets are unknown, so
        // only the owner counts for one.
        const bool by_unit = src.caster_machine != nullptr;
        const bool self = by_unit ? who.machine == src.caster_machine && who.node == src.caster_node
                                  : who.machine == nullptr;
        const bool pet = by_unit ? false : w.pet;
        if (src.target_type == 10 && !self && !pet) return false;
        if (src.target_type == 9 && !pet) return false;
        // `CCharacter::isEnemy` from the owner's side.
        // INVENTED: the player's AI flag 1 is never set, its side never 4.
        const bool enemy =
            by_unit
                ? src.caster_machine->unit_is_enemy_of(
                      src.caster_node,
                      static_cast<tl::core::LevelScript*>(const_cast<void*>(who.machine)), who.node)
                : w.side == tl::core::UnitStats::Alignment::Hostile;
        if (src.target_alignment == 2) return enemy;
        if (src.target_alignment == 1) return !enemy;
        return false;
    };
    // `CSkillEvent::characterEffectedByDamageShape`.
    listener->act = [this, src, shape_height, describe](const tl::core::DamageShape& shape,
                                                        const tl::core::DamageShapeUnit& who) {
        Who w;
        if (!describe(who, w)) return false;
        float shape_y = 0.0f;
        if (!shape_height(shape, shape_y) || !(std::fabs(shape_y - w.centre_y) <= 3.0f))
            return false;
        auto* machine = static_cast<tl::core::LevelScript*>(const_cast<void*>(who.machine));
        (void)shape;
        return skill_event_strike(src, machine, who.node);
    };
    // `CSkillEvent::itemEffectedByDamageShape`: within 3
    // of the item's height, then `applyWeaponDamage(null, item, ...)`,
    // true also when `isDamaging`.
    // INVENTED: `SINGLETARGET`, the owner's height test and `+0x14a` sight ray are not run.
    listener->item = [this, src, shape_height](const tl::core::DamageShape& shape,
                                               const tl::core::DamageShapeItem& item) {
        float shape_y = 0.0f;
        if (!shape_height(shape, shape_y) || !(std::fabs(shape_y - item.at.y) <= 3.0f))
            return false;
        bool acted = false;
        if (src.spec.weapon_damage != 0.0f) {
            queue_item_interact(item.id, /*by_player=*/src.caster_machine == nullptr);
            acted = true;
        }
        if (src.event != nullptr) {
            tl::core::SkillCastContext context;
            context.types = &effect_types;
            context.unit_types = &unit_types;
            context.affix = [this](const std::string& name) -> const tl::core::Affix* {
                return affixes.get_affix(name);
            };
            if (tl::core::event_is_damaging(*src.event, context, src.property, src.unauthored)) {
                acted = true;
            }
        }
        return acted;
    };
    for (const auto& entry : shapes) {
        entry.first->damage_shape_take_event(entry.second, source.spec.target_corpses,
                                             source.spec.max_units_hit);
        entry.first->damage_shape_add_listener(entry.second, listener.get());
    }
    skill_event_listeners.push_back(std::move(listener));
}

// `CSkillEvent::applyEffects` and
// `applyWeaponDamage`: `CLevel::rayCollision`
// from master owner to target, both raised by 1, skipping
// `objectRayCollision`: level geometry only.
bool Renderer::Impl::skill_event_sight_blocked(const SkillEventSource& src,
                                               tl::core::LevelScript* machine, int64_t node) {
    if (!src.spec.damage_requires_los) return false;
    tl::core::Vec3 from = player.position();
    tl::core::Vec3 to = player.position();
    if (src.caster_machine != nullptr &&
        !src.caster_machine->unit_position(src.caster_node, from)) {
        return false;
    }
    if (machine != nullptr && !machine->unit_position(node, to)) return false;
    from = {from.x + 0.0f, from.y + 1.0f, from.z + 0.0f};
    to = {to.x + 0.0f, to.y + 1.0f, to.z + 0.0f};
    tl::core::CollisionHit hit;
    if (!collision.ray_collision(from, to, /*objects=*/false, hit)) return false;
    TL_LOG_INFO(
        "skills: '%s' has no line of sight: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f) meets "
        "the level at (%.2f, %.2f, %.2f); nothing applied",
        src.skill.c_str(), static_cast<double>(from.x), static_cast<double>(from.y),
        static_cast<double>(from.z), static_cast<double>(to.x), static_cast<double>(to.y),
        static_cast<double>(to.z), static_cast<double>(hit.point.x),
        static_cast<double>(hit.point.y), static_cast<double>(hit.point.z));
    return true;
}

bool Renderer::Impl::skill_event_effects_on(const SkillEventSource& src,
                                            tl::core::LevelScript* machine, int64_t node) {
    if (src.event == nullptr) return false;
    // Out of sight: nothing is applied, and the call still answers true.
    if (skill_event_sight_blocked(src, machine, node)) return true;
    if (machine == nullptr) {
        // `applyEffects` on the player: its own manager.
        tl::core::SkillCastContext context = player_cast_context();
        if (src.caster_machine != nullptr) {
            // A placed unit's event: the player is its target, not its caster.
            context.target_is_caster = false;
            context.target_is_enemy =
                src.caster_machine->unit_is_enemy_of(src.caster_node, nullptr, 0);
            context.target_is_casters_pet = false;
        }
        // `addListenerToUnit` from `applyAffixesAndEffects`: the skill watches the player.
        context.observe_target = [this, &src](const std::string& skill) {
            unit_observers.add({nullptr, 0}, skill_observer_for(src.caster_machine, src.caster_node,
                                                                skill, src.run));
        };
        // `setOwner(effect, caster)`: the event's caster owns what lands.
        context.source = tl::core::EffectSource{
            true, src.caster_machine, src.caster_machine != nullptr ? src.caster_node : 0};
        context.owner_level = event_caster_level(src.caster_machine, src.caster_node);
        tl::core::LevelScript* const saved_machine = instant_caster_machine;
        const int64_t saved_node = instant_caster_node;
        instant_caster_machine = src.caster_machine;
        instant_caster_node = src.caster_node;
        const bool on_player = tl::core::apply_event_effects(
            *src.event, context, src.skill, src.property, src.unauthored, src.duration_override);
        instant_caster_machine = saved_machine;
        instant_caster_node = saved_node;
        return on_player;
    }
    // `applyEffects` on a placed character, through its
    // `applyEffectOnUnit` (`unit_effect_on_unit`).
    const tl::core::UnitStats::Alignment side =
        machine->unit_side(node).value_or(tl::core::UnitStats::Alignment::Neutral);
    tl::core::EffectManager* manager = unit_effect_manager(machine, node);
    if (manager == nullptr) return false;
    tl::core::SkillCastContext context;
    context.target = manager;
    context.types = &effect_types;
    context.unit_types = &unit_types;
    context.affix = [this](const std::string& name) -> const tl::core::Affix* {
        return affixes.get_affix(name);
    };
    context.target_is_caster = false;
    context.target_is_enemy = side == tl::core::UnitStats::Alignment::Hostile;
    context.target_is_casters_pet = machine->unit_is_player_pet(node);
    // `addListenerToUnit` from `applyAffixesAndEffects`: the skill watches the unit.
    context.observe_target = [this, &src, machine, node](const std::string& skill) {
        unit_observers.add({machine, node},
                           skill_observer_for(src.caster_machine, src.caster_node, skill, src.run));
    };
    // `setOwner(effect, caster)`: the event's caster owns what lands.
    context.source = tl::core::EffectSource{true, src.caster_machine,
                                            src.caster_machine != nullptr ? src.caster_node : 0};
    context.owner_level = event_caster_level(src.caster_machine, src.caster_node);
    if (src.caster_machine != nullptr) {
        // From the caster's side (`CCharacter::isEnemy`).
        // INVENTED: a placed unit's own pets are not known.
        context.target_is_caster = machine == src.caster_machine && node == src.caster_node;
        context.target_is_enemy = !context.target_is_caster && src.caster_machine->unit_is_enemy_of(
                                                                   src.caster_node, machine, node);
        context.target_is_casters_pet = false;
    }
    tl::core::LevelScript* const saved_machine = instant_caster_machine;
    const int64_t saved_node = instant_caster_node;
    instant_caster_machine = src.caster_machine;
    instant_caster_node = src.caster_node;
    // The direction `applyEffects` hands on. INVENTED:
    // caster to target; the original's is from the event's object (`startEvent`),
    // a shape's (`characterEffectedByDamageShape`)
    // or the missile's (`missileApplyingEffects`).
    const bool saved_set = effect_direction_set;
    const std::array<float, 3> saved_direction = effect_direction;
    {
        tl::core::Vec3 from;
        tl::core::Vec3 to;
        bool known = machine->unit_position(node, to);
        if (src.caster_machine != nullptr) {
            known = known && src.caster_machine->unit_position(src.caster_node, from);
        } else {
            from = player.position();
        }
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        effect_direction_set = known && 1e-08f < length;
        if (effect_direction_set) effect_direction = {dx / length, dy / length, dz / length};
    }
    // `alive()`: a body with hit points after gets up (`HP()`,
    // `setAIState(2)`).
    const bool was_alive = !machine->unit_is_dead(node);
    const bool applied = tl::core::apply_event_effects(*src.event, context, src.skill, src.property,
                                                       src.unauthored, src.duration_override);
    effect_direction_set = saved_set;
    effect_direction = saved_direction;
    instant_caster_machine = saved_machine;
    instant_caster_node = saved_node;
    std::vector<tl::core::ScriptEvent> events;
    if (machine->unit_gets_up(node, was_alive, quest_log.state(), events)) {
        TL_LOG_INFO("skill event: %s gets up (%s)", machine->unit_of(node).c_str(),
                    src.skill.c_str());
    }
    if (NodeMap* nodes = nodes_of(machine); nodes != nullptr && !events.empty()) {
        apply_script_events(events, *nodes);
    }
    return applied;
}

float Renderer::Impl::skill_event_weapon_blow(const SkillEventSource& source,
                                              tl::core::LevelScript* machine, int64_t node,
                                              float damage_scale, float soak_scale) {
    // `applyWeaponDamage`: with `WEAPONDAMAGEPCT`, the
    // caster's right-hand `performAttack` at that share, soak by `SOAKSCALEPCT`;
    // `DAMAGE_REQUIRES_LOS` first.
    if (skill_event_sight_blocked(source, machine, node)) return 0.0f;
    if (source.caster_machine != nullptr) {
        // A placed unit's skill: its own `rollAttack` at the event's shares.
        if (source.spec.weapon_damage == 0.0f) return 0.0f;
        if (machine != nullptr && machine->unit_is_dead(node)) return 0.0f;
        std::vector<tl::core::ScriptEvent> events;
        if (!source.caster_machine->unit_skill_blow(source.caster_node, machine, node,
                                                    damage_scale * source.spec.weapon_damage,
                                                    soak_scale * source.spec.soak_scale, events)) {
            return 0.0f;
        }
        const float amount = events.empty() ? 0.0f : events.back().amount;
        if (NodeMap* nodes = nodes_of(source.caster_machine)) apply_script_events(events, *nodes);
        return amount;
    }
    if (source.spec.weapon_damage == 0.0f || machine == nullptr || machine->unit_is_dead(node)) {
        return 0.0f;
    }
    const float weapon_part = player_blow(node, damage_scale * source.spec.weapon_damage,
                                          soak_scale * source.spec.soak_scale);
    const bool critical = last_blow_critical;
    const tl::core::KillingBlow killing = last_killing_blow;
    show_player_blow(node, machine->unit_of(node), weapon_part);
    if (weapon_part > 0.0f) {
        hurt_unit(node, machine->unit_of(node), weapon_part, critical, 0, machine, &killing,
                  /*struck=*/true);
    }
    // The weapon half is `rollAttack`'s, which slows the game on a kill.
    slow_on_player_kill(machine->unit_is_dead(node));
    return weapon_part;
}

bool Renderer::Impl::skill_event_strike(const SkillEventSource& src, tl::core::LevelScript* machine,
                                        int64_t node) {
    // `characterEffectedByDamageShape` and `startEvent`'s
    // `APPLYEFFECTS`: `applyEffects`, `applyWeaponDamage`,
    // `invokeHitSkills`.
    const bool applied = skill_event_effects_on(src, machine, node);
    skill_event_weapon_blow(src, machine, node, 1.0f, 1.0f);
    invoke_hit_skills(src, src.group, machine, node);
    return applied;
}

// PORT: a player skill found in the book, or among the skills executed by name.
const tl::core::KnownSkill* Renderer::Impl::player_skill(const std::string& name) {
    if (const tl::core::KnownSkill* known = player_skills.find_by_name(name)) return known;
    const auto found = executed_skills.find(tl::core::upper_ascii(name));
    return found != executed_skills.end() ? &found->second : nullptr;
}

const tl::core::KnownSkill* Renderer::Impl::add_executed_skill(const std::string& name,
                                                               uint32_t level) {
    // `CSkillManager::addSkill(name, false)`: the class's
    // own skill, else a hidden new one (`+0x6b`); then `_setLevelOfSkillFromSkillManager`.
    if (tl::core::KnownSkill* known = player_skills.find_by_name(name)) return known;
    ensure_player_skill_files();
    const std::string key = tl::core::upper_ascii(name);
    const auto file = player_skill_files.find(key);
    if (file == player_skill_files.end()) return nullptr;
    tl::core::KnownSkill& skill = executed_skills[key];
    skill.entry.name = file->second->def.name;
    skill.def = &file->second->def;
    skill.invested = level;
    skill.effective = level;
    return &skill;
}

bool Renderer::Impl::find_character_within_view(const tl::core::Vec3& at,
                                                const Ogre::Quaternion& facing, float near,
                                                float angle, float range, int32_t alignment,
                                                bool nearest, const ViewUnit& ignore,
                                                ViewUnit& found,
                                                const std::function<bool(const ViewUnit&)>& valid) {
    // `CLevel::findCharacterWithinView`: the player, then
    // each machine's characters; the first in view, or the nearest. Passed: dead, hidden,
    // untargetable, ignored, wrong alignment (3 any, 1/6 `GOOD`, 2/5 `EVIL`), dying, AI
    // 0x29/0x2a, or refused by `CSkill::getTargetIsValid`. Farther than `near`: within
    // `angle` (nearer than 2.0: 30) and `range`, sight clear 5.0 up (`sightUnobstructed`).
    const Ogre::Quaternion inverse = facing.Inverse();
    const auto passes_side = [alignment](tl::core::UnitStats::Alignment side) {
        using A = tl::core::UnitStats::Alignment;
        if (alignment == 1 || alignment == 6) return side == A::Friendly;
        if (alignment == 2 || alignment == 5) return side == A::Hostile;
        return true;
    };
    float best = 9999.0f;
    bool got = false;
    const auto consider = [&](const ViewUnit& who, const tl::core::Vec3& centre) {
        const Ogre::Vector3 local =
            inverse * Ogre::Vector3(centre.x - at.x, centre.y - at.y, centre.z - at.z);
        const float distance = std::sqrt(local.x * local.x + 0.0f + local.z * local.z);
        if (near < distance) {
            const float off = std::fabs(std::atan2(local.x, local.z) * 57.295826f);
            if (angle < off && (angle <= 0.0f || 2.0f <= distance || 30.0f <= off)) return false;
            if (range < distance) return false;
        }
        tl::core::CollisionHit hit;
        if (collision.ray_collision({at.x, at.y + 5.0f, at.z},
                                    {centre.x, centre.y + 5.0f, centre.z}, false, hit)) {
            return false;
        }
        if (!nearest) {
            found = who;
            got = true;
            return true;
        }
        if (best <= distance) return false;
        found = who;
        best = distance;
        got = true;
        return false;
    };
    if (!player_health.dead && subject_node != nullptr && !ignore.player &&
        passes_side(tl::core::UnitStats::Alignment::Friendly)) {
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        const tl::core::Vec3 feet = player.position();
        ViewUnit you;
        you.player = true;
        if ((!valid || valid(you)) && consider(you, {feet.x, feet.y + body, feet.z})) return true;
    }
    for (tl::core::LevelScript* machine : level_machines) {
        std::vector<tl::core::LevelScript::Reachable> list;
        machine->reachable_characters(list);
        for (const tl::core::LevelScript::Reachable& r : list) {
            if (!ignore.player && ignore.machine == machine && ignore.node == r.id) continue;
            // `ISA(0xa7)` with the `MIMICIDLE` theme.
            if (r.mimic_idle) continue;
            if (!passes_side(r.alignment)) continue;
            ViewUnit who;
            who.machine = machine;
            who.node = r.id;
            if (valid && !valid(who)) continue;
            if (consider(who, r.center)) return true;
        }
    }
    return got;
}

bool Renderer::Impl::player_skill_target_valid(const tl::core::KnownSkill& skill,
                                               const ViewUnit& unit) {
    // `CSkill::getTargetIsValid(candidate, owner, at, false)`,
    // the player `GOOD` (1). `TARGET_UNITTYPE` and `TARGET_SPECIFIC_UNITS` pass:
    // no player skill authors them (`LevelScript::unit_skill_target_valid`).
    tl::core::SkillTargetQuery query;
    const tl::core::SkillProperty* property = skill.property(skill.effective);
    query.has_property = property != nullptr;
    query.target_type = property != nullptr ? property->target_type : 0;
    query.target_alignment = skill.def != nullptr ? skill.def->target_alignment : 2;
    query.dead_wanted = skill.def != nullptr && skill.def->target_corpses;
    query.owner_alignment = 1;
    query.owner.alive = !player_health.dead;
    query.owner.is_owner = true;
    tl::core::SkillTargetFacts& t = query.candidate;
    if (unit.player) {
        t = query.owner;
    } else if (unit.machine != nullptr) {
        t.alive = !unit.machine->unit_is_dead(unit.node);
        // `+0x640`: the player's pet has the player for its master.
        t.master_is_owner = unit.machine->unit_is_player_pet(unit.node);
        t.enemy = unit.machine->unit_is_enemy_of(unit.node, nullptr, 0);
    }
    return tl::core::skill_target_is_valid(query);
}

void Renderer::Impl::find_player_skill_target(const tl::core::KnownSkill& skill) {
    // `CCharacter::castSkill`: no target, no
    // target item, and a `FINDTARGETANGLE` above 0.
    if (player_target_node != 0 || player_skill_item != 0) return;
    if (subject_node == nullptr) return;
    const tl::core::SkillProperty* property = skill.property(skill.effective);
    const float angle = property != nullptr ? property->find_target_angle : 0.0f;
    if (!(0.0f < angle)) return;
    // The reach: `(scale * REACH_BONUS + 0.2 + RANGE) * RANGE_MULTIPLIER`, and 1
    // more for the characters.
    const float range = property->range;
    const float reach =
        (player_stats.scale * player_stats.reach + 0.2f + range) * player_stats.range_multiplier;
    // `getTargetAlignment`: no AI manager, so 5, `EVIL`'s
    // side.
    const int32_t alignment = 5;
    // The frame: the character's position, turned by its facing.
    const tl::core::Vec3 me = player.position();
    const Ogre::Quaternion facing = subject_node->getOrientation();
    ViewUnit ignore;
    ignore.player = true;
    ViewUnit found;
    // `findCharacterWithinView(frame, alignment, 0, angle, reach + 1, true, this, skill)`,
    // then `setTarget`.
    if (find_character_within_view(
            me, facing, 0.0f, angle, reach + 1.0f, alignment, /*nearest=*/true, ignore, found,
            [&](const ViewUnit& who) { return player_skill_target_valid(skill, who); }) &&
        !found.player) {
        set_player_target(found.node);
        TL_LOG_INFO("skills: '%s' finds its target in view, node %lld", skill.entry.name.c_str(),
                    static_cast<long long>(found.node));
        return;
    }
    // Nothing: `findItemWithinView`, the multiplier applied to the range twice
    // as the original computes it, and `setTargetItem` on the breakable.
    const float item_reach =
        (player_stats.scale * player_stats.reach + 0.2f + reach) * player_stats.range_multiplier +
        1.0f;
    const int index = breakable_within_view(angle, item_reach);
    if (index < 0) return;
    // `setTargetItem`: refused while dying or dead.
    if (player_health.dead) return;
    const BreakableWorld& b = breakables[static_cast<size_t>(index)];
    player_target_node = 0;
    player_skill_item = b.id;
    const Ogre::Vector3 there = b.node->getPosition();
    TL_LOG_INFO("skills: '%s' finds %s in view, %.1f away", skill.entry.name.c_str(),
                b.unit.c_str(),
                static_cast<double>(Ogre::Vector3(there.x - me.x, 0.0f, there.z - me.z).length()));
}

void Renderer::Impl::execute_event_skills(const SkillEventSource& source,
                                          const tl::core::SkillEventSpec& spec,
                                          const ViewUnit* unit, const tl::core::Vec3& origin,
                                          const Ogre::Quaternion& facing,
                                          const tl::core::Vec3& point) {
    if (spec.execute_skills.empty()) return;
    // INVENTED: a placed unit's `EXECUTE_SKILL` is not run (no unit skill authors one yet).
    if (source.caster_machine != nullptr) return;
    const tl::core::KnownSkill* parent = player_skill(source.skill);
    // The 6th argument: the parent skill's invested level (`CSkill +0xdc`).
    const uint32_t level = parent != nullptr ? parent->invested : source.level;
    // `CASTFROMTARGET`: by the target, from the point, at no one; else by the caster.
    const bool from_target = spec.cast_from_target && unit != nullptr;
    const tl::core::Vec3 from = from_target ? point : origin;
    const auto run = [&](const std::string& name) {
        // `CExecuteSkillProps::executeSkill`.
        const tl::core::KnownSkill* skill = add_executed_skill(name, level);
        if (skill == nullptr || skill->def == nullptr) {
            TL_LOG_WARN("skills: '%s' executes '%s', which no skill file names",
                        source.skill.c_str(), name.c_str());
            return;
        }
        const tl::core::SkillProperty* property = skill->property(level);
        ViewUnit caster_unit;
        if (from_target) {
            caster_unit = *unit;
        } else {
            caster_unit.player = true;
        }
        ViewUnit target;
        bool has_target = !from_target && unit != nullptr;
        if (has_target) target = *unit;
        // No target: the nearest character in view from the point, `FINDTARGETANGLE` and
        // range plus 1.2, the caster passed by.
        if (!has_target) {
            const float range = property != nullptr ? property->range : 1.0f;
            has_target = find_character_within_view(
                from, facing, 0.0f, property != nullptr ? property->find_target_angle : 0.0f,
                range + 0.2f + 1.0f, skill->def->target_alignment, true, caster_unit, target);
        }
        tl::core::SkillCaster caster = player_caster();
        caster.position = from;
        caster.target_position = point;
        tl::core::SkillCastContext context = player_cast_context();
        context.event_target = tl::core::SkillEventTarget{};
        context.event_target.origin = from;
        context.event_target.yaw = facing.getYaw().valueRadians();
        if (has_target) {
            tl::core::Vec3 centre;
            if (target.player) {
                const float body = player_stats.radius > 0.0f ? player_stats.radius
                                                              : tl::core::kDefaultCollisionRadius;
                const tl::core::Vec3 feet = player.position();
                centre = {feet.x, feet.y + body, feet.z};
            } else {
                tl::core::Vec3 feet;
                target.machine->unit_position(target.node, feet);
                centre = {feet.x, feet.y + target.machine->unit_radius(target.node), feet.z};
            }
            tl::core::SkillTargetUnit aimed;
            aimed.position = centre;
            caster.target_unit = aimed;
            context.event_target.has_unit = true;
            context.event_target.unit = target.player ? 0 : target.node;
            context.event_target.machine = target.player ? nullptr : target.machine;
        }
        // `getSkillCanBeExecuted`.
        if (!player_skill_runner.can_be_executed(*skill, level, caster)) {
            TL_LOG_INFO("skills: '%s' executes '%s': %s", source.skill.c_str(),
                        skill->entry.name.c_str(), has_target ? "refused" : "no target");
            return;
        }
        // The events it starts belong to the unit that cast it.
        Ogre::SceneNode* cast_by = nullptr;
        if (from_target && !caster_unit.player) {
            if (NodeMap* nodes = nodes_of(caster_unit.machine)) {
                const auto at = nodes->find(caster_unit.node);
                if (at != nodes->end()) cast_by = at->second;
            }
        }
        Ogre::SceneNode* const saved = event_caster_override;
        tl::core::LevelScript* const saved_machine = event_caster_override_machine;
        const int64_t saved_node = event_caster_override_node;
        event_caster_override = cast_by;
        event_caster_override_machine = cast_by != nullptr ? caster_unit.machine : nullptr;
        event_caster_override_node = cast_by != nullptr ? caster_unit.node : 0;
        const bool started = player_skill_runner.execute(*skill, level, caster, context);
        event_caster_override = saved;
        event_caster_override_machine = saved_machine;
        event_caster_override_node = saved_node;
        TL_LOG_INFO("skills: '%s' executes '%s' at level %u%s", source.skill.c_str(),
                    skill->entry.name.c_str(), level, started ? "" : " (not started)");
    };
    if (spec.execute_skill_count <= 0) {
        for (const std::string& name : spec.execute_skills) {
            if (!name.empty()) run(name);
        }
        return;
    }
    // `CRandomizer::getRandom` on `g_Rand`, each at 100.
    tl::core::Randomizer picks;
    for (size_t i = 0; i < spec.execute_skills.size(); ++i) {
        picks.add_choice(static_cast<int32_t>(i), 100);
    }
    for (int k = 0; k < spec.execute_skill_count; ++k) {
        const int32_t index = picks.get_random(tl::core::game_random());
        run(tl::core::tarray_at(spec.execute_skills, static_cast<size_t>(index)));
    }
}

// `CSkillEvent::invokeHitSkills(CCharacter*)`: the
// event's skill fires group 4 (`EVENT_UNITHIT`) at the struck unit, and group 5
// (`EVENT_UNITDIE`) when its `HP()` is under 1, never its own group.
void Renderer::Impl::invoke_hit_skills(const SkillEventSource& source, int group,
                                       tl::core::LevelScript* machine, int64_t node) {
    tl::core::LevelScript* const caster_machine = source.caster_machine;
    tl::core::LevelScript::UnitCasting* casting =
        caster_machine != nullptr ? caster_machine->unit_casting(source.caster_node) : nullptr;
    const tl::core::KnownSkill* skill =
        caster_machine != nullptr
            ? unit_skill_named(*caster_machine, source.caster_node, source.skill)
            : player_skill(source.skill);
    if (skill == nullptr || (caster_machine != nullptr && casting == nullptr)) return;
    // The unit's centre (`getPosition(true)`) and turn, the unit itself, no point.
    tl::core::SkillEventTarget target;
    target.has_unit = true;
    target.machine = machine;
    target.unit = node;
    float hp = 0.0f;
    if (machine == nullptr) {
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        const tl::core::Vec3 feet = player.position();
        target.origin = tl::core::Vec3{feet.x, feet.y + body, feet.z};
        target.yaw = player.facing();
        hp = player_health.dead ? 0.0f : player_health.current;
    } else {
        tl::core::Vec3 feet;
        if (!machine->unit_position(node, feet)) return;
        target.origin = tl::core::Vec3{feet.x, feet.y + machine->unit_radius(node), feet.z};
        float yaw = 0.0f;
        if (machine->unit_facing(node, yaw)) target.yaw = yaw;
        float max = 0.0f;
        machine->unit_health(node, hp, max);
    }
    target.run = source.run;
    constexpr int kUnitHit = 4;
    constexpr int kUnitDie = 5;
    if (caster_machine != nullptr) {
        tl::core::SkillCastContext context =
            unit_cast_context(*caster_machine, source.caster_node, machine, node, true);
        context.event_target = target;
        const UnitEventCaster saved = unit_event_caster;
        unit_event_caster = unit_event_caster_for(caster_machine, source.caster_node);
        if (group != kUnitHit) casting->runner.trigger_skill_event(*skill, kUnitHit, context);
        if (hp < 1.0f && group != kUnitDie) {
            casting->runner.trigger_skill_event(*skill, kUnitDie, context);
        }
        unit_event_caster = saved;
        return;
    }
    tl::core::SkillCastContext context = player_cast_context();
    context.event_target = target;
    // On the manager the skill runs on: the player's, or a worn item's.
    tl::core::SkillRunner& runner = skill_runner_running(skill->entry.name);
    if (group != kUnitHit) runner.trigger_skill_event(*skill, kUnitHit, context);
    if (hp < 1.0f && group != kUnitDie) runner.trigger_skill_event(*skill, kUnitDie, context);
}

// PORT: one observer per running skill, made on first use.
Renderer::Impl::SkillObserver* Renderer::Impl::skill_observer_for(
    tl::core::LevelScript* caster_machine, int64_t caster_node, const std::string& skill,
    uint32_t run) {
    std::unique_ptr<SkillObserver>& held =
        skill_observers[std::make_tuple(caster_machine, caster_node, skill, run)];
    if (!held) {
        held = std::make_unique<SkillObserver>();
        held->impl = this;
        held->caster_machine = caster_machine;
        held->caster_node = caster_node;
        held->skill = skill;
        held->run = run;
    }
    return held.get();
}

void Renderer::Impl::SkillObserver::unit_state_change(const void* machine, int64_t unit,
                                                      int state) {
    impl->skill_unit_state_change(
        *this, static_cast<tl::core::LevelScript*>(const_cast<void*>(machine)), unit, state);
}

// `CSkill::unitStateChange(CBaseUnit*, EUNIT_STATES)`: at
// 1 and 4 the skill stops watching; at 4, if the unit holds one of its effects
// (`CEffectManager::hasEffectFromSkill`, inline) and a node, group 8
// `EVENT_DIEBYEFFECT` fires at it (`triggerEvent`).
void Renderer::Impl::skill_unit_state_change(SkillObserver& observer,
                                             tl::core::LevelScript* machine, int64_t node,
                                             int state) {
    if (state != tl::core::unit_state::kDied && state != tl::core::unit_state::kDiedOfEffect) {
        return;
    }
    unit_observers.remove({machine, node}, &observer);
    if (state != tl::core::unit_state::kDiedOfEffect) return;
    const tl::core::EffectManager* effects =
        machine != nullptr ? unit_effect_manager(machine, node) : &player_effects;
    if (effects == nullptr || !effects->has_effect_from_skill(observer.skill)) return;

    tl::core::SkillEventTarget target;
    target.has_unit = true;
    target.machine = machine;
    target.unit = node;
    target.run = observer.run;
    if (machine == nullptr) {
        if (subject_node == nullptr) return;
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        const tl::core::Vec3 feet = player.position();
        target.origin = tl::core::Vec3{feet.x, feet.y + body, feet.z};
        target.yaw = player.facing();
    } else {
        tl::core::Vec3 feet;
        if (!machine->unit_position(node, feet)) return;
        target.origin = tl::core::Vec3{feet.x, feet.y + machine->unit_radius(node), feet.z};
        float yaw = 0.0f;
        if (machine->unit_facing(node, yaw)) target.yaw = yaw;
    }
    constexpr int kDieByEffect = 8;
    tl::core::LevelScript* const caster_machine = observer.caster_machine;
    if (caster_machine != nullptr) {
        tl::core::LevelScript::UnitCasting* casting =
            caster_machine->unit_casting(observer.caster_node);
        const tl::core::KnownSkill* skill =
            unit_skill_named(*caster_machine, observer.caster_node, observer.skill);
        if (casting == nullptr || skill == nullptr) return;
        tl::core::SkillCastContext context =
            unit_cast_context(*caster_machine, observer.caster_node, machine, node, true);
        context.event_target = target;
        const UnitEventCaster saved = unit_event_caster;
        unit_event_caster = unit_event_caster_for(caster_machine, observer.caster_node);
        if (casting->runner.trigger_skill_event(*skill, kDieByEffect, context)) {
            TL_LOG_INFO("skills: %s's '%s' fires EVENT_DIEBYEFFECT",
                        caster_machine->unit_of(observer.caster_node).c_str(),
                        observer.skill.c_str());
        }
        unit_event_caster = saved;
        return;
    }
    const tl::core::KnownSkill* skill = player_skill(observer.skill);
    if (skill == nullptr) return;
    tl::core::SkillCastContext context = player_cast_context();
    context.event_target = target;
    if (skill_runner_running(skill->entry.name)
            .trigger_skill_event(*skill, kDieByEffect, context)) {
        TL_LOG_INFO("skills: the player's '%s' fires EVENT_DIEBYEFFECT", observer.skill.c_str());
    }
}

void Renderer::Impl::skill_unit_spawned(const SkillEventSource& source,
                                        tl::core::LevelScript& machine, int64_t node) {
    // `CSkillEvent::unitSpawnedFromUnitSpawner`: the
    // caster's side and level (`setLevel(+0x100, false)`, vtable +0x318), then `MAKEPET`.
    if (source.caster_machine != nullptr) {
        // A placed unit's summon is on that unit's side, at its level.
        machine.set_unit_side(node, source.caster_machine->unit_side(source.caster_node)
                                        .value_or(tl::core::UnitStats::Alignment::Hostile));
        const int level = effective_monster_level(source.caster_node);
        machine.set_unit_level(node, level);
        if (source.spec.make_pet) {
            // `CCharacter::addPet` -> `setMaster`.
            add_monster_pet(machine, node, *source.caster_machine, source.caster_node);
        }
        skill_event_effects_on(source, &machine, node);
        TL_LOG_INFO("skill event: %s's '%s' summons %s (level %d)",
                    source.caster_machine->unit_of(source.caster_node).c_str(),
                    source.skill.c_str(), machine.unit_of(node).c_str(), level);
        return;
    }
    machine.set_unit_side(node, tl::core::UnitStats::Alignment::Friendly);
    machine.set_unit_level(node, static_cast<int>(player_character.level()));
    if (source.spec.make_pet) add_player_pet(machine, node);
    if (source.spec.synch_pets_to_owner) {
        // INVENTED: the caster's animation and its time are not handed on.
        TL_LOG_WARN("skill event: '%s' SYNCH_PETS_TO_OWNER is not carried", source.skill.c_str());
    }
    // `applyEffects(unit)`: onto the summon's manager, filtered for the caster's pet.
    skill_event_effects_on(source, &machine, node);
    const std::string unit = machine.unit_of(node);
    if (source.spec.match_pet_duration) {
        // INVENTED: `+0x15c = getEffectValue(unit, 0x2f, 7)`: no per-event time left, so
        // `SUMMON DURATION` is not read back.
        TL_LOG_INFO("skill event: '%s' MATCH_PET_DURATION: the summon holds %.1fs",
                    source.skill.c_str(),
                    static_cast<double>(machine.unit_effects(node) != nullptr
                                            ? machine.unit_effects(node)->effect_value(0x2f)
                                            : 0.0f));
    }
    TL_LOG_INFO("skill event: '%s' summons %s (level %u)%s", source.skill.c_str(), unit.c_str(),
                player_character.level(), source.spec.make_pet ? " as a pet" : "");
}

void Renderer::Impl::add_monster_pet(tl::core::LevelScript& machine, int64_t node,
                                     tl::core::LevelScript& master_machine, int64_t master_node) {
    // `CCharacter::addPet` on a placed master: its pets
    // (`+0x648`) are the characters with that master, oldest first.
    const std::string unit = machine.unit_of(node);
    const int most = stats_for_unit(unit).maximum_pet_instances;
    if (0 < most) {
        std::vector<std::pair<tl::core::LevelScript*, int64_t>> same;
        for (const MachineView& view : machines()) {
            for (int64_t id : view.script->pets_of(&master_machine, master_node)) {
                const std::string name = view.script->unit_of(id);
                if (name != unit) continue;
                if (stats_for_unit(name).invincible_flag) continue;
                same.emplace_back(view.script, id);
            }
        }
        const int count = static_cast<int>(same.size());
        if (most <= count) {
            int left = count + 1 - most;
            for (const auto& pet : same) {
                TL_LOG_INFO("skills: %s's %s is past MAXIMUM_PET_INSTANCES %d and dies",
                            master_machine.unit_of(master_node).c_str(), unit.c_str(), most);
                kill_unit(pet.second, unit, /*fall=*/true, tl::core::LevelScript::kNoAttacker);
                if (--left == 0) break;
            }
        }
    }
    std::vector<tl::core::ScriptEvent> events;
    machine.set_unit_master(node, master_machine, master_node, quest_log.state(), events);
    if (NodeMap* nodes = nodes_of(&machine); nodes != nullptr && !events.empty()) {
        apply_script_events(events, *nodes);
    }
}

void Renderer::Impl::add_player_pet(tl::core::LevelScript& machine, int64_t node) {
    // `hasPet`: already in the player's list.
    if (machine.unit_listed_as_player_pet(node)) return;
    const std::string unit = machine.unit_of(node);
    const int most = stats_for_unit(unit).maximum_pet_instances;
    if (0 < most) {
        // The player's pets of that name (`+0x40`), oldest first; `INVINCIBLE` ones not
        // counted.
        std::vector<std::pair<tl::core::LevelScript*, int64_t>> same;
        for (const MachineView& view : machines()) {
            for (int64_t id : view.script->player_pets()) {
                const std::string name = view.script->unit_of(id);
                if (name != unit) continue;
                if (stats_for_unit(name).invincible_flag) continue;
                same.emplace_back(view.script, id);
            }
        }
        const int count = static_cast<int>(same.size());
        if (most <= count) {
            // `die` (vtable +0x330) on as many as make room for this one.
            int left = count + 1 - most;
            for (const auto& pet : same) {
                TL_LOG_INFO("skills: %s is past MAXIMUM_PET_INSTANCES %d and dies", unit.c_str(),
                            most);
                kill_unit(pet.second, unit, /*fall=*/true, tl::core::LevelScript::kNoAttacker);
                if (--left == 0) break;
            }
        }
    }
    std::vector<tl::core::ScriptEvent> events;
    machine.make_player_pet(node, quest_log.state(), events);
    if (NodeMap* nodes = nodes_of(&machine); nodes != nullptr && !events.empty()) {
        apply_script_events(events, *nodes);
    }
}

// `CMissile::doDamageToCharacter` with no listeners:
// `DAMAGE_MONSTER` (`CResourceManager::getGraphDamage`) at
// the victim's level x `randomBetweenVolatile(+0x284, +0x288)` x 1.0, no attacker.
bool Renderer::Impl::level_missile_harm(const tl::core::MissileSpec& spec, int64_t unit,
                                        const Ogre::Vector3& at, float damage_scale,
                                        const tl::core::Vec3& push) {
    const bool you = unit == kMissilePlayer;
    tl::core::LevelScript* owner = nullptr;
    std::string victim = "the player";
    int level = static_cast<int>(player_character.level());
    if (!you) {
        owner = script_owning(unit);
        if (owner == nullptr || owner->unit_is_dead(unit)) return false;
        std::vector<tl::core::LevelScript::Reachable> found;
        owner->reachable_characters(found);
        const auto it = std::find_if(found.begin(), found.end(),
                                     [unit](const auto& r) { return r.id == unit; });
        if (it == found.end()) return false;
        victim = it->unit;
        level = effective_monster_level(unit);
    } else if (player_health.dead) {
        return false;
    }
    const std::string curve =
        std::string(tl::core::kMonsterDamageGraph) + tl::core::difficulty_suffix(config.difficulty);
    const float graph = stat_graphs.value(curve, static_cast<float>(level), 0.0f);
    const float roll =
        tl::core::volatile_random().between(spec.min_damage_percent, spec.max_damage_percent);
    // `doDamageToCharacter` hands its scale only to the
    // listeners.
    (void)damage_scale;
    const float amount = graph * roll * 1.0f;
    if (you) {
        hurt_player(amount, spec.name, tl::core::LevelScript::kNoAttacker, /*struck=*/true);
    } else {
        // The direction the missile hands on, and its `FORCE` (+0x28c).
        tl::core::KillingBlow blow;
        blow.has_direction = true;
        blow.direction = push;
        blow.force = spec.knockback;
        hurt_unit(unit, victim, amount, false, 0, owner, &blow, /*struck=*/true);
    }
    TL_LOG_INFO(
        "missile: the level's %s strikes %s at (%.2f, %.2f, %.2f) for %.1f (graph %.1f x %.2f)",
        spec.name.c_str(), victim.c_str(), static_cast<double>(at.x), static_cast<double>(at.y),
        static_cast<double>(at.z), static_cast<double>(amount), static_cast<double>(graph),
        static_cast<double>(roll));
    return true;
}

bool Renderer::Impl::skill_event_reaches_height(const Ogre::Vector3& at,
                                                tl::core::LevelScript* machine, int64_t node) {
    // CSkillEvent::canEffectPosObject: the body's centre.
    float centre = 0.0f;
    if (machine == nullptr) {
        const float body =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        centre = player.position().y + body;
    } else {
        tl::core::Vec3 feet;
        if (!machine->unit_position(node, feet)) return false;
        centre = feet.y + machine->unit_radius(node);
    }
    return std::fabs(at.y - centre) <= 3.0f;
}

void Renderer::Impl::fire_missile_hit_event(const SkillEventSource& source,
                                            tl::core::LevelScript* machine, int64_t node,
                                            const Ogre::Vector3& at, float yaw) {
    // `missileApplyingEffects`: not for a turned-back missile (`+0x294`, never
    // set here) or an `EVENT_MISSILEHIT` event.
    constexpr int kMissileHit = 6;
    if (source.group == kMissileHit) return;
    // Whether the skill authors the group at all, for the log.
    const auto authors_it = [&source](const tl::core::KnownSkill& skill) {
        const tl::core::SkillProperty* property = skill.property(source.level);
        if (property == nullptr || property->section == nullptr) return false;
        for (const tl::formats::AdmSection& child : property->event_section().children) {
            if (tl::core::upper_ascii(child.name) == "EVENT_MISSILEHIT") return true;
        }
        return false;
    };
    tl::core::SkillEventTarget target;
    target.run = source.run;
    target.origin = tl::core::Vec3{at.x, at.y, at.z};
    target.yaw = yaw;
    target.has_unit = true;
    target.unit = machine != nullptr ? node : 0;
    target.machine = machine;
    if (source.caster_machine == nullptr) {
        const tl::core::KnownSkill* skill = player_skill(source.skill);
        if (skill == nullptr) return;
        tl::core::SkillCastContext context = player_cast_context();
        context.event_target = target;
        tl::core::SkillRunner& runner = skill_runner_running(skill->entry.name);
        const bool running = runner.is_running(skill->entry.name);
        runner.trigger_skill_event(*skill, kMissileHit, context);
        if (authors_it(*skill)) {
            TL_LOG_INFO("missile: '%s' EVENT_MISSILEHIT%s", source.skill.c_str(),
                        running ? "" : " (the skill is not running: nothing)");
        }
        return;
    }
    tl::core::LevelScript& owner = *source.caster_machine;
    tl::core::LevelScript::UnitCasting* casting = owner.unit_casting(source.caster_node);
    const tl::core::KnownSkill* skill = unit_skill_named(owner, source.caster_node, source.skill);
    if (casting == nullptr || skill == nullptr) return;
    // The hit can come mid-cast of another unit (`unit_cast_caster`): put back after.
    const tl::core::SkillCaster outer_caster = unit_cast_caster;
    tl::core::SkillCastContext context =
        unit_cast_context(owner, source.caster_node, machine, node, true);
    context.event_target = target;
    const UnitEventCaster saved = unit_event_caster;
    unit_event_caster = unit_event_caster_for(&owner, source.caster_node);
    const bool running = casting->runner.is_running(skill->entry.name);
    casting->runner.trigger_skill_event(*skill, kMissileHit, context);
    unit_event_caster = saved;
    unit_cast_caster = outer_caster;
    if (authors_it(*skill)) {
        TL_LOG_INFO("missile: %s's '%s' EVENT_MISSILEHIT%s",
                    owner.unit_of(source.caster_node).c_str(), source.skill.c_str(),
                    running ? "" : " (the skill is not running: nothing)");
    }
}

bool Renderer::Impl::missile_harm(const SkillEventSource& source, int64_t unit, float soak_scale,
                                  const Ogre::Vector3& at, float damage_scale, float yaw) {
    if (source.caster_machine != nullptr) {
        // A placed unit's missile: `CSkillEvent::getCharacterCanBeHarmedByMissile`
        // from its owner's side (`MISSILE_TARGET_SELF`, `TARGET_CORPSES`,
        // `EVERYBODY`, else `CCharacter::isEnemy` against `TARGET_ALIGNMENT`). INVENTED: as in
        // the player's half, `SINGLETARGET` and the rest are not read.
        const bool you = unit == kMissilePlayer;
        tl::core::LevelScript* machine = you ? nullptr : script_owning(unit);
        const int64_t node = you ? 0 : unit;
        if (!you && machine == nullptr) return false;
        const bool self = !you && machine == source.caster_machine && unit == source.caster_node;
        const bool dead = you ? player_health.dead : machine->unit_is_dead(unit);
        const bool target_self =
            source.event != nullptr && source.event->bool_or("MISSILE_TARGET_SELF", false);
        if (!(target_self && self)) {
            if (source.spec.target_corpses ? !dead : dead) return false;
            if (source.target_type != 4) {
                const bool enemy = !self && source.caster_machine->unit_is_enemy_of(
                                                source.caster_node, machine, node);
                if (source.target_alignment == 2   ? !enemy
                    : source.target_alignment == 1 ? enemy
                                                   : true) {
                    return false;
                }
            }
        }
        const std::string victim = you ? std::string("the player") : machine->unit_of(unit);
        // `missileApplyingEffects`: `canEffectPosObject`,
        // `EVENT_MISSILEHIT`, `applyWeaponDamage`, `applyEffects`,
        // `invokeHitSkills`.
        if (!skill_event_reaches_height(at, machine, node)) return true;
        fire_missile_hit_event(source, machine, node, at, yaw);
        const float weapon_part =
            skill_event_weapon_blow(source, machine, node, damage_scale, soak_scale);
        const bool applied = skill_event_effects_on(source, machine, node);
        invoke_hit_skills(source, source.group, machine, node);
        TL_LOG_INFO("missile: %s's '%s' strikes %s at (%.2f, %.2f, %.2f): weapon %.1f%s",
                    source.caster_machine->unit_of(source.caster_node).c_str(),
                    source.skill.c_str(), victim.c_str(), static_cast<double>(at.x),
                    static_cast<double>(at.y), static_cast<double>(at.z),
                    static_cast<double>(weapon_part), applied ? ", effects applied" : "");
        return true;
    }
    tl::core::LevelScript* owner = script_owning(unit);
    if (owner == nullptr || owner->unit_is_dead(unit)) return false;
    std::vector<tl::core::LevelScript::Reachable> found;
    owner->reachable_characters(found);
    const auto it =
        std::find_if(found.begin(), found.end(), [unit](const auto& r) { return r.id == unit; });
    if (it == found.end()) return false;
    const tl::core::LevelScript::Reachable target = *it;

    // `CSkillEvent::getCharacterCanBeHarmedByMissile`, its last test:
    // `TARGET_ALIGNMENT` from the player's side; a unit it may not affect is flown through.
    using Alignment = tl::core::UnitStats::Alignment;
    bool affects = false;
    switch (source.target_alignment) {
        case 0: affects = target.alignment == Alignment::Neutral; break;
        case 1: affects = target.alignment == Alignment::Friendly; break;
        case 3: affects = true; break;
        default: affects = target.alignment == Alignment::Hostile; break;
    }
    if (!affects) return false;

    // `missileApplyingEffects`: the height test
    // (`canEffectPosObject`), `EVENT_MISSILEHIT`, then
    // `applyWeaponDamage`.
    if (!skill_event_reaches_height(at, owner, unit)) return true;
    fire_missile_hit_event(source, owner, unit, at, yaw);
    const float weapon_part =
        skill_event_weapon_blow(source, owner, unit, damage_scale, soak_scale);

    // `applyEffects` onto the unit's own manager
    // (`skill_event_effects_on`). INVENTED: resistances are not applied to effect damage.
    float effect_part = 0.0f;
    if (source.event != nullptr && !owner->unit_is_dead(unit)) {
        float before = 0.0f;
        float max = 0.0f;
        owner->unit_health(unit, before, max);
        skill_event_effects_on(source, owner, unit);
        float after = 0.0f;
        owner->unit_health(unit, after, max);
        effect_part = std::max(0.0f, before - after);
    }
    // `invokeHitSkills`.
    invoke_hit_skills(source, source.group, owner, unit);
    TL_LOG_INFO("missile: '%s' strikes %s at (%.2f, %.2f, %.2f): weapon %.1f, effects %.1f",
                source.skill.c_str(), target.unit.c_str(), static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z),
                static_cast<double>(weapon_part), static_cast<double>(effect_part));
    return true;
}

namespace renderer_detail {
/// The level's characters a missile can meet, gathered across every machine.
std::vector<tl::core::LevelScript::Reachable> missile_candidates(
    const std::vector<tl::core::LevelScript*>& machines) {
    std::vector<tl::core::LevelScript::Reachable> out;
    for (const tl::core::LevelScript* machine : machines) machine->reachable_characters(out);
    return out;
}

/// The first `t` in [0, 1] at which a sphere of `radius` from `from` to `to` touches a
/// sphere of `other` at `centre`, or a value above 1.
float sphere_sweep_t(const tl::core::Vec3& from, const tl::core::Vec3& to, float radius,
                     const tl::core::Vec3& centre, float other) {
    const tl::core::Vec3 d = to - from;
    const tl::core::Vec3 f = from - centre;
    const float reach = radius + other;
    const float c = f.x * f.x + f.y * f.y + f.z * f.z - reach * reach;
    if (c <= 0.0f) return 0.0f;
    const float a = d.x * d.x + d.y * d.y + d.z * d.z;
    if (!(a > 0.0f)) return 2.0f;
    const float b = 2.0f * (f.x * d.x + f.y * d.y + f.z * d.z);
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return 2.0f;
    const float t = (-b - std::sqrt(disc)) / (2.0f * a);
    return t >= 0.0f ? t : 2.0f;
}

/// The first `t` in [0, 1] at which a sphere of `radius` from `from` to `to` touches the
/// box `low`..`high` (grown by the radius, cut slab by slab), or a value above 1.
float box_sweep_t(const tl::core::Vec3& from, const tl::core::Vec3& to, float radius,
                  const tl::core::Vec3& low, const tl::core::Vec3& high) {
    const float start[3] = {from.x, from.y, from.z};
    const float delta[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
    const float lo[3] = {low.x - radius, low.y - radius, low.z - radius};
    const float hi[3] = {high.x + radius, high.y + radius, high.z + radius};
    float enter = 0.0f;
    float leave = 1.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(delta[axis]) < 1e-8f) {
            if (start[axis] < lo[axis] || hi[axis] < start[axis]) return 2.0f;
            continue;
        }
        float a = (lo[axis] - start[axis]) / delta[axis];
        float b = (hi[axis] - start[axis]) / delta[axis];
        if (b < a) std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        if (leave < enter) return 2.0f;
    }
    return enter;
}

/// `CLevel::findCharacterWithinView` as a missile asks it (`CMissile::getClosestTarget`):
/// the nearest enemy of the player's in the cone.
int64_t missile_closest(const std::vector<tl::core::LevelScript::Reachable>& candidates,
                        const tl::core::Vec3& at, const tl::core::Vec3& forward, float angle,
                        float sight) {
    const float facing = std::atan2(forward.x, forward.z);
    int64_t best = 0;
    float best_d = 0.0f;
    for (const auto& u : candidates) {
        if (u.alignment != tl::core::UnitStats::Alignment::Hostile) continue;
        if (u.mimic_idle) continue;  // `findCharacterWithinView` passes it over
        if (!tl::core::can_see(at, facing, u.center, 0.0f, angle, sight)) continue;
        const float dx = u.center.x - at.x;
        const float dz = u.center.z - at.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (best == 0 || d < best_d) {
            best = u.id;
            best_d = d;
        }
    }
    return best;
}

bool missile_unit(const std::vector<tl::core::LevelScript::Reachable>& candidates, int64_t id,
                  tl::core::Vec3& c, float& r, bool& alive) {
    for (const auto& u : candidates) {
        if (u.id != id) continue;
        c = u.center;
        r = u.radius;
        alive = true;
        return true;
    }
    return false;
}

}  // namespace renderer_detail

namespace {
template <typename ImplT>
std::vector<tl::core::LevelScript::Reachable> gather_missile_candidates(const ImplT* impl,
                                                                        bool level_placed) {
    std::vector<tl::core::LevelScript::Reachable> pool =
        renderer_detail::missile_candidates(impl->level_machines);
    if (level_placed && !impl->player_health.dead && impl->subject_node != nullptr) {
        const float body = impl->player_stats.radius > 0.0f ? impl->player_stats.radius
                                                            : tl::core::kDefaultCollisionRadius;
        const Ogre::Vector3 feet = impl->subject_node->getPosition();
        tl::core::LevelScript::Reachable you;
        you.id = kMissilePlayer;
        you.unit = "the player";
        you.center = {feet.x, feet.y + body, feet.z};
        you.radius = body;
        you.alignment = tl::core::UnitStats::Alignment::Friendly;
        pool.push_back(you);
    }
    return pool;
}

template <typename ImplT, typename FlyingT>
std::function<bool(int64_t, const tl::core::Vec3&, float, float)> make_missile_harm_hook(
    ImplT* impl, const FlyingT& flying, std::vector<tl::core::LevelScript::Reachable>& candidates,
    const Ogre::Vector3& struck_at,
    const std::function<std::vector<tl::core::LevelScript::Reachable>()>& gather) {
    return [impl, &flying, &candidates, &struck_at, gather](
               int64_t unit, const tl::core::Vec3& push, float damage_scale, float soak) {
        const bool landed =
            flying.level_placed || flying.detached
                ? impl->level_missile_harm(flying.missile.spec, unit, struck_at, damage_scale, push)
            : flying.by_weapon
                ? (flying.source.caster_machine != nullptr
                       ? impl->unit_weapon_missile_harm(flying.source.caster_machine,
                                                        flying.source.caster_node, unit, struck_at,
                                                        damage_scale, soak)
                       : impl->weapon_missile_harm(unit, struck_at, damage_scale, soak))
                : impl->missile_harm(
                      flying.source, unit, soak, struck_at, damage_scale,
                      std::atan2(flying.missile.forward.x, flying.missile.forward.z));
        if (landed) candidates = gather();
        return landed;
    };
}
}  // namespace

// `CBaseUnit::sphereCollision(start, end, radius, ...)`.
bool Renderer::Impl::unit_collider_sweep(int64_t id, const tl::core::Vec3& from,
                                         const tl::core::Vec3& to, float radius, float& t) {
    t = 2.0f;
    const Ogre::Entity* entity = nullptr;
    for (const auto& entry : scene_units) {
        if (entry.second.node == id && entry.second.entity != nullptr) entity = entry.second.entity;
    }
    if (entity == nullptr) {
        for (const auto& owner : spawned) {
            for (const SpawnedUnit& made : owner.second) {
                if (made.id == id && made.entity != nullptr) entity = made.entity;
            }
        }
    }
    if (entity == nullptr || !entity->isVisible() || entity->getParentSceneNode() == nullptr) {
        return false;
    }
    // `getUnitCollisionModel` (vtable +0x1e8, `+0x4c8`): `CCharacter::loadModel`
    // reads `COLLISIONFILE`, else `media/models/collider.mesh`.
    // INVENTED: the 26 units with their own `collider` / `colliderbig` get it too.
    if (!unit_collider_read) {
        unit_collider_read = true;
        const CollisionModel& model = collision_model("media/models/collider.mesh");
        size_t faces = 0;
        for (const CollisionModel::Submesh& sub : model.submeshes) {
            for (size_t i = 0; i + 2 < sub.corners.size(); i += 3) {
                const Ogre::Vector3& a = sub.points[sub.corners[i]];
                const Ogre::Vector3& b = sub.points[sub.corners[i + 1]];
                const Ogre::Vector3& c = sub.points[sub.corners[i + 2]];
                unit_collider.add_triangle({a.x, a.y, a.z}, {b.x, b.y, b.z}, {c.x, c.y, c.z});
                ++faces;
            }
        }
        unit_collider.build();
        if (model.has_bounds) {
            TL_LOG_INFO(
                "units: collider.mesh has %zu face(s), (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)",
                faces, static_cast<double>(model.low.x), static_cast<double>(model.low.y),
                static_cast<double>(model.low.z), static_cast<double>(model.high.x),
                static_cast<double>(model.high.y), static_cast<double>(model.high.z));
        } else {
            TL_LOG_WARN("units: media/models/collider.mesh gave %zu face(s)", faces);
        }
    }
    if (unit_collider.empty()) return false;

    // The unit's bounds first; then both ends into the unit's frame: less its
    // `getPosition(true)`, through the inverse of its node's orientation, no scale.
    const Ogre::SceneNode* node = entity->getParentSceneNode();
    const Ogre::Vector3 at = node->_getDerivedPosition();
    const Ogre::Quaternion inverse = node->_getDerivedOrientation().Inverse();
    const Ogre::Vector3 a = inverse * (Ogre::Vector3(from.x, from.y, from.z) - at);
    const Ogre::Vector3 b = inverse * (Ogre::Vector3(to.x, to.y, to.z) - at);
    const tl::core::Vec3 start{a.x, a.y, a.z};
    const tl::core::Vec3 end{b.x, b.y, b.z};
    // `CCollisionList::sphereCollision` over the model's faces (`+0x38`).
    unit_collider.sort_for_collision(
        {std::min(a.x, b.x) - radius, std::min(a.y, b.y) - radius, std::min(a.z, b.z) - radius},
        {std::max(a.x, b.x) + radius, std::max(a.y, b.y) + radius, std::max(a.z, b.z) + radius});
    tl::core::CollisionHit hit;
    if (!unit_collider.pre_sorted_sphere_collision(start, end, radius, /*objects=*/false, hit)) {
        return true;
    }
    const tl::core::Vec3 d = end - start;
    const tl::core::Vec3 m = hit.centre - start;
    const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
    t = len2 > 0.0f ? std::sqrt((m.x * m.x + m.y * m.y + m.z * m.z) / len2) : 0.0f;
    return true;
}

const tl::formats::CmpNode* Renderer::Impl::missile_named(const std::string& name) {
    // `CMissilePreloader::CMissilePreloader` loads
    // nothing until asked; `CMissilePreloader::reloadMissiles`:
    // every missile `*.layout` (`CMissilePreloader::loadMissile`),
    // each `Missile` kept by its upper-cased `MISSILE NAME` (`CMissile +0x2c8`),
    // which a spawner's `RESOURCE` names. Empty names skipped; the first of a name kept.
    // INVENTED: `getFileList`'s order is not read; the sorted paths.
    if (!missiles_indexed) {
        missiles_indexed = true;
        for (const std::string& path : archive_paths_under("media/missiles/", ".layout.cmp")) {
            const std::string reference = path.substr(0, path.size() - 4);
            const tl::formats::CmpScene* layout = cached_layout(reference);
            if (layout == nullptr) continue;
            for (const tl::formats::CmpNode& n : layout->nodes) {
                if (n.type != tl::formats::cmp_type::kMissile) continue;
                std::string key = tl::core::read_missile_spec(n).name;
                if (key.empty()) continue;
                missiles_by_name.emplace(std::move(key), &n);
            }
        }
        TL_LOG_INFO("missiles: %zu by name from media/missiles/", missiles_by_name.size());
    }
    const auto found = missiles_by_name.find(tl::core::upper_ascii(name));
    return found != missiles_by_name.end() ? found->second : nullptr;
}

void Renderer::Impl::fire_spawner_missile(int64_t node_id, const std::string& resource,
                                          const Ogre::Vector3& at, const Ogre::Vector3& forward) {
    const auto found = skill_spawners.find(node_id);
    // A spawner no skill placed is the level's: no owner, no listeners
    // (`createAndFireMissile`).
    const bool level_placed = found == skill_spawners.end();
    const SkillSpawner* spawner = level_placed ? nullptr : &found->second;
    // `CResourceManager::createMissile` -> `CMissilePreloader::createNewMissileRef`:
    // the missile of that name, upper-cased.
    const tl::formats::CmpNode* node = missile_named(resource);
    if (node == nullptr) {
        TL_LOG_WARN("missiles: no missile named '%s' in media/missiles/", resource.c_str());
        return;
    }
    const tl::core::MissileSpec spec = tl::core::read_missile_spec(*node);

    // The owner: the player, its body sphere and target; none for a level's own missile.
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    const Ogre::Vector3 feet =
        subject_node != nullptr ? subject_node->getPosition() : Ogre::Vector3::ZERO;
    // A placed unit's skill: the owner is that unit, and the missile can meet
    // the player as well as the level's characters.
    tl::core::LevelScript* const caster = level_placed ? nullptr : spawner->source.caster_machine;
    const int64_t caster_node = level_placed ? 0 : spawner->source.caster_node;
    const bool by_unit = caster != nullptr;
    const auto gather = [this, level_placed, by_unit]() {
        return gather_missile_candidates(this, level_placed || by_unit);
    };
    std::vector<tl::core::LevelScript::Reachable> candidates = gather();
    int64_t target = 0;
    if (by_unit) {
        // The owner's target (`CCharacter +0x340`).
        tl::core::LevelScript* target_machine = nullptr;
        int64_t target_node = 0;
        if (caster->unit_target(caster_node, target_machine, target_node)) {
            target = target_machine == nullptr ? kMissilePlayer : target_node;
        }
    } else if (!level_placed) {
        for (const auto& r : candidates) {
            if (r.id == player_target_node &&
                r.alignment == tl::core::UnitStats::Alignment::Hostile) {
                target = r.id;
            }
        }
    }

    FlyingMissile flying;
    flying.level_placed = level_placed;
    if (!level_placed) flying.source = spawner->source;
    flying.missile = tl::core::missile_from_template(spec);
    flying.missile.owner = 0;
    if (by_unit) {
        flying.missile.owner = caster_node;
        tl::core::Vec3 caster_feet;
        if (caster->unit_position(caster_node, caster_feet)) {
            const float radius = caster->unit_radius(caster_node);
            flying.missile.owner_center = {caster_feet.x, caster_feet.y + radius, caster_feet.z};
            flying.missile.owner_radius = radius;
        }
    } else if (!level_placed) {
        flying.missile.owner_center = {feet.x, feet.y + body, feet.z};
        flying.missile.owner_radius = body;
    }
    flying.missile.target = target;

    // `CMissile::fireMissile`: on first launch (`+0x1b5`) the event's
    // `CSkillEvent::missileValidateTargetBeforeLaunch`
    // settles the target: `MISSILE_TARGET_SELF` (`+0x14f`) the owner (hittable, `+0x144`);
    // target type 0 nobody; a target unit (`CSkill +0x40`) unless type 1 or 11; else the
    // event's point.
    if (!level_placed && spawner->source.event != nullptr && !flying.missile.fired) {
        if (spawner->source.event->bool_or("MISSILE_TARGET_SELF", false)) {
            // INVENTED: aimed at the target's centre, not the owner's bone
            // (`getBonePosition(owner, 1, 1)`); a player-owned one cannot meet the player.
            if (by_unit) {
                flying.missile.target = caster_node;
                flying.missile.hits_owner = true;
            }
        } else if (spawner->source.target_type == 0) {
            flying.missile.target = 0;
        }
        // INVENTED: the skill's target (`+0x40`) is not kept per run; the owner's stands in.
    }

    launch_missile(std::move(flying), at, forward,
                   level_placed ? std::string("the level") : spawner->source.skill);
}

// `CMissile::fireMissile`, with the world it flies in.
void Renderer::Impl::launch_missile(FlyingMissile flying, const Ogre::Vector3& at,
                                    const Ogre::Vector3& forward, const std::string& who) {
    const tl::core::MissileSpec spec = flying.missile.spec;
    const bool level_placed = flying.level_placed;
    const int64_t target = flying.missile.target;
    // What it can meet: the level's characters, and the player for a level's or unit's.
    const bool with_player = level_placed || flying.source.caster_machine != nullptr;
    const auto gather = [this, with_player]() {
        return gather_missile_candidates(this, with_player);
    };
    std::vector<tl::core::LevelScript::Reachable> candidates = gather();

    tl::core::MissileWorld world;
    world.unit = [&candidates](int64_t id, tl::core::Vec3& c, float& r, bool& alive) {
        return missile_unit(candidates, id, c, r, alive);
    };
    world.closest_target = [&candidates](const tl::core::Vec3& from, const tl::core::Vec3& fwd,
                                         float angle, float sight) {
        return missile_closest(candidates, from, fwd, angle, sight);
    };

    // For fire_missile's point-blank hit.
    Ogre::Vector3 struck_at = at;
    world.harm = make_missile_harm_hook(this, flying, candidates, struck_at, gather);
    world.get_active_units_at_position = [this, &candidates, &spec](
                                             const tl::core::Vec3& position, float radius,
                                             std::vector<int64_t>* items_out) {
        std::vector<tl::core::ActiveUnitCandidate> active_candidates;
        active_candidates.reserve(candidates.size());
        for (const auto& c : candidates) {
            active_candidates.push_back({c.id, c.center});
        }
        std::vector<int64_t> hit = tl::core::get_active_units_at_position(
            active_candidates, position, radius, breakable_item_candidates(), items_out);
        TL_LOG_INFO("missile: '%s' area %.2f at (%.2f, %.2f, %.2f): %zu unit(s), %zu item(s)",
                    spec.name.c_str(), static_cast<double>(radius), static_cast<double>(position.x),
                    static_cast<double>(position.y), static_cast<double>(position.z), hit.size(),
                    items_out != nullptr ? items_out->size() : size_t{0});
        return hit;
    };
    world.interact_item = [this](int64_t item) { return queue_item_interact(item); };
    world.unit_kind = [this](int64_t unit) {
        return standing_breakable(unit) != nullptr ? tl::core::MissileUnitKind::Item
                                                   : tl::core::MissileUnitKind::Character;
    };
    // `CLevel::rayCollision`, objects included.
    world.ray = [this](const tl::core::Vec3& from, const tl::core::Vec3& to,
                       tl::core::Vec3& point) {
        tl::core::CollisionHit hit;
        if (!collision.ray_collision(from, to, /*objects=*/true, hit)) return false;
        point = hit.point;
        return true;
    };
    world.volatile_rolls = &tl::core::volatile_random();
    world.reflect = [this](int64_t unit) {
        return missile_reflect_share(unit == kMissilePlayer ? 0 : unit);
    };
    tl::core::fire_missile(flying.missile, tl::core::Vec3{at.x, at.y, at.z},
                           tl::core::Vec3{forward.x, forward.y, forward.z}, world);

    const tl::core::Vec3& p = flying.missile.position;
    flying.node = scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(p.x, p.y, p.z));
    const tl::core::Vec3& f = flying.missile.forward;
    flying.node->setDirection(Ogre::Vector3(f.x, f.y, f.z), Ogre::Node::TS_WORLD,
                              Ogre::Vector3::UNIT_Z);
    flying.active = place_particle_layout(spec.active, Ogre::Vector3::ZERO,
                                          Ogre::Quaternion::IDENTITY, flying.node);
    if (!spec.release.empty()) {
        transient_particles.push_back(
            TransientParticles{place_particle_layout(spec.release, Ogre::Vector3(p.x, p.y, p.z),
                                                     flying.node->getOrientation(), nullptr),
                               kMissileParticleStopSeconds});
    }
    missiles.push_back(std::move(flying));
    if (level_placed) {
        TL_LOG_INFO("missiles: %s deals %.3f to %.3f of the damage graph", spec.name.c_str(),
                    static_cast<double>(spec.min_damage_percent),
                    static_cast<double>(spec.max_damage_percent));
    }
    TL_LOG_INFO("missiles: '%s' fires %s from (%.2f, %.2f, %.2f) along (%.2f, %.2f, %.2f)%s",
                who.c_str(), spec.name.c_str(), static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z),
                static_cast<double>(forward.x), static_cast<double>(forward.y),
                static_cast<double>(forward.z), target != 0 ? ", at its target" : "");
}

// `CEquipment::fireMissiles(CCharacter*, CCharacter*)`.
bool Renderer::Impl::fire_weapon_missiles(int64_t target, int breakable) {
    // no `MISSILE` (`+0x400`), no missile.
    if (!has_weapon || weapon.missile.empty() || subject_node == nullptr) return false;

    // The hand's tag point (`+0x2e8`, or `+0x2f8` left; vtable +0x200). INVENTED: with no
    // weapon model, the character's centre.
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    const Ogre::Vector3 feet = subject_node->getPosition();
    const bool drawn = weapon_entity != nullptr && weapon_entity->getParentNode() != nullptr;
    Ogre::Vector3 start = drawn ? weapon_entity->getParentNode()->_getDerivedPosition()
                                : feet + Ogre::Vector3(0.0f, body, 0.0f);

    // `getForwardAbsolute` (vtable +0x138), or from the hand to the target's centre.
    Ogre::Vector3 direction = subject_node->_getDerivedOrientation() * Ogre::Vector3::UNIT_Z;
    tl::core::Vec3 aim;
    bool aimed = false;
    if (target != 0) {
        if (tl::core::LevelScript* owner = script_owning(target)) {
            aimed = owner->unit_position(target, aim);
        }
    } else if (breakable >= 0 && breakables[static_cast<size_t>(breakable)].node != nullptr) {
        const Ogre::Vector3 p = breakables[static_cast<size_t>(breakable)].node->getPosition();
        aim = tl::core::Vec3{p.x, p.y, p.z};
        aimed = true;
    }
    if (aimed) {
        direction = Ogre::Vector3(aim.x - start.x, aim.y - start.y, aim.z - start.z);
        const float length = direction.length();
        if (1e-08f < length) direction = direction * (1.0f / length);
    }

    // out to the tip, the model's box height (vtable +0xd8) times
    // `WEAPON_SCALE * 0.8`; one unit with no model.
    if (drawn) {
        const Ogre::AxisAlignedBox& box = weapon_entity->getBoundingBox();
        const float height = box.isFinite() ? box.getMaximum().y - box.getMinimum().y : 0.0f;
        start += direction * height * (player_stats.weapon_scale * 0.8f);
    } else {
        start += direction;
    }

    // `CMissilePreloader::createNewMissileRef`; none of
    // that name fires nothing.
    const tl::formats::CmpNode* node = missile_named(weapon.missile);
    if (node == nullptr) {
        TL_LOG_WARN("missiles: the weapon's '%s' is not a missile in media/missiles/",
                    weapon.missile.c_str());
        return false;
    }

    // `CMissile::fireMissile(start, target or ZERO, missile, getEquippedTo(), ..., target)`,
    // the weapon its listener.
    FlyingMissile flying;
    flying.by_weapon = true;
    flying.missile = tl::core::missile_from_template(tl::core::read_missile_spec(*node));
    flying.missile.owner = 0;
    flying.missile.owner_center = {feet.x, feet.y + body, feet.z};
    flying.missile.owner_radius = body;
    flying.missile.target = target;
    launch_missile(std::move(flying), start, direction, "the weapon");
    return true;
}

// `CEquipment::fireMissiles(CCharacter*, CCharacter*)`,
// for a placed character.
void Renderer::Impl::fire_unit_weapon_missile(tl::core::LevelScript* machine, int64_t node,
                                              const std::string& missile, bool left, bool aimed,
                                              tl::core::LevelScript* target_machine,
                                              int64_t target_node) {
    if (machine == nullptr || missile.empty()) return;
    tl::core::Vec3 feet;
    if (!machine->unit_position(node, feet)) return;
    const std::string unit = machine->unit_of(node);
    const tl::core::UnitStats stats = stats_for_unit(unit);
    const float radius = machine->unit_radius(node);

    // The hand's node (`+0x2f8` left, else `+0x2e8`). INVENTED: no item drawn, the centre.
    Ogre::Entity* body = nullptr;
    for (const auto& entry : scene_units) {
        if (entry.second.node == node) body = entry.second.entity;
    }
    if (body == nullptr) {
        for (const auto& owner : spawned) {
            for (const SpawnedUnit& made : owner.second) {
                if (made.id == node) body = made.entity;
            }
        }
    }
    Ogre::Entity* held = nullptr;
    if (body != nullptr) {
        const auto found = unit_held_items.find(body);
        if (found != unit_held_items.end()) held = found->second.held[left ? 1 : 0];
    }
    const bool drawn = held != nullptr && held->getParentNode() != nullptr;
    Ogre::Vector3 start = drawn ? held->getParentNode()->_getDerivedPosition()
                                : Ogre::Vector3(feet.x, feet.y + radius, feet.z);

    // `getForwardAbsolute`, or from the hand to the target's centre.
    float facing = 0.0f;
    machine->unit_facing(node, facing);
    Ogre::Vector3 direction(std::sin(facing), 0.0f, std::cos(facing));
    int64_t target = 0;
    if (aimed) {
        tl::core::Vec3 aim;
        bool have = false;
        if (target_machine == nullptr) {
            if (subject_node != nullptr) {
                const float own = player_stats.radius > 0.0f ? player_stats.radius
                                                             : tl::core::kDefaultCollisionRadius;
                const Ogre::Vector3 p = subject_node->getPosition();
                aim = {p.x, p.y + own, p.z};
                have = true;
                target = kMissilePlayer;
            }
        } else if (target_machine->unit_position(target_node, aim)) {
            aim.y += target_machine->unit_radius(target_node);
            have = true;
            target = target_node;
        }
        if (have) {
            direction = Ogre::Vector3(aim.x - start.x, aim.y - start.y, aim.z - start.z);
            const float length = direction.length();
            if (1e-08f < length) direction = direction * (1.0f / length);
        }
    }

    // Out to the weapon's tip: its model's height times `WEAPON_SCALE * 0.8`;
    // one unit with no model.
    if (drawn) {
        const Ogre::AxisAlignedBox& box = held->getBoundingBox();
        const float height = box.isFinite() ? box.getMaximum().y - box.getMinimum().y : 0.0f;
        const float scale = stats.weapon_scale > 0.0f ? stats.weapon_scale : 1.0f;
        start += direction * height * (scale * 0.8f);
    } else {
        start += direction;
    }

    const tl::formats::CmpNode* spec = missile_named(missile);
    if (spec == nullptr) {
        TL_LOG_WARN("missiles: %s's weapon '%s' is not a missile in media/missiles/", unit.c_str(),
                    missile.c_str());
        return;
    }
    // `CMissile::fireMissile(start, ..., getEquippedTo(), ..., target)`.
    FlyingMissile flying;
    flying.by_weapon = true;
    flying.source.caster_machine = machine;
    flying.source.caster_node = node;
    flying.missile = tl::core::missile_from_template(tl::core::read_missile_spec(*spec));
    flying.missile.owner = node;
    flying.missile.owner_center = {feet.x, feet.y + radius, feet.z};
    flying.missile.owner_radius = radius;
    flying.missile.target = target;
    launch_missile(std::move(flying), start, direction, unit + "'s weapon");
}

// `CEquipment::missileApplyingEffects` and
// `CEquipment::getCharacterCanBeHarmedByMissile`, for a
// placed wielder: a turned-back missile (`CMissile +0x294`, X360 `+0x28c`, never set here)
// harms anyone; else the wielder's `isEnemy`.
bool Renderer::Impl::unit_weapon_missile_harm(tl::core::LevelScript* wielder, int64_t wielder_node,
                                              int64_t unit, const Ogre::Vector3& at,
                                              float damage_scale, float soak_scale) {
    const bool you = unit == kMissilePlayer;
    tl::core::LevelScript* machine = you ? nullptr : script_owning(unit);
    if (!you && machine == nullptr) return false;
    const int64_t node = you ? 0 : unit;
    if (you ? player_health.dead : machine->unit_is_dead(unit)) return false;
    if (!wielder->unit_is_enemy_of(wielder_node, machine, node)) return false;
    // `rollAttack(level, unit, weapon, flags 0, damage scale, soak scale, 7)`.
    std::vector<tl::core::ScriptEvent> events;
    if (!wielder->unit_skill_blow(wielder_node, machine, node, damage_scale, soak_scale, events)) {
        return true;
    }
    const float amount = events.empty() ? 0.0f : events.back().amount;
    if (NodeMap* nodes = nodes_of(wielder)) apply_script_events(events, *nodes);
    TL_LOG_INFO("missile: %s's weapon strikes %s at (%.2f, %.2f, %.2f): %.1f",
                wielder->unit_of(wielder_node).c_str(),
                you ? "the player" : machine->unit_of(unit).c_str(), static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(amount));
    return true;
}

// `CEquipment::missileApplyingEffects`.
bool Renderer::Impl::weapon_missile_harm(int64_t unit, const Ogre::Vector3& at, float damage_scale,
                                         float soak_scale) {
    tl::core::LevelScript* owner = script_owning(unit);
    if (owner == nullptr || owner->unit_is_dead(unit)) return false;
    // `CCharacter::isEnemy(unit, the missile's owner)`, as in
    // `getCharacterCanBeHarmedByMissile`.
    if (!owner->unit_is_enemy_of(unit, nullptr, 0)) return false;
    // `getEquippedTo()->rollAttack(level, unit, weapon, 0, damage scale, soak scale, 7)`.
    const std::string name = owner->unit_of(unit);
    const float dealt = player_blow(unit, damage_scale, soak_scale);
    const bool critical = last_blow_critical;
    const tl::core::KillingBlow killing = last_killing_blow;
    show_player_blow(unit, name, dealt);
    if (dealt > 0.0f) hurt_unit(unit, name, dealt, critical, 0, owner, &killing, /*struck=*/true);
    slow_on_player_kill(owner->unit_is_dead(unit));
    TL_LOG_INFO("missile: the weapon's '%s' strikes %s at (%.2f, %.2f, %.2f): %.1f",
                weapon.missile.c_str(), name.c_str(), static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(dealt));
    return true;
}

void Renderer::Impl::step_missiles(float delta) {
    for (size_t i = 0; i < transient_particles.size();) {
        TransientParticles& transient = transient_particles[i];
        transient.left -= delta;
        if (transient.left > 0.0f) {
            ++i;
            continue;
        }
        // `CParticle::Stop(false)`, then wait for `getNumberOfParticlesUpdating`
        // to reach zero.
        bool alive = false;
        for (ParticleSystem& system : particle_systems) {
            Ogre::SceneNode* holder = system.holder();
            if (holder == nullptr) continue;
            if (std::find(transient.holders.begin(), transient.holders.end(), holder) ==
                transient.holders.end()) {
                continue;
            }
            if (!transient.stopped && !transient.self_timed) stop_particle_emission(system);
            // Alive until `CParticle::updateLevelObject` has stopped it.
            if (!system.finished) alive = true;
        }
        transient.stopped = true;
        if (alive) {
            ++i;
            continue;
        }
        destroy_particle_holders(transient.holders);
        transient_particles.erase(transient_particles.begin() + static_cast<long>(i));
    }
    flush_item_interacts();
    if (missiles.empty() || !(delta > 0.0f)) return;

    for (size_t i = 0; i < missiles.size();) {
        FlyingMissile& flying = missiles[i];
        // What it can meet: the characters, and the player for a level's missile.
        const auto gather = [this, &flying]() {
            return gather_missile_candidates(
                this, flying.level_placed || flying.source.caster_machine != nullptr);
        };
        std::vector<tl::core::LevelScript::Reachable> candidates = gather();
        const SkillEventSource source = flying.source;
        Ogre::Vector3 struck_at = Ogre::Vector3::ZERO;

        tl::core::MissileWorld world;
        world.unit = [&candidates](int64_t id, tl::core::Vec3& c, float& r, bool& alive) {
            return missile_unit(candidates, id, c, r, alive);
        };
        world.closest_target = [&candidates](const tl::core::Vec3& at, const tl::core::Vec3& fwd,
                                             float angle, float sight) {
            return missile_closest(candidates, at, fwd, angle, sight);
        };
        // `CLevel::preSortedSphereCollision`: level faces and body spheres, first met.
        world.sweep = [this, &candidates](
                          const tl::core::Vec3& from, const tl::core::Vec3& to, float radius,
                          bool objects, const std::vector<int64_t>& ignore, tl::core::Vec3& centre,
                          tl::core::Vec3& normal, int64_t& unit) {
            float best = 2.0f;
            unit = 0;
            for (const auto& u : candidates) {
                if (std::find(ignore.begin(), ignore.end(), u.id) != ignore.end()) continue;
                // `CLevel::objectSphereCollision` asks
                // each unit's `CBaseUnit::sphereCollision`
                // (`unit_collider_sweep`). INVENTED: the body sphere where nothing is drawn.
                float t = 2.0f;
                if (!unit_collider_sweep(u.id, from, to, radius, t)) {
                    t = sphere_sweep_t(from, to, radius, u.center, u.radius);
                }
                if (t < best) {
                    best = t;
                    unit = u.id;
                }
            }
            // A breakable is a unit too: the box of its collision group, whatever
            // `COLLIDES WITH OBJECTS` says.
            for (const BreakableWorld& b : breakables) {
                if (b.node == nullptr || !b.state.usable || !b.has_collision) continue;
                if (std::find(ignore.begin(), ignore.end(), b.id) != ignore.end()) continue;
                tl::core::Vec3 box_low;
                tl::core::Vec3 box_high;
                if (!collision.group_bounds(b.id, box_low, box_high)) continue;
                const float t = box_sweep_t(from, to, radius, box_low, box_high);
                if (t < best) {
                    best = t;
                    unit = b.id;
                }
            }
            if (!collision.empty()) {
                // CMissile::checkCollision: `sortForCollisionByPoints`.
                collision.sort_for_collision_by_points(from, to, radius);
                tl::core::CollisionHit hit;
                if (collision.pre_sorted_sphere_collision(from, to, radius, objects, hit)) {
                    const tl::core::Vec3 d = to - from;
                    const tl::core::Vec3 m = hit.centre - from;
                    const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
                    const float t =
                        len2 > 0.0f ? std::sqrt((m.x * m.x + m.y * m.y + m.z * m.z) / len2) : 0.0f;
                    if (t < best) {
                        // A face of a breakable's own collision is that breakable.
                        unit = standing_breakable(hit.group) != nullptr ? hit.group : 0;
                        centre = hit.centre;
                        normal = hit.normal;
                        return true;
                    }
                }
            }
            if (!(best <= 1.0f)) return false;
            const tl::core::Vec3 d = to - from;
            centre = {from.x + d.x * best, from.y + d.y * best, from.z + d.z * best};
            for (const auto& u : candidates) {
                if (u.id != unit) continue;
                const tl::core::Vec3 n = centre - u.center;
                const float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                normal = l > 0.0f ? tl::core::Vec3{n.x / l, n.y / l, n.z / l}
                                  : tl::core::Vec3{0.0f, 1.0f, 0.0f};
            }
            return true;
        };
        world.harm = make_missile_harm_hook(this, flying, candidates, struck_at, gather);
        world.get_active_units_at_position = [this, &candidates, &flying](
                                                 const tl::core::Vec3& position, float radius,
                                                 std::vector<int64_t>* items_out) {
            std::vector<tl::core::ActiveUnitCandidate> active_candidates;
            active_candidates.reserve(candidates.size());
            for (const auto& c : candidates) {
                active_candidates.push_back({c.id, c.center});
            }
            std::vector<int64_t> hit = tl::core::get_active_units_at_position(
                active_candidates, position, radius, breakable_item_candidates(), items_out);
            TL_LOG_INFO("missile: '%s' area %.2f at (%.2f, %.2f, %.2f): %zu unit(s), %zu item(s)",
                        flying.missile.spec.name.c_str(), static_cast<double>(radius),
                        static_cast<double>(position.x), static_cast<double>(position.y),
                        static_cast<double>(position.z), hit.size(),
                        items_out != nullptr ? items_out->size() : size_t{0});
            return hit;
        };
        world.interact_item = [this](int64_t item) { return queue_item_interact(item); };
        world.unit_kind = [this](int64_t unit) {
            return standing_breakable(unit) != nullptr ? tl::core::MissileUnitKind::Item
                                                       : tl::core::MissileUnitKind::Character;
        };
        world.volatile_rolls = &tl::core::volatile_random();
        world.reflect = [this](int64_t unit) {
            return missile_reflect_share(unit == kMissilePlayer ? 0 : unit);
        };

        const tl::core::Vec3 before = flying.missile.position;
        struck_at = Ogre::Vector3(before.x, before.y, before.z);
        const tl::core::MissileStep step = tl::core::update_missile(flying.missile, delta, world);
        const tl::core::Vec3& p = flying.missile.position;
        const Ogre::Vector3 now(p.x, p.y, p.z);
        if (flying.node != nullptr) {
            flying.node->setPosition(now);
            const tl::core::Vec3& f = flying.missile.forward;
            flying.node->setDirection(Ogre::Vector3(f.x, f.y, f.z), Ogre::Node::TS_WORLD,
                                      Ogre::Vector3::UNIT_Z);
        }
        if (step.hit) {
            // `HIT` where it struck, facing along the velocity.
            transient_particles.push_back(TransientParticles{
                place_particle_layout(flying.missile.spec.hit, now,
                                      flying.node != nullptr ? flying.node->getOrientation()
                                                             : Ogre::Quaternion::IDENTITY,
                                      nullptr),
                kMissileParticleStopSeconds});
        }
        if (step.killed) {
            TL_LOG_INFO("missile: %s ends at (%.2f, %.2f, %.2f) after %.2f units",
                        flying.missile.spec.name.c_str(), static_cast<double>(p.x),
                        static_cast<double>(p.y), static_cast<double>(p.z),
                        static_cast<double>(flying.missile.travelled_total));
            transient_particles.push_back(
                TransientParticles{place_particle_layout(flying.missile.spec.die, now,
                                                         Ogre::Quaternion::IDENTITY, nullptr),
                                   kMissileParticleStopSeconds});
            // `CSkillEvent::missileDieing`: unless the event is
            // `EVENT_MISSILEDIE`, group 7 from the missile's centre, no unit or point.
            constexpr int kMissileDie = 7;
            if (!flying.level_placed && !flying.by_weapon && !flying.detached &&
                flying.source.group != kMissileDie) {
                if (const tl::core::KnownSkill* skill = player_skill(flying.source.skill)) {
                    tl::core::SkillCastContext context = player_cast_context();
                    context.event_target = tl::core::SkillEventTarget{};
                    context.event_target.run = flying.source.run;
                    context.event_target.origin = p;
                    context.event_target.yaw =
                        flying.node != nullptr
                            ? flying.node->getOrientation().getYaw().valueRadians()
                            : 0.0f;
                    skill_runner_running(skill->entry.name)
                        .trigger_skill_event(*skill, kMissileDie, context);
                }
            }
        }
        if (!step.active_visible && !flying.active.empty()) destroy_particle_holders(flying.active);
        if (step.finished) {
            destroy_particle_holders(flying.active);
            if (flying.node != nullptr) scene->destroySceneNode(flying.node);
            missiles.erase(missiles.begin() + static_cast<long>(i));
            continue;
        }
        ++i;
    }
    flush_item_interacts();
}

std::vector<tl::core::ActiveUnitCandidate> Renderer::Impl::breakable_item_candidates() const {
    // The near item list (`CLevel::updateNearUnitList`), newest
    // first (`CLevel::addItem`); broken ones too.
    std::vector<tl::core::ActiveUnitCandidate> out;
    for (size_t i = breakables.size(); i-- > 0;) {
        const BreakableWorld& b = breakables[i];
        if (b.node == nullptr || !b.listed || !b.state.usable) continue;
        const Ogre::Vector3 at = b.node->getPosition();
        out.push_back({b.id, {at.x, at.y, at.z}});
    }
    return out;
}

const Renderer::Impl::BreakableWorld* Renderer::Impl::standing_breakable(int64_t id) const {
    if (id == 0) return nullptr;
    for (const BreakableWorld& b : breakables) {
        if (b.id == id) return b.node != nullptr && b.state.usable ? &b : nullptr;
    }
    return nullptr;
}

// `CMissile::doAOEDamage`'s `CItem::interact(nullptr)` per item reached.
// DIVERGENCE: queued until the missiles have stepped, so the walked list cannot grow.
bool Renderer::Impl::queue_item_interact(int64_t item, bool by_player) {
    if (standing_breakable(item) == nullptr) return false;
    for (const PendingItemInteract& pending : pending_item_interacts) {
        if (pending.item == item) return false;
    }
    pending_item_interacts.push_back(PendingItemInteract{item, by_player});
    return true;
}

void Renderer::Impl::step_breakable_shapes() {
    // `CLevel::updateItems`: active `ISA 0x1d` items go
    // to every update listener; here before the script steps, not after each `update`.
    if (!script_ready || breakables.empty() || !level_items.list) return;
    for (const tl::core::DamageShapeItem& item : level_items.list()) {
        script.item_updated_in_level(item);
    }
}

void Renderer::Impl::flush_item_interacts() {
    if (pending_item_interacts.empty()) return;
    std::vector<PendingItemInteract> items;
    items.swap(pending_item_interacts);
    for (const PendingItemInteract& pending : items) {
        for (size_t i = 0; i < breakables.size(); ++i) {
            if (breakables[i].id != pending.item) continue;
            // `CBreakable::interact`: by nobody, or the
            // skill's owner.
            break_breakable(i, pending.by_player);
            break;
        }
    }
}

void Renderer::Impl::clear_missiles() {
    for (FlyingMissile& flying : missiles) {
        destroy_particle_holders(flying.active);
        if (flying.node != nullptr) scene->destroySceneNode(flying.node);
    }
    missiles.clear();
    for (TransientParticles& t : transient_particles) destroy_particle_holders(t.holders);
    transient_particles.clear();
    skill_spawners.clear();
    live_skill_events.clear();
    skill_event_holds.clear();
}

namespace renderer_detail {
// PORT: an item of the port's index as a spawn class sees it (`SpawnUnitData::from_adm`'s fields).
tl::core::SpawnUnitData spawn_data_of(const tl::core::ItemStats& item) {
    tl::core::SpawnUnitData u;
    u.name = item.name;
    u.unittype = item.unittype;
    u.rarity = item.spawn_rarity;
    u.rarity_hardcore = item.rarity_hardcore;
    u.create_as = item.create_as;
    u.level = item.level;
    u.min_level = item.spawn_min_level;
    u.max_level = item.spawn_max_level;
    return u;
}

}  // namespace renderer_detail
// `CSpawnClassParser::createEmptySpawnClass`:
// the file read and parsed (`parse_spawn_class`), and the root's `LEVEL_BONUS`.
std::optional<tl::core::SpawnClass> Renderer::Impl::load_spawn_class(
    const std::string& name, const tl::core::SpawnContext& context) {
    const std::string path = spawn_class_path(name);
    if (path.empty()) return std::nullopt;
    const std::vector<uint8_t> bytes = read_resource(path);
    if (bytes.empty()) return std::nullopt;
    const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!file.ok) return std::nullopt;
    tl::core::SpawnClass made = tl::core::parse_spawn_class(file.root, context);
    // `createEmptySpawnClass`'s one value: the root's `LEVEL_BONUS` (0).
    made.set_level_bonus(static_cast<int32_t>(file.root.integer_or("LEVEL_BONUS", 0)));
    return made;
}

void Renderer::Impl::build_spawn_context(SpawnTables& tables, bool items_by_type) {
    load_unit_types();
    load_stat_graphs();

    tl::core::SpawnContext& context = tables.context;
    context.unit_types = &unit_types;
    context.stats = &stat_graphs;
    // `CGameGlobals`' rarity weights and magic-find influences, as the file
    // authors them.
    {
        const std::vector<uint8_t> bytes = read_resource("media/GLOBALS.DAT.adm");
        const tl::formats::AdmFile globals =
            bytes.empty() ? tl::formats::AdmFile{}
                          : tl::formats::parse_adm(bytes.data(), bytes.size());
        if (globals.ok) tl::core::read_spawn_weights(globals, context);
    }

    // `CSpawnClassParser::getSpawnClassDataByName(const std::wstring&)`:
    // keyed upper-case, null for none; each file read on first asking.
    context.get_spawn_class = [this,
                               &tables](const std::string& name) -> const tl::core::SpawnClass* {
        const std::string key = tl::core::upper_ascii(name);
        auto it = tables.classes.find(key);
        if (it != tables.classes.end()) return &it->second;
        std::optional<tl::core::SpawnClass> loaded = load_spawn_class(name, tables.context);
        if (!loaded) return nullptr;
        auto inserted = tables.classes.emplace(key, std::move(*loaded));
        return &inserted.first->second;
    };

    context.get_unit_by_name =
        [this, &tables](const std::string& name) -> const tl::core::SpawnUnitData* {
        auto it = tables.units.find(name);
        if (it != tables.units.end()) return &it->second;

        // 1. Check items (media/masterresourceunits.dat)
        ensure_item_index();
        if (const tl::core::ItemStats* item = items.find(name)) {
            auto inserted = tables.units.emplace(name, spawn_data_of(*item));
            return &inserted.first->second;
        }

        // 2. Check monsters (media/units/monsters/)
        ensure_monster_index();
        auto it_m = monster_files.find(lowered(name));
        if (it_m != monster_files.end()) {
            const std::vector<uint8_t> m_bytes = read_resource(it_m->second);
            if (!m_bytes.empty()) {
                const tl::formats::AdmFile m_file =
                    tl::formats::parse_adm(m_bytes.data(), m_bytes.size());
                if (m_file.ok) {
                    auto inserted =
                        tables.units.emplace(name, tl::core::SpawnUnitData::from_adm(m_file.root));
                    return &inserted.first->second;
                }
            }
        }

        // 3. Check props (media/units/props/)
        ensure_prop_index();
        auto it_p = prop_files.find(lowered(name));
        if (it_p != prop_files.end()) {
            const std::vector<uint8_t> p_bytes = read_resource(it_p->second);
            if (!p_bytes.empty()) {
                const tl::formats::AdmFile p_file =
                    tl::formats::parse_adm(p_bytes.data(), p_bytes.size());
                if (p_file.ok) {
                    auto inserted =
                        tables.units.emplace(name, tl::core::SpawnUnitData::from_adm(p_file.root));
                    return &inserted.first->second;
                }
            }
        }

        return nullptr;
    };

    context.get_units_by_type =
        [this, items_by_type](uint32_t type_id) -> std::vector<tl::core::SpawnUnitData> {
        if (!items_by_type) {
            static std::set<std::string> warned_unittypes;
            std::string type_name = unit_types.name_of(type_id);
            if (type_name.empty()) type_name = std::to_string(type_id);
            if (warned_unittypes.insert(type_name).second) {
                TL_LOG_WARN(
                    "spawn class picks by UNITTYPE '%s', which a spawner here does not index",
                    type_name.c_str());
            }
            // DIVERGENCE: a `Unit Spawner`'s roll has no units by UNITTYPE: empty.
            return {};
        }
        // `CUnitResourceList::getUnitsByType`: each unit
        // that `ISA`s the type, groups in order, each by upper-cased `NAME`; the order is
        // what a weighted pick draws against. DIVERGENCE: only the items are walked.
        ensure_item_index();
        std::vector<std::pair<std::string, const tl::core::ItemStats*>> ordered;
        for (const auto& [key, item] : items.all()) {
            if (item.unittype.empty() || !unit_types.isa(item.unittype, type_id)) continue;
            ordered.emplace_back(tl::core::upper_ascii(item.name), &item);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<tl::core::SpawnUnitData> found;
        for (const auto& [upper, item] : ordered) found.push_back(spawn_data_of(*item));
        return found;
    };
}

/// Resolve a `Unit Spawner`'s `RESOURCE` (by `GROUP`: a unit name, or a weighted spawn
/// class) to a unit: CSpawnClass::rollSpawnClass and
/// CSpawnClassData::rollSpawnClassData, on `g_Rand` as
/// it stands; the first unit, as `loadAndEquipSpawnClass` uses.
std::string Renderer::Impl::resolve_spawn_resource(const std::string& group,
                                                   const std::string& resource, int depth,
                                                   int32_t level, uint32_t flags) {
    if (resource.empty()) return {};
    // A table may name another; four stops a cycle.
    constexpr int kMaxDepth = 4;
    if (depth > kMaxDepth) return {};

    const bool spawn_class = group == "Spawn Class";
    if (!spawn_class) {
        // `Monsters` and `Props` name a unit; `Items` and anything else are not handled.
        if (group == "Monsters" || group == "Props" || group.empty()) return resource;
        return {};
    }

    SpawnTables tables;
    build_spawn_context(tables, /*items_by_type=*/false);
    std::optional<tl::core::SpawnClass> loaded = load_spawn_class(resource, tables.context);
    if (!loaded) return {};
    const tl::core::SpawnContext& context = tables.context;
    const tl::core::SpawnClass& sc = *loaded;
    std::vector<std::string> rolled;

    // DIVERGENCE: rollSpawnClass can return several units (MINCOUNT/MAXCOUNT); only the
    // first is returned.
    sc.roll_spawn_class(rolled, nullptr, nullptr, nullptr, level, 0xffffffffu,
                        /*level_bonus=*/0, /*rarity_override=*/-1, /*magic_find=*/0, flags,
                        tl::core::game_random(), context);

    if (rolled.empty()) return {};
    return rolled[0];
}

// PORT: the mesh a unit names (`MESHFILE` under `RESOURCEDIRECTORY`), in the archive.
std::string Renderer::Impl::unit_mesh_path(const std::string& unit) {
    index_unit(unit);
    ensure_monster_index();
    const auto found = monster_files.find(lowered(unit));
    if (found == monster_files.end()) return std::string();
    const std::vector<uint8_t> bytes = read_resource(found->second);
    if (bytes.empty()) return std::string();
    const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!file.ok) return std::string();
    // The stats first: both keys inherit through `BASEFILE` (`UnitStats::mesh`).
    std::string mesh_name = stats_for_unit(unit).mesh;
    if (mesh_name.empty()) {
        std::string directory = file.string_or("RESOURCEDIRECTORY", "");
        const std::string mesh_file = file.string_or("MESHFILE", "");
        if (directory.empty() || mesh_file.empty()) return std::string();
        if (directory.back() != '/' && directory.back() != '\\') directory += '/';
        mesh_name = directory + mesh_file;
    }
    const std::string mesh_path = resolve_archive_path(mesh_name + ".MESH");
    if (mesh_path.empty()) {
        TL_LOG_WARN("unit '%s' names no mesh at '%s.MESH'", unit.c_str(), mesh_name.c_str());
        return std::string();
    }
    parse_sibling_material(mesh_path);
    return mesh_path;
}

void Renderer::Impl::convert_unit_body(tl::core::LevelScript* machine, int64_t node,
                                       const std::string& unit) {
    SceneUnit* drawn = scene_unit_of(machine, node);
    if (drawn == nullptr || drawn->entity == nullptr) {
        TL_LOG_WARN("transform: no model for node %lld to change", static_cast<long long>(node));
        return;
    }
    Ogre::Entity* old_entity = drawn->entity;
    Ogre::SceneNode* holder = old_entity->getParentSceneNode();
    if (holder == nullptr) return;
    const std::string mesh_path = unit_mesh_path(unit);
    if (mesh_path.empty()) return;

    // `unitInit`'s model, made on its own node and moved onto the character's, so the node
    // every map holds by the character's id stays.
    MeshPlacement placement;
    placement.mesh = mesh_path;
    placement.position = holder->getPosition();
    placement.orientation = holder->getOrientation();
    placement.is_unit = true;
    Ogre::Entity* entity = place_mesh(placement);
    if (entity == nullptr) return;
    if (Ogre::SceneNode* made = entity->getParentSceneNode()) {
        made->detachObject(entity);
        scene->destroySceneNode(made);
    }

    // `deleteUnitSpecificData`: the old model and every
    // list it was in; the spawner's list keeps the character.
    std::vector<int64_t> owners;
    for (const auto& entry : spawned) {
        for (const SpawnedUnit& made : entry.second) {
            if (made.entity == old_entity) owners.push_back(entry.first);
        }
    }
    const bool was_pet = old_entity == pet_entity;
    forget_unit_entity(old_entity);
    {
        std::vector<Ogre::MovableObject*> held(old_entity->getAttachedObjects().begin(),
                                               old_entity->getAttachedObjects().end());
        old_entity->detachAllObjectsFromBone();
        for (Ogre::MovableObject* object : held) scene->destroyMovableObject(object);
        holder->detachObject(old_entity);
        scene->destroyEntity(old_entity);
    }
    holder->attachObject(entity);
    const float scale = machine != nullptr ? machine->unit_scale(node) : 0.0f;
    const float drawn_scale = scale > 0.0f ? scale : stats_for_unit(unit).scale;
    holder->setScale(drawn_scale, drawn_scale, drawn_scale);

    const tl::core::UnitStats stats = stats_for_unit(unit);
    Ogre::AnimationState* started = nullptr;
    play_scene_animation(entity, mesh_path, kUnitIdleAnimation, /*loops=*/true, 1.0f,
                         /*optional=*/true, &started, stats.armed);
    remember_state(entity, kUnitIdleAnimation, started);
    scene_units.emplace(unit, SceneUnit{entity, mesh_path, 1.0f, node, kUnitIdleAnimation, true,
                                        kUnitIdleAnimation, started, 0.0f, stats.armed});
    for (int64_t owner : owners) spawned[owner].push_back(SpawnedUnit{holder, entity, unit, node});
    std::string right_hand;
    std::string left_hand;
    if (machine != nullptr) machine->unit_hands(node, right_hand, left_hand);
    equip_unit_items(entity, unit, right_hand, left_hand);
    apply_render_always(entity, stats.render_always);
    apply_rim_light(entity, rim_light_texture);
    // `CCharacter::loadModel`: `setRenderBehind(+0x4a1)`, `RENDERBEHIND`.
    set_render_behind(entity, stats.render_behind, outline_hostile(stats));
    if (was_pet) {
        pet_entity = entity;
        pet_mesh = mesh_path;
    }

    // `CParticle::Start` on the new model's particle 8, `TRANSFORM`.
    tl::core::Vec3 feet;
    if (machine != nullptr && machine->unit_position(node, feet)) {
        start_character_particle(
            stats, tl::core::character_particle::kTransform,
            Ogre::Vector3(feet.x, feet.y + machine->unit_radius(node), feet.z));
    }
    TL_LOG_INFO("transform: node %lld drawn as %s (%s)", static_cast<long long>(node), unit.c_str(),
                mesh_path.c_str());
}

Ogre::Entity* Renderer::Impl::spawn_unit(const std::string& unit, const Ogre::Vector3& at,
                                         int64_t owner, int level, bool champion, bool spawn_anim,
                                         const Ogre::Quaternion& facing, float scale) {
    // Its own numbers first: the script wants speed, radius and attacks at once.
    index_unit(unit);
    ensure_monster_index();
    const auto found = monster_files.find(lowered(unit));
    if (found == monster_files.end()) return nullptr;

    const std::vector<uint8_t> bytes = read_resource(found->second);
    if (bytes.empty()) return nullptr;
    const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!file.ok) return nullptr;

    (void)file;
    const std::string mesh_path = unit_mesh_path(unit);
    if (mesh_path.empty()) return nullptr;

    MeshPlacement placement;
    placement.mesh = mesh_path;
    // An id of its own, not the spawner's: `Hide` on the spawner must not reach it.
    const int64_t id = kSpawnedNodeBase + static_cast<int64_t>(++spawn_serial);
    placement.node_id = id;
    placement.position = at;
    placement.orientation = facing;
    placement.is_unit = true;
    // The drawn scale when the caller has it; else the authored one until
    // `apply_unit_scale`.
    const float placed_scale = scale > 0.0f ? scale : stats_for_unit(unit).scale;
    placement.scale = Ogre::Vector3(placed_scale, placed_scale, placed_scale);

    Ogre::Entity* entity = place_mesh(placement);
    if (entity == nullptr) return nullptr;

    const bool armed = stats_for_unit(unit).armed;
    Ogre::AnimationState* started = nullptr;
    play_scene_animation(entity, mesh_path, kUnitIdleAnimation, /*loops=*/true, 1.0f,
                         /*optional=*/true, &started, armed);
    remember_state(entity, kUnitIdleAnimation, started);

    Ogre::SceneNode* node = entity->getParentSceneNode();
    const auto placed =
        scene_units.emplace(unit, SceneUnit{entity, mesh_path, 1.0f, id, kUnitIdleAnimation, true,
                                            kUnitIdleAnimation, started, 0.0f, armed});
    spawned[owner].push_back(SpawnedUnit{node, entity, unit, id});
    {
        // The authored hands; a rolled one goes on later (`sync_unit_equipment`).
        const tl::core::UnitStats held = stats_for_unit(unit);
        equip_unit_items(entity, unit, held.right_hand, held.left_hand);
        apply_render_always(entity, held.render_always);
        // `CCharacter::loadModel`: `setRenderBehind` with `RENDERBEHIND`;
        // an entrance turns it off below.
        set_render_behind(entity, held.render_behind, outline_hostile(held));
    }
    apply_rim_light(entity, rim_light_texture);

    // And its entrance, over the idle (`SceneUnit::animation`, which step_fidgets() returns
    // to). Its sound is the animation's own `PLAYSOUND` key (step_unit_sounds()).
    if (spawn_anim && begin_spawn_animation(entity, mesh_path, stats_for_unit(unit))) {
        const Fidget& f = fidgets[entity];
        placed->second.playing = f.playing;
        placed->second.playing_state = f.state;
        placed->second.last_time = 0.0f;
    }
    if (level > 0 || champion) {
        unit_spawn_level[id] = SpawnLevel{level, champion};
    }
    return entity;
}

// PORT: the script's unit scale put on the unit's OGRE node.
void Renderer::Impl::apply_unit_scale(int64_t node) {
    const float scale = script.unit_scale(node);
    if (!(scale > 0.0f)) return;
    for (const auto& entry : spawned) {
        for (const SpawnedUnit& made : entry.second) {
            if (made.id != node || made.node == nullptr) continue;
            made.node->setScale(scale, scale, scale);
            return;
        }
    }
}

namespace renderer_detail {
/// `Ogre::Quaternion::FromAxes` on the axes a spawner hands over.
Ogre::Quaternion quaternion_from_axes(const tl::core::SpawnAxes& axes) {
    Ogre::Quaternion q;
    q.FromAxes(Ogre::Vector3(axes.x.x, axes.x.y, axes.x.z),
               Ogre::Vector3(axes.y.x, axes.y.y, axes.y.z),
               Ogre::Vector3(axes.z.x, axes.z.y, axes.z.z));
    return q;
}

}  // namespace renderer_detail
// PORT: `CLevel +0x1a8`, the level's monster level, for the spawner host.
int32_t Renderer::Impl::SpawnerHost::level_monster_level() const {
    // `CLevel +0x1a8`: every spawner here stands in a level.
    return impl_.level_monster_level;
}

tl::core::Vec3 Renderer::Impl::SpawnerHost::random_open_position(const tl::core::Vec3& at,
                                                                 float radius,
                                                                 bool avoid_no_spawn) {
    return tl::core::random_open_position(
        at, 0.0f, radius,
        [this, avoid_no_spawn](const tl::core::Vec3& p) {
            return !impl_.passability.ready() ||
                   (impl_.passability.map_passable(p) && impl_.passability.position_passable(p) &&
                    !(avoid_no_spawn &&
                      tl::core::is_in_no_spawn_region(impl_.no_spawn_regions, p, 0.0f)));
        },
        tl::core::volatile_random());
}

bool Renderer::Impl::SpawnerHost::ray_collision(const tl::core::Vec3& from,
                                                const tl::core::Vec3& to, tl::core::Vec3& normal) {
    tl::core::CollisionHit hit;
    if (!impl_.collision.ray_collision(from, to, /*objects=*/true, hit)) return false;
    normal = hit.normal;
    return true;
}

float Renderer::Impl::SpawnerHost::range_random(float lo, float hi) {
    return Ogre::Math::RangeRandom(lo, hi);
}

tl::core::RunicRandom& Renderer::Impl::SpawnerHost::volatile_random() {
    return tl::core::volatile_random();
}

void Renderer::Impl::SpawnerHost::create_particles(const std::string& file,
                                                   const tl::core::Vec3& at,
                                                   const tl::core::SpawnAxes& axes,
                                                   uint32_t index) {
    // `CUnitSpawner::createParticleSystemAt`: the layout
    // at this index used again, a new index loads one. INVENTED: no
    // `CLevel::snapToValidGround(p, 0.01)`, and a used layout is replaced, not restarted.
    std::map<uint32_t, std::vector<Ogre::SceneNode*>>& layouts = impl_.spawner_particles[node_];
    const auto used = layouts.find(index);
    if (used != layouts.end()) impl_.destroy_particle_holders(used->second);
    // A kind-1 `CLayout`: a `CParticle` once particles are cached.
    layouts[index] = impl_.place_particle_layout(
        file, Ogre::Vector3(at.x, at.y, at.z), quaternion_from_axes(axes), nullptr,
        renderer_detail::caching_particles() ? ParticleStart::Now : ParticleStart::Live);
}

void Renderer::Impl::SpawnerHost::fire_missile(const std::string& resource,
                                               const tl::core::Vec3& at,
                                               const tl::core::SpawnAxes& axes) {
    // `fireMissile` is handed the orientation; the missile flies along its z.
    impl_.fire_spawner_missile(node_, resource, Ogre::Vector3(at.x, at.y, at.z),
                               Ogre::Vector3(axes.z.x, axes.z.y, axes.z.z));
}

// `CSpawnClassParser::getSpawnClassByName` and
// `CSpawnClass::rollSpawnClass` with `(level, 0xffffffff,
// 0, -1, 0, 0)` on `g_Rand`, as `CUnitSpawner::spawnUnitByIndex` rolls.
tl::core::SpawnClassPick Renderer::Impl::SpawnerHost::roll_spawn_class(const std::string& name,
                                                                       int32_t level) {
    if (!impl_.loot_tables_built) {
        impl_.loot_tables_built = true;
        impl_.build_spawn_context(impl_.loot_tables, /*items_by_type=*/true);
    }
    const tl::core::SpawnContext& context = impl_.loot_tables.context;
    tl::core::SpawnClassPick pick;
    const tl::core::SpawnClass* table =
        context.get_spawn_class ? context.get_spawn_class(name) : nullptr;
    if (table == nullptr) return pick;
    pick.found_class = true;
    std::vector<std::string> units;
    std::vector<bool> enchant;
    table->roll_spawn_class(units, &enchant, nullptr, nullptr, level, 0xffffffffu, 0, -1, 0, 0,
                            tl::core::game_random(), context);
    if (!units.empty()) {
        pick.unit = units[0];
        pick.enchant = !enchant.empty() && enchant[0];
    }
    return pick;
}

tl::core::SpawnedUnitRef Renderer::Impl::SpawnerHost::create_unit(
    const tl::core::SpawnRelease& release, const std::string& name, bool enchant) {
    using tl::core::SpawnGroup;
    using tl::core::SpawnSettle;
    const Ogre::Quaternion facing = quaternion_from_axes(release.axes);
    const Ogre::Vector3 at(release.position.x, release.position.y, release.position.z);
    const Ogre::Vector3 spawner(release.spawner_position.x, release.spawner_position.y,
                                release.spawner_position.z);

    // `CResourceManager::createUnit`: an item by name for `Items`, or a table's pick.
    impl_.ensure_item_index();
    const tl::core::ItemStats* data = nullptr;
    if (release.group == SpawnGroup::Items || release.group == SpawnGroup::SpawnClass) {
        data = impl_.items.find(name);
    }
    if (data != nullptr) {
        // `createItem(data, false, true, false)`, then `enchant(true)` on the table's flag.
        const tl::core::ItemStats made = impl_.create_item(*data, false, true, enchant);
        const size_t before = impl_.dropped_items.size();
        if (made.is_gold()) {
            impl_.place_gold(made.gold, at, &made, /*drop_sound=*/release.item_drop);
        } else if (release.item_drop) {
            // `addItem(item, p, true)`: `CEquipment::drop` tosses it from there.
            impl_.drop_item(made, 1, at);
        } else {
            impl_.place_ground_item(made, 1, at, facing, tl::core::QuestUnitTag{});
        }
        if (impl_.dropped_items.size() == before) return {};
        DroppedWorldItem& placed = impl_.dropped_items.back();
        if (release.settle != SpawnSettle::AsAdded && placed.node != nullptr) {
            placed.node->setPosition(release.settle == SpawnSettle::AtSpawner ? spawner : at);
            placed.node->setOrientation(facing);
            placed.rest = facing;
        }
        const int64_t id = impl_.next_spawned_id();
        placed.unit_id = id;
        impl_.spawner_units[id] = SpawnerUnit{machine_, node_};
        TL_LOG_INFO("spawner: '%s' releases %s", release.resource.c_str(),
                    made.display_name.c_str());
        // Every item but gold is a `CEquipment`.
        return {tl::core::SpawnedKind::Item, id, !made.is_gold()};
    }
    if (release.group != SpawnGroup::Monsters && release.group != SpawnGroup::Props &&
        release.group != SpawnGroup::SpawnClass) {
        TL_LOG_WARN("spawner: no %s called '%s'", release.group_upper.c_str(), name.c_str());
        return {};
    }

    // A `CBreakable` is an item: `addItem(b, p, item drop)`,
    // which tosses only a `CEquipment`; then the spawner settles it.
    if (impl_.is_breakable_unit(name)) {
        const int64_t id = impl_.next_spawned_id();
        const Ogre::Vector3* settle = release.settle == SpawnSettle::AtSpawner    ? &spawner
                                      : release.settle == SpawnSettle::AtPosition ? &at
                                                                                  : nullptr;
        if (!impl_.place_breakable(name, at, facing, id, settle)) return {};
        impl_.spawner_units[id] = SpawnerUnit{machine_, node_};
        TL_LOG_INFO("spawner: '%s' releases %s", release.resource.c_str(), name.c_str());
        return {tl::core::SpawnedKind::Item, id, false};
    }

    // `CLevel::addCharacter(c, p, false)`: grounded, then
    // settled by the spawner.
    Ogre::Vector3 place = impl_.dropped_to_ground(at);
    if (release.settle == SpawnSettle::AtPosition) place = at;
    if (release.settle == SpawnSettle::AtSpawner) place = spawner;

    const int level = release.level > 0 ? release.level : 0;
    if (impl_.spawn_unit(name, place, node_, level, /*champion=*/false, release.play_spawn_anim,
                         facing) == nullptr) {
        // A prop whose body is a particle layout (see place_layout_bodied_unit()).
        if (!impl_.place_layout_bodied_unit(name, place)) {
            TL_LOG_WARN("spawner: '%s' resolved to no unit", name.c_str());
            return {};
        }
        const int64_t id = impl_.next_spawned_id();
        impl_.spawner_units[id] = SpawnerUnit{machine_, node_};
        return {tl::core::SpawnedKind::Item, id, false};
    }
    const SpawnedUnit& made = impl_.spawned[node_].back();
    const Ogre::Vector3 forward = facing * Ogre::Vector3::UNIT_Z;
    impl_.script.add_spawned_unit(made.id, name, tl::core::Vec3{place.x, place.y, place.z},
                                  release.target_player, std::atan2(forward.x, forward.z));
    impl_.apply_spawn_level(made.id);
    impl_.spawner_units[made.id] = SpawnerUnit{machine_, node_};
    // `addCharacter` -> `createPathfinder`: `MINION_SPAWNCLASS` brings its minions.
    impl_.spawn_unit_minions(made.id, node_);
    if (!release.give_loot) impl_.no_loot_units.insert(made.id);
    if (!release.give_xp) impl_.no_xp_units.insert(made.id);
    // INVENTED: `DESTROY BODY`'s effect 0x81 is not transcribed; without `PLAY SPAWN ANIM`
    // its AI state 4 is the machine's own starting state.
    if (release.play_spawn_anim) {
        const auto entrance = impl_.fidgets.find(made.entity);
        if (entrance != impl_.fidgets.end() && entrance->second.state != nullptr) {
            impl_.spawning_units[made.id] = entrance->second.state;
            impl_.script.set_spawning(made.id, true);
            // `CCharacter::spawn`: `setRenderBehind(false)` for the
            // entrance; `spawningAI` puts `RENDERBEHIND` back.
            if (const RenderBehind* behind = render_behind_of(made.entity)) {
                set_render_behind(made.entity, false, behind->hostile);
            }
        }
    }
    impl_.ensure_prop_index();
    const bool prop = impl_.prop_files.count(lowered(name)) != 0;
    TL_LOG_INFO("spawner: '%s' releases %s at (%.2f, %.2f, %.2f)", release.resource.c_str(),
                name.c_str(), static_cast<double>(place.x), static_cast<double>(place.y),
                static_cast<double>(place.z));
    return {prop ? tl::core::SpawnedKind::Item : tl::core::SpawnedKind::Character, made.id, false};
}

// The spawn listeners (`CUnitSpawner +0x218`): a skill event's (`CSkillEvent::addListeners`)
// is told of each character released
// (`unitSpawnedFromUnitSpawner`).
void Renderer::Impl::SpawnerHost::unit_spawned(const tl::core::SpawnedUnitRef& unit) {
    // `create_unit` put it in the level's own machine.
    if (unit.kind != tl::core::SpawnedKind::Character) return;
    const auto found = impl_.skill_spawners.find(node_);
    if (found == impl_.skill_spawners.end()) return;
    if (impl_.script.unit_of(unit.id).empty()) return;
    impl_.skill_unit_spawned(found->second.source, impl_.script, unit.id);
}

// A released unit still exists: the spawner's `TSafePointer` is not cleared.
// PORT: answered from the port's list of what the spawner made.
bool Renderer::Impl::SpawnerHost::unit_exists(int64_t id) const {
    const auto found = impl_.spawned.find(node_);
    if (found == impl_.spawned.end()) return false;
    for (const SpawnedUnit& unit : found->second) {
        if (unit.id == id) return true;
    }
    return false;
}

bool Renderer::Impl::SpawnerHost::character_alive(int64_t id) const {
    return !impl_.script.unit_is_dead(id);
}

// `CCharacter::die` (vtable +0x330) with no one to blame, on a released unit
// (`CUnitSpawner::destroyUnits`).
void Renderer::Impl::SpawnerHost::kill_character(int64_t id) {
    if (impl_.script.unit_is_dead(id)) return;
    const auto found = impl_.spawned.find(node_);
    if (found == impl_.spawned.end()) return;
    for (const SpawnedUnit& unit : found->second) {
        if (unit.id != id) continue;
        const std::string name = unit.unit;
        impl_.kill_unit(id, name, /*fall=*/true, tl::core::LevelScript::kNoAttacker);
        return;
    }
}

void Renderer::Impl::SpawnerHost::hide_character(int64_t id, bool hide) {
    // `setEnabled(false)`: the machine forgets it (no thinking, not hunted); still alive.
    const auto found = impl_.spawned.find(node_);
    if (found == impl_.spawned.end()) return;
    for (const SpawnedUnit& unit : found->second) {
        if (unit.id != id) continue;
        if (unit.node != nullptr) unit.node->setVisible(!hide);
        if (hide) impl_.script.remove_spawned_unit(id);
        return;
    }
}

void Renderer::Impl::SpawnerHost::run_particles(bool run) {
    // `CLayout::stop(false)`: nothing more released, each layout goes once dead.
    // INVENTED: `CLayout::start` again is not modelled.
    if (run) return;
    const auto found = impl_.spawner_particles.find(node_);
    if (found == impl_.spawner_particles.end()) return;
    for (auto& layout : found->second) {
        impl_.transient_particles.push_back(TransientParticles{layout.second, 0.0f});
    }
    impl_.spawner_particles.erase(found);
}

void Renderer::Impl::SpawnerHost::deliver() {
    if (outputs_.empty() || machine_ == nullptr) return;
    std::vector<tl::core::ScriptEvent> events;
    for (uint32_t raised : outputs_) {
        TL_LOG_INFO("spawner: %lld raises '%s'", static_cast<long long>(node_),
                    tl::core::logic::output_name(raised));
        machine_->raise_output(node_, raised, impl_.quest_log.state(), events);
    }
    outputs_.clear();
    if (events.empty()) return;
    if (NodeMap* nodes = impl_.nodes_of(machine_)) impl_.apply_script_events(events, *nodes);
}

Renderer::Impl::NodeMap* Renderer::Impl::nodes_of(tl::core::LevelScript* machine) {
    for (MachineView& view : machines()) {
        if (view.script == machine) return view.nodes;
    }
    return nullptr;
}

// PORT: the port's own id for a spawned unit.
int64_t Renderer::Impl::next_spawned_id() {
    return kSpawnedNodeBase + static_cast<int64_t>(++spawn_serial);
}

// PORT: a layout input handed to the spawner node's `UnitSpawner::input`.
void Renderer::Impl::spawner_input(const tl::core::ScriptEvent& event) {
    if (event.spawner == nullptr) return;
    SpawnerHost host(*this, event.machine, event.node_id);
    event.spawner->input(event.input, host);
    TL_LOG_INFO("script: '%s' %s (%s '%s'): %u of %u out", event.name.c_str(),
                tl::core::logic::input_name(event.input), event.spawner->spec().group.c_str(),
                event.spawner->spec().resource.c_str(), event.spawner->released(),
                event.spawner->total());
    host.deliver();
}

// Every spawner's `CUnitSpawner::update`, once a frame. INVENTED: all stepped
// here, not among the level's units in their order.
void Renderer::Impl::step_spawners(float delta) {
    if (!(delta > 0.0f)) return;
    for (MachineView& view : machines()) {
        for (const auto& entry : view.script->unit_spawners()) {
            SpawnerHost host(*this, view.script, entry.first);
            entry.second->update(delta, host);
            host.deliver();
        }
    }
}

void Renderer::Impl::spawned_item_state(int64_t id, int state) {
    const auto found = spawner_units.find(id);
    if (found == spawner_units.end()) return;
    const SpawnerUnit owner = found->second;
    spawner_units.erase(found);
    tl::core::UnitSpawner* spawner =
        owner.machine != nullptr ? owner.machine->unit_spawner(owner.spawner) : nullptr;
    if (spawner == nullptr) return;
    SpawnerHost host(*this, owner.machine, owner.spawner);
    spawner->unit_state_change(id, state, host);
    host.deliver();
}

/// `CGenericModel::setAmbient`: every material the model
/// draws with (`+0x1f0`) takes `colour` as ambient and diffuse (`Ogre::Material::setAmbient`
/// and `setDiffuse`, every pass). `seen` keeps a shared material to one write.
void Renderer::Impl::set_model_ambient(Ogre::Entity* entity, const Ogre::ColourValue& colour,
                                       std::set<Ogre::Material*>& seen) {
    for (unsigned i = 0; i < entity->getNumSubEntities(); ++i) {
        const Ogre::MaterialPtr& mat = entity->getSubEntity(i)->getMaterial();
        if (!mat || !seen.insert(mat.get()).second) continue;
        mat->setAmbient(colour);
        mat->setDiffuse(colour);
    }
}

/// The level's `MATERIAL AMBIENT`, on its room pieces: `CLevelTemplateData::load`
/// reads it (92 each by default, times 1/255 by `fmuls`) and
/// `CLevel::setProjectorPass` hands it to `setAmbient` on
/// every `Room Piece`; MIDWATER and `LIGHT` water too
/// (apply_environment()). MISSING: `LEVELLIGHTING` items (`CLevel::addItem`).
///
void Renderer::Impl::apply_scenery_fill(const tl::formats::AdmSection& rules) {
    const auto channel = [&rules](const char* key) {
        return static_cast<float>(static_cast<int32_t>(rules.integer_or(key, 92))) *
               0.003921568859368563f;
    };
    material_ambient =
        Ogre::ColourValue(channel("MATERIAL AMBIENT RED"), channel("MATERIAL AMBIENT GREEN"),
                          channel("MATERIAL AMBIENT BLUE"), 1.0f);
    std::set<Ogre::Material*> seen;
    for (Ogre::Entity* entity : room_piece_meshes)
        set_model_ambient(entity, material_ambient, seen);
    TL_LOG_INFO("  material ambient (%.3f, %.3f, %.3f) on %zu room piece material(s)",
                static_cast<double>(material_ambient.r), static_cast<double>(material_ambient.g),
                static_cast<double>(material_ambient.b), seen.size());
}

namespace renderer_detail {
/// A screen quad of the light map: `Ogre::Rectangle2D` with texture coordinates, bounds
/// +-100000 as makes them (never culled).
std::unique_ptr<Ogre::Rectangle2D> light_map_quad(float half, const Ogre::MaterialPtr& material,
                                                  Ogre::uint8 group) {
    auto quad = std::make_unique<Ogre::Rectangle2D>(true);
    quad->setCorners(-half, half, half, -half);
    quad->setBoundingBox(Ogre::AxisAlignedBox(Ogre::Vector3(-100000.0f), Ogre::Vector3(100000.0f)));
    quad->setMaterial(material);
    quad->setRenderQueueGroup(group);
    return quad;
}

/// `RttLight` and `RttLightMask`: one pass,
/// unlit, fog off, a single texture, no depth check or write, and the scene
/// blend given.
Ogre::MaterialPtr light_map_material(const std::string& name, Ogre::SceneBlendType blend) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr material = materials.getByName(name, Ogre::RGN_INTERNAL);
    if (!material) material = materials.create(name, Ogre::RGN_INTERNAL);
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setFog(true, Ogre::FOG_NONE);
    pass->removeAllTextureUnitStates();
    pass->createTextureUnitState();
    pass->setDepthWriteEnabled(false);
    pass->setDepthCheckEnabled(false);
    material->setSceneBlending(blend);
    return material;
}

bool ShadowCasters::renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort,
                                     Ogre::Technique** technique, Ogre::RenderQueue*) {
    if (group == kShadowFadeGroup) return true;
    auto* sub = dynamic_cast<Ogre::SubEntity*>(rend);
    if (sub == nullptr || technique == nullptr) return false;
    Ogre::Entity* entity = sub->getParent();
    if (!entity->getCastShadows()) return false;
    const float opacity = entity_opacity(entity);
    if (!(opacity > 0.0f)) return false;
    // `floorf(opacity * 12)` less one, 0 when not above 0.
    int level = static_cast<int>(std::floor(opacity * 12.0f)) - 1;
    if (level <= 0) level = 0;
    if (level >= kShadowLevels) level = kShadowLevels - 1;
    unsigned bones = 0;
    unsigned weights = 0;
    const bool skinned = gpu_ != nullptr && gpu_->gpu_skin(rend, bones, weights);
    const Ogre::MaterialPtr mat = material(level, skinned ? bones : 0u, skinned ? weights : 0u);
    if (!mat) return false;
    Ogre::Technique* best = mat->getBestTechnique(0, rend);
    if (best == nullptr && mat->getCompilationRequired()) {
        // The shader generator rewrote every shader since: touched so it compiles
        // (compile_waiting_material()).
        mat->touch();
        best = mat->getBestTechnique(0, rend);
    }
    if (best == nullptr) return false;
    *technique = best;
    return true;
}

/// `CGame::createMaterials`: `RenderShadow<i>`,
/// ambient and diffuse black, emissive 0.5 less a 24th a level (`fnmsubs`), no depth
/// check or write, no lights (`+0xce`, `setMaxSimultaneousLights(0)`).
Ogre::MaterialPtr ShadowCasters::material(int level, unsigned bones, unsigned weights) {
    const auto key = std::make_tuple(level, bones, weights);
    const auto found = materials_.find(key);
    if (found != materials_.end()) return found->second;
    std::string name = "tl/RenderShadow" + std::to_string(level);
    if (weights > 0) name += "/skin" + std::to_string(bones) + "x" + std::to_string(weights);
    auto& manager = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = manager.getByName(name, Ogre::RGN_INTERNAL);
    if (!mat) {
        mat = manager.create(name, Ogre::RGN_INTERNAL);
        Ogre::Pass* pass = mat->getTechnique(0)->getPass(0);
        const float glow = std::fma(-static_cast<float>(level), 0.0416666679f, 0.5f);
        pass->setAmbient(Ogre::ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
        pass->setDiffuse(Ogre::ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
        pass->setSelfIllumination(glow, glow, glow);
        pass->setDepthCheckEnabled(false);
        pass->setDepthWriteEnabled(false);
        pass->setMaxSimultaneousLights(0);
        mat->load();
        // Its shaders made now (the scheme resolver does not build internal materials); a
        // skinned twin's by GpuSkinning::skin_material().
        auto* generator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        if (weights == 0 && generator != nullptr &&
            generator->createShaderBasedTechnique(*mat, Ogre::MSN_DEFAULT, Ogre::MSN_SHADERGEN)) {
            generator->validateMaterial(Ogre::MSN_SHADERGEN, *mat);
        }
    }
    if (weights > 0 && (gpu_ == nullptr || !gpu_->skin_material(mat, bones, weights))) {
        // No twin to skin this one: refused, as the bind pose would be wrong.
        mat.reset();
    }
    materials_[key] = mat;
    return mat;
}

namespace {
/// The key `set_render_behind` keeps an entity's state under.
const char* const kRenderBehindKey = "tl_render_behind";
}  // namespace

void set_render_behind(Ogre::Entity* entity, bool on, bool hostile) {
    if (entity == nullptr) return;
    entity->getUserObjectBindings().setUserAny(kRenderBehindKey,
                                               Ogre::Any(RenderBehind{on, hostile}));
    // `CCharacter::setRenderBehind`: queue 0x32 on, 0x58 off (0x5b with
    // `+0x216`, never set here).
    entity->setRenderQueueGroup(static_cast<Ogre::uint8>(
        on ? static_cast<unsigned>(Ogre::RENDER_QUEUE_MAIN) : kCharacterGroup));
    // And each equipped item's model (`CEquipment::setRenderBehind`).
    for (Ogre::MovableObject* attached : entity->getAttachedObjects()) {
        if (auto* held = dynamic_cast<Ogre::Entity*>(attached))
            set_render_behind(held, on, hostile);
    }
}

const RenderBehind* render_behind_of(const Ogre::Entity* entity) {
    if (entity == nullptr) return nullptr;
    const Ogre::Any& held = entity->getUserObjectBindings().getUserAny(kRenderBehindKey);
    return held.has_value() ? Ogre::any_cast<RenderBehind>(&held) : nullptr;
}

bool render_behind_swap(Ogre::Renderable* rend, Ogre::ushort priority, Ogre::Technique** technique,
                        Ogre::RenderQueue* queue, RenderBehindMaterials& materials,
                        GpuSkinning* gpu) {
    auto* sub = dynamic_cast<Ogre::SubEntity*>(rend);
    if (sub == nullptr || technique == nullptr || *technique == nullptr ||
        (*technique)->getNumPasses() == 0) {
        return false;
    }
    Ogre::Entity* entity = sub->getParent();
    const RenderBehind* state = render_behind_of(entity);
    // The record's on flag and its opacity above 0.
    if (state == nullptr || !state->on || !(entity_opacity(entity) > 0.0f)) return false;
    // The record's `[0]`: `setLightOverride` sets it while the unit's
    // light^4 is 0.4 or less: none in the dark.
    if (entity_outline_hidden(entity)) return false;
    unsigned bones = 0;
    unsigned weights = 0;
    const bool skinned = gpu != nullptr && gpu->gpu_skin(rend, bones, weights);
    const Ogre::MaterialPtr mat =
        materials.material(state->hostile, skinned ? bones : 0u, skinned ? weights : 0u);
    if (!mat) return false;
    Ogre::Technique* best = mat->getBestTechnique(0, rend);
    if (best == nullptr && mat->getCompilationRequired()) {
        mat->touch();
        best = mat->getBestTechnique(0, rend);
    }
    if (best == nullptr) return false;
    // The body on 0x58 with its technique, then the render-behind
    // technique in its place (`+0x3c`'s).
    queue->getQueueGroup(static_cast<Ogre::uint8>(kCharacterGroup))
        ->addRenderable(rend, *technique, priority);
    *technique = best;
    return true;
}

/// `CGenericModel::reInitialize`:
/// `rbmat_N`, ambient and diffuse black, no depth write, no fog, `outlineblue.dds`
/// (`outlinered.dds` for a hostile unit, `setRenderBehindTexture`,
/// `CCharacter::setAlignment`) as a curved environment map, depth test `GREATER`, blended
/// `SBT_ADD`; skinned via GpuSkinning::skin_material. Self-illumination 1, scaled by the
/// renderable's light.
Ogre::MaterialPtr RenderBehindMaterials::material(bool hostile, unsigned bones, unsigned weights) {
    const auto key = std::make_tuple(hostile, bones, weights);
    const auto found = materials_.find(key);
    if (found != materials_.end()) return found->second;
    std::string name = std::string("tl/RenderBehind/") + (hostile ? "red" : "blue");
    if (weights > 0) name += "/skin" + std::to_string(bones) + "x" + std::to_string(weights);
    auto& manager = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = manager.getByName(name, Ogre::RGN_INTERNAL);
    if (!mat) {
        mat = manager.create(name, Ogre::RGN_INTERNAL);
        Ogre::Technique* technique = mat->getTechnique(0);
        Ogre::Pass* pass = technique->getPass(0);
        pass->setAmbient(Ogre::ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
        pass->setDiffuse(Ogre::ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
        pass->setSelfIllumination(1.0f, 1.0f, 1.0f);
        pass->setFog(true, Ogre::FOG_NONE);
        pass->setDepthWriteEnabled(false);
        pass->setDepthFunction(Ogre::CMPF_GREATER);
        pass->setSceneBlending(Ogre::SBT_ADD);
        const std::string path =
            resolve_archive_path(hostile ? "media/sharedtextures/outlinered.dds"
                                         : "media/sharedtextures/outlineblue.dds");
        Ogre::TextureUnitState* unit = pass->createTextureUnitState();
        if (!path.empty()) unit->setTextureName(path);
        unit->setEnvironmentMap(true, Ogre::TextureUnitState::ENV_CURVED);
        mat->load();
        auto* generator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        if (weights == 0 && generator != nullptr &&
            generator->createShaderBasedTechnique(*mat, Ogre::MSN_DEFAULT, Ogre::MSN_SHADERGEN)) {
            generator->validateMaterial(Ogre::MSN_SHADERGEN, *mat);
        }
    }
    if (weights > 0 && (gpu_ == nullptr || !gpu_->skin_material(mat, bones, weights))) {
        mat.reset();
    }
    materials_[key] = mat;
    return mat;
}

/// `TextureUnitState::setTextureName(name, TEX_TYPE_2D)` on a light map material, from the
/// rules' archive path (`CGameClient::refreshLighting`); a
/// missing path leaves it.
void set_light_map_texture(const Ogre::MaterialPtr& material, const std::string& reference) {
    const std::string path = resolve_archive_path(reference);
    if (path.empty()) {
        TL_LOG_WARN("  light map texture '%s' is not in the archive", reference.c_str());
        return;
    }
    Ogre::TextureUnitState* unit = material->getTechnique(0)->getPass(0)->getTextureUnitState(0);
    unit->setTextureName(path);
    unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
}

}  // namespace renderer_detail
}  // namespace tl::render
