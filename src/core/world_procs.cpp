// The gameplay half of render/renderer_procs.cpp (core/world.h).
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

namespace {

/// A placed unit's known list (`CSkillManager +0x60`) as the procs ask of it.
tl::core::SkillCopies known_list(const tl::core::LevelScript::UnitCasting& casting,
                                 bool one_per_name = false) {
    tl::core::SkillCopies known(one_per_name);
    for (const tl::core::LevelScript::UnitSkill& held : casting.skills) {
        tl::core::SkillCopy copy;
        copy.name = held.known.entry.name;
        copy.level = held.known.invested;
        copy.enabled = held.known.enabled;
        copy.hidden = held.known.def != nullptr && held.known.def->hidden;
        copy.charges = held.known.charges;
        known.push_back(copy);
    }
    return known;
}

}  // namespace

tl::core::SkillFileLookup World::skill_file_lookup() {
    ensure_player_skill_files();
    return [this](const std::string& name) -> std::optional<tl::core::SkillFileFacts> {
        const auto found = player_skill_files.find(tl::core::upper_ascii(name));
        if (found == player_skill_files.end()) return std::nullopt;
        // `CSkill::CSkill`: `NAME`, the
        // `[LEVELn]` count (`+0xa8`), `HIDDEN` and `CHARGES` (default -1).
        tl::core::SkillFileFacts facts;
        const tl::core::SkillDef& def = found->second->def;
        facts.name = def.name;
        facts.level_count = static_cast<uint32_t>(def.levels.size());
        facts.hidden = def.hidden;
        facts.charges = static_cast<int32_t>(found->second->file.integer_or("CHARGES", -1));
        return facts;
    };
}

// PORT: a level unit as a proc's target (its machine, node and centre).
World::ProcTarget World::proc_target_unit(int64_t node) {
    ProcTarget target;
    if (node == 0) return target;
    tl::core::LevelScript* machine = script_owning(node);
    tl::core::Vec3 feet;
    if (machine == nullptr || !machine->unit_position(node, feet)) return target;
    target.machine = machine;
    target.node = node;
    target.radius = machine->unit_radius(node);
    target.centre = tl::core::Vec3{feet.x, feet.y + target.radius, feet.z};
    target.placed = true;
    return target;
}

void World::update_worn_skill_managers(float delta) {
    // `CInventory::updateSkillManagers` (X360 inline):
    // locations 0..11 in order, the first item at each.
    const std::vector<tl::core::InventorySlot>& slots = player_inventory.slots();
    const auto lookup = [this](const std::string& n) -> const tl::core::KnownSkill* {
        return player_skill(n);
    };
    for (size_t location = 0; location < worn_skill_runners.size(); ++location) {
        const bool worn = location < slots.size() && slots[location].count > 0;
        const std::string item = worn ? slots[location].item.name : std::string();
        if (item != worn_skill_runner_items[location]) {
            worn_skill_runners[location].clear();
            worn_skill_runner_items[location] = item;
        }
        if (!worn || worn_skill_runners[location].entries().empty()) continue;
        worn_skill_runners[location].update(delta, player_cast_context(), lookup);
    }
}

// PORT: the runner that is running a skill: the player's, or one of the worn items'.
tl::core::SkillRunner& World::skill_runner_running(const std::string& name) {
    if (player_skill_runner.is_running(name)) return player_skill_runner;
    for (tl::core::SkillRunner& runner : worn_skill_runners) {
        if (runner.is_running(name)) return runner;
    }
    return player_skill_runner;
}

void World::player_execute_procs(int type, const ProcTarget& target) {
    // CCharacter::executeProcs, with
    // CInventory::executeProcs.
    load_stat_graphs();
    load_unit_types();
    const tl::core::SkillFileLookup files = skill_file_lookup();
    // The worn items' managers, rebuilt with what their `activateEffect`
    // taught them.
    std::array<tl::core::EffectManager, 12> worn_effects;
    std::array<tl::core::SkillCopies, 12> worn_skills;
    std::array<tl::core::ProcHolder, 12> worn{};
    const std::vector<tl::core::InventorySlot>& slots = player_inventory.slots();
    for (size_t location = 0; location < worn.size() && location < slots.size(); ++location) {
        if (slots[location].count <= 0) continue;
        worn_effects[location] =
            tl::core::weapon_proc_effects(slots[location].item, item_effect_context(), stat_graphs,
                                          files, &worn_skills[location]);
        worn[location] = tl::core::ProcHolder{&worn_effects[location], &worn_skills[location]};
    }
    // `getPosition(true)` of the character and of the target (the
    // character's own with none).
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    const tl::core::Vec3 feet = player.position();
    const tl::core::Vec3 me{feet.x, feet.y + body, feet.z};
    const tl::core::Vec3 there = target.any() ? target.centre : me;
    tl::core::execute_procs(type, tl::core::ProcHolder{&player_effects, &player_proc_skills}, worn,
                            stat_graphs, tl::core::volatile_random(),
                            [&](const tl::core::ProcCast& cast) {
                                // Held by value: what the skill puts on the player can teach a
                                // skill, and the list it came from grows.
                                const tl::core::SkillCopy skill = *cast.skill;
                                if (cast.lockout) player_proc_lockout = 1.0f;
                                const tl::core::Vec3 start = cast.from_target ? there : me;
                                char why[48];
                                std::snprintf(why, sizeof why, "effect 0x%x%s", type,
                                              cast.location >= 0 ? " on a worn item" : "");
                                run_proc_skill(skill, start, there, target, why, cast.location);
                            });
}

