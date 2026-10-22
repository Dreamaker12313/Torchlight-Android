// The renderer: procs and weapon skills for the player (CCharacter::executeProcs,
// doWeaponSkill, doWeaponProcs around step_swing) and a placed unit's own effects,
// the skills they teach it and its procs on being struck and on death.
#include "render/renderer_impl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace tl::render {

bool Renderer::Impl::player_weapon_skill(const ProcTarget& target) {
    // CCharacter::doWeaponSkill
    if (!has_weapon || subject_node == nullptr) return false;
    load_stat_graphs();
    const tl::core::SkillCopies known = tl::core::weapon_known_skills(
        weapon, item_effect_context(), stat_graphs, skill_file_lookup());
    if (known.empty()) return false;
    const float body =
        player_stats.radius > 0.0f ? player_stats.radius : tl::core::kDefaultCollisionRadius;
    const tl::core::Vec3 feet = player.position();
    const tl::core::Vec3 me{feet.x, feet.y + body, feet.z};
    // The hand's tag point: the one the weapon's model hangs from. INVENTED:
    // with no model drawn, the character's centre.
    tl::core::Vec3 hand = me;
    if (weapon_entity != nullptr && weapon_entity->getParentNode() != nullptr) {
        const Ogre::Vector3 p = weapon_entity->getParentNode()->_getDerivedPosition();
        hand = tl::core::Vec3{p.x, p.y, p.z};
    }
    const float facing = player.facing();
    const tl::core::WeaponSkillPoints points = tl::core::weapon_skill_points(
        hand, me, body, tl::core::Vec3{std::sin(facing), 0.0f, std::cos(facing)});
    bool started = false;
    for (const tl::core::SkillCopy& copy : known.copies()) {
        if (!tl::core::weapon_skill_runs(copy)) continue;
        started =
            run_proc_skill(copy, points.start, points.aim, target, "the weapon", attack_location) ||
            started;
    }
    return started;
}

// ---- a placed unit's own effects and procs ---------------------------------

}  // namespace tl::render
