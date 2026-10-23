// A skill event's life after it starts: `CSkillEvent::updateEvent`'s per-frame half, when it
// stops (`shouldStop`, `canStop`, `stopEvent`) and what is released after (core/world.h). Split
// from world_effects.cpp.
#include "core/world.h"

#include <algorithm>
#include <cmath>

namespace tl::core {

void World::update_live_skill_events(float delta) {
    release_ended_skill_events();
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
    // `getOwnerCharacter`'s.
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
        ogre::Vector3 to;
        if (event.skill_target_type != 1) {
            ogre::Vector3 offset(local.x, local.y, local.z);
            if (event.scale_distance) {
                offset.x *= event.distance;
                offset.z *= event.distance;
            }
            to = event.facing * offset + event.at;
        } else {
            to = ogre::Vector3(event.to_target.x * local.z, event.to_target.y * local.z,
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
            stop_skill_event(event);
            ++k;
            continue;
        }
        ++k;
    }
}

void World::update_skill_event_lines(LiveSkillEvent& event) {
    // `CSkillEvent::updateEvent`. INVENTED: the target's
    // lead (`TARGET_AHEAD_OF_UNIT`, +0x164) is not added.
    const bool want_lines =
        (event.attach_line_emitter || event.to_max_distance) && !event.line_holders.empty();
    if (!event.far_target && !want_lines) return;
    ogre::Vector3 start;
    ogre::Vector3 forward;
    if (!skill_bone_world(event.owner, event.fire_from, start) ||
        !skill_unit_forward(event.owner, forward)) {
        return;
    }
    // The far end: the range along the owner's facing, cut 0.1 short of a
    // `LINE_EMITTER_COLLIDES` hit (`CLevel::rayCollision`).
    const auto cut = [this, &event](const ogre::Vector3& from, ogre::Vector3 to) {
        tl::core::CollisionHit hit;
        if (event.collides &&
            collision.ray_collision({from.x, from.y, from.z}, {to.x, to.y, to.z}, true, hit)) {
            to = ogre::Vector3(hit.point.x, hit.point.y, hit.point.z) -
                 ogre::Vector3(hit.normal.x, hit.normal.y, hit.normal.z) * 0.1f;
        }
        return to;
    };
    const ogre::Vector3 far = cut(start, start + forward * event.range);

    // `ATTACHTOTARGET` with no target, or +0x148: at the far end. INVENTED: moved, not
    // turned (vtable +0x118); its machine's nodes stay.
    if (event.far_target) {
        const ogre::Vector3 by = far - event.far_point;
        if (by.squaredLength() > 0.0f) view_translate(event.holders, by);
        event.far_point = far;
    }

    if (!want_lines) return;
    // The line: `FIREFROMBONE` to `FIREATBONE`, or to the far end (no target, or +0x148).
    ogre::Vector3 end = far;
    if (event.has_target && !event.to_max_distance) {
        ogre::Vector3 at;
        if (skill_bone_world(event.target, event.fire_at, at)) end = cut(start, at);
    }
    // The wrapper's particle at the bone, `LineEmitter::setEnd` to the end.
    view_stretch_lines(event.line_holders, start, end);
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

bool World::move_player_to(const ogre::Vector3& to, bool ignore_units, bool ignore_obstructions) {
    // `CCharacter::moveToPosition`: unless obstructions
    // are ignored, the way (or 0.4 along it) must be passable; then `dropToGround(4.0)`.
    // INVENTED: occupancy (vtables +0x268/+0x270), `passableBetween`'s units, `+0x228`.
    (void)ignore_units;
    const tl::core::Vec3 from = player.position();
    if (!ignore_obstructions && passability.ready()) {
        if (!passability.passable_between(from, {to.x, to.y, to.z}, true)) {
            ogre::Vector3 step(to.x - from.x, 0.0f, to.z - from.z);
            const float length = step.length();
            if (1e-8f < length) step /= length;
            const tl::core::Vec3 nearer{from.x + step.x * 0.4f, from.y, from.z + step.z * 0.4f};
            if (!passability.passable_between(nearer, {to.x, to.y, to.z}, true)) return false;
        }
    }
    const ogre::Vector3 ground = dropped_to_ground(to);
    player.place({ground.x, ground.y, ground.z}, player.facing());
    if (subject_node != nullptr) view_set_position(subject_node, ground);
    return true;
}

bool World::move_unit_to(tl::core::LevelScript* machine, int64_t node, const ogre::Vector3& to,
                         bool ignore_units, bool ignore_obstructions) {
    // `CCharacter::moveToPosition` on a placed unit, from
    // its feet. INVENTED: as `move_player_to`.
    (void)ignore_units;
    if (machine == nullptr) return false;
    tl::core::Vec3 from;
    if (!machine->unit_position(node, from)) return false;
    if (!ignore_obstructions && passability.ready()) {
        if (!passability.passable_between(from, {to.x, to.y, to.z}, true)) {
            ogre::Vector3 step(to.x - from.x, 0.0f, to.z - from.z);
            const float length = step.length();
            if (1e-8f < length) step /= length;
            const tl::core::Vec3 nearer{from.x + step.x * 0.4f, from.y, from.z + step.z * 0.4f};
            if (!passability.passable_between(nearer, {to.x, to.y, to.z}, true)) return false;
        }
    }
    const ogre::Vector3 ground = dropped_to_ground(to);
    machine->move_unit(node, {ground.x, ground.y, ground.z});
    return true;
}

bool World::skill_events_alive(const std::string& skill, uint32_t run,
                               tl::core::LevelScript* caster_machine, int64_t caster_node) {
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

bool World::skill_events_can_stop(const std::string& skill, uint32_t run,
                                  tl::core::LevelScript* caster_machine, int64_t caster_node) {
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
bool World::skill_event_particles_alive(const LiveSkillEvent& event) const {
    if (view_particles_alive(event.holders)) return true;
    for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
        for (const tl::core::LevelScript::NodeRecord& n :
             nested_scripts[i].script.nodes_of_type(tl::formats::cmp_type::kUnitSpawner)) {
            const auto placed = spawner_particles.find(n.node);
            if (placed == spawner_particles.end()) continue;
            for (const auto& layout : placed->second) {
                if (view_particles_alive(layout.second)) return true;
            }
        }
    }
    return false;
}

void World::reset_skill_events(const std::string& skill, uint32_t run,
                               tl::core::LevelScript* caster_machine, int64_t caster_node) {
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
        stop_skill_event(event);
        ++k;
    }
}

void World::stop_skill_event(LiveSkillEvent& event) {
    // `CSkillEvent::stopEvent`: `CLayout::stop(false)`,
    // `stopAllParticles(loop)` and `disableAllDamageShapes`.
    // The layout is a timeline layout (`CSkillEvent::CSkillEvent`), so the stop
    // reaches its timelines and its `Sound` nodes.
    for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
        NestedScript& machine = nested_scripts[i];
        std::vector<tl::core::ScriptEvent> events;
        machine.script.stop_layout_timelines(quest_log.state(), events);
        machine.script.stop_layout_sounds(quest_log.state(), events);
        machine.script.disable_damage_shapes(quest_log.state(), events);
        if (!events.empty()) apply_script_events(events, machine.nodes);
    }
    // `CSkillEvent::stopAllParticles`: the layout's
    // own `Particle` is `CParticleWrapper::stop` at once; a `Layout Link
    // Particle`'s `CParticle::Stop(loop)`. Holders go
    // once nothing in them lives.
    view_stop_particles(event.holders, event.loop);
    // and `CUnitSpawner::stopAllParticles()` on each spawner
    // (`+0xc0`): its particle layouts `CLayout::stop(false)`.
    for (size_t i = event.first; i < event.end && i < nested_scripts.size(); ++i) {
        for (const tl::core::LevelScript::NodeRecord& n :
             nested_scripts[i].script.nodes_of_type(tl::formats::cmp_type::kUnitSpawner)) {
            const auto placed = spawner_particles.find(n.node);
            if (placed == spawner_particles.end()) continue;
            for (const auto& layout : placed->second) view_stop_emission(layout.second);
        }
    }
    // Its particles and its layout go with the ended ones, between walks of the level's
    // machines (release_ended_skill_events()).
    event.listed = false;
}

void World::retire_skill_layout(size_t first, size_t end) {
    for (size_t i = first; i < end && i < nested_scripts.size(); ++i) {
        NestedScript& machine = nested_scripts[i];
        if (machine.retired || machine.script.has_characters()) continue;
        machine.retired = true;
        machine.running = false;
        // The layout goes, and its `Sound` nodes' banks with it (`CSoundBank::~CSoundBank`).
        machine.script.release_sound_nodes();
        // Nothing addresses its nodes again; what they hold goes with its holder lists.
        machine.nodes.clear();
        level_machines.erase(
            std::remove(level_machines.begin(), level_machines.end(), &machine.script),
            level_machines.end());
    }
}

void World::release_ended_skill_events() {
    // `CSkill::updateSkill` updates only the events in its
    // list, and `CSkillEvent::updateEvent` is what updates
    // an event's layout (`CLayout::update`, vtable +0x208): one that left the list, or was
    // reset (`CSkillEvent::reset`), is not updated again.
    // INVENTED: the original keeps that one layout and restarts it at the event's next
    // `startEvent`; here each start places its own.
    for (size_t k = 0; k < live_skill_events.size();) {
        LiveSkillEvent& event = live_skill_events[k];
        if (event.listed) {
            ++k;
            continue;
        }
        retire_skill_layout(event.first, event.end);
        if (!event.holders.empty()) {
            transient_particles.push_back(TransientParticles{event.holders, 0.0f, true});
        }
        live_skill_events.erase(live_skill_events.begin() + static_cast<long>(k));
    }
}

}  // namespace tl::core