void World::player_weapon_procs(const ProcTarget& struck) {
    // CCharacter::doWeaponProcs
    if (!has_weapon || !struck.any()) return;
    load_stat_graphs();
    const tl::core::SkillCopies known = tl::core::weapon_known_skills(
        weapon, item_effect_context(), stat_graphs, skill_file_lookup());
    if (known.empty()) return;
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    const tl::core::Vec3 feet = player.position();
    const tl::core::Vec3 point = tl::core::weapon_proc_point(
        tl::core::Vec3{feet.x, feet.y + body, feet.z}, struck.centre, struck.radius);
    for (const tl::core::SkillCopy& copy : known.copies()) {
        run_proc_skill(copy, point, point, struck, "the weapon's proc", attack_location);
    }
}

// PORT: an attacker as a proc's target.
World::ProcTarget World::proc_target_attacker(int64_t attacker) {
    if (attacker == tl::core::LevelScript::kNoAttacker || attacker == 0) return ProcTarget{};
    return proc_target_unit(attacker);
}

void World::unit_activate_effect(tl::core::LevelScript* machine, int64_t node,
                                 tl::core::Effect& effect) {
    // CBaseUnit::activateEffect: the machine's.
    if (machine != nullptr) machine->activate_unit_effect(node, effect);
}

World::ProcTarget World::proc_target_breakable(int index) {
    ProcTarget target;
    if (index < 0 || static_cast<size_t>(index) >= breakables.size()) return target;
    const BreakableWorld& b = breakables[static_cast<size_t>(index)];
    if (b.node == nullptr) return target;
    const ogre::Vector3 p = view_position(b.node);
    target.breakable_id = b.id;
    // `CBaseUnit::CBaseUnit`: a breakable's
    // `+0x194` is 0.3.
    target.radius = 0.3f;
    target.centre = tl::core::Vec3{p.x, p.y + target.radius, p.z};
    target.placed = true;
    return target;
}

bool World::run_proc_skill(const tl::core::SkillCopy& copy, const tl::core::Vec3& start,
                           const tl::core::Vec3& aim, const ProcTarget& target, const char* why,
                           int location) {
    // CSkillManager::executeSkill, on the
    // manager that holds the copy.
    tl::core::SkillRunner& runner =
        0 <= location && location < static_cast<int>(worn_skill_runners.size())
            ? worn_skill_runners[static_cast<size_t>(location)]
            : player_skill_runner;
    ensure_player_skill_files();
    const auto file = player_skill_files.find(tl::core::upper_ascii(copy.name));
    if (file == player_skill_files.end() || subject_node == nullptr) return false;
    // The copy at the level it holds, where the event code finds the skills
    // the player runs by name (`player_skill`).
    tl::core::KnownSkill& skill = executed_skills[tl::core::upper_ascii(copy.name)];
    skill.entry.name = file->second->def.name;
    skill.def = &file->second->def;
    skill.invested = copy.level;
    skill.effective = copy.level;
    skill.enabled = copy.enabled;

    // `executeSkill(skill, owner, type, start, orientation, aim, target)`: the
    // run keeps `aim` (`+0x50`) and the target; `EVENT_START` is handed both.
    tl::core::SkillCaster caster = player_caster();
    caster.position = start;
    caster.target_position = aim;
    tl::core::SkillCastContext context = player_cast_context();
    context.event_target = tl::core::SkillEventTarget{};
    context.event_target.origin = start;
    context.event_target.yaw = view_orientation(subject_node).getYaw().valueRadians();
    if (target.machine != nullptr) {
        tl::core::SkillTargetUnit aimed;
        aimed.position = target.centre;
        caster.target_unit = aimed;
        context.event_target.has_unit = true;
        context.event_target.unit = target.node;
        context.event_target.machine = target.machine;
    } else if (target.breakable_id != 0) {
        context.event_target.item = target.breakable_id;
    }
    const bool started = runner.execute(skill, copy.level, caster, context);
    TL_LOG_INFO("procs: %s casts '%s' at level %u%s%s", why, skill.entry.name.c_str(), copy.level,
                started ? "" : " (not started)",
                &runner != &player_skill_runner ? " on the worn item's manager" : "");
    return started;
}

