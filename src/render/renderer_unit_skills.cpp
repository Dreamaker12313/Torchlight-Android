// A placed unit's skills: the [SKILL] blocks CBaseUnit::unitInit gives it and the scene's
// half of CCharacter::castSkill, events run as the unit's. What to cast and when is the
// level script's (LevelScript::select_offensive_skill).
#include "core/unit_skills_reader.h"
#include "render/renderer_impl.h"

namespace tl::render {

// PORT: the drawn unit of a machine's node (the port's scene bookkeeping).
Renderer::Impl::SceneUnit* Renderer::Impl::scene_unit_of(tl::core::LevelScript* machine,
                                                         int64_t node) {
    NodeMap* nodes = nodes_of(machine);
    if (nodes == nullptr) return nullptr;
    const auto range = nodes->equal_range(node);
    for (auto& entry : scene_units) {
        SceneUnit& unit = entry.second;
        if (unit.node != node || unit.entity == nullptr) continue;
        const Ogre::SceneNode* parent = unit.entity->getParentSceneNode();
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == parent) return &unit;
        }
    }
    return nullptr;
}

std::vector<tl::core::LevelScript::UnitSkill> Renderer::Impl::unit_skill_list(
    const std::string& unit) {
    std::vector<tl::core::LevelScript::UnitSkill> out;
    ensure_monster_index();
    const auto found = monster_files.find(lowered(unit));
    if (found == monster_files.end()) return out;
    ensure_player_skill_files();
    // The unit with its BASEFILE merged under it:
    // the base's [SKILL] blocks come first.
    const tl::formats::AdmFile file = read_unit_file(found->second);
    if (!file.ok) return out;
    out = tl::core::read_unit_skills(
        file,
        [this](const std::string& upper_name) -> const tl::core::SkillDef* {
            const auto skill = player_skill_files.find(upper_name);
            return skill == player_skill_files.end() ? nullptr : &skill->second->def;
        },
        [&unit](const std::string& name) {
            // `addSkillByName` found nothing: the original logs it and moves on.
            TL_LOG_WARN("skills: Skill not found : %s (%s)", name.c_str(), unit.c_str());
        });
    if (!out.empty()) {
        std::string names;
        for (const auto& s : out) {
            names += (names.empty() ? "" : ", ") + s.known.entry.name + " (" +
                     std::to_string(s.cast_chance) + "%)";
        }
        TL_LOG_INFO("skills: %s holds %zu skill(s): %s", unit.c_str(), out.size(), names.c_str());
    }
    return out;
}

// PORT: a mesh's animation catalogue, read once and kept.
const tl::core::AnimationCatalog* Renderer::Impl::unit_catalog(const std::string& mesh) {
    if (mesh.empty()) return nullptr;
    const auto cached = unit_catalogs.find(mesh);
    if (cached != unit_catalogs.end()) return &cached->second;
    return &unit_catalogs.emplace(mesh, model_catalog(mesh, dir_of_asset(mesh))).first->second;
}

