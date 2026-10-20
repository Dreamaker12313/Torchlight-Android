// What `CCharacter::rollAttack` puts on the one it struck -
// the `KNOCKBACK` and `DEGRADEARMOR` effects and the attacker's `TRANSFER`
// effects (`CEffectManager::transferEffects`) - and the two
// ways the console makes an effect for itself: `CEffect::clone`
// and the value constructor.
#include "core/combat.h"
#include "core/effect_manager.h"
#include "core/effects.h"
#include "test_support.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace {

using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;

/// A manager that works values out (`calculateBaseValue` needs the curves,
/// none of which these effects name).
EffectManager manager_with(const tl::core::StatBook& graphs) {
    EffectManager manager;
    EffectManager::Context context;
    context.graphs = &graphs;
    manager.set_context(context);
    return manager;
}

Effect transfer_effect(int type, float value, float duration) {
    Effect effect;
    effect.type = type;
    effect.activation = EffectActivation::Transfer;
    effect.values[0] = value;
    effect.duration = duration;
    effect.name = "CHAMPIONCOLDSLOW";
    effect.exclusive = true;
    effect.save = true;
    effect.unit_theme = "FROZEN";
    return effect;
}

}  // namespace

TL_TEST(clone_effect_copies_what_the_effect_carries) {
    // `CEffect::clone`: `+0x21`, the authored `DURATION`,
    // goes across as it.
    Effect authored;
    authored.duration = tl::core::kDurationInstant;
    authored.authored_duration = true;
    authored.name = "SLOW";
    authored.use_owner_level = true;
    authored.values = {1.0f, 2.0f, 3.0f, 4.0f, 0.0f};
    const Effect copy = tl::core::clone_effect(authored);
    TL_CHECK(copy.authored_duration);
    TL_CHECK(copy.use_owner_level);
    TL_CHECK(copy.name == "SLOW");
    TL_CHECK(copy.values[3] == 4.0f);
}

TL_TEST(value_effect_is_init_values_with_type_duration_and_number) {
    const Effect effect = tl::core::value_effect(0x15, 2.0f, -30.0f, EffectActivation::Dynamic);
    TL_CHECK(effect.type == 0x15);
    TL_CHECK(effect.duration == 2.0f);
    TL_CHECK(effect.values[0] == -30.0f);
    TL_CHECK(effect.values[1] == 0.0f);
    TL_CHECK(effect.activation == EffectActivation::Dynamic);
    TL_CHECK(!effect.authored_duration);
    // `initValues`' damage type is 1, not `PHYSICAL`.
    TL_CHECK(effect.damage_type == tl::core::DamageType::Magical);
    TL_CHECK(!effect.b30);
    TL_CHECK(effect.f38 == 0.0f);
    TL_CHECK(effect.display);
    TL_CHECK(effect.i3c == 100);
    TL_CHECK(effect.name.empty());
}

TL_TEST(knock_back_effect_lasts_half_the_force_and_points_on_the_ground) {
    const Effect effect =
        tl::core::knock_back_effect(10.0f, {1.0f, 0.0f, 1.0f}, {4.0f, 7.0f, 5.0f});
    TL_CHECK(effect.type == tl::core::kEffectTypeKnockBackEffect);
    TL_CHECK(effect.name == "KNOCKBACK");
    TL_CHECK(effect.activation == EffectActivation::Dynamic);
    TL_CHECK(effect.duration == 7.0f);                          // (int)(10 * 0.5 + 2)
    TL_CHECK(effect.values[0] == std::fma(10.0f, 0.1f, 2.0f));  // 3
    TL_CHECK(std::fabs(effect.direction[0] - 0.6f) < 1e-6f);
    TL_CHECK(effect.direction[1] == 0.0f);
    TL_CHECK(std::fabs(effect.direction[2] - 0.8f) < 1e-6f);
    // The duration is truncated, not rounded: 1.5 * 0.5 + 2 = 2.75.
    TL_CHECK(tl::core::knock_back_effect(1.5f, {}, {1.0f, 0.0f, 0.0f}).duration == 2.0f);
    // No length to divide by: the difference is kept as it is.
    const Effect still = tl::core::knock_back_effect(1.0f, {2.0f, 0.0f, 2.0f}, {2.0f, 3.0f, 2.0f});
    TL_CHECK(still.direction[0] == 0.0f && still.direction[2] == 0.0f);
}

