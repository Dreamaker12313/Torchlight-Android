// Who watches which unit, and when a skill watches the unit its effects land on
// (`CSkillEffectAndAffixes::applyAffixesAndEffects`).
#include "core/unit_observers.h"
#include "core/effect_manager.h"
#include "core/effects.h"
#include "core/item_use.h"
#include "core/skill_cast.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::UnitObserver;
using tl::core::UnitObservers;

struct Heard {
    int64_t unit;
    int state;
};

/// An observer that writes down what it hears, and can take itself off the
/// unit's list as it hears it, as `CSkill::unitStateChange` does.
struct Listener final : UnitObserver {
    UnitObservers* map = nullptr;
    bool leaves = false;
    std::vector<Heard> heard;
    void unit_state_change(const void* machine, int64_t unit, int state) override {
        heard.push_back({unit, state});
        if (leaves) map->remove({machine, unit}, this);
    }
};

}  // namespace

// `addListenerToUnit`: GUID -1 is refused and an observer is
// on a unit's list once, however often it is added.
TL_TEST(an_observer_is_added_to_a_unit_once_and_never_to_guid_minus_one) {
    UnitObservers map;
    Listener a;
    map.add({nullptr, 7}, &a);
    map.add({nullptr, 7}, &a);
    TL_CHECK(map.observers_of({nullptr, 7}) != nullptr);
    TL_CHECK_EQ(map.observers_of({nullptr, 7})->size(), size_t{1});
    map.add({nullptr, -1}, &a);
    TL_CHECK(map.observers_of({nullptr, -1}) == nullptr);
}

// `unitBroadcastMessage`: each observer of the unit hears the
// state; only the unit broadcast is heard about; state 0 erases the entry.
TL_TEST(a_broadcast_reaches_the_units_observers_and_state_zero_forgets_the_unit) {
    UnitObservers map;
    Listener a;
    Listener b;
    map.add({nullptr, 1}, &a);
    map.add({nullptr, 1}, &b);
    map.add({nullptr, 2}, &b);
    map.broadcast({nullptr, 1}, tl::core::unit_state::kDiedOfEffect);
    TL_CHECK_EQ(a.heard.size(), size_t{1});
    TL_CHECK_EQ(b.heard.size(), size_t{1});
    TL_CHECK_EQ(b.heard[0].unit, int64_t{1});
    TL_CHECK_EQ(b.heard[0].state, tl::core::unit_state::kDiedOfEffect);
    map.broadcast({nullptr, 1}, tl::core::unit_state::kDestroyed);
    TL_CHECK(map.observers_of({nullptr, 1}) == nullptr);
    TL_CHECK(map.observers_of({nullptr, 2}) != nullptr);
}

// `TArrayList::remove` moves the last entry into the hole and the broadcast
// walks by index, so an observer leaving as it hears skips the last one. Kept.
TL_TEST(an_observer_leaving_mid_broadcast_makes_the_last_one_miss_it) {
    UnitObservers map;
    Listener first;
    Listener second;
    Listener last;
    first.map = &map;
    first.leaves = true;
    map.add({nullptr, 5}, &first);
    map.add({nullptr, 5}, &second);
    map.add({nullptr, 5}, &last);
    map.broadcast({nullptr, 5}, tl::core::unit_state::kDied);
    TL_CHECK_EQ(first.heard.size(), size_t{1});
    TL_CHECK_EQ(second.heard.size(), size_t{1});
    TL_CHECK_EQ(last.heard.size(), size_t{0});
    // `last` took `first`'s place.
    const std::vector<UnitObserver*>* left = map.observers_of({nullptr, 5});
    TL_CHECK(left != nullptr);
    TL_CHECK_EQ(left->size(), size_t{2});
    TL_CHECK((*left)[0] == &last);
}

// `removeListenerFromUnits`: an observer that goes away comes
// off every unit's list, and the lists stay.
TL_TEST(an_observer_going_away_leaves_every_units_list) {
    UnitObservers map;
    Listener a;
    Listener b;
    map.add({nullptr, 1}, &a);
    map.add({nullptr, 2}, &a);
    map.add({nullptr, 2}, &b);
    map.remove_all(&a);
    TL_CHECK_EQ(map.observers_of({nullptr, 1})->size(), size_t{0});
    TL_CHECK_EQ(map.observers_of({nullptr, 2})->size(), size_t{1});
    map.broadcast({nullptr, 1}, tl::core::unit_state::kDied);
    TL_CHECK(a.heard.empty());
}