tl::core::AnimationCatalog Renderer::Impl::model_catalog(const std::string& mesh,
                                                         const std::string& dir, size_t* missing) {
    // CGenericModel::loadAnimations: the .ANIMATION
    // blocks in order, a missing file passed over, a duplicate skipped, the mesh's BIND
    // filed; each as long as its skeleton's first animation. INVENTED: filed by file stem
    // where the console uses the skeleton's own animation name.
    const tl::core::AnimationManifest& manifest = manifest_for(mesh);
    std::vector<tl::core::AnimationFileEntry> entries;
    std::vector<float> lengths;
    for (const std::string& stem : manifest.order()) {
        tl::core::AnimationFileEntry entry;
        entry.file = stem + ".skeleton";
        float length = 0.0f;
        const std::string path = archive_path_in_dir(stem + ".SKELETON", dir);
        if (!path.empty()) {
            try {
                Ogre::SkeletonPtr skeleton = Ogre::static_pointer_cast<Ogre::Skeleton>(
                    Ogre::SkeletonManager::getSingleton().load(path, kGameGroup));
                if (skeleton) {
                    entry.exists = true;
                    for (unsigned short i = 0; i < skeleton->getNumAnimations(); ++i) {
                        entry.skeleton_names.push_back(skeleton->getAnimation(i)->getName());
                    }
                    if (skeleton->getNumAnimations() > 0)
                        length = skeleton->getAnimation(0)->getLength();
                }
            } catch (const Ogre::Exception& e) {
                TL_LOG_WARN("skills: could not load '%s': %s", path.c_str(),
                            e.getDescription().c_str());
            }
        } else if (missing != nullptr) {
            ++*missing;
        }
        entries.push_back(entry);
        lengths.push_back(length);
    }
    // What the mesh's own skeleton holds before anything is linked onto it.
    std::vector<std::string> has;
    try {
        Ogre::MeshPtr model = Ogre::MeshManager::getSingleton().getByName(mesh, kGameGroup);
        if (model && model->hasSkeleton() && model->getSkeleton()) {
            for (unsigned short i = 0; i < model->getSkeleton()->getNumAnimations(); ++i) {
                has.push_back(model->getSkeleton()->getAnimation(i)->getName());
            }
        }
    } catch (const Ogre::Exception&) {}
    std::string model_stem = mesh;
    if (const size_t slash = model_stem.find_last_of("/\\"); slash != std::string::npos) {
        model_stem = model_stem.substr(slash + 1);
    }
    if (model_stem.size() >= 5) model_stem.resize(model_stem.size() - 5);
    const std::vector<tl::core::LoadedAnimation> list =
        tl::core::load_animation_list(entries, model_stem, has);

    tl::core::AnimationCatalog catalog;
    for (size_t b = 0; b < list.size(); ++b) {
        if (list[b].index < 0) continue;
        const size_t at = static_cast<size_t>(list[b].index);
        if (catalog.size() <= at) catalog.resize(at + 1);
        catalog[at] = {manifest.order()[b], lengths[b]};
    }
    return catalog;
}

tl::core::SkillCaster Renderer::Impl::unit_caster(tl::core::LevelScript& machine, int64_t node,
                                                  tl::core::LevelScript* target_machine,
                                                  int64_t target_node, bool has_target) {
    tl::core::SkillCaster caster;
    if (const tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node)) {
        caster.mana = k->mana;
    }
    load_unit_types();
    caster.unit_types = &unit_types;
    // `CBaseUnit::ISA(0x1b)`: `MONSTERCOOLDOWNMS`.
    caster.is_monster = true;
    caster.is_character = true;
    caster.effects = machine.unit_effects(node);
    // `getWeaponInRightHand` / `InLeftHand`: what the unit was given to hold.
    std::string right_hand, left_hand;
    machine.unit_hands(node, right_hand, left_hand);
    ensure_item_index();
    if (!right_hand.empty()) {
        if (const tl::core::ItemStats* item = items.find(right_hand)) {
            caster.right_hand_type = item->unittype;
        }
    }
    if (!left_hand.empty()) {
        if (const tl::core::ItemStats* item = items.find(left_hand)) {
            caster.left_hand_type = item->unittype;
        }
    }
    if (SceneUnit* drawn = scene_unit_of(&machine, node)) caster.model = unit_catalog(drawn->mesh);
    // `getPosition(true)`: the sphere's centre.
    tl::core::Vec3 feet;
    if (machine.unit_position(node, feet)) {
        caster.position = {feet.x, feet.y + machine.unit_radius(node), feet.z};
    }
    if (has_target) {
        tl::core::Vec3 at;
        bool got = false;
        if (target_machine == nullptr) {
            const float body = player_stats.radius > 0.0f ? player_stats.radius
                                                          : tl::core::kDefaultCollisionRadius;
            at = {player.position().x, player.position().y + body, player.position().z};
            got = true;
        } else if (target_machine->unit_position(target_node, at)) {
            at.y += target_machine->unit_radius(target_node);
            got = true;
        }
        if (got) {
            tl::core::SkillTargetUnit target;
            target.position = at;
            caster.target_unit = target;
            caster.target_position = at;
        }
    }
    // `CLevel::passableBetween` on the map
    // grid. INVENTED: the caster and the target are not taken out of the object
    // grid first.
    caster.passable_between = [this](const tl::core::Vec3& from, const tl::core::Vec3& to) {
        return !passability.ready() || passability.passable_between(from, to, true);
    };
    return caster;
}

