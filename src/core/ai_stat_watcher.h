// AI stat watchers:
// each watches a stat of its
// character (HP, mana, their percentages, active units around it) and adds or
// removes AI flags and skills on targets. World access goes through hooks.
#pragma once

#include "core/ai_flags.h"
#include "core/ai_skills.h"
#include "core/unit_types.h"
#include "formats/adm.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tl::core {

/// `EAISTAT_TYPES`, in `g_AISTAT_TYPE_NAMES`' order.
enum class AIStatType : uint32_t {
    None = 0,
    Hp = 1,
    Mana = 2,
    HpPct = 3,
    ManaPct = 4,
    ActiveUnits = 5,
};

/// `EAISTAT_LOGIC`, in `g_AISTAT_LOGIC_NAMES`' order.
enum class AIStatLogic : uint32_t {
    Below = 0,
    Above = 1,
};

/// `EAISTAT_TARGET`, in `g_AISTAT_TARGET_NAMES`' order.
enum class AIStatTarget : uint32_t {
    Self = 0,
    Formation = 1,
    Area = 2,
    AreaUnitTypes = 3,
    AreaFormation = 4,
};

/// One parsed [STAT] definition from a unit's [AI] block.
/// Fields, types and defaults match CAIStatWatcher::CAIStatWatcher.
///
struct AIStatWatcherDef {
    AIStatType stat = AIStatType::None;        ///< +0x20, STAT
    AIStatLogic logic = AIStatLogic::Below;    ///< +0x24, LOGIC
    std::vector<AIFlag> flags;                 ///< +0x28, FLAG records
    std::vector<std::string> skills;           ///< +0x40, SKILL records
    AIStatTarget target = AIStatTarget::Self;  ///< +0x88, TARGET
    uint32_t target_unittype = 0;              ///< +0x8c, TARGETUNITTYPE (default 0 = ANY)
    uint32_t target_alignment = 3;             ///< +0x90, TARGET_ALIGNMENT (default 3 = ALL)
    float target_area = 10.0f;                 ///< +0x94, TARGETAREA (default 10.0)
    float duration = 0.0f;                     ///< +0x98, DURATION
    float value = 0.0f;                        ///< +0x9c, VALUE
    bool only_once = false;                    ///< +0xa0, ONLYONCE (quirk: set if value == 0)
    std::string animation;                     ///< +0xa8, ANIMATION (uppercased)
    bool enable_on_event = true;               ///< +0xb0, ENABLE_ON_EVENT (inverts when false)
    bool include_living = true;                ///< +0xb1, INCLUDE_LIVING
    bool include_dead = false;                 ///< +0xb2, INCLUDE_DEAD
};

/// Pure parsing functions for [STAT] blocks within an [AI] block.
/// Cited: CAIStatWatcher::CAIStatWatcher.
std::vector<AIStatWatcherDef> parse_ai_stat_watchers(const formats::AdmSection& ai_section,
                                                     const UnitTypeHierarchy& hierarchy);

std::vector<AIStatWatcherDef> parse_ai_stat_watchers(const formats::AdmSection& ai_section);

/// Fills out with watchers from the first [AI] section in root, if not already populated.
void parse_unit_ai_watchers(std::vector<AIStatWatcherDef>& out, const formats::AdmSection& root,
                            const UnitTypeHierarchy& hierarchy);

/// Environment access hooks supplied by the owning character/level.
struct AIStatWatcherHooks {
    std::function<int64_t()> owner_id;
    std::function<int32_t()> hp;
    std::function<int32_t()> max_hp;
    std::function<int32_t()> mana;
    std::function<int32_t()> max_mana;
    std::function<bool()> alive;
    std::function<bool()> has_level;
    std::function<std::vector<int64_t>()> formation_units;
    std::function<AIFlags*(int64_t id)> find_flags_by_id;
    std::function<float(uint32_t unittype, uint32_t alignment, float radius, bool include_living,
                        bool include_dead)>
        active_units_count;
    std::function<std::vector<int64_t>(float radius, uint32_t unittype)> area_targets;
    std::function<std::vector<int64_t>(float radius)> area_formation_targets;
    std::function<void(AISkill* skill, float duration)> add_ai_skill;
    std::function<bool(AISkill* skill)> has_ai_skill;
    std::function<void(AISkill* skill)> remove_ai_skill;
    /// `CCharacter::setAIPlayAnimation` on the unit: each flagged target in
    /// `addFlag`, the watcher's own unit in `addSkill`.
    std::function<void(int64_t unit_id, const std::string& anim)> play_animation;
};

/// Transcribed AI stat watcher.
class AIStatWatcher {
public:
    explicit AIStatWatcher(AIStatWatcherDef def, AIStatWatcherHooks hooks = {});
    // Not copyable: a character's `AISkillManager` lists this watcher's
    // `AISkill`s by address, and a copy would leave it pointing at the source's.
    AIStatWatcher(const AIStatWatcher&) = delete;
    AIStatWatcher& operator=(const AIStatWatcher&) = delete;
    AIStatWatcher(AIStatWatcher&&) = default;
    AIStatWatcher& operator=(AIStatWatcher&&) = default;

    void set_hooks(AIStatWatcherHooks hooks) { hooks_ = std::move(hooks); }

    // CAIStatWatcher::update
    void update(float dt);

    // CAIStatWatcher::addFlag
    void add_flag();

    // CAIStatWatcher::removeFlag
    void remove_flag();

    // CAIStatWatcher::addSkill
    void add_skill();

    // CAIStatWatcher::removeSkill
    void remove_skill();

    // CAIStatWatcher::getTargets
    void get_targets(std::vector<int64_t>& out);

    const AIStatWatcherDef& def() const { return def_; }
    bool has_fired() const { return has_fired_; }
    const std::vector<int64_t>& recorded_target_guids() const { return recorded_target_guids_; }
    const std::vector<std::unique_ptr<AISkill>>& skills() const { return skills_; }

private:
    AIStatWatcherDef def_;
    AIStatWatcherHooks hooks_;
    std::vector<std::unique_ptr<AISkill>> skills_;  ///< +0x40 TArrayList<CAISkill*>
    bool has_fired_ = false;                        ///< +0xa1
    std::vector<int64_t> recorded_target_guids_;    ///< +0x58
};

}  // namespace tl::core