void World::unit_execute_procs(tl::core::LevelScript& machine, int64_t node, int type,
                               tl::core::LevelScript* target_machine, int64_t target_node,
                               bool has_target) {
    // CCharacter::executeProcs
    tl::core::LevelScript::UnitCasting* k = machine.unit_casting(node);
    const tl::core::EffectManager* effects = machine.unit_effects(node);
    if (k == nullptr || effects == nullptr || !effects->has_effect(type)) return;
    load_stat_graphs();
    const tl::core::SkillCopies known = known_list(*k);
    // `getPosition(true)` of the unit, and of the target (the unit's own with
    // none).
    tl::core::Vec3 feet;
    if (!machine.unit_position(node, feet)) return;
    const tl::core::Vec3 me{feet.x, feet.y + machine.unit_radius(node), feet.z};
    tl::core::Vec3 there = me;
    if (has_target) {
        if (target_machine == nullptr) {
            const float body = player_stats.radius > 0.0f ? player_stats.radius
                                                          : tl::core::kDefaultCollisionRadius;
            there = {player.position().x, player.position().y + body, player.position().z};
        } else if (target_machine->unit_position(target_node, there)) {
            there.y += target_machine->unit_radius(target_node);
        } else {
            has_target = false;
        }
    }
    float yaw = 0.0f;
    machine.unit_facing(node, yaw);
    const std::string unit = machine.unit_of(node);
    // INVENTED: CInventory::executeProcs not walked for a
    // placed unit; its weapons are kept by name only.
    const std::array<tl::core::ProcHolder, 12> worn{};
    tl::core::execute_procs(
        type, tl::core::ProcHolder{effects, &known}, worn, stat_graphs, tl::core::volatile_random(),
        [&](const tl::core::ProcCast& cast) {
            if (cast.lockout) machine.lock_unit_procs(node);
            // The copy `getSkill` found, held by value: the cast can teach the
            // unit more skills, and its list grows.
            tl::core::LevelScript::UnitCasting* casting = machine.unit_casting(node);
            if (casting == nullptr) return;
            const std::string key = tl::core::upper_ascii(cast.skill->name);
            const uint32_t level = cast.skill->level;
            tl::core::KnownSkill skill;
            for (const tl::core::LevelScript::UnitSkill& held : casting->skills) {
                if (tl::core::upper_ascii(held.known.entry.name) == key &&
                    held.known.invested == level) {
                    skill = held.known;
                    break;
                }
            }
            if (skill.def == nullptr) return;
            const tl::core::Vec3 start = cast.from_target ? there : me;
            // executeSkill(skill, this, 1, ...). A proc can
            // land mid-way through another unit's cast: unit_cast_caster is restored after.
            const tl::core::SkillCaster outer_caster = unit_cast_caster;
            tl::core::SkillCastContext context =
                unit_cast_context(machine, node, target_machine, target_node, has_target);
            unit_cast_caster.position = start;
            unit_cast_caster.target_position = there;
            context.event_target.origin = start;
            context.event_target.yaw = yaw;
            const UnitEventCaster saved = unit_event_caster;
            unit_event_caster = unit_event_caster_for(&machine, node);
            const bool started = casting->runner.execute(skill, level, unit_cast_caster, context);
            unit_event_caster = saved;
            // The count `decrementSkillCharges` took is the held skill's.
            if (tl::core::LevelScript::UnitCasting* after = machine.unit_casting(node)) {
                for (tl::core::LevelScript::UnitSkill& held : after->skills) {
                    if (tl::core::upper_ascii(held.known.entry.name) == key &&
                        held.known.invested == level) {
                        held.known.charges = skill.charges;
                        break;
                    }
                }
            }
            unit_cast_caster = outer_caster;
            TL_LOG_INFO("procs: %s's effect 0x%x casts '%s' at level %u%s", unit.c_str(), type,
                        skill.entry.name.c_str(), level, started ? "" : " (not started)");
        });
}

}  // namespace tl::core