// PORT: the caster a unit's skill events are handed (its scene node and radius).
Renderer::Impl::UnitEventCaster Renderer::Impl::unit_event_caster_for(
    tl::core::LevelScript* machine, int64_t node) {
    UnitEventCaster caster;
    caster.machine = machine;
    caster.node = node;
    if (NodeMap* nodes = nodes_of(machine)) {
        const auto found = nodes->find(node);
        if (found != nodes->end()) caster.scene = found->second;
    }
    const float radius = machine->unit_radius(node);
    caster.radius = radius > 0.0f ? radius : tl::core::kDefaultCollisionRadius;
    return caster;
}

// PORT: the context `SkillRunner::cast` reads for a unit's skill: the caster's
// own effect manager, the tables, and the event hooks into the renderer.
// `setOwner(effect, caster)`: the casting unit owns what lands.
tl::core::SkillCastContext Renderer::Impl::unit_cast_context(tl::core::LevelScript& machine,
                                                             int64_t node,
                                                             tl::core::LevelScript* target_machine,
                                                             int64_t target_node, bool has_target) {
    ensure_effect_data();
    tl::core::SkillCastContext context;
    // A self-buff lands on the caster's own manager.
    context.target = unit_effect_manager(&machine, node);
    context.types = &effect_types;
    context.unit_types = &unit_types;
    context.affix = [this](const std::string& name) -> const tl::core::Affix* {
        return affixes.get_affix(name);
    };
    context.target_is_caster = true;
    context.target_is_enemy = false;
    context.target_is_casters_pet = false;
    // `setOwner(effect, caster)`: the casting unit owns what lands.
    context.source = tl::core::EffectSource{true, &machine, node};
    context.owner_level = event_caster_level(&machine, node);
    unit_cast_caster = unit_caster(machine, node, target_machine, target_node, has_target);
    context.caster = &unit_cast_caster;
    tl::core::LevelScript* const owner = &machine;
    context.on_event = [this, owner, node](const tl::core::KnownSkill& skill, uint32_t level,
                                           int group, const tl::formats::AdmSection& event,
                                           const tl::core::SkillEventTarget& target) {
        const UnitEventCaster saved = unit_event_caster;
        unit_event_caster = unit_event_caster_for(owner, node);
        start_skill_event(skill, level, group, event, target);
        unit_event_caster = saved;
    };
    context.events_alive = [this, owner, node](const std::string& skill, uint32_t run) {
        return skill_events_alive(skill, run, owner, node);
    };
    context.events_can_stop = [this, owner, node](const std::string& skill, uint32_t run) {
        return skill_events_can_stop(skill, run, owner, node);
    };
    context.reset_events = [this, owner, node](const std::string& skill, uint32_t run) {
        reset_skill_events(skill, run, owner, node);
    };
    context.reset_event = [this, owner, node](const std::string& skill, uint32_t run,
                                              const tl::formats::AdmSection& block) {
        detach_event_missiles(skill, run, block, owner, node);
    };
    if (has_target) {
        // A null machine with a unit is the player.
        context.event_target.has_unit = true;
        context.event_target.unit = target_machine != nullptr ? target_node : 0;
        context.event_target.machine = target_machine;
    }
    return context;
}

// PORT: a unit's known skill found by name in its casting list.
const tl::core::KnownSkill* Renderer::Impl::unit_skill_named(tl::core::LevelScript& machine,
                                                             int64_t node,
                                                             const std::string& name) {
    tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
    if (k == nullptr) return nullptr;
    for (const tl::core::LevelScript::UnitSkill& s : k->skills) {
        if (s.known.entry.name == name) return &s.known;
    }
    return nullptr;
}

