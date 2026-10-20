// A skill's animation on its caster.
#include "core/skill_animation.h"

#include "core/effect_manager.h"
#include "core/random.h"
#include "core/unit_types.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string_view>

namespace tl::core {

namespace {

/// `gKEYFRAME_TYPES` (`_GLOBAL__I_CKeyframe`), 0x1d entries.
constexpr std::array<std::string_view, 29> kKeyframeTypes = {
    "HIT",
    "BLENDIN",
    "BLENDOUT",
    "PLAYSOUND",
    "SPAWNPARTICLE",
    "SPAWNPARTICLE_STOP_ON_DEATH",
    "FOOTSTEP",
    "SHOWWEAPONTRAIL",
    "HIDEWEAPONTRAIL",
    "ATTACKSOUND",
    "ENABLECOLLISION",
    "DISABLECOLLISION",
    "REMOVEPARTICLES",
    "REMOVEANIMATIONPARTICLES",
    "CAMERASHAKE",
    "ATTACKEND",
    "UNTARGETABLE",
    "TARGETABLE",
    "DAMPVELOCITY",
    "UNDAMPVELOCITY",
    "SHOWWEAPONS",
    "HIDEWEAPONS",
    "HIDEMESH",
    "SHOWMESH",
    "FADEOUTMESH",
    "FADEINMESH",
    "CAMERASHAKE_NO_FALLOFF",
    "PLAYSOUND_NO_FALLOFF",
    "HITTWO",
};

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

// `getAnimationIndex`, then `findRandomAnimation` on the same name: the shape every
// lookup in `CSkill::assignSkillAnimations` takes. PORT: a helper of it.
int index_or_random(const AnimationCatalog& model, const std::string& name) {
    const int found = model_animation_index(model, name);
    return found != -1 ? found : model_find_random_animation(model, name);
}

/// The two fallbacks the main and dual-wield lookups share: any `ATTACK*`, then
/// any `RSLASH*`.
int attack_fallback(const AnimationCatalog& model) {
    const int attack = model_find_random_animation(model, "ATTACK");
    return attack != -1 ? attack : model_find_random_animation(model, "RSLASH");
}

/// `CBaseUnit::ISA` on a hand's item against a property's resolved requirement.
/// An unauthored requirement resolves to 0, `[Any]`, which every type is.
bool hand_isa(const SkillCaster& caster, const std::string& hand_type,
              const std::string& requirement) {
    if (hand_type.empty() || caster.unit_types == nullptr) return false;
    const uint32_t want =
        requirement.empty() ? unit_type::kAny : caster.unit_types->id_of(requirement);
    return caster.unit_types->isa(hand_type, want);
}

/// `CCharacter::getWeaponInLeftHand`: the left slot's item when it
/// is a `[weapon]`.
bool left_hand_weapon(const SkillCaster& caster) {
    return !caster.left_hand_type.empty() && caster.unit_types != nullptr &&
           caster.unit_types->isa(caster.left_hand_type, unit_type::kWeapon);
}

// Whether a key on an animation belongs to the active skill, as
// `CCharacter::updateSkillKeys` tests it: loop end,
// main, dual-wield, loop, dual-wield loop.
bool is_skills_own(const SkillAnimations& a, int animation) {
    return animation == a.loop_end || animation == a.animation || animation == a.animation_dw ||
           animation == a.loop_into || animation == a.dw_loop_into;
}

}  // namespace

bool manifest_adds_sound_sample(const AnimationManifest& manifest,
                                const SoundDataInformation& sounds) {
    for (const std::string& stem : manifest.order()) {
        const std::vector<AnimationTag>* tags = manifest.tags_of_file(stem);
        if (tags == nullptr) continue;
        for (const AnimationTag& tag : *tags) {
            const int type = keyframe_type_of(tag.name);
            if (type != kKeyframePlaySound && type != kKeyframePlaySoundNoFalloff) continue;
            if (sounds.find(tag.sound) != nullptr) return true;
        }
    }
    return false;
}

int keyframe_type_of(const std::string& name) {
    // `CKeyframe::CKeyframe`: `STRINGS::getStringIndex`
    // answers a miss with 0, so an unknown or missing
    // name is a `HIT`.
    const std::string key = upper(name);
    for (size_t i = 0; i < kKeyframeTypes.size(); ++i) {
        if (kKeyframeTypes[i] == key) return static_cast<int>(i);
    }
    return kKeyframeHit;
}

int model_animation_index(const AnimationCatalog& model, const std::string& name) {
    // `CGenericModel::getAnimationIndex(const std::string&)`: both
    // sides upper-cased, whole-string equality, first match.
    const std::string want = upper(name);
    for (size_t i = 0; i < model.size(); ++i) {
        if (upper(model[i].first) == want) return static_cast<int>(i);
    }
    return -1;
}

int model_find_random_animation(const AnimationCatalog& model, const std::string& prefix) {
    // `CGenericModel::findRandomAnimation(const std::string&)`.
    // Only the model's name is upper-cased;
    // the prefix is compared as passed.
    int chosen = -1;
    for (size_t i = 0; i < model.size(); ++i) {
        const std::string name = upper(model[i].first);
        if (prefix.size() > name.size()) continue;
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        if (chosen == -1) {
            chosen = static_cast<int>(i);
            continue;
        }
        // A later match replaces the one held on a coin flip, drawn only once
        // there is something to replace.
        if (volatile_random().between(0.0f, 1000.0f) < 500.0f) chosen = static_cast<int>(i);
    }
    return chosen;
}

float model_animation_length(const AnimationCatalog& model, int index) {
    // `CGenericModel::getAnimationLengthSeconds(int)`.
    //
    if (index < 0 || static_cast<size_t>(index) >= model.size()) return 0.0f;
    return model[static_cast<size_t>(index)].second;
}

void assign_skill_animations(const SkillProperty& root, const SkillCaster& caster,
                             SkillAnimations& animations) {
    // `CSkill::assignSkillAnimations(CBaseUnit*)`. The
    // per-skill name overrides (`CSkill +0x128`..`+0x148`) have no known writer, so the
    // property's names are used.
    if (caster.model == nullptr) {
        animations.animation = -1;
        animations.animation_dw = -1;
        animations.loop_into = -1;
        animations.dw_loop_into = -1;
        // `+0xf4`, the loop end, is left as it was.
        animations.length = 1.0f;
        return;
    }
    const AnimationCatalog& model = *caster.model;

    if (caster.is_character && root.use_weapon_animation) {
        // `USEWEAPONANIMATION`: the right hand's attack when it satisfies
        // `REQUIREMENT_RIGHT`, else the left item's **left** attack
        // (`CEquipment +0x2a8`), else whatever the skill last held.
        int index = animations.animation;
        bool looked = false;
        if (hand_isa(caster, caster.right_hand_type, root.requirement_right)) {
            index = model_find_random_animation(model, caster.right_hand_attack_animation);
            looked = true;
        } else if (!caster.left_hand_type.empty()) {
            index = model_find_random_animation(model, caster.left_hand_attack_animation);
            looked = true;
        }
        if (looked) {
            animations.animation = index;
            animations.animation_dw = index;
        }
        if (index == -1) {
            index = attack_fallback(model);
            animations.animation = index;
            animations.animation_dw = index;
            if (index == -1) return;
        }
        animations.length = model_animation_length(model, index);
        // The loops and the dual-wield half are not looked at on this path.
        return;
    }

    if (!root.animation.empty()) {
        int index = index_or_random(model, root.animation);
        if (index == -1) index = attack_fallback(model);
        animations.animation = index;
        animations.animation_dw = index;
        // Nothing found anywhere leaves the length the skill already held.
        if (index != -1) animations.length = model_animation_length(model, index);
    }
    if (!root.animation_loop.empty()) {
        const int index = index_or_random(model, root.animation_loop);
        animations.loop_into = index;
        animations.dw_loop_into = index;
    }
    if (!root.animation_loop_end.empty()) {
        animations.loop_end = index_or_random(model, root.animation_loop_end);
    }

    // The dual-wield half: a character whose left hand holds a weapon that
    // satisfies `REQUIREMENT_LEFT`.
    if (!caster.is_character || !left_hand_weapon(caster)) return;
    if (!hand_isa(caster, caster.left_hand_type, root.requirement_left)) return;
    if (!root.animation_dw.empty()) {
        int index = index_or_random(model, root.animation_dw);
        if (index == -1) index = attack_fallback(model);
        animations.animation_dw = index;
        if (index != -1) {
            // The longer of the two, so a cast waits out whichever plays.
            animations.length = std::max(animations.length, model_animation_length(model, index));
        }
    }
    if (!root.animation_dw_loop.empty()) {
        animations.dw_loop_into = index_or_random(model, root.animation_dw_loop);
    }
}

bool is_dual_wielding(const SkillCaster& caster) {
    // `CCharacter::isDualWielding()`.
    if (caster.unit_types == nullptr) return false;
    return !caster.right_hand_type.empty() && !caster.left_hand_type.empty() &&
           caster.unit_types->isa(caster.right_hand_type, unit_type::kWeapon) &&
           caster.unit_types->isa(caster.left_hand_type, unit_type::kWeapon);
}

float skill_cast_speed(CharacterEffects effects, const SkillProperty& root) {
    // `CCharacter::castSkill`, on, in the console's
    // arithmetic: a slowdown (or NaN) is resisted by
    // `SLOW RESISTANCE` * 0.01f clamped to [0, 1]; the rate is `SPEED * fma(cast, 0.01f, 1)`,
    // at least 0.2 (NaN gives 0.2). `root` is `KnownSkill::current_property`.
    float cast = effects.effect_value(0x1d, kDamageTypeAny);
    if (!(cast >= 0.0f)) {
        float resist = effects.effect_value(0x8c, kDamageTypeAny) * 0.01f;
        resist = resist < 1.0f ? resist : 1.0f;
        resist = resist > 0.0f ? resist : 0.0f;
        cast = (1.0f - resist) * cast;
    }
    const float speed = root.animation_speed * std::fma(cast, 0.01f, 1.0f);
    return speed >= 0.2f ? speed : 0.2f;
}

bool still_casting(const ActiveSkill& active, const SkillAnimations& animations, bool running) {
    if (active.empty()) return false;
    if (animations.loop_into != -1 && !running) return false;
    return active.casting || active.animation_left > 0.0f;
}

// `CCharacter::update`: the path stop at
//
bool skill_halts_motion(const ActiveSkill& active, const SkillAnimations& animations, bool running,
                        bool moves_to_target) {
    if (active.empty()) return false;
    if (animations.loop_into != -1 && !running) return false;
    if (moves_to_target) return false;
    return active.casting || (active.animation_left > 0.0f && !active.attack_ended);
}

// X360 `CCharacter::update` (vtable slot 63), its turn block
// for a unit that `ISA` the player.
PlayerSkillTurn player_skill_turn(const ActiveSkill& active, const SkillAnimations& animations,
                                  bool running, bool allows_turning, bool moves_to_target,
                                  float find_target_angle, bool has_target, bool holding) {
    // holding a skill. Out of one, the hold (attack button down) takes the branch
    // and turns towards the aim point; with neither, the standing turn at
    // applies.
    if (!still_casting(active, animations, running)) {
        return holding ? PlayerSkillTurn::Point : PlayerSkillTurn::None;
    }
    // `ALLOWS_TURNING` (`CSkill +0x3e`) once 0.1 s or more is left
    // (`+0x2b4`), else true.
    const bool turning = active.animation_left < 0.1f ? true : allows_turning;
    if (!turning) return PlayerSkillTurn::Aim;
    // `MOVES_TO_TARGET` (`+0x40`) - no turn.
    if (moves_to_target) return PlayerSkillTurn::None;
    // a target (`+0x284`) or a target item (`+0x28c`), and the
    // skill's level property (`+0x5c`) has `FINDTARGETANGLE` (`+0x64`) above 0.
    if (has_target && 0.0f < find_target_angle) return PlayerSkillTurn::Target;
    return PlayerSkillTurn::Point;
}

SkillAnimationCue begin_skill_animation(ActiveSkill& active, const std::string& skill_name,
                                        const SkillProperty& root, const SkillCaster& caster,
                                        SkillRunState& state) {
    // `CCharacter::castSkill`,
    // after `performSkill`.
    active.skill = skill_name;
    active.struck = false;        // `+0x384`
    active.attack_ended = false;  // `+0x37d`

    assign_skill_animations(root, caster, state.animations);
    const float speed = skill_cast_speed(CharacterEffects(caster.effects, caster.inventory), root);
    state.speed = speed;  // `CSkill +0xf8`

    const bool dual = caster.is_character && is_dual_wielding(caster);
    const int animation = dual ? state.animations.animation_dw : state.animations.animation;
    const int loop = dual ? state.animations.dw_loop_into : state.animations.loop_into;

    // `+0x380`: the length is in the animation's own seconds, so over the rate
    // it is in frame time. A missing animation is 0 long.
    const float length =
        caster.model != nullptr ? model_animation_length(*caster.model, animation) : 0.0f;
    active.animation_left = length / speed;

    SkillAnimationCue cue;
    cue.speed = speed;
    if (animation != -1 && loop == -1) {
        cue.animation = animation;
        cue.duration = active.animation_left;
        cue.then_idle = true;
    } else if (animation != -1) {
        cue.animation = animation;
        cue.duration = active.animation_left;
        cue.then_loop = loop;
    } else if (loop != -1) {
        cue.animation = loop;
        cue.duration = -1.0f;
    } else {
        // Nothing to play, and the casting flag is not raised.
        return cue;
    }
    active.casting = true;  // `+0x267`
    return cue;
}

ActiveSkillUpdate update_active_skill(ActiveSkill& active, const SkillAnimations& animations,
                                      bool running, int64_t mana_cost_ot, float mana, float dt) {
    // `CCharacter::updateActiveSkill(float)`.
    ActiveSkillUpdate out;
    if (active.empty() || (animations.loop_into != -1 && !running) ||
        (!active.casting && active.animation_left <= 0.0f)) {
        active.casting = false;
        active.animation_left = 0.0f;
        active.attack_ended = true;
        return out;
    }

    // `getManaCostOT` returns an unsigned integer, converted to float here.
    const float upkeep = static_cast<float>(static_cast<uint32_t>(mana_cost_ot));
    if (upkeep > 0.0f && (animations.loop_into == -1 || active.attack_ended)) {
        if (mana < upkeep * dt) {
            out.loop_end = animations.loop_end;
            active.casting = false;
            active.animation_left = 0.0f;
            out.stop_all = true;
            return out;
        }
        out.mana_spent = upkeep * dt;
    }

    if (active.animation_left > 0.0f) {
        active.animation_left -= dt;
        if (active.animation_left <= 0.0f) {
            // `+0x37d`, and the hands' weapon trails off.
            out.hide_trails = true;
            active.attack_ended = true;
            active.animation_left = 0.0f;
            if (animations.loop_into == -1) active.casting = false;
        }
    }
    return out;
}

std::vector<FiredKey> keys_crossed(const std::vector<AnimationTag>& tags, int animation, float from,
                                   float to) {
    // `CGenericModel::updateAnimation`,
    // a key at `k` fires when `from <= k <= to`, or after a wrap
    // (`to < from`) when `k <= to` or `k >= from`. `k` is frame * 0.033333335f, not frame / 30
    // (they differ in the last place). Same test as `ModelAnimator::update`.
    std::vector<FiredKey> fired;
    if (animation < 0) return fired;
    for (size_t i = 0; i < tags.size(); ++i) {
        const AnimationTag& tag = tags[i];
        const float k = tag.frame * 0.033333335f;
        const bool hit =
            (!(from > k) && to >= k) || (!(from > k) && to < from) || (!(to < k) && !(to >= from));
        if (hit) fired.push_back(FiredKey{animation, keyframe_type_of(tag.name), i});
    }
    return fired;
}

std::vector<int> update_skill_keys(ActiveSkill& active, const SkillAnimations& animations,
                                   bool running, bool alive, const std::vector<FiredKey>& keys) {
    // `CCharacter::updateSkillKeys(float)`,
    // which `CCharacter::updateAnimation`
    // runs after the model's own update.
    std::vector<int> groups;
    if (active.empty()) return groups;
    if (animations.loop_into != -1 && !running) return groups;
    if (!active.casting && active.animation_left <= 0.0f) return groups;
    if (!alive) return groups;
    for (const FiredKey& key : keys) {
        if (key.type == kKeyframeHit || key.type == kKeyframeHitTwo) {
            if (is_skills_own(animations, key.animation)) {
                active.struck = true;
                groups.push_back(key.type == kKeyframeHit ? kSkillEventTrigger
                                                          : kSkillEventTriggerTwo);
            }
        }
        // Tested separately, and on any animation.
        if (key.type == kKeyframeAttackEnd) active.attack_ended = true;
    }
    return groups;
}

SkillStop attempt_stop_of_active_skill(ActiveSkill& active, const SkillAnimations* animations,
                                       bool can_stop) {
    // `CCharacter::attemptStopOfActiveSkill()`.
    //
    if (active.empty() || animations == nullptr) {
        active.casting = false;
        return SkillStop::Released;
    }
    if (animations->loop_into != -1 && can_stop) {
        active.casting = false;
        active.animation_left = 0.0f;
        return SkillStop::StopByName;
    }
    if (!active.casting && active.attack_ended) {
        active.casting = false;
        active.animation_left = 0.0f;
        return SkillStop::StopAll;
    }
    return SkillStop::None;
}

}  // namespace tl::core
