// Unit tests for the two hands' part of a character's attack (core/combat.h):
// `CCharacter::attackRange`, `meleeRange`, `rangedRange`,
// `selectAttack`'s choice and the
// hand step of `attack`. No game data is read.

#include "core/combat.h"

#include <cmath>
#include "test_support.h"

namespace {

using tl::core::AttackDescription;
using tl::core::AttackHand;
using tl::core::AttackHandStep;
using tl::core::HeldHand;
using tl::core::HeldHands;
using tl::core::UnitStats;

UnitStats body(float reach, float scale) {
    UnitStats stats;
    stats.reach = reach;
    stats.scale = scale;
    stats.range_multiplier = 1.0f;
    stats.radius = 0.0f;
    return stats;
}

HeldHand sword(float range) {
    HeldHand hand;
    hand.held = true;
    hand.weapon = true;
    hand.melee = true;
    hand.range = range;
    return hand;
}

HeldHand pistol(float range) {
    HeldHand hand;
    hand.held = true;
    hand.weapon = true;
    hand.ranged = true;
    hand.range = range;
    return hand;
}

}  // namespace

// `fma(reach, scale, range)`, then 0.2 - not `reach * scale + 0.2
// + range`, which rounds three times and lands one unit higher here.
TL_TEST(attack_range_is_fused) {
    const UnitStats stats = body(1.2f, 1.3538057804107666f);
    AttackDescription attack;
    attack.range = 0.6f;
    const float range = tl::core::attack_range(stats, attack);
    TL_CHECK_EQ(range, std::fma(1.2f, 1.3538057804107666f, 0.6f) + 0.2f);
    TL_CHECK(range != 1.2f * 1.3538057804107666f + 0.2f + 0.6f);
}

// Two melee weapons: the shortest of the attack's and the two hands' ranges;
// with a ranged weapon in either hand, the longest.
TL_TEST(attack_range_of_two_weapons) {
    const UnitStats stats = body(1.0f, 1.0f);
    AttackDescription attack;
    attack.range = 1.0f;
    HeldHands hands;
    hands.using_weapon = true;
    hands.right = sword(1.5f);
    hands.left = sword(0.5f);
    TL_CHECK_EQ(tl::core::attack_range(stats, attack, &hands), std::fma(1.0f, 1.0f, 0.5f) + 0.2f);
    hands.left = pistol(4.0f);
    TL_CHECK_EQ(tl::core::attack_range(stats, attack, &hands), std::fma(1.0f, 1.0f, 4.0f) + 0.2f);
    // A shield (not a `Weapon`) in the left hand: the attack's own range.
    hands.left = HeldHand{};
    hands.left.held = true;
    hands.left.range = 0.1f;
    TL_CHECK_EQ(tl::core::attack_range(stats, attack, &hands), std::fma(1.0f, 1.0f, 1.0f) + 0.2f);
    // Bare hands (`+0x3ec` empty): the attack's own range.
    hands.left = sword(0.5f);
    hands.using_weapon = false;
    TL_CHECK_EQ(tl::core::attack_range(stats, attack, &hands), std::fma(1.0f, 1.0f, 1.0f) + 0.2f);
}

// `meleeRange` lowers by the melee hands only, `rangedRange` raises by the
// ranged hands only; neither needs both hands full.
TL_TEST(melee_and_ranged_ranges) {
    const UnitStats stats = body(1.0f, 1.0f);
    AttackDescription attack;
    attack.range = 1.0f;
    HeldHands hands;
    hands.using_weapon = true;
    hands.right = sword(0.75f);
    hands.left = pistol(6.0f);
    TL_CHECK_EQ(tl::core::melee_range(stats, attack, hands), std::fma(1.0f, 1.0f, 0.75f) + 0.2f);
    TL_CHECK_EQ(tl::core::ranged_range(stats, attack, hands), std::fma(1.0f, 1.0f, 6.0f) + 0.2f);
    hands.right = HeldHand{};
    TL_CHECK_EQ(tl::core::melee_range(stats, attack, hands), std::fma(1.0f, 1.0f, 1.0f) + 0.2f);
    TL_CHECK_EQ(tl::core::ranged_range(stats, attack, hands), std::fma(1.0f, 1.0f, 6.0f) + 0.2f);
}

// `selectAttack`: the left hand alone, the alternation, and modes 1 and 2.
TL_TEST(select_attack_hand) {
    HeldHands hands;
    TL_CHECK(tl::core::select_attack_hand(hands, 0, false) == AttackHand::Own);
    hands.left = sword(1.0f);
    TL_CHECK(tl::core::select_attack_hand(hands, 0, false) == AttackHand::Left);
    hands.right = sword(1.0f);
    TL_CHECK(tl::core::select_attack_hand(hands, 0, false) == AttackHand::Right);
    TL_CHECK(tl::core::select_attack_hand(hands, 0, true) == AttackHand::Left);
    TL_CHECK(tl::core::select_attack_hand(hands, 1, false) == AttackHand::Left);
    TL_CHECK(tl::core::select_attack_hand(hands, 2, true) == AttackHand::Right);
    hands.left = pistol(5.0f);
    TL_CHECK(tl::core::select_attack_hand(hands, 2, false) == AttackHand::Left);
    TL_CHECK(tl::core::select_attack_hand(hands, 1, true) == AttackHand::Right);
    // Any other mode is the right hand's.
    TL_CHECK(tl::core::select_attack_hand(hands, 3, true) == AttackHand::Right);
    // A shield in the left hand never swings.
    hands.left = HeldHand{};
    hands.left.held = true;
    TL_CHECK(tl::core::select_attack_hand(hands, 0, true) == AttackHand::Right);
    hands.right = HeldHand{};
    TL_CHECK(tl::core::select_attack_hand(hands, 0, true) == AttackHand::Own);
}

// `attack`: two weapons of a kind keep the attack in use; one of each is mixed.
TL_TEST(attack_hand_step) {
    HeldHands hands;
    hands.right = sword(1.0f);
    TL_CHECK(tl::core::attack_hand_step(hands) == AttackHandStep::SelectAny);
    hands.left = sword(1.0f);
    TL_CHECK(tl::core::attack_hand_step(hands) == AttackHandStep::Keep);
    hands.left = pistol(5.0f);
    TL_CHECK(tl::core::attack_hand_step(hands) == AttackHandStep::Mixed);
    TL_CHECK(tl::core::attack_melee_hand_is_right(hands));
    hands.right = pistol(5.0f);
    TL_CHECK(tl::core::attack_hand_step(hands) == AttackHandStep::Keep);
    hands.right = pistol(5.0f);
    hands.left = sword(1.0f);
    TL_CHECK(!tl::core::attack_melee_hand_is_right(hands));
}

// The distance in `inAttackRange` and `inStrikeRange` is `sqrt(fma(dz, dz,
// dx * dx))` and the strike reach `fma(fma(reach, scale, strike) + 0.2,
// multiplier, bonus)`.
TL_TEST(strike_reach_is_fused) {
    UnitStats stats = body(1.2f, 1.3538057804107666f);
    AttackDescription attack;
    attack.strike_range = 0.6f;
    const float reach = std::fma(1.2f, 1.3538057804107666f, 0.6f) + 0.2f;
    const tl::core::Vec3 here{0.0f, 0.0f, 0.0f};
    TL_CHECK(tl::core::in_strike_range(stats, attack, here, {reach, 0.0f, 0.0f}, 0.0f));
    TL_CHECK(!tl::core::in_strike_range(stats, attack, here,
                                        {std::nextafter(reach, 10.0f), 0.0f, 0.0f}, 0.0f));
}