// PORT: where a unit stands and faces, for `position_for_skill`: the player's
// from its OGRE node, a machine unit's from the machine.
bool Renderer::Impl::skill_position_unit(tl::core::LevelScript* machine, int64_t node,
                                         tl::core::SkillPositionUnit& out) {
    // A null machine is the player.
    if (machine == nullptr) {
        if (subject_node == nullptr) return false;
        out.radius =
            player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
        out.centre = {player.position().x, player.position().y + out.radius, player.position().z};
        const Ogre::Vector3 forward = subject_node->getOrientation() * Ogre::Vector3::UNIT_Z;
        out.forward = {forward.x, forward.y, forward.z};
        return true;
    }
    tl::core::Vec3 feet;
    if (!machine->unit_position(node, feet)) return false;
    out.radius = machine->unit_radius(node);
    out.centre = {feet.x, feet.y + out.radius, feet.z};
    float yaw = 0.0f;
    machine->unit_facing(node, yaw);
    out.forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
    return true;
}

void Renderer::Impl::aim_unit_position_skill(tl::core::LevelScript& machine, int64_t node,
                                             const tl::core::KnownSkill& skill) {
    // `CCharacter::castSkill`: a skill aimed at a place
    // (`POSITION`, 0xb, `POSITIONRANDOM`, `POSITIONFLEE`) is cast at the point
    // the level machine's `cast_skill` left as the final target position.
    const tl::core::SkillProperty* property = skill.property(skill.effective);
    if (property == nullptr) return;
    const int32_t type = property->target_type;
    if (type != 1 && type != 0xb && type != 5 && type != 6) return;
    tl::core::Vec3 at;
    if (machine.unit_final_target(node, at)) unit_cast_caster.target_position = at;
}

// `CCharacter::castSkill` for a level unit:
// the runner's cast (`SkillRunner::cast`), the mana spent, and the cast's
// animation cue played on the unit's model.
bool Renderer::Impl::cast_unit_skill(tl::core::LevelScript& machine, int64_t node,
                                     tl::core::LevelScript* target_machine, int64_t target_node,
                                     bool has_target) {
    tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
    if (k == nullptr || k->active < 0 || k->active >= static_cast<int>(k->skills.size())) {
        return false;
    }
    tl::core::LevelScript::UnitSkill& s = k->skills[static_cast<size_t>(k->active)];
    if (s.known.def == nullptr) return false;
    const std::string name = s.known.entry.name;
    const std::string unit = machine.unit_of(node);

    tl::core::SkillCastContext context =
        unit_cast_context(machine, node, target_machine, target_node, has_target);
    aim_unit_position_skill(machine, node, s.known);
    float spent = 0.0f;
    const UnitEventCaster saved = unit_event_caster;
    unit_event_caster = unit_event_caster_for(&machine, node);
    const tl::core::SkillCastResult result =
        k->runner.cast(s.known, unit_cast_caster, context, &spent);
    unit_event_caster = saved;
    if (result != tl::core::SkillCastResult::Cast) {
        TL_LOG_INFO("skills: %s does not cast '%s': %s", unit.c_str(), name.c_str(),
                    tl::core::skill_cast_result_name(result));
        return false;
    }
    // `modifyMana(-getManaCost())`.
    k->mana = std::max(0.0f, k->mana - spent);

    // The animation: `castSkill`'s cue, played on the unit's own model.
    tl::core::SkillRunState* running = k->runner.state(name);
    if (running != nullptr) {
        const tl::core::SkillAnimationCue cue = tl::core::begin_skill_animation(
            k->cast, name, *s.known.current_property(), unit_cast_caster, *running);
        const tl::core::AnimationCatalog* catalog = unit_cast_caster.model;
        SceneUnit* drawn = scene_unit_of(&machine, node);
        if (catalog != nullptr && drawn != nullptr && 0 <= cue.animation &&
            static_cast<size_t>(cue.animation) < catalog->size()) {
            const auto& entry = (*catalog)[static_cast<size_t>(cue.animation)];
            Ogre::AnimationState* started = nullptr;
            // `blendAnimation(animation, .., 0.2, speed, duration)`, through the
            // unit's animator.
            if (play_scene_animation(drawn->entity, drawn->mesh, entry.first, cue.duration < 0.0f,
                                     drawn->rate * cue.speed, /*optional=*/true, &started,
                                     drawn->armed, nullptr, /*blend=*/0.2f)) {
                remember_state(drawn->entity, entry.first, started);
                drawn->playing = entry.first;
                drawn->playing_state = started;
                drawn->last_time = 0.0f;
                unit_skill_playbacks.erase(
                    std::remove_if(unit_skill_playbacks.begin(), unit_skill_playbacks.end(),
                                   [&](const UnitSkillPlayback& p) {
                                       return p.machine == &machine && p.node == node;
                                   }),
                    unit_skill_playbacks.end());
                UnitSkillPlayback playback;
                playback.machine = &machine;
                playback.node = node;
                playback.animation = entry.first;
                playback.index = cue.animation;
                playback.length = entry.second;
                playback.speed = cue.speed;
                playback.held = cue.duration;
                playback.then_loop = cue.then_loop;
                unit_skill_playbacks.push_back(playback);
            }
        }
    }
    TL_LOG_INFO("skills: %s casts '%s' at level %u%s", unit.c_str(), name.c_str(),
                s.known.effective,
                !has_target                 ? ""
                : target_machine == nullptr ? ", at the player"
                                            : ", at a unit");
    return true;
}

