// The gameplay half of render/renderer_player.cpp (core/world.h).
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

/// One step of the player controller against a given stick state; the scripted walk
/// drives the same code.
bool World::player_motion_held() {
    // A swing with more than 0.2 s to run: setDestination
    // takes no new destination. DIVERGENCE: the port steers straight, so the
    // character stops at once rather than walking out its old point.
    if (swing.active && (swing.length - swing.elapsed) / swing_rate() > 0.2f) return true;
    if (player_active_skill.empty()) return false;
    const std::string& name = player_active_skill.skill;
    const tl::core::SkillRunState* state = player_skill_runner.state(name);
    const tl::core::KnownSkill* known = player_skills.find_by_name(name);
    const bool moves = known != nullptr && known->def != nullptr && known->def->moves_to_target;
    return tl::core::skill_halts_motion(
        player_active_skill, state != nullptr ? state->animations : tl::core::SkillAnimations{},
        player_skill_runner.is_running(name), moves);
}

void World::set_player_destination_held(float x, float z) {
    // Its two early exits do not arise here.
    //
    // (0, 0) is the point aimed at (`+0x1c0`).
    if (x == 0.0f && z == 0.0f) {
        x = player_turn_point.x;
        z = player_turn_point.z;
    }
    // the hold does not count while a skill that
    // `MOVES_TO_TARGET` is being performed. The route `setDestination` then
    // asks for is not carried: the port's player routes no stick point.
    const tl::core::KnownSkill* known = player_skills.find_by_name(player_active_skill.skill);
    const bool moves = known != nullptr && known->def != nullptr && known->def->moves_to_target;
    const bool hold = attack_button.hold && !(moves && player_performing_skill());
    if (hold) {
        // stopped, and `CCharacter::attack()`,
        // which under the hold swings
        // with no target.
        player.stop_following();
        begin_swing(player_target_node);
    }
    // (and on the walking path): the point kept.
    player_turn_point = {x, 0.0f, z};
}

// PORT: an `INTERACTIVE CHARACTER` the player talks to made its machine's player
// target (`LevelScript::set_player_target`, which holds the rule).
void World::set_talk_target(const std::string& unit) {
    clear_talk_target();
    load_unit_types();
    const tl::core::UnitStats stats = stats_for_unit(unit);
    if (!unit_types.isa(unit_types.id_of(stats.unittype),
                        tl::core::unit_type::kInteractiveCharacter)) {
        return;
    }
    int64_t node = 0;
    const int machine = unit_machine(unit, node);
    tl::core::LevelScript* owner = machine_script(machine);
    if (owner == nullptr) return;
    talk_target_node = node;
    talk_target_machine = machine;
    owner->set_player_target(node);
}

// PORT: the talk target let go (`LevelScript::release_player_target`).
void World::clear_talk_target() {
    if (tl::core::LevelScript* owner = machine_script(talk_target_machine)) {
        std::vector<tl::core::ScriptEvent> events;
        owner->release_player_target(events);
        apply_machine_events(talk_target_machine, events);
    }
    talk_target_node = 0;
    talk_target_machine = kNoMachine;
}

// PORT: the speaker turned to the player (`LevelScript::target_player`).
void World::speaker_looks_at_player(const std::string& unit) {
    int64_t node = 0;
    const int machine = unit_machine(unit, node);
    tl::core::LevelScript* owner = machine_script(machine);
    if (owner == nullptr) return;
    std::vector<tl::core::ScriptEvent> events;
    owner->target_player(node, events);
    apply_machine_events(machine, events);
}

// The unit's collidable flag (`+0x18d`) and `CBaseUnit::addToAvoidanceMap`
// or `removeFromAvoidanceMap`
// on the collision group of a placed item.
void World::set_trigger_collidable(int64_t key, bool on) {
    if (!collision.has_group(key)) return;
    if (on) {
        trigger_not_collidable.erase(key);
        collision.set_group_enabled(key, true);
        tl::core::Vec3 low, high;
        if (passability.ready() && door_avoidance_added.insert(key).second &&
            collision.group_bounds(key, low, high)) {
            passability.increment_map_passability_collision(low, high, collision, key);
            nav.refresh(passability, low, high);
        }
        return;
    }
    // The map grid while the faces can be probed: decrement_map_passability_collision's
    // probe ignores whether collision is on.
    if (trigger_range_off.erase(key) > 0) collision.set_group_enabled(key, true);
    tl::core::Vec3 low, high;
    if (door_avoidance_added.erase(key) > 0 && collision.group_bounds(key, low, high)) {
        passability.decrement_map_passability_collision(low, high, collision, key);
        nav.refresh(passability, low, high);
    }
    collision.set_group_enabled(key, false);
    trigger_not_collidable.insert(key);
}

