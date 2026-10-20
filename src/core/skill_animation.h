// A skill's animation on its caster, and the keys on it that fire the skill's
// `EVENT_TRIGGER` groups: `CSkill::assignSkillAnimations`,
// `CCharacter::castSkill`'s animation half,
// `CCharacter::updateActiveSkill`,
// `CCharacter::updateSkillKeys` and
// `CCharacter::attemptStopOfActiveSkill`.
// This is what makes a self-targeted cast visible: its affix is in `[EVENT_TRIGGER]`,
// fired only by a `HIT` key on the skill's own animation. Keys are `.ANIMATION` trigger
// tags, their `NAME` an index of `gKEYFRAME_TYPES` (`CKeyframe::CKeyframe`).
//
// No OGRE and no file I/O: the renderer hands in the animations and the keys crossed.
#pragma once

#include "core/animation_manifest.h"
#include "core/skill_cast.h"
#include "core/sound_data.h"

#include <string>
#include <vector>

namespace tl::core {

/// `gKEYFRAME_TYPES`, in the order the table's static initialiser builds it
/// (`_GLOBAL__I_CKeyframe`). The index is `CKeyframe +0x58`.
inline constexpr int kKeyframeHit = 0;                  ///< `HIT`: fires `EVENT_TRIGGER`
inline constexpr int kKeyframeAttackEnd = 15;           ///< `ATTACKEND` (0xf)
inline constexpr int kKeyframeHitTwo = 28;              ///< `HITTWO` (0x1c): `EVENT_TRIGGER_TWO`
inline constexpr int kKeyframePlaySound = 3;            ///< `PLAYSOUND`
inline constexpr int kKeyframePlaySoundNoFalloff = 27;  ///< `PLAYSOUND_NO_FALLOFF` (0x1b)

/// Whether `CCharacter::loadModel` adds a sample for one of the model's
/// keys: a `PLAYSOUND` (3) or `PLAYSOUND_NO_FALLOFF` (0x1b) key
/// whose path `CSoundBankDataInformation::getSoundDataObject` finds,
/// as written. `CSoundBank::addSample` keeps only `g_Rand`'s low word.
bool manifest_adds_sound_sample(const AnimationManifest& manifest,
                                const SoundDataInformation& sounds);

/// `EVENT_TRIGGER` and `EVENT_TRIGGER_TWO` in `gSKILL_EVENT_TYPE_NAMES`.
inline constexpr int kSkillEventTrigger = 2;
inline constexpr int kSkillEventTriggerTwo = 3;

/// A tag's `NAME` as its `gKEYFRAME_TYPES` index, upper-cased first (`CKeyframe::CKeyframe`).
/// `STRINGS::getStringIndex`
/// answers a miss with 0, so an unknown or missing name is a `HIT`.
int keyframe_type_of(const std::string& name);

/// `CGenericModel::getAnimationIndex(const std::string&)`: the first
/// animation whose name equals `name` without regard to case, or -1.
int model_animation_index(const AnimationCatalog& model, const std::string& name);

/// `CGenericModel::findRandomAnimation(const std::string&)`:
/// the first animation whose upper-cased name starts with `prefix`, each later
/// match replacing it on `randomBetweenVolatile(0, 1000) < 500`. Quirk kept: the prefix is
/// not upper-cased, so a lower-case one matches nothing.
int model_find_random_animation(const AnimationCatalog& model, const std::string& prefix);

/// `CGenericModel::getAnimationLengthSeconds(int)`:
/// 0 for a negative or out-of-range index.
float model_animation_length(const AnimationCatalog& model, int index);

/// `CSkill::assignSkillAnimations(CBaseUnit*)`: the
/// caster's animations for the skill by the **root** property's names, and the main one's
/// length. Updated in place: a property with no `ANIMATION` leaves what was held.
void assign_skill_animations(const SkillProperty& root, const SkillCaster& caster,
                             SkillAnimations& animations);

/// `CCharacter::isDualWielding()`: both hands
/// hold a `[weapon]`.
bool is_dual_wielding(const SkillCaster& caster);

/// The rate `CCharacter::castSkill` plays a skill at:
/// `PERCENT CAST SPEED` (effect 0x1d), a slowdown reduced by `SLOW RESISTANCE` (0x8c),
/// then `max(0.2, (1 + cast / 100) * SPEED)`. `effects` is the caster's total
/// (`CCharacter::getEffectValue`).
float skill_cast_speed(CharacterEffects effects, const SkillProperty& root);

/// What a character holds while it casts: `CCharacter +0x398` and the flags
/// around it. The original keeps these on the character, not on the skill, and
/// they outlive the skill's own run by a frame or two.
struct ActiveSkill {
    /// `+0x398`, the skill being cast, by name. Empty for none.
    std::string skill;
    /// `+0x380`: the cast animation's time left, in seconds of frame time
    /// (length over speed, so it is counted down by the plain delta).
    float animation_left = 0.0f;
    /// `+0x267`: an animation is playing for the skill. Set by `castSkill` when
    /// it queues one; cleared when the animation runs out on a skill with no
    /// loop, or when the cast is stopped.
    bool casting = false;
    /// `+0x37d`: the swing is over. Set by an `ATTACKEND` key, when the
    /// animation runs out, and whenever the character holds no live cast.
    bool attack_ended = false;
    /// `+0x384`: a `HIT` or `HITTWO` key has fired for this cast.
    bool struck = false;