void Renderer::Impl::step_unit_skills(float delta) {
    if (!(delta > 0.0f)) return;
    // The cast animations: the keys each frame steps past (`updateAnimation`),
    // then `updateSkillKeys`
    // and the groups they fire.
    for (size_t i = 0; i < unit_skill_playbacks.size();) {
        UnitSkillPlayback& p = unit_skill_playbacks[i];
        tl::core::LevelScript::UnitCasting* k = p.machine->unit_casting(p.node);
        SceneUnit* drawn = scene_unit_of(p.machine, p.node);
        if (k == nullptr || drawn == nullptr || p.machine->unit_is_dead(p.node)) {
            unit_skill_playbacks.erase(unit_skill_playbacks.begin() + static_cast<long>(i));
            continue;
        }
        const float from = p.time;
        float to = from + delta * p.speed;
        if (p.length > 0.0f && to > p.length) {
            to = p.held < 0.0f ? std::fmod(to, p.length) : p.length;
        }
        p.time = to;
        const tl::core::AnimationManifest& manifest = manifest_for(drawn->mesh);
        const std::vector<tl::core::FiredKey> keys =
            tl::core::keys_crossed(manifest.tags_for(p.animation), p.index, from, to);
        const std::string name = k->cast.skill;
        const tl::core::SkillRunState* state = k->runner.state(name);
        if (state != nullptr && !keys.empty()) {
            const std::vector<int> groups =
                tl::core::update_skill_keys(k->cast, state->animations, k->runner.is_running(name),
                                            !p.machine->unit_is_dead(p.node), keys);
            tl::core::LevelScript* const owner = p.machine;
            const int64_t node = p.node;
            for (int group : groups) {
                // The run's own target (`CSkill +0x40`) and point stand: the
                // group is fired with none of its own.
                tl::core::SkillCastContext context =
                    unit_cast_context(*owner, node, nullptr, 0, false);
                const UnitEventCaster saved = unit_event_caster;
                unit_event_caster = unit_event_caster_for(owner, node);
                const int applied = k->runner.trigger_event(
                    group, context, [this, owner, node](const std::string& n) {
                        return unit_skill_named(*owner, node, n);
                    });
                unit_event_caster = saved;
                TL_LOG_INFO(
                    "skills: %s's '%s' key at %.3fs fires %s, %d skill(s) applied",
                    owner->unit_of(node).c_str(), p.animation.c_str(), static_cast<double>(to),
                    group == tl::core::kSkillEventTrigger ? "EVENT_TRIGGER" : "EVENT_TRIGGER_TWO",
                    applied);
            }
        }
        // `CCharacter::updateAnimation`:
        // a looping cue lasts while the cast holds; once the flag
        // (`+0x267`) is down and no swing has time left, the gait takes the
        // model back.
        if (p.held < 0.0f && !k->cast.casting &&
            !(k->cast.animation_left > 0.0f && !k->cast.attack_ended)) {
            unit_skill_playbacks.erase(unit_skill_playbacks.begin() + static_cast<long>(i));
            continue;
        }
        // The cue's hold: then its queued loop, or nothing - the unit's gait
        // takes the model back.
        if (p.held >= 0.0f) {
            p.held -= delta;
            if (p.held <= 0.0f) {
                if (p.then_loop >= 0 &&
                    p.then_loop < static_cast<int>(unit_catalog(drawn->mesh)->size())) {
                    const auto& entry =
                        (*unit_catalog(drawn->mesh))[static_cast<size_t>(p.then_loop)];
                    Ogre::AnimationState* started = nullptr;
                    if (play_scene_animation(drawn->entity, drawn->mesh, entry.first, true,
                                             drawn->rate, true, &started, drawn->armed)) {
                        remember_state(drawn->entity, entry.first, started);
                        drawn->playing = entry.first;
                        drawn->playing_state = started;
                    }
                    p.animation = entry.first;
                    p.index = p.then_loop;
                    p.length = entry.second;
                    p.speed = 1.0f;
                    p.held = -1.0f;
                    p.time = 0.0f;
                    p.then_loop = -1;
                } else {
                    unit_skill_playbacks.erase(unit_skill_playbacks.begin() + static_cast<long>(i));
                    continue;
                }
            }
        }
        ++i;
    }

    // Every unit holding a skill: `updateActiveSkill`,
    // then the manager's `update`
    // - cooldowns, runs, `EVENT_END`.
    for (const MachineView& view : machines()) {
        tl::core::LevelScript* const owner = view.script;
        for (int64_t node : owner->casting_units()) {
            tl::core::LevelScript::UnitCasting* k = owner->unit_casting(node);
            if (k == nullptr) continue;
            // DIVERGENCE: `CCharacter::unitInit`
            // fires these as the unit is
            // made; here it is the first frame the unit's skills are stepped,
            // when its model is in the scene for the events to use.
            if (!k->create_fired) {
                k->create_fired = true;
                fire_unit_skills_on_create(*owner, node);
            }
            const std::string name = k->cast.skill;
            const tl::core::SkillRunState* state = k->runner.state(name);
            int64_t upkeep = 0;
            if (const tl::core::KnownSkill* known = unit_skill_named(*owner, node, name)) {
                if (state != nullptr) {
                    if (const tl::core::SkillProperty* property = known->property(state->level)) {
                        upkeep = static_cast<int64_t>(property->mana_cost_ot);
                    }
                }
            }
            const tl::core::ActiveSkillUpdate update = tl::core::update_active_skill(
                k->cast, state != nullptr ? state->animations : tl::core::SkillAnimations{},
                k->runner.is_running(name), upkeep, k->mana, delta);
            k->mana = std::max(0.0f, k->mana - update.mana_spent);
            if (update.hide_trails) {
                if (const SceneUnit* drawn = scene_unit_of(owner, node)) {
                    const auto trails = unit_trails.find(drawn->entity);
                    if (trails != unit_trails.end()) trails->second.trails.hide_hands();
                }
            }
            tl::core::SkillCastContext context = unit_cast_context(*owner, node, nullptr, 0, false);
            const auto lookup = [this, owner, node](const std::string& n) {
                return unit_skill_named(*owner, node, n);
            };
            const UnitEventCaster saved = unit_event_caster;
            unit_event_caster = unit_event_caster_for(owner, node);
            // `updateActiveSkill`:
            // `stopAllSkills(false, false, false)` when the upkeep runs dry.
            if (update.stop_all) k->runner.stop_all(context, lookup, false, false);
            k->runner.update(delta, context, lookup);
            unit_event_caster = saved;
        }
    }
}