TL_TEST(degrade_armor_effect_is_ten_for_the_truncated_seconds) {
    const Effect effect = tl::core::degrade_armor_effect(3.9f);
    TL_CHECK(effect.type == tl::core::kEffectTypeDegradeArmorEffect);
    TL_CHECK(effect.name == "DEGRADEARMOR");
    TL_CHECK(effect.duration == 3.0f);
    TL_CHECK(effect.values[0] == 10.0f);
    TL_CHECK(!effect.exclusive);
}

TL_TEST(transfer_effects_makes_each_transfer_of_the_type_again) {
    const tl::core::StatBook graphs;
    EffectManager source = manager_with(graphs);
    source.add_effect(transfer_effect(0x15, -30.0f, 2.0f));
    Effect dynamic = transfer_effect(0x15, -50.0f, 2.0f);
    dynamic.activation = EffectActivation::Dynamic;  // not on the transfer list
    source.add_effect(dynamic);
    source.add_effect(transfer_effect(0x16, -30.0f, 2.0f));

    std::vector<Effect> made;
    source.transfer_effects(
        0x15, [](const Effect&) { return true; }, [&](Effect& e) { made.push_back(e); });
    TL_CHECK(made.size() == 1);
    TL_CHECK(made[0].type == 0x15);
    TL_CHECK(made[0].values[0] == -30.0f);  // the applied value
    TL_CHECK(made[0].duration == 2.0f);
    TL_CHECK(made[0].activation == EffectActivation::Dynamic);
    TL_CHECK(made[0].exclusive);
    TL_CHECK(!made[0].save);
    TL_CHECK(made[0].unit_theme == "FROZEN");
    TL_CHECK(made[0].name.empty());

    // The unit's `isEffectValidForUnit` is asked of the held effect.
    made.clear();
    source.transfer_effects(
        0x15, [](const Effect& held) { return held.name != "CHAMPIONCOLDSLOW"; },
        [&](Effect& e) { made.push_back(e); });
    TL_CHECK(made.empty());
}

TL_TEST(roll_attack_transfers_type_by_type_across_its_sources) {
    const tl::core::StatBook graphs;
    EffectManager own = manager_with(graphs);
    own.add_effect(transfer_effect(5, 1.0f, 2.0f));
    own.add_effect(transfer_effect(3, 2.0f, 2.0f));
    EffectManager worn = manager_with(graphs);
    worn.add_effect(transfer_effect(3, 3.0f, 2.0f));
    EffectManager sets = manager_with(graphs);
    sets.add_effect(transfer_effect(5, 4.0f, 2.0f));

    tl::core::AttackEffectSources sources;
    sources.own = &own;
    sources.has_inventory = true;
    sources.inventory = &sets;
    sources.worn[7] = &worn;
    std::vector<float> order;
    tl::core::transfer_attack_effects(
        sources, [](const Effect&) { return true; },
        [&](Effect& e) { order.push_back(e.values[0]); });
    TL_CHECK(order == (std::vector<float>{2.0f, 3.0f, 1.0f, 4.0f}));

    // Without an inventory only the character's own go across.
    sources.has_inventory = false;
    order.clear();
    tl::core::transfer_attack_effects(
        sources, [](const Effect&) { return true; },
        [&](Effect& e) { order.push_back(e.values[0]); });
    TL_CHECK(order == (std::vector<float>{2.0f, 1.0f}));
}

TL_TEST(set_owner_level_takes_the_owners_level_only_when_asked) {
    // `CEffect::setOwner`: `USEOWNERLEVEL`, an owner.
    Effect effect;
    effect.level = 4;
    tl::core::set_owner_level(effect, 12);
    TL_CHECK(effect.level == 4);  // not `USEOWNERLEVEL`
    effect.use_owner_level = true;
    tl::core::set_owner_level(effect, std::nullopt);
    TL_CHECK(effect.level == 4);  // no owner
    tl::core::set_owner_level(effect, 12);
    TL_CHECK(effect.level == 12);
    tl::core::set_owner_level(effect, -1);
    TL_CHECK(effect.level == 0);
    tl::core::set_owner_level(effect, 1000);
    TL_CHECK(effect.level == 1000);
    tl::core::set_owner_level(effect, 1001);
    TL_CHECK(effect.level == 0);
}

