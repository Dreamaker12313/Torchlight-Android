// Brightness and contrast, pinned against the console's ramp
// and its per-frame check, and the port's blend
// steps against the ramp they stand for.
#include <algorithm>
#include <cmath>

#include "core/display_gamma.h"
#include "core/settings_menu.h"
#include "test_support.h"

/// 257 / 256 is the identity: `i * 257`, 0 .. 65535.
TL_TEST(gamma_ramp_identity_at_257_256) {
    const auto ramp = tl::core::gamma_ramp(257, 256);
    for (int i = 0; i < 256; ++i) TL_CHECK(ramp[static_cast<size_t>(i)] == i * 257);
    TL_CHECK(tl::core::gamma_blend_steps(tl::core::gamma_line(257, 256)).empty());
}

/// The defaults: slope 290 * 210 / 256 = 237.890625, middle 290 * 128 = 37120.
TL_TEST(gamma_ramp_defaults_lift_black) {
    const auto ramp = tl::core::gamma_ramp(290, 210);
    TL_CHECK(ramp[0] == 6670);
    TL_CHECK(ramp[128] == 37120);
    TL_CHECK(ramp[247] == 65428);  // 28308.984375 + 37120, truncated
    TL_CHECK(ramp[248] == 65535);  // 65666.875, held
    TL_CHECK(ramp[255] == 65535);
}

/// Held at 0 below: brightness 100, contrast 400 starts 7200 under.
TL_TEST(gamma_ramp_holds_at_zero) {
    const auto ramp = tl::core::gamma_ramp(100, 400);
    TL_CHECK(ramp[0] == 0);
    TL_CHECK(ramp[46] == 0);    // -12812.5 + 12800 < 0
    TL_CHECK(ramp[47] == 143);  // (47 - 128) * 156.25 + 12800 = 143.75
    TL_CHECK(ramp[128] == 12800);
}

/// The defaults are one blend: `0.926 x + 0.102`.
TL_TEST(gamma_defaults_are_one_multiply_add) {
    const auto steps = tl::core::gamma_blend_steps(tl::core::gamma_line(290, 210));
    TL_CHECK(steps.size() == 1);
    TL_CHECK(steps[0].blend == tl::core::GammaBlend::MultiplyAdd);
    TL_CHECK(std::fabs(steps[0].factor - 0.92564f) < 1e-4f);
    TL_CHECK(std::fabs(steps[0].offset - 0.10178f) < 1e-4f);
}

/// Over every slider value (brightness 100..600 by 10, contrast 100..400 by 5) the steps give
/// the ramp's entry, clamps and all, in at most four blends.
TL_TEST(gamma_steps_follow_the_ramp_over_the_sliders) {
    size_t worst_steps = 0;
    float worst_error = 0.0f;
    for (int b = 100; b <= 600; b += 10) {
        for (int c = 100; c <= 400; c += 5) {
            const auto ramp = tl::core::gamma_ramp(b, c);
            const auto steps = tl::core::gamma_blend_steps(tl::core::gamma_line(b, c));
            worst_steps = std::max(worst_steps, steps.size());
            for (int i = 0; i < 256; ++i) {
                const float want = static_cast<float>(ramp[static_cast<size_t>(i)]) / 65535.0f;
                const float got =
                    tl::core::apply_gamma_steps(steps, static_cast<float>(i) / 255.0f);
                worst_error = std::max(worst_error, std::fabs(got - want));
            }
        }
    }
    TL_CHECK(worst_steps <= 4);
    TL_CHECK(worst_error < 1e-4f);
}

/// The check sends on the first frame (the cache starts at zero), not again
/// for the same ints, and again when a truncated value moves.
TL_TEST(gamma_watch_sends_on_change_of_the_ints) {
    tl::core::GameSettings settings;
    settings.brightness = tl::core::kConsoleBrightness;
    settings.contrast = tl::core::kConsoleContrast;
    tl::core::GammaWatch watch;
    TL_CHECK(watch.changed(settings));
    TL_CHECK(watch.sent().brightness == 290 && watch.sent().contrast == 210);
    TL_CHECK(!watch.changed(settings));
    settings.brightness = 290.9f;  // still 290
    TL_CHECK(!watch.changed(settings));
    settings.brightness = 289.99997f;  // 289
    TL_CHECK(watch.changed(settings));
    TL_CHECK(watch.sent().brightness == 289);
    settings.contrast = 215.0f;
    TL_CHECK(watch.changed(settings));
}