namespace {

tl::core::Effect timed_effect(int type, float duration) {
    tl::core::Effect effect;
    effect.name = "TEST";
    effect.type = type;
    effect.duration = duration;
    return effect;
}

/// The skills that asked to watch the target, by name.
struct Watch {
    std::vector<std::string> asked;
};

tl::core::SkillCastContext watching(tl::core::EffectManager& target, Watch& watch,
                                    const tl::core::Affix* affix = nullptr) {
    tl::core::SkillCastContext context;
    context.target = &target;
    context.affix = [affix](const std::string&) { return affix; };
    context.observe_target = [&watch](const std::string& skill) { watch.asked.push_back(skill); };
    return context;
}

}  // namespace

// `applyAffixesAndEffects`: a timed `DAMAGE`, `HP RECHARGE` or
// `HP RECHARGE PLAYER` effect makes the skill watch the unit.
TL_TEST(a_timed_damage_effect_makes_its_skill_watch_the_target) {
    tl::core::SkillEffectAndAffixes block;
    tl::core::SkillEffectAndAffixes::EffectGroup group;
    group.target = tl::core::SkillTargetFilter::Everybody;
    group.effects.push_back(timed_effect(tl::core::kEffectTypeDamage, 5.0f));
    block.effect_groups.push_back(group);

    tl::core::EffectManager target;
    Watch watch;
    TL_CHECK(tl::core::apply_affixes_and_effects(block, watching(target, watch), "Poison Blast"));
    TL_CHECK_EQ(watch.asked.size(), size_t{1});
    TL_CHECK_EQ(watch.asked[0], std::string("Poison Blast"));
}

// Untimed, or of another type, it does not.
TL_TEST(an_untimed_or_harmless_effect_does_not_make_its_skill_watch) {
    tl::core::SkillEffectAndAffixes block;
    tl::core::SkillEffectAndAffixes::EffectGroup group;
    group.target = tl::core::SkillTargetFilter::Everybody;
    group.effects.push_back(timed_effect(tl::core::kEffectTypeDamage, tl::core::kDurationAlways));
    group.effects.push_back(timed_effect(tl::core::kEffectTypeArmorBonus, 5.0f));
    block.effect_groups.push_back(group);

    tl::core::EffectManager target;
    Watch watch;
    tl::core::apply_affixes_and_effects(block, watching(target, watch), "Shield");
    TL_CHECK(watch.asked.empty());
}

// `CAffix::effectHasDuration`, after the affix is: a timed
// `HP RECHARGE` in an affix makes the skill watch the unit.
TL_TEST(an_affix_with_a_timed_hp_effect_makes_its_skill_watch_the_target) {
    tl::core::Affix affix;
    affix.name = "BLEED";
    affix.effects.push_back(timed_effect(tl::core::kEffectTypeHpRecharge, 3.0f));
    tl::core::SkillEffectAndAffixes block;
    tl::core::SkillEffectAndAffixes::AffixSpec spec;
    spec.name = "BLEED";
    spec.level = 1;
    spec.target = tl::core::SkillTargetFilter::Everybody;
    block.affix_specs.push_back(spec);

    tl::core::EffectManager target;
    Watch watch;
    tl::core::apply_affixes_and_effects(block, watching(target, watch, &affix), "Rend");
    TL_CHECK_EQ(watch.asked.size(), size_t{1});
    TL_CHECK_EQ(watch.asked[0], std::string("Rend"));
}

namespace {

tl::core::Effect harmful(int type, float value, tl::core::EffectActivation activation) {
    tl::core::Effect effect = timed_effect(type, 5.0f);
    effect.values[0] = value;
    effect.values[1] = value;
    effect.activation = activation;
    return effect;
}

}  // namespace

