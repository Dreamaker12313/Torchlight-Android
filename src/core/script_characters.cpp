// The level's characters each frame: `CLevel::updateCharacters` over them, and what each
// update hands the scene (its place, its facing, its gait). Split from script_units.cpp.
#include "core/script.h"

#include "core/section_profile.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace tl::core {
namespace {

namespace type = tl::formats::cmp_type;
using namespace tl::core::logic;

std::string upper(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return text;
}

}  // namespace

void LevelScript::emit_character(const Object& o, Character& c, std::vector<ScriptEvent>& out) {
    const Vec3 at = feet_of(c);
    const bool moved = std::fabs(at.x - c.sent_at.x) > 1e-5f ||
                       std::fabs(at.y - c.sent_at.y) > 1e-5f ||
                       std::fabs(at.z - c.sent_at.z) > 1e-5f;
    const bool turned = std::fabs(c.facing - c.sent_facing) > 1e-5f;
    if (moved || turned) {
        c.sent_at = at;
        if (turned) c.sent_facing = c.facing;
        ScriptEvent e;
        e.action = ScriptAction::MoveUnit;
        e.node_id = o.id;
        const Object* path = path_of(o);
        e.name = path != nullptr ? path->name : std::string();
        e.unit = o.unit;
        e.position = at;
        e.facing = c.facing;
        e.turned = turned;
        e.running = c.run;
        out.push_back(e);
    }
    // The gait: `updatePathing`'s choice of speed while a route runs, idle when
    // it does not; a swing, a cast or a scripted animation owns the model, and a
    // body keeps its death.
    if (0.0f < c.swing_left || o.ai == ai_state::kPlayAnimation) return;
    if (dead_.count(o.id) != 0 || o.ai == ai_state::kDying || o.ai == ai_state::kDead) return;
    // A jump owns the model until its `IDLE` plays (`jumpDownAI`).
    if (o.ai == ai_state::kJumpDown) return;
    if (c.casting.cast.casting || 0.0f < c.casting.cast.animation_left) return;
    const UnitStats& stats = stats_for(o.unit);
    // `updateAnimation`: a mimic holding
    // `MIMICIDLE` whose model has an `ASLEEP` blends that where another unit
    // would stand `IDLE`.
    // INVENTED: `animationExists` is not asked; both mimics ship the file.
    std::string gait = c.mimic_idle ? "Asleep" : "Idle";
    // ... and a stunned unit (0x28) its `STUNNED`, where the model has one.
    //
    if (o.ai == 0x28 && animation_catalog_) {
        for (const std::pair<std::string, float>& entry : animation_catalog_(o.unit)) {
            if (upper(entry.first) == "STUNNED") {
                gait = "Stunned";
                break;
            }
        }
    }
    // The gait's speed: `+0x25c` times `WALK_ANIM_MULT` or `RUN_ANIM_MULT`, at least
    // 0.1. INVENTED: the further `1 / scale` is not applied.
    float rate = 1.0f;
    if (c.pathing) {
        const bool running =
            c.run && !(stats.movement.running_speed <= stats.movement.walking_speed);
        gait = running ? "Run" : "Walk";
        rate = speed_multiplier(c, stats) * (running ? stats.run_anim_mult : stats.walk_anim_mult);
        if (rate < 0.1f) rate = 0.1f;
    }
    if (gait == c.gait && std::fabs(rate - c.gait_rate) <= 1e-3f) return;
    c.gait = gait;
    c.gait_rate = rate;
    if (trace_on_) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%.3f A %s blend %s loop=1",
                      static_cast<double>(clock_), o.unit.c_str(), gait.c_str());
        trace_.push_back(buffer);
    }
    ScriptEvent e;
    e.action = ScriptAction::Animation;
    e.node_id = o.id;
    e.name = o.name;
    e.unit = o.unit;
    e.on = true;
    e.loops = true;
    e.text = gait;
    e.rate = rate;
    out.push_back(e);
}

// `CLevel::updateCharacters`: each character, followed
// by the listeners (Damage Shapes) when in active range (a corpse too). The player
// updates in the renderer.
void LevelScript::update_characters(float delta, QuestState& quests,
                                    std::vector<ScriptEvent>& out) {
    const SectionScope timed(Section::Characters);
    flush_character_update_listeners();
    if (level_machines().front() == this) {
        character_updated_in_level(updated_player(), delta, quests, out);
    }
    // NOLINTNEXTLINE(modernize-loop-convert): an update can add or delete characters.
    for (size_t i = 0; i < objects_.size(); ++i) {
        Object& o = objects_[i];
        if (o.type != type::kMonster) continue;
        Character* c = character(o.id);
        // `CLevel::updateCharacters` walks
        // the near list (`+0x88`): one marked for deletion (`+0x190`) goes
        // first; the others update, and only an active character (`+0x199`)
        // goes on to the listeners.
        if (c == nullptr || !c->listed) continue;
        const int64_t id = o.id;
        if (c->pending_delete) {
            delete_character(id, out);
            continue;
        }
        if (dead_.count(id) == 0) {
            character_update(o, *c, delta, quests, out);
        } else {
            body_update(o, *c, delta, out);
        }
        c = character(id);
        if (c == nullptr || !c->active) continue;
        character_updated_in_level(updated_character(id), delta, quests, out);
    }
    // Every character's position as the original's tracer samples it: every 0.1 s of level
    // time, when it moved, the sphere's centre.
    if (trace_on_ && trace_next_sample_ <= clock_) {
        trace_next_sample_ = clock_ + 0.1f;
        for (const Object& o : objects_) {
            Character* c = character(o.id);
            if (o.type != type::kMonster || c == nullptr) continue;
            const Vec3& at = c->motion.position;
            const Vec3 rounded{std::round(at.x * 100.0f), std::round(at.y * 100.0f),
                               std::round(at.z * 100.0f)};
            if (c->traced && rounded.x == c->traced_at.x && rounded.y == c->traced_at.y &&
                rounded.z == c->traced_at.z) {
                continue;
            }
            c->traced = true;
            c->traced_at = rounded;
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%.3f P %s %.2f %.2f %.2f",
                          static_cast<double>(clock_), o.unit.c_str(), static_cast<double>(at.x),
                          static_cast<double>(at.y), static_cast<double>(at.z));
            trace_.push_back(buffer);
        }
    }
}

}  // namespace tl::core
