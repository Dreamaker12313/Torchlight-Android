// Unit tests for AISkill and AISkillManager.
// Everything is built in code; no game data files are accessed.

#include "core/ai_skills.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include "test_support.h"

namespace {

using tl::core::AISkill;
using tl::core::AISkillManager;
using tl::core::AISkillsHooks;

}  // namespace

// CAISkill::update
// A skill counts down and reports live while above zero: 1.0 minus 0.4 is live,
// down to exactly 0 is not, and a NaN timer is not.
TL_TEST(a_skill_counts_down_and_reports_live_while_above_zero) {
    AISkill skill("TestSkill");
    skill.set_timer(1.0f);

    // 1.0 - 0.4 = 0.6 > 0 -> returns true (live)
    bool live1 = skill.update(0.4f);
    TL_CHECK(live1);
    TL_CHECK(std::fabs(skill.timer() - 0.6f) < 0.0001f);

    // 0.6 - 0.6 = 0.0 -> returns false (not live)
    bool live2 = skill.update(0.6f);
    TL_CHECK(!live2);
    TL_CHECK_EQ(skill.timer(), 0.0f);

    // NaN timer: comparison > 0.0f is false -> returns false
    skill.set_timer(std::numeric_limits<float>::quiet_NaN());
    bool live_nan = skill.update(0.1f);
    TL_CHECK(!live_nan);
}

// CAISkillManager::addAISkill, skillAdded
// Adding writes the duration and enables: the passed skill's timer becomes the
// duration, the list holds it, the hook sees (name, true) once.
TL_TEST(adding_writes_the_duration_and_enables) {
    AISkillManager mgr;
    std::vector<std::pair<std::string, bool>> hook_events;

    AISkillsHooks hooks;
    hooks.on_skill_enabled = [&](const std::string& name, bool enabled) {
        hook_events.emplace_back(name, enabled);
    };
    mgr.set_hooks(hooks);

    AISkill skill("Fireball");
    skill.set_timer(0.0f);

    mgr.add(&skill, 5.0f);

    TL_CHECK_EQ(skill.timer(), 5.0f);
    TL_CHECK(mgr.has(&skill));
    TL_CHECK(mgr.get("Fireball") != nullptr);
    TL_CHECK_EQ(mgr.skills().size(), 1u);
    TL_CHECK_EQ(mgr.skills()[0], &skill);

    TL_CHECK_EQ(hook_events.size(), 1u);
    TL_CHECK_EQ(hook_events[0].first, "Fireball");
    TL_CHECK(hook_events[0].second);
}

// CAISkillManager::addAISkill
// Adding a held name changes nothing: a second AISkill of the same name, added
// with another duration, is not listed, keeps its timer, and no hook fires.
TL_TEST(adding_a_held_name_changes_nothing) {
    AISkillManager mgr;
    std::vector<std::pair<std::string, bool>> hook_events;

    AISkillsHooks hooks;
    hooks.on_skill_enabled = [&](const std::string& name, bool enabled) {
        hook_events.emplace_back(name, enabled);
    };
    mgr.set_hooks(hooks);

    AISkill skill1("Fireball");
    mgr.add(&skill1, 5.0f);
    TL_CHECK_EQ(hook_events.size(), 1u);

    AISkill skill2("Fireball");
    skill2.set_timer(10.0f);

    mgr.add(&skill2, 20.0f);

    // skill2 was not added; timer kept its original value
    TL_CHECK_EQ(skill2.timer(), 10.0f);
    TL_CHECK_EQ(mgr.skills().size(), 1u);
    TL_CHECK_EQ(mgr.skills()[0], &skill1);
    TL_CHECK_EQ(hook_events.size(), 1u);  // No additional hook call
}

// CAISkillManager::hasAISkill, getSkill
// Names match exactly: Summon and SUMMON are different skills (case-sensitive
// std::string comparison in the original).
TL_TEST(names_match_exactly) {
    AISkillManager mgr;
    AISkill skill_lower("summon");
    AISkill skill_title("Summon");
    AISkill skill_upper("SUMMON");

    mgr.add(&skill_lower, 3.0f);
    mgr.add(&skill_title, 4.0f);
    mgr.add(&skill_upper, 5.0f);

    TL_CHECK_EQ(mgr.skills().size(), 3u);
    TL_CHECK(mgr.has(&skill_lower));
    TL_CHECK(mgr.has(&skill_title));
    TL_CHECK(mgr.has(&skill_upper));

    TL_CHECK_EQ(mgr.get("summon"), &skill_lower);
    TL_CHECK_EQ(mgr.get("Summon"), &skill_title);
    TL_CHECK_EQ(mgr.get("SUMMON"), &skill_upper);
    TL_CHECK_EQ(mgr.get("SuMmOn"), nullptr);
}