TL_TEST(set_duration_override_reaches_one_effect_per_group) {
    // `CSkillEffectAndAffixes::setDurationOverride`: the
    // group's counter picks the effect.
    tl::core::SkillEffectAndAffixes block;
    block.affix_specs.resize(2);
    block.affix_specs[0].duration = 1.0f;
    block.effect_groups.resize(3);
    block.effect_groups[0].effects.resize(2);  // group 0: effect 0
    block.effect_groups[1].effects.resize(3);  // group 1: effect 1
    block.effect_groups[2].effects.resize(1);  // group 2: past the end, effect 0
    for (auto& group : block.effect_groups) {
        for (Effect& effect : group.effects) effect.duration = 7.0f;
    }
    block.effect_groups[1].effects[1].authored_duration = true;  // keeps its own
    tl::core::set_duration_override(block, 4.5f);
    TL_CHECK(block.affix_specs[0].duration == 4.5f);
    TL_CHECK(block.affix_specs[1].duration == 4.5f);
    TL_CHECK(block.effect_groups[0].effects[0].duration == 4.5f);
    TL_CHECK(block.effect_groups[0].effects[1].duration == 7.0f);
    TL_CHECK(block.effect_groups[1].effects[0].duration == 7.0f);
    TL_CHECK(block.effect_groups[1].effects[1].duration == 7.0f);
    TL_CHECK(block.effect_groups[1].effects[2].duration == 7.0f);
    TL_CHECK(block.effect_groups[2].effects[0].duration == 4.5f);
}

TL_TEST(get_effect_reads_the_passive_list_then_the_dynamic_one) {
    const tl::core::StatBook graphs;
    EffectManager manager = manager_with(graphs);
    Effect transfer = transfer_effect(0x3c, 1.0f, 2.0f);
    manager.add_effect(transfer);  // the TRANSFER list is not read
    TL_CHECK(manager.get_effect(0x3c) == nullptr);
    Effect dynamic = transfer;
    dynamic.activation = EffectActivation::Dynamic;
    dynamic.values[2] = 5.0f;
    manager.add_effect(dynamic);
    Effect passive = dynamic;
    passive.activation = EffectActivation::Passive;
    passive.values[2] = 9.0f;
    passive.name = "OTHER";
    manager.add_effect(passive);
    const tl::core::LiveEffect* found = manager.get_effect(0x3c);
    TL_CHECK(found != nullptr && found->effect.values[2] == 9.0f);
}

TL_TEST(missile_reflect_share_takes_the_larger_ends_across_the_worn_items) {
    const tl::core::StatBook graphs;
    tl::core::RunicRandom rolls(1234);
    std::array<const EffectManager*, 12> worn{};
    // No chance: nothing reflects, and nothing is drawn.
    TL_CHECK(tl::core::dmg_to_reflect_from_missile(0.0f, nullptr, false, worn, rolls) == 0.0f);
    // A certain chance and no ends anywhere: the whole of it.
    TL_CHECK(tl::core::dmg_to_reflect_from_missile(100.0f, nullptr, false, worn, rolls) == 1.0f);
    // The character's own 20..0, an item's 10..0: the low end, 20 -> 0.2.
    EffectManager own = manager_with(graphs);
    Effect reflect = transfer_effect(tl::core::kEffectTypeMissileReflect, 100.0f, 2.0f);
    reflect.activation = EffectActivation::Passive;
    reflect.values[2] = 20.0f;
    own.add_effect(reflect);
    EffectManager item = manager_with(graphs);
    reflect.values[2] = 10.0f;
    item.add_effect(reflect);
    worn[3] = &item;
    TL_CHECK(tl::core::dmg_to_reflect_from_missile(100.0f, &own, true, worn, rolls) ==
             20.0f * 0.01f);
    // An item's high end of 50: a roll between 20 and 50.
    reflect.values[3] = 50.0f;
    EffectManager high = manager_with(graphs);
    high.add_effect(reflect);
    worn[5] = &high;
    const float share = tl::core::dmg_to_reflect_from_missile(100.0f, &own, true, worn, rolls);
    TL_CHECK(0.2f <= share && share <= 0.5f);
    // The worn items count only with an inventory.
    TL_CHECK(tl::core::dmg_to_reflect_from_missile(100.0f, &own, false, worn, rolls) ==
             20.0f * 0.01f);
}

TL_TEST(missile_reflect_visual_holds_the_share) {
    const Effect effect = tl::core::missile_reflect_visual_effect(0.35f);
    TL_CHECK(effect.type == tl::core::kEffectTypeMissileReflectVisual);
    TL_CHECK(effect.duration == 0.3f);
    TL_CHECK(effect.values[0] == 1.0f);
    TL_CHECK(effect.values[2] == 0.35f);
    TL_CHECK(effect.activation == EffectActivation::Passive);
    TL_CHECK(effect.b30);
}