void Renderer::Impl::fire_unit_skills_on_death(tl::core::LevelScript& machine, int64_t node) {
    // CCharacter::die: stopAllSkills(true, true, false)
    // spares EVENT_CASTERDIE and DONT_STOP_ON_DEATH;
    // then CSkillManager::fireSkillsOnDeath executes
    // every EVENT_CASTERDIE skill (group 9, CSkill::hasEvent) and fires group 9.
    tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
    if (k == nullptr || k->skills.empty()) return;
    const auto has_caster_die = [](const tl::core::KnownSkill& skill) {
        const tl::core::SkillProperty* property = skill.property(skill.effective);
        if (property == nullptr || property->section == nullptr) return false;
        for (const tl::formats::AdmSection& child : property->event_section().children) {
            if (tl::core::upper_ascii(child.name) == "EVENT_CASTERDIE") return true;
        }
        return false;
    };
    tl::core::LevelScript* target_machine = nullptr;
    int64_t target_node = 0;
    const bool has_target = machine.unit_target(node, target_machine, target_node);
    tl::core::SkillCastContext context =
        unit_cast_context(machine, node, target_machine, target_node, has_target);
    const auto lookup = [this, &machine, node](const std::string& n) {
        return unit_skill_named(machine, node, n);
    };
    const UnitEventCaster saved = unit_event_caster;
    unit_event_caster = unit_event_caster_for(&machine, node);
    for (const tl::core::LevelScript::UnitSkill& s : k->skills) {
        if (s.known.def == nullptr || !k->runner.is_running(s.known.entry.name)) continue;
        const tl::formats::AdmSection* root = s.known.def->base.section;
        const bool stays = has_caster_die(s.known) ||
                           (root != nullptr && root->bool_or("DONT_STOP_ON_DEATH", false));
        if (!stays) k->runner.stop(s.known, context);
    }
    for (const tl::core::LevelScript::UnitSkill& s : k->skills) {
        if (s.known.def == nullptr || !has_caster_die(s.known)) continue;
        const tl::core::SkillProperty* property = s.known.property(s.known.effective);
        const int32_t type = property != nullptr ? property->target_type : 0;
        const std::optional<tl::core::Vec3> aimed_at = unit_cast_caster.target_position;
        if (type == 1 || type == 5 || type == 6) {
            // getPositionForSkill(skill, null), no unit
            // - for every later skill too (the argument is cleared).
            // INVENTED: a never-started skill (CSkill +0x50, written by startSkill
            // and castSkill) uses the origin.
            context.event_target.has_unit = false;
            context.event_target.unit = 0;
            context.event_target.machine = nullptr;
            tl::core::Vec3 point{0.0f, 0.0f, 0.0f};
            if (const tl::core::SkillRunState* state = k->runner.state(s.known.entry.name)) {
                if (state->target_position.has_value()) point = *state->target_position;
            }
            tl::core::SkillPositionUnit owner;
            if (property != nullptr && skill_position_unit(&machine, node, owner)) {
                const std::optional<tl::core::Vec3> at = tl::core::position_for_skill(
                    *property, point, &owner, nullptr, tl::core::volatile_random());
                if (at.has_value()) {
                    unit_cast_caster.target_position = *at;
                    TL_LOG_INFO("skills: %s's '%s' is aimed at (%.2f, %.2f, %.2f)",
                                machine.unit_of(node).c_str(), s.known.entry.name.c_str(),
                                static_cast<double>(at->x), static_cast<double>(at->y),
                                static_cast<double>(at->z));
                }
            }
        }
        const tl::core::SkillCastContext started = context;
        if (k->runner.execute(s.known, s.known.effective, unit_cast_caster, started)) {
            TL_LOG_INFO("skills: %s dies and starts '%s'", machine.unit_of(node).c_str(),
                        s.known.entry.name.c_str());
        }
        unit_cast_caster.target_position = aimed_at;
    }
    constexpr int kCasterDie = 9;
    k->runner.trigger_event(kCasterDie, context, lookup);
    unit_event_caster = saved;
}