// `CCharacter::updateHP`: a death by effects credits the owner of the first
// owned `DAMAGE` effect.
TL_TEST(a_death_by_effect_credits_the_owner_of_the_first_damage_effect) {
    tl::core::EffectManager effects;
    const tl::core::EffectSource monster{true, &effects, 42};
    effects.add_effect(
        harmful(tl::core::kEffectTypeDamage, -5.0f, tl::core::EffectActivation::Dynamic));
    {
        const tl::core::EffectSourceScope owned(&effects, monster);
        effects.add_effect(
            harmful(tl::core::kEffectTypeDamage, -5.0f, tl::core::EffectActivation::Dynamic));
    }
    // The scope is over: what is held next is nobody's.
    TL_CHECK(!effects.applying_source().set);
    const tl::core::EffectSource peeked = effects.death_credit();
    TL_CHECK(peeked.set);
    TL_CHECK_EQ(peeked.node, int64_t{42});
    const tl::core::EffectSource credited = effects.take_death_credit();
    TL_CHECK(credited.set);
    TL_CHECK(credited.machine == &effects);
    TL_CHECK_EQ(credited.node, int64_t{42});
    // `setOwner(0, true)`: credited once.
    TL_CHECK(!effects.take_death_credit().set);
}

// An `HP RECHARGE` credits only while its `value(0)` is below 0 - a
// regeneration is nobody's kill; the player is credited as itself.
TL_TEST(a_draining_hp_recharge_credits_and_a_healing_one_does_not) {
    tl::core::EffectManager effects;
    const tl::core::EffectSource player{true, nullptr, 0};
    {
        const tl::core::EffectSourceScope owned(&effects, player);
        effects.add_effect(
            harmful(tl::core::kEffectTypeHpRecharge, 3.0f, tl::core::EffectActivation::Passive));
    }
    TL_CHECK(!effects.death_credit().set);
    {
        const tl::core::EffectSourceScope owned(&effects, player);
        effects.add_effect(harmful(tl::core::kEffectTypeHpRechargePlayer, -3.0f,
                                   tl::core::EffectActivation::Passive));
    }
    const tl::core::EffectSource credited = effects.take_death_credit();
    TL_CHECK(credited.player());
}

// The two lists are walked in turn and each can credit; the last credit is
// what `updateHP` returns. The third list (`TRANSFER`) is not walked.
TL_TEST(each_of_the_first_two_lists_can_credit_and_the_last_wins) {
    tl::core::EffectManager effects;
    {
        const tl::core::EffectSourceScope owned(&effects, {true, &effects, 1});
        effects.add_effect(
            harmful(tl::core::kEffectTypeDamage, -1.0f, tl::core::EffectActivation::Passive));
    }
    {
        const tl::core::EffectSourceScope owned(&effects, {true, &effects, 2});
        effects.add_effect(
            harmful(tl::core::kEffectTypeDamage, -1.0f, tl::core::EffectActivation::Dynamic));
    }
    {
        const tl::core::EffectSourceScope owned(&effects, {true, &effects, 3});
        effects.add_effect(
            harmful(tl::core::kEffectTypeDamage, -1.0f, tl::core::EffectActivation::Transfer));
    }
    TL_CHECK_EQ(effects.take_death_credit().node, int64_t{2});
    TL_CHECK(!effects.take_death_credit().set);
}

// A skill's apply hands its caster to every effect it puts on
// (`setOwner(effect, caster)`).
TL_TEST(a_skills_effects_are_owned_by_its_caster) {
    tl::core::SkillEffectAndAffixes block;
    tl::core::SkillEffectAndAffixes::EffectGroup group;
    group.target = tl::core::SkillTargetFilter::Everybody;
    group.effects.push_back(
        harmful(tl::core::kEffectTypeDamage, -2.0f, tl::core::EffectActivation::Dynamic));
    block.effect_groups.push_back(group);
    tl::core::EffectManager target;
    Watch watch;
    tl::core::SkillCastContext context = watching(target, watch);
    context.source = {true, &target, 7};
    tl::core::apply_affixes_and_effects(block, context, "Poison");
    TL_CHECK(!target.applying_source().set);
    TL_CHECK_EQ(target.take_death_credit().node, int64_t{7});
}
