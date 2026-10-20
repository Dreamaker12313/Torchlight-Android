// Unit tests for AIFlags (CAIFlag, CAIFlagManager, CAIManager flag forwarders).
// Everything is built in code; no game data files are accessed.

#include "core/ai_flags.h"

#include <cstdint>
#include <limits>
#include "test_support.h"

namespace {

using tl::core::AIFlag;
using tl::core::AIFlags;
using tl::core::AIFlagsHooks;
using tl::core::kAIFlagCount;

}  // namespace

// Type order: enum numeric values match original EAIFLAG_TYPES order (0..6).
TL_TEST(ai_flags_type_order_and_values) {
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::Aware), 0u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::Berserk), 1u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::CannotInterrupt), 2u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::Frighten), 3u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::NoLineOfSight), 4u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::NeverChangeTarget), 5u);
    TL_CHECK_EQ(static_cast<uint32_t>(AIFlag::CannotTarget), 6u);
    TL_CHECK_EQ(kAIFlagCount, 7u);
}

// Starts clear: a newly constructed AIFlags has none of the seven flags set,
// and remaining time for each flag is 0.0f.
TL_TEST(ai_flags_starts_clear) {
    AIFlags flags;
    for (uint32_t i = 0; i < kAIFlagCount; ++i) {
        const auto flag = static_cast<AIFlag>(i);
        TL_CHECK(!flags.has(flag));
        TL_CHECK_EQ(flags.time(flag), 0.0f);
    }
    TL_CHECK(flags.formation_units().empty());
}

// add(AWARE, 3.0f), then update(1.0f) three times: active after two, unset after the third.
TL_TEST(ai_flags_add_and_expire) {
    AIFlags flags;
    flags.add(AIFlag::Aware, 3.0f);

    TL_CHECK(flags.has(AIFlag::Aware));
    TL_CHECK_EQ(flags.time(AIFlag::Aware), 3.0f);

    // Step 1: 1.0s elapsed -> 2.0s left, still active
    flags.update(1.0f);
    TL_CHECK(flags.has(AIFlag::Aware));
    TL_CHECK_EQ(flags.time(AIFlag::Aware), 2.0f);

    // Step 2: 1.0s elapsed -> 1.0s left, still active
    flags.update(1.0f);
    TL_CHECK(flags.has(AIFlag::Aware));
    TL_CHECK_EQ(flags.time(AIFlag::Aware), 1.0f);

    // Step 3: 1.0s elapsed -> 0.0s left, expired and unset
    flags.update(1.0f);
    TL_CHECK(!flags.has(AIFlag::Aware));
    TL_CHECK_EQ(flags.time(AIFlag::Aware), 0.0f);
}

// Longer wins, shorter does not shorten:
// add(X, 5), add(X, 2) leaves 5; add(X, 8) raises it to 8.
TL_TEST(ai_flags_longer_wins_shorter_does_not_shorten) {
    AIFlags flags;
    flags.add(AIFlag::CannotInterrupt, 5.0f);
    TL_CHECK_EQ(flags.time(AIFlag::CannotInterrupt), 5.0f);

    // Shorter duration does not shorten
    flags.add(AIFlag::CannotInterrupt, 2.0f);
    TL_CHECK_EQ(flags.time(AIFlag::CannotInterrupt), 5.0f);

    // Longer duration raises it
    flags.add(AIFlag::CannotInterrupt, 8.0f);
    TL_CHECK_EQ(flags.time(AIFlag::CannotInterrupt), 8.0f);
}

// flagSet fires once per rise from unset (<= 0) to set; again only after expiry and re-add.
TL_TEST(ai_flags_flag_set_fires_once_per_rise) {
    int aware_spread_count = 0;
    AIFlagsHooks hooks;
    hooks.on_spread_aware = [&](int64_t /*id*/, float /*dur*/) { ++aware_spread_count; };

    AIFlags flags(hooks);
    flags.add_formation_unit(101);

    // Initial rise: fires once
    flags.add(AIFlag::Aware, 3.0f);
    TL_CHECK_EQ(aware_spread_count, 1);

    // Re-add with larger duration while still active: does NOT fire again
    flags.add(AIFlag::Aware, 5.0f);
    TL_CHECK_EQ(aware_spread_count, 1);

    // Age out to 0.0f
    flags.update(5.0f);
    TL_CHECK(!flags.has(AIFlag::Aware));
    TL_CHECK_EQ(aware_spread_count, 1);

    // Re-add after expiration: second rise, fires again
    flags.add(AIFlag::Aware, 2.0f);
    TL_CHECK_EQ(aware_spread_count, 2);
}

// Zero-duration add on an unset flag fires flagSet while the flag stays unset (old time
// <= 0 triggers the hook; duration 0 does not raise the time).
TL_TEST(ai_flags_zero_duration_add_fires_hook_while_flag_stays_unset) {
    int aware_spread_count = 0;
    AIFlagsHooks hooks;
    hooks.on_spread_aware = [&](int64_t /*id*/, float /*dur*/) { ++aware_spread_count; };

    AIFlags flags(hooks);
    flags.add_formation_unit(42);

    // Flag is unset; add with 0.0f
    flags.add(AIFlag::Aware, 0.0f);

    // Hook fires despite duration 0
    TL_CHECK_EQ(aware_spread_count, 1);
    // Flag stays unset
    TL_CHECK(!flags.has(AIFlag::Aware));
    TL_CHECK_EQ(flags.time(AIFlag::Aware), 0.0f);
}