void Renderer::Impl::fire_unit_skills_on_create(tl::core::LevelScript& machine, int64_t node) {
    // CSkillManager::fireSkillsOnCreate, from
    // CCharacter::unitInit: every EVENT_UNIT_CREATE skill
    // (group 10) executed without a target, then group 10 fired.
    tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
    if (k == nullptr || k->skills.empty()) return;
    const auto has_unit_create = [](const tl::core::KnownSkill& skill) {
        const tl::core::SkillProperty* property = skill.property(skill.effective);
        if (property == nullptr || property->section == nullptr) return false;
        for (const tl::formats::AdmSection& child : property->event_section().children) {
            if (tl::core::upper_ascii(child.name) == "EVENT_UNIT_CREATE") return true;
        }
        return false;
    };
    bool any = false;
    for (const tl::core::LevelScript::UnitSkill& s : k->skills) {
        if (s.known.def != nullptr && has_unit_create(s.known)) any = true;
    }
    if (!any) return;
    tl::core::SkillCastContext context = unit_cast_context(machine, node, nullptr, 0, false);
    const auto lookup = [this, &machine, node](const std::string& n) {
        return unit_skill_named(machine, node, n);
    };
    const UnitEventCaster saved = unit_event_caster;
    unit_event_caster = unit_event_caster_for(&machine, node);
    for (const tl::core::LevelScript::UnitSkill& s : k->skills) {
        if (s.known.def == nullptr || !has_unit_create(s.known)) continue;
        if (k->runner.execute(s.known, s.known.effective, unit_cast_caster, context)) {
            TL_LOG_INFO("skills: %s is made and starts '%s'", machine.unit_of(node).c_str(),
                        s.known.entry.name.c_str());
        }
    }
    constexpr int kUnitCreate = 10;
    k->runner.trigger_event(kUnitCreate, context, lookup);
    unit_event_caster = saved;
}