// `CQuestDialogMenu::setOpen` ->
// `setInteractiveMenuVisible`.
void World::set_quest_dialog_interactive(bool on) {
    if (quest_dialog_interactive == on) return;
    quest_dialog_interactive = on;
    set_interactive_menu_visible(on);
}

bool World::node_targetable(int64_t node, const Ogre::SceneNode* where) const {
    if (node == 0) return true;
    for (const NestedScript& machine : nested_scripts) {
        if (!machine.script.has_character(node)) continue;
        const auto range = machine.nodes.equal_range(node);
        for (auto n = range.first; n != range.second; ++n) {
            // A mimic lying in wait is nobody's target
            // (`CLevel::findCharacterWithinView`).
            if (n->second == where) {
                return machine.script.node_targetable(node) &&
                       !machine.script.unit_mimic_idle(node);
            }
        }
    }
    return script.node_targetable(node) && !script.unit_mimic_idle(node);
}

// PORT: one of the port's machines by index.
tl::core::LevelScript* World::machine_script(int machine) {
    if (machine == kLevelMachine) return &script;
    if (machine >= 0 && static_cast<size_t>(machine) < nested_scripts.size()) {
        return &nested_scripts[static_cast<size_t>(machine)].script;
    }
    return nullptr;
}

// PORT: the machine whose dialog is open.
int World::dialog_machine() const {
    if (script.dialog_open()) return kLevelMachine;
    for (size_t i = 0; i < nested_scripts.size(); ++i) {
        if (nested_scripts[i].script.dialog_open()) return static_cast<int>(i);
    }
    return kNoMachine;
}

void World::apply_machine_events(int machine, const std::vector<tl::core::ScriptEvent>& events) {
    if (events.empty()) return;
    if (machine == kLevelMachine) {
        apply_script_events(events, scripted_nodes);
    } else if (machine >= 0 && static_cast<size_t>(machine) < nested_scripts.size()) {
        NestedScript& owner = nested_scripts[static_cast<size_t>(machine)];
        apply_script_events(events, owner.nodes);
    }
}

// `CGameStateController::doActualGameState`,
// the interface's half: nothing without the player's character (the client's
// `+0x58`).
void World::game_state_interface(uint32_t type, bool on) {
    if (subject_node == nullptr) return;
    switch (type) {
        case 1:
            // `stopPathing`, after the level's `setEnabled(!on)`.
            player.stop_following();
            break;
        case 3:
            set_interactive_menu_visible(on);
            // Coming out, after the level's `setEnabled(true)`.
            if (!on) attempt_to_stop_player_skill(true);
            // `+0x38` is set before the first frame in the original; here the
            // first frame sets it, so a scene that starts with the level
            // starts from the same far end.
            if (!(camera_dolly > 0.0f)) camera_dolly = kDefaultCameraDistance * camera_mult;
            tl::core::set_cinematic_mode(camera_cinematic, camera_dolly, on, camera_mult);
            update_npc_icons();
            break;
        case 6: close_ingame_ui(); break;
        default: break;
    }
}

// `CGameStateController::healPlayer`:
// `modifyHP(maxHP())` on the player's
// character, which adds it and holds the health to `maxHP()`, an integer. At 0
// health a heal smaller than `maxHP()` does nothing; this one never is.
void World::heal_player() {
    if (subject_node == nullptr || !(player_health.max > 0.0f)) return;
    const float ceiling = static_cast<float>(static_cast<int>(player_health.max));
    player_health.current = std::min(ceiling, player_health.current + ceiling);
}

}  // namespace tl::core
