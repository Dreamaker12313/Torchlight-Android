// The level machine's characters walking: their movement through the level; split from
// script_units.cpp.
#include "core/script.h"

#include "core/ogre_math.h"
#include "core/section_profile.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tl::core {

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

// `CCharacter::stopPathing`.
void LevelScript::stop_pathing(Character& c) {
    if (c.pathing) c.since_route = 10.0f;
    c.pathing = false;
    c.next = c.motion.position;
}

// `CCharacter::setDestination(CLevel&, float, float)`.
//
bool LevelScript::set_destination(Character& c, const UnitStats& stats, float x, float z) {
    // No pathfinder, nothing moves (`+0x268 == 0`).
    if (nav_ == nullptr || passability_ == nullptr || !nav_->ready() || !passability_->ready()) {
        return false;
    }
    Object* o = find(c.node);
    const int32_t state = o != nullptr ? o->ai : ai_state::kIdle;
    if (state == 6 || state == 5 || state == 0xc ||
        (stats.movement.walking_speed == 0.0f && stats.movement.running_speed == 0.0f)) {
        if (c.pathing) c.since_route = 10.0f;
        c.pathing = false;
        c.next = c.motion.position;
        c.destination = {x, 0.0f, z};
        return false;
    }
    c.since_route = 0.0f;
    c.destination = {x, c.motion.position.y, z};

    const bool was_on_grid = c.on_grid;
    avoidance_remove(c);
    // `+0x69`: the object grid is ignored while the interface is in a cinematic.
    const bool cinematic = ui_in_cinematic();
    std::vector<Vec3> nodes;
    const Vec3 from = c.motion.position;
    bool found = false;
    timed(Section::Route, [&] {
        found = nav_->find_nodes(from, {x, 0.0f, z}, *passability_, cinematic, nodes, c.search_cap);
    });
    auto restore = [&]() {
        if (was_on_grid && state != 5 && state != 6) avoidance_add(c);
    };
    if (!found) {
        restore();
        return false;
    }

    // `reachedGoal` is "the current node has no next"; `advanceNextNode` steps.
    size_t cursor = 1;
    auto reached = [&]() { return nodes.size() < 2 || cursor + 1 >= nodes.size(); };
    if (!reached()) {
        c.route.clear();
        Vec3 last{};
        Vec3 node{};
        while (!reached()) {
            node = {nodes[cursor].x, 0.0f, nodes[cursor].z};
            bool added = false;
            if (c.route.empty()) {
                last = {from.x, 0.0f, from.z};
                c.route.add_point(last);
                added = true;
            } else if (!nav_->passable_between(*passability_, last, node, false)) {
                last = node;
                c.route.add_point(last);
                added = true;
            }
            ++cursor;
            if (!reached() || added) continue;
            if (nav_->passable_between(*passability_, last, c.destination, false) &&
                nav_->position_passable(*passability_, c.destination)) {
                continue;
            }
            c.route.add_point(node);
        }
        if (!c.route.empty()) {
            if (!nav_->position_passable(*passability_, c.destination)) {
                c.destination = c.route.back();
            } else if (nav_->passable_between(*passability_, c.route.back(), c.destination,
                                              false)) {
                c.route.add_point({x, 0.0f, z});
            } else {
                c.destination = c.route.back();
            }
            c.pathing = true;
            const Vec3 first = c.route.point(0);
            c.next = {first.x, 0.0f, first.z};
        }
    }
    if (c.route.length() <= 0.0f) c.pathing = false;
    restore();
    return c.pathing;
}

// `CCharacter::updatePathing(float)`.
void LevelScript::update_pathing(Character& c, const UnitStats& stats, float delta) {
    if (!c.pathing) return;
    const SectionScope timed(Section::Pathing);
    Vec3 nearest;
    float along = 0.0f;
    c.route.find_nearest_point(c.motion.position, nearest, along);
    if (c.route.length() <= along) {
        stop_pathing(c);
        return;
    }
    const float ahead = std::min(along + 0.1f, c.route.length());
    c.next = c.route.position_at_distance(ahead);
    const float far_along = std::min(along + 0.3f, c.route.length());
    const Vec3 far_point = c.route.spline_position_at_distance(far_along);
    const float near_along = std::max(far_along - 0.3f, 0.0f);
    const Vec3 near_point = c.route.spline_position_at_distance(near_along);
    float dx = far_point.x - near_point.x;
    float dz = far_point.z - near_point.z;
    const float dy = far_point.y - near_point.y;
    const float span = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (1e-08f < span) {
        dx = dx / span;
        dz = dz / span;
    }
    c.facing = turn_toward(c.facing, dx, dz, stats.turn_rate, delta, nullptr, turn_factor(c));

    Vec3 check;
    float check_along = 0.0f;
    c.route.find_nearest_point(far_point, check, check_along);
    if (check_along <= along && c.route.length() < 1.0f) {
        stop_pathing(c);
    }
    float speed = character_speed(c, stats.movement.walking_speed);
    if (c.run && !(stats.movement.running_speed <= stats.movement.walking_speed)) {
        speed = character_speed(c, stats.movement.running_speed);
    }
    // Times `+0x25c`, and under AI flag 1 times it again and half
    // as much once more.
    const float factor = speed_multiplier(c, stats);
    speed = speed * factor;
    if (c.ai_flags.has(AIFlag::Berserk)) speed = speed * (factor * 1.5f);
    steer(c.motion, c.next.x - c.motion.position.x, c.next.z - c.motion.position.z,
          speed * c.motion.velocity_scale);
}

// `CCharacter::followCharacter`: the quarry's
// own circle comes off the grid while the route to where it stands is found.
void LevelScript::follow_character(Object& o, Character& c, const UnitRef& target,
                                   std::vector<ScriptEvent>& out) {
    if (!target.player()) {
        const Object* them = target_object(target);
        if (them == nullptr || them->ai == ai_state::kDying || them->ai == ai_state::kDead) return;
    }
    set_target(o, c, true, target, out);
    Vec3 at;
    if (!target_at(target, at)) return;
    LevelScript* owner = target.machine;
    Character* them = owner != nullptr ? owner->character(target.node) : nullptr;
    const bool was = them != nullptr && them->on_grid;
    if (them != nullptr) owner->avoidance_remove(*them);
    set_destination(c, stats_for(o.unit), at.x, at.z);
    if (them != nullptr && was) owner->avoidance_add(*them);
}

}  // namespace tl::core