// PORT: the hooks a level's machines call for their units' skills, into the renderer.
tl::core::LevelScript::UnitSkills Renderer::Impl::unit_skill_hooks() {
    tl::core::LevelScript::UnitSkills hooks;
    hooks.load = [this](const std::string& unit) { return unit_skill_list(unit); };
    hooks.skill_files = skill_file_lookup();
    hooks.skill_def = [this](const std::string& upper_name) -> const tl::core::SkillDef* {
        ensure_player_skill_files();
        const auto found = player_skill_files.find(upper_name);
        return found == player_skill_files.end() ? nullptr : &found->second->def;
    };
    hooks.layout_adds_sample = [this](const std::string& file) {
        ensure_sound_index();
        const tl::formats::CmpScene* scene = layout_file(file);
        return scene != nullptr && tl::core::layout_adds_sound_sample(*scene, sound_info);
    };
    hooks.procs = [this](tl::core::LevelScript& machine, int64_t node, int type,
                         tl::core::LevelScript* target_machine, int64_t target_node,
                         bool has_target) {
        unit_execute_procs(machine, node, type, target_machine, target_node, has_target);
    };
    hooks.caster = [this](tl::core::LevelScript& machine, int64_t node,
                          tl::core::LevelScript* target_machine, int64_t target_node,
                          bool has_target) {
        return unit_caster(machine, node, target_machine, target_node, has_target);
    };
    hooks.cast = [this](tl::core::LevelScript& machine, int64_t node,
                        tl::core::LevelScript* target_machine, int64_t target_node,
                        bool has_target) {
        return cast_unit_skill(machine, node, target_machine, target_node, has_target);
    };
    hooks.stop_all = [this](tl::core::LevelScript& machine, int64_t node, bool force) {
        tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
        if (k == nullptr) return;
        tl::core::SkillCastContext context = unit_cast_context(machine, node, nullptr, 0, false);
        const UnitEventCaster saved = unit_event_caster;
        unit_event_caster = unit_event_caster_for(&machine, node);
        k->runner.stop_all(
            context,
            [this, &machine, node](const std::string& n) {
                return unit_skill_named(machine, node, n);
            },
            force, /*passives=*/false);
        unit_event_caster = saved;
    };
    hooks.stop_by_name = [this](tl::core::LevelScript& machine, int64_t node,
                                const std::string& name) {
        tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
        if (k == nullptr) return;
        tl::core::SkillCastContext context = unit_cast_context(machine, node, nullptr, 0, false);
        const UnitEventCaster saved = unit_event_caster;
        unit_event_caster = unit_event_caster_for(&machine, node);
        k->runner.stop_by_name(name, context, [this, &machine, node](const std::string& n) {
            return unit_skill_named(machine, node, n);
        });
        unit_event_caster = saved;
    };
    load_recharge_rates();
    hooks.mana_recharge_rate = recharge_rates.mana;
    return hooks;
}

}  // namespace tl::render
