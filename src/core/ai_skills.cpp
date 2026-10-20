#include "core/ai_skills.h"

#include <utility>

namespace tl::core {

// CAISkill::CAISkill —
// initialize timer to 0.0f and store skill name.
AISkill::AISkill(std::string name) : name_(std::move(name)), time_(0.0f) {}

// CAISkill::update —
// subtract dt from remaining time, compare against 0.0f with seta;
// a NaN timer sets parity and returns false.
bool AISkill::update(float dt) {
    time_ -= dt;
    return time_ > 0.0f;
}

// CAISkillManager::CAISkillManager —
// initialize empty skills list with growth step 2.
AISkillManager::AISkillManager(AISkillsHooks hooks) : hooks_(std::move(hooks)) {}

// CAISkillManager::hasAISkill —
// exact case-sensitive match (length check then wmemcmp) against listed skills.
bool AISkillManager::has(AISkill* skill) const {
    if (!skill) return false;
    const std::string& target_name = skill->name();
    for (const AISkill* entry : skills_) {
        if (entry && entry->name() == target_name) {
            return true;
        }
    }
    return false;
}

// CAISkillManager::getSkill —
// walk in list order and return the first exact matching entry, or nullptr.
AISkill* AISkillManager::get(const std::string& name) const {
    for (AISkill* entry : skills_) {
        if (entry && entry->name() == name) {
            return entry;
        }
    }
    return nullptr;
}

// CAISkillManager::addAISkill —
// if skill name is already held, do nothing (timer is not refreshed).
// If not held: append pointer, set passed skill's remaining time, call skillAdded.
void AISkillManager::add(AISkill* skill, float duration) {
    if (!skill) return;
    if (has(skill)) return;
    skills_.push_back(skill);
    skill->set_timer(duration);
    skill_added(skill->name());
}

// CAISkillManager::removeAISkill —
// null argument does nothing. If held, passed skill's timer is set to 0.0f.
// All listed entries with matching name are removed via swap-with-last (re-testing
// current index). Then skillRemoved runs unconditionally.
void AISkillManager::remove(AISkill* skill) {
    if (!skill) return;
    if (has(skill)) {
        skill->set_timer(0.0f);
    }
    const std::string name = skill->name();
    size_t j = 0;
    while (j < skills_.size()) {
        if (skills_[j] && skills_[j]->name() == name) {
            skills_[j] = skills_.back();
            skills_.pop_back();
        } else {
            ++j;
        }
    }
    skill_removed(name);
}

// CAISkillManager::updateAISkill —
// ticks the first entry matching name via getSkill.
void AISkillManager::update_one(const std::string& name, float dt) {
    AISkill* s = get(name);
    if (s) {
        s->update(dt);
    }
}

// CAISkillManager::updateAISkills —
// count is re-read each step. Entry is visited only when non-null and time > 0.0f.
// Ticks first match via updateAISkill, then entry itself via CAISkill::update (double tick).
// On expiration (update returns false), removes entry. Index advances unconditionally,
// so the swapped-in entry waits a frame.
void AISkillManager::update(float dt) {
    size_t i = 0;
    while (i < skills_.size()) {
        AISkill* entry = skills_[i];
        if (!entry || !(entry->timer() > 0.0f)) {
            ++i;
            continue;
        }
        update_one(entry->name(), dt);
        const bool alive = entry->update(dt);
        if (!alive) {
            remove(entry);
        }
        ++i;
    }
}

// CAISkillManager::skillAdded —
// notify owner to enable skill (+0x6d = 1).
void AISkillManager::skill_added(const std::string& name) {
    if (hooks_.on_skill_enabled) {
        hooks_.on_skill_enabled(name, true);
    }
}

// CAISkillManager::skillRemoved —
// notify owner to disable skill (+0x6d = 0).
void AISkillManager::skill_removed(const std::string& name) {
    if (hooks_.on_skill_enabled) {
        hooks_.on_skill_enabled(name, false);
    }
}

}  // namespace tl::core