// removeAIFlag quirk: flagRemoved runs only when the old time was
// <= 0 - so on expiry and on removing an unset flag, never on removing a live one.
TL_TEST(ai_flags_remove_branch_quirk) {
    int berserk_end_count = 0;
    AIFlagsHooks hooks;
    hooks.on_berserk_end = [&]() { ++berserk_end_count; };

    AIFlags flags(hooks);

    // Case 1: Manual remove on a live flag -> flagRemoved is SKIPPED
    flags.add(AIFlag::Berserk, 10.0f);
    TL_CHECK(flags.has(AIFlag::Berserk));
    flags.remove(AIFlag::Berserk);
    TL_CHECK(!flags.has(AIFlag::Berserk));
    TL_CHECK_EQ(flags.time(AIFlag::Berserk), 0.0f);
    TL_CHECK_EQ(berserk_end_count, 0);  // Quirk: did NOT fire on live removal!

    // Case 2: Manual remove on an already-unset flag (old time <= 0) -> flagRemoved FIRES
    flags.remove(AIFlag::Berserk);
    TL_CHECK_EQ(berserk_end_count, 1);

    // Case 3: Expiration via update() -> flagRemoved FIRES
    flags.add(AIFlag::Berserk, 2.0f);
    TL_CHECK(flags.has(AIFlag::Berserk));
    flags.update(1.0f);
    TL_CHECK(flags.has(AIFlag::Berserk));
    TL_CHECK_EQ(berserk_end_count, 1);  // Still alive, no new removal

    flags.update(1.0f);  // Expires: time <= 0, remove called
    TL_CHECK(!flags.has(AIFlag::Berserk));
    TL_CHECK_EQ(berserk_end_count, 2);  // Fired on natural expiration!
}

// Formation spread between two mutually linked units terminates: the already active one's
// hook does not fire again.
TL_TEST(ai_flags_formation_spread_terminates) {
    AIFlags unit_a;
    AIFlags unit_b;

    unit_a.add_formation_unit(2);
    unit_b.add_formation_unit(1);

    int a_to_b_calls = 0;
    int b_to_a_calls = 0;

    AIFlagsHooks hooks_a;
    hooks_a.on_spread_aware = [&](int64_t target_id, float duration) {
        if (target_id == 2) {
            ++a_to_b_calls;
            unit_b.add(AIFlag::Aware, duration);
        }
    };
    unit_a.set_hooks(hooks_a);

    AIFlagsHooks hooks_b;
    hooks_b.on_spread_aware = [&](int64_t target_id, float duration) {
        if (target_id == 1) {
            ++b_to_a_calls;
            unit_a.add(AIFlag::Aware, duration);
        }
    };
    unit_b.set_hooks(hooks_b);

    // Trigger AWARE on unit A:
    unit_a.add(AIFlag::Aware, 3.0f);

    // A spread to B (1 call), B spread to A (1 call), A was already set so no recursion
    TL_CHECK_EQ(a_to_b_calls, 1);
    TL_CHECK_EQ(b_to_a_calls, 1);

    TL_CHECK(unit_a.has(AIFlag::Aware));
    TL_CHECK_EQ(unit_a.time(AIFlag::Aware), 3.0f);
    TL_CHECK(unit_b.has(AIFlag::Aware));
    TL_CHECK_EQ(unit_b.time(AIFlag::Aware), 3.0f);
}

// Berserk start hook: fires once when BERSERK is added to an unset flag.
TL_TEST(ai_flags_berserk_start_hook) {
    int start_count = 0;
    AIFlagsHooks hooks;
    hooks.on_berserk_start = [&]() { ++start_count; };

    AIFlags flags(hooks);
    flags.add(AIFlag::Berserk, 5.0f);
    TL_CHECK_EQ(start_count, 1);

    // Re-add while active: does not refire
    flags.add(AIFlag::Berserk, 10.0f);
    TL_CHECK_EQ(start_count, 1);
}

// `CAIFlag::update` keeps a flag only while `time > 0` (`seta`), so a NaN
// countdown expires and runs `flagRemoved`; a `time <= 0` test would keep it.
TL_TEST(ai_flags_nan_time_expires) {
    int berserk_end_count = 0;
    AIFlagsHooks hooks;
    hooks.on_berserk_end = [&]() { ++berserk_end_count; };
    AIFlags flags(hooks);
    flags.add(AIFlag::Berserk, 1.0f);
    flags.update(std::numeric_limits<float>::quiet_NaN());
    TL_CHECK(!flags.has(AIFlag::Berserk));
    TL_CHECK_EQ(flags.time(AIFlag::Berserk), 0.0f);
    TL_CHECK_EQ(berserk_end_count, 1);
}
