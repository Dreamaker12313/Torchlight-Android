#include "core/ai_flags.h"

namespace tl::core {

// CAIFlagManager::CAIFlagManager and
// CAIFlag::CAIFlag: all 7 flags at 0. Made by
// `CAIManager::CAIManager(CCharacter*)`.
AIFlags::AIFlags(AIFlagsHooks hooks) : hooks_(std::move(hooks)) { times_.fill(0.0f); }

// CAIFlagManager::hasAIFlag and CAIManager::hasAIFlag —
// return whether the flag's remaining duration is strictly greater than 0.
bool AIFlags::has(AIFlag flag) const {
    const size_t idx = static_cast<size_t>(flag);
    if (idx >= kAIFlagCount) return false;
    return times_[idx] > 0.0f;
}

// CAIFlagManager::addAIFlag and CAIManager::addAIFlag —
// if duration is greater than current time, raise to duration; if flag was unset
// (old time <= 0), execute flagSet side effects.
void AIFlags::add(AIFlag flag, float duration) {
    const size_t idx = static_cast<size_t>(flag);
    if (idx >= kAIFlagCount) return;
    const float old_time = times_[idx];
    if (old_time < duration) {
        times_[idx] = duration;
    }
    if (old_time <= 0.0f) {
        flag_set(flag);
    }
}

// CAIFlagManager::removeAIFlag and CAIManager::removeAIFlag —
// reset remaining time to 0.0f. Original's quirk: flagRemoved
// is executed only when old time was <= 0.0f; manual removal of a live flag returns early.
void AIFlags::remove(AIFlag flag) {
    const size_t idx = static_cast<size_t>(flag);
    if (idx >= kAIFlagCount) return;
    const float old_time = times_[idx];
    times_[idx] = 0.0f;
    // `seta`: a NaN time counts as not set.
    if (!(old_time > 0.0f)) {
        flag_removed(flag);
    }
}

// CAIFlagManager::updateAIFlags and CAIFlag::update —
// age active flags by dt; on expiration, removeAIFlag executes flagRemoved.
void AIFlags::update(float dt) {
    for (size_t i = 0; i < kAIFlagCount; ++i) {
        if (times_[i] <= 0.0f) continue;
        times_[i] -= dt;
        // `CAIFlag::update` returns `time > 0` (`seta`), so a NaN expires.
        if (!(times_[i] > 0.0f)) {
            remove(static_cast<AIFlag>(i));
        }
    }
}

// CAIFlagManager::updateAIFlag — empty no-op in the original.
void AIFlags::update_ai_flag(AIFlag flag, float dt) {
    (void)flag;
    (void)dt;
}

// CAIManager::addFormationUnit — append unit ID to formation list.
void AIFlags::add_formation_unit(int64_t id) { formation_units_.push_back(id); }

// PORT: an accessor of an AI flag's time.
float AIFlags::time(AIFlag flag) const {
    const size_t idx = static_cast<size_t>(flag);
    if (idx >= kAIFlagCount) return 0.0f;
    return times_[idx];
}

// CAIFlagManager::flagSet — side effects when a flag transitions from unset to set.
void AIFlags::flag_set(AIFlag flag) {
    if (flag == AIFlag::Aware) {
        const float duration = times_[static_cast<size_t>(AIFlag::Aware)];
        if (hooks_.on_spread_aware) {
            for (int64_t member_id : formation_units_) {
                hooks_.on_spread_aware(member_id, duration);
            }
        }
    } else if (flag == AIFlag::Berserk) {
        if (hooks_.on_berserk_start) {
            hooks_.on_berserk_start();
        }
    }
}

// CAIFlagManager::flagRemoved — side effects on flag expiration.
void AIFlags::flag_removed(AIFlag flag) {
    if (flag == AIFlag::Berserk) {
        if (hooks_.on_berserk_end) {
            hooks_.on_berserk_end();
        }
    }
}

}  // namespace tl::core
