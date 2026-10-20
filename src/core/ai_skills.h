// Timed AI skills (CAISkill, CAISkillManager, CAIManager skill forwarders): a
// character's list of borrowed AISkill pointers, granted by stat watchers or
// scripts. Side effects (enable/disable) go through owner-provided hooks.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// One timed AI skill instance.
/// Owned by watchers (or character), borrowed by AISkillManager.
class AISkill {
public:
    // CAISkill::CAISkill
    explicit AISkill(std::string name);

    // CAISkill::update
    bool update(float dt);

    // PORT: an accessor of the port's own bookkeeping.
    const std::string& name() const { return name_; }
    // PORT: an accessor of the port's own bookkeeping.
    float timer() const { return time_; }
    // PORT: an accessor of the port's own bookkeeping.
    void set_timer(float time) { time_ = time; }

private:
    std::string name_;   ///< +0x18
    float time_ = 0.0f;  ///< +0x10
};

/// Side effect hooks supplied by the owning character/level.
struct AISkillsHooks {
    /// CAISkillManager::skillAdded and CAISkillManager::skillRemoved:
    /// switches CSkill +0x6d (enabled for use).
    std::function<void(const std::string& name, bool enabled)> on_skill_enabled;
};

/// Transcribed AI skill manager (CAISkillManager;
/// and CAIManager skill forwarders).
class AISkillManager {
public:
    // CAISkillManager::CAISkillManager
    explicit AISkillManager(AISkillsHooks hooks = {});

    void set_hooks(AISkillsHooks hooks) { hooks_ = std::move(hooks); }

    // CAISkillManager::hasAISkill
    bool has(AISkill* skill) const;

    // CAISkillManager::getSkill
    AISkill* get(const std::string& name) const;

    // CAISkillManager::addAISkill
    void add(AISkill* skill, float duration);

    // CAISkillManager::removeAISkill
    void remove(AISkill* skill);

    // CAISkillManager::updateAISkill
    void update_one(const std::string& name, float dt);

    // CAISkillManager::updateAISkills
    void update(float dt);

    // Inspection accessors (no original counterparts, for tests)
    size_t size() const { return skills_.size(); }
    bool empty() const { return skills_.empty(); }
    AISkill* at(size_t index) const { return skills_.at(index); }
    const std::vector<AISkill*>& skills() const { return skills_; }

private:
    // CAISkillManager::skillAdded
    void skill_added(const std::string& name);

    // CAISkillManager::skillRemoved
    void skill_removed(const std::string& name);

    std::vector<AISkill*> skills_;  ///< +0x10 TArrayList<CAISkill*>
    AISkillsHooks hooks_;
};

}  // namespace tl::core