    bool empty() const { return skill.empty(); }
};

/// `castSkill`'s test for "still casting" (and the same shape in
/// `updateSkillKeys` and `updateActiveSkill`): the active skill has no loop or
/// is still running, **and** its animation is up or still has time.
bool still_casting(const ActiveSkill& active, const SkillAnimations& animations, bool running);

/// `CCharacter::update`: the cast
/// stops the character's path while the active skill has no loop or is running, its
/// animation has time left before the swing is over, and the skill lacks
/// `MOVES_TO_TARGET`. `setDestination` refuses a new one
/// under the same test.
bool skill_halts_motion(const ActiveSkill& active, const SkillAnimations& animations, bool running,
                        bool moves_to_target);

/// Where the console's `CCharacter::update` (X360 vtable slot 63)
/// turns the player while it holds a skill or the attack button.
enum class PlayerSkillTurn {
    None,    ///< holding neither a skill nor the button, or in a skill that `MOVES_TO_TARGET`
    Aim,     ///< towards its position plus its last direction (`+0x234`), at the frame's turn
    Target,  ///< towards the target, else the target item, at 30 times the frame's turn
    Point,   ///< towards the point it last aimed at (`+0x228`), at the frame's turn
};

/// the player's half (the target turns are
/// skipped for the player). In a skill the player turns along its
/// aim while the cast has 0.1 s or more to run and the skill does not allow turning, else
/// at its target when `FINDTARGETANGLE` > 0, else at its aim point. `has_target`: a target
/// or target item. `ai_state` 0x28 has no turn along the aim. `holding`: the attack button
/// is down, which takes the same branch.
PlayerSkillTurn player_skill_turn(const ActiveSkill& active, const SkillAnimations& animations,
                                  bool running, bool allows_turning, bool moves_to_target,
                                  float find_target_angle, bool has_target, bool holding = false);

/// What `castSkill` asks the model to play: `blendAnimation(animation, 0.2,
/// speed, duration)`, then either the loop or `IDLE` queued after it.
struct SkillAnimationCue {
    int animation = -1;  ///< -1: nothing to play
    float speed = 1.0f;
    float duration = -1.0f;  ///< how long to hold it; -1 loops it
    int then_loop = -1;      ///< queued, looping, after `animation`
    bool then_idle = false;  ///< `IDLE` queued after `animation`
};

/// `CCharacter::castSkill` after `performSkill`
/// succeeded: flags cleared, `assignSkillAnimations` again, the rate, the hold time and
/// the queued animation. The caller has charged the mana; `speed` goes into `state`.
SkillAnimationCue begin_skill_animation(ActiveSkill& active, const std::string& skill_name,
                                        const SkillProperty& root, const SkillCaster& caster,
                                        SkillRunState& state);

/// What `updateActiveSkill` asks of the rest of the character this frame.
struct ActiveSkillUpdate {
    /// Mana to take off: `MANACOSTOT` per second while the cast holds.
    float mana_spent = 0.0f;
    /// Out of mana for the upkeep: `stopAllSkills(false, false, false)`, after
    /// blending `loop_end` (when it is not -1) and queueing `IDLE`.
    bool stop_all = false;
    int loop_end = -1;
    /// The skill's animation time ran out: the hands' weapon
    /// trails (`+0x698`, `+0x6a0`) are switched off and hidden.
    bool hide_trails = false;
};

/// `CCharacter::updateActiveSkill(float)`, run by
/// `CCharacter::update` after `updateEffects`.
/// `mana_cost_ot` is `CSkill::getManaCostOT`, an **integer** (2.5 drains 2).
ActiveSkillUpdate update_active_skill(ActiveSkill& active, const SkillAnimations& animations,
                                      bool running, int64_t mana_cost_ot, float mana, float dt);

/// One key the caster's model stepped past this frame: `CGenericModel +0x1b0`,
/// which `CGenericModel::updateAnimation`
/// refills every frame.
struct FiredKey {
    int animation = -1;  ///< `CKeyframe +0x28`, the model's animation index
    int type = -1;       ///< `CKeyframe +0x58`, a `gKEYFRAME_TYPES` index
    /// Which of the animation's tags it is, for a key that carries more than
    /// its type (a `SPAWNPARTICLE`'s layout, bone and offset).
    size_t tag = 0;
};

/// The keys on one animation a frame stepped past, from `from` to `to` seconds:
/// `CGenericModel::updateAnimation`'s fill. A key at `k`
/// fires when `from <= k <= to`, or after a wrap when `k <= to` or `k >= from`, so a key
/// where a clock stands still fires again. The caller skips invisible models and zero
/// deltas.
std::vector<FiredKey> keys_crossed(const std::vector<AnimationTag>& tags, int animation, float from,
                                   float to);

/// `CCharacter::updateSkillKeys(float)`: the event
/// groups the keys fire, in key order. `HIT` (2) and `HITTWO` (3) count only on the active
/// skill's own animations; `ATTACKEND` on any. `alive`: not dying (5) or dead (6). Each
/// group goes to `CSkillManager::triggerSkillEvent`,
/// which is `SkillRunner::trigger_event`.
std::vector<int> update_skill_keys(ActiveSkill& active, const SkillAnimations& animations,
                                   bool running, bool alive, const std::vector<FiredKey>& keys);

/// What `attemptStopOfActiveSkill` did.
enum class SkillStop {
    None,        ///< nothing: the cast is still inside its minimum time
    Released,    ///< no active skill: only the casting flag cleared
    StopByName,  ///< a looping skill past its minimum: `stopSkillsByName`
    StopAll,     ///< a finished swing: `stopAllSkills(false, false, false)`
};

/// `CCharacter::attemptStopOfActiveSkill()`:
/// the player letting go of the button. `can_stop` is `CSkill::getCanStop`
/// on the active skill.
SkillStop attempt_stop_of_active_skill(ActiveSkill& active, const SkillAnimations* animations,
                                       bool can_stop);

}  // namespace tl::core