// CAISkillManager::removeAISkill, skillRemoved
// Removing zeroes the passed skill and unlists the name.
TL_TEST(removing_zeroes_the_passed_skill_and_unlists_the_name) {
    AISkillManager mgr;
    std::vector<std::pair<std::string, bool>> hook_events;

    AISkillsHooks hooks;
    hooks.on_skill_enabled = [&](const std::string& name, bool enabled) {
        hook_events.emplace_back(name, enabled);
    };
    mgr.set_hooks(hooks);

    AISkill skill("Heal");
    mgr.add(&skill, 8.0f);
    TL_CHECK(mgr.has(&skill));
    TL_CHECK_EQ(hook_events.size(), 1u);

    mgr.remove(&skill);

    TL_CHECK_EQ(skill.timer(), 0.0f);
    TL_CHECK(!mgr.has(&skill));
    TL_CHECK_EQ(mgr.get("Heal"), nullptr);
    TL_CHECK(mgr.skills().empty());

    TL_CHECK_EQ(hook_events.size(), 2u);
    TL_CHECK_EQ(hook_events[1].first, "Heal");
    TL_CHECK(!hook_events[1].second);  // disabled (false)
}

// CAISkillManager::removeAISkill, skillRemoved
// Removing an unheld skill still disables: skillRemoved runs unconditionally at the
// end of removeAISkill, so the hook sees (name, false) even though nothing was listed.
TL_TEST(removing_an_unheld_skill_still_disables) {
    AISkillManager mgr;
    std::vector<std::pair<std::string, bool>> hook_events;

    AISkillsHooks hooks;
    hooks.on_skill_enabled = [&](const std::string& name, bool enabled) {
        hook_events.emplace_back(name, enabled);
    };
    mgr.set_hooks(hooks);

    AISkill unheld_skill("UnheldSkill");
    unheld_skill.set_timer(7.5f);

    // Removing unheld pointer: passed object timer untouched (gates zeroing on hasAISkill)
    mgr.remove(&unheld_skill);
    TL_CHECK_EQ(unheld_skill.timer(), 7.5f);
    TL_CHECK_EQ(hook_events.size(), 1u);
    TL_CHECK_EQ(hook_events[0].first, "UnheldSkill");
    TL_CHECK(!hook_events[0].second);
}

// CAISkillManager::updateAISkills
// A live skill ticks twice per update: duration 1.0, update(0.3) leaves 0.4.
// updateAISkills calls updateAISkill(entry->name, dt) (ticks first matching entry),
// followed by entry->update(dt).
TL_TEST(a_live_skill_ticks_twice_per_update) {
    AISkillManager mgr;
    AISkill skill("Haste");
    mgr.add(&skill, 1.0f);

    mgr.update(0.3f);

    // 1.0 - 0.3 (updateAISkill) - 0.3 (entry->update) = 0.4
    TL_CHECK(std::fabs(skill.timer() - 0.4f) < 0.0001f);
}

// CAISkillManager::updateAISkills
// A zero-duration skill is never ticked or removed:
// updateAISkills guards entry processing with ucomiss, timer; jbe skip.
TL_TEST(a_zero_duration_skill_is_never_ticked_or_removed) {
    AISkillManager mgr;
    AISkill skill("EternalBuff");
    mgr.add(&skill, 0.0f);

    TL_CHECK_EQ(mgr.skills().size(), 1u);
    TL_CHECK_EQ(skill.timer(), 0.0f);

    mgr.update(1.0f);

    // Entry skipped: still held, timer still 0.0f
    TL_CHECK_EQ(mgr.skills().size(), 1u);
    TL_CHECK_EQ(skill.timer(), 0.0f);
    TL_CHECK(mgr.has(&skill));
}

// CAISkillManager::updateAISkills, removeAISkill: an expired skill
// is removed and the last one swapped into its slot is skipped this frame (i++ regardless).
TL_TEST(an_expired_skill_is_removed_and_the_swapped_in_one_waits_a_frame) {
    AISkillManager mgr;
    AISkill skill1("ExpiringSkill");
    AISkill skill2("MiddleSkill");
    AISkill skill3("SwappedSkill");

    // skill1: duration 0.5f. On update(0.3f), ticks 0.5 -> 0.2 -> -0.1 <= 0 -> expires and removed
    mgr.add(&skill1, 0.5f);
    // skill2: duration 2.0f
    mgr.add(&skill2, 2.0f);
    // skill3: duration 2.0f
    mgr.add(&skill3, 2.0f);

    TL_CHECK_EQ(mgr.skills().size(), 3u);
    TL_CHECK_EQ(mgr.skills()[0], &skill1);
    TL_CHECK_EQ(mgr.skills()[1], &skill2);
    TL_CHECK_EQ(mgr.skills()[2], &skill3);

    mgr.update(0.3f);

    // skill1 was removed; skill3 was swapped into index 0; skill2 remained at index 1
    TL_CHECK_EQ(mgr.skills().size(), 2u);
    TL_CHECK_EQ(mgr.skills()[0], &skill3);
    TL_CHECK_EQ(mgr.skills()[1], &skill2);

    // skill3 was swapped into index 0 while i=0, then loop did i++, so index 0 was skipped!
    // skill3 was NOT ticked this frame: timer remains 2.0f
    TL_CHECK_EQ(skill3.timer(), 2.0f);

    // skill2 was at index 1, which the loop visited next: ticked twice (2.0 - 0.3 - 0.3 = 1.4f)
    TL_CHECK(std::fabs(skill2.timer() - 1.4f) < 0.0001f);
}
