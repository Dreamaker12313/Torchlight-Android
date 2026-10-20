#include "core/display_gamma.h"

#include <algorithm>
#include <cmath>

#include "core/settings_menu.h"

namespace tl::core {

namespace {

constexpr float kRampStep = 0.00390625f;  // 1/256
constexpr float kRampMiddle = 128.0f;
constexpr float kRampTop = 65535.0f;

/// Below this a line is the identity: the ramp at 257 / 256 is exactly
/// `i * 257`.
constexpr float kIdentityEpsilon = 1e-6f;

// PORT: drawing, the console's gamma ramp drawn as blends; a value held to 0..1.
float clamp_unit(float v) { return std::clamp(v, 0.0f, 1.0f); }

}  // namespace

std::array<std::uint16_t, 256> gamma_ramp(int brightness, int contrast) {
    // `fcfid` + `frsp` of each argument, then three single
    // products.
    const float c = static_cast<float>(contrast);
    const float b = static_cast<float>(brightness);
    const float product = c * b;
    const float middle = b * kRampMiddle;
    const float slope = product * kRampStep;

    std::array<std::uint16_t, 256> ramp{};
    for (int i = 0; i < 256; ++i) {
        // `fmadds`: one rounding, as `fmaf` does.
        float value = std::fmaf(static_cast<float>(i) - kRampMiddle, slope, middle);
        // `bge` to the upper test, so anything not at least 0 (NaN too) is 0.
        if (!(value >= 0.0f)) {
            value = 0.0f;
        } else if (value > kRampTop) {
            value = kRampTop;
        }
        // `fctidz`, and the store takes the low half-word.
        ramp[static_cast<size_t>(i)] =
            static_cast<std::uint16_t>(static_cast<std::int64_t>(value) & 0xffff);
    }
    return ramp;
}

GammaPair gamma_pair(const GameSettings& settings) {
    // `fctiwz` of each key's float (brightness into r4, contrast
    // into r5).
    return GammaPair{static_cast<int>(settings.brightness), static_cast<int>(settings.contrast)};
}

// PORT: drawing, whether the gamma settings changed since they were last drawn.
bool GammaWatch::changed(const GameSettings& settings) {
    const GammaPair now = gamma_pair(settings);
    if (now == sent_) return false;
    sent_ = now;
    return true;
}

// PORT: drawing, the console's gamma ramp as the line it is.
GammaLine gamma_line(int brightness, int contrast) {
    // The ramp's own terms, in its precision; entry i is x = i / 255.
    const float b = static_cast<float>(brightness);
    const float slope = static_cast<float>(contrast) * b * kRampStep;
    const float middle = b * kRampMiddle;
    const double top = static_cast<double>(kRampTop);
    GammaLine line;
    line.scale = static_cast<float>(static_cast<double>(slope) * 255.0 / top);
    line.offset =
        static_cast<float>((static_cast<double>(middle) -
                            static_cast<double>(kRampMiddle) * static_cast<double>(slope)) /
                           top);
    return line;
}

// PORT: drawing, the gamma ramp's line as frame-buffer blends.
std::vector<GammaBlendStep> gamma_blend_steps(const GammaLine& line) {
    std::vector<GammaBlendStep> steps;
    const float scale = std::max(line.scale, 0.0f);
    const float offset = line.offset;
    if (std::fabs(scale - 1.0f) < kIdentityEpsilon && std::fabs(offset) < kIdentityEpsilon) {
        return steps;
    }

    if (scale <= 1.0f) {
        // One blend: the scale and the offset together, clamped once.
        if (offset >= 0.0f) {
            steps.push_back({GammaBlend::MultiplyAdd, scale, clamp_unit(offset)});
        } else {
            steps.push_back({GammaBlend::MultiplySubtract, scale, clamp_unit(-offset)});
        }
        return steps;
    }

    // Brighter than the frame: the frame buffer can only double per blend,
    // and each blend clamps. Taking a negative offset off first (scaled down
    // by the gain still to come) and adding a positive one last keeps every
    // clamp where the line's own would be.
    if (offset < 0.0f) {
        steps.push_back({GammaBlend::MultiplySubtract, 1.0f, clamp_unit(-offset / scale)});
    }
    float remaining = scale;
    while (remaining > 1.0f + kIdentityEpsilon) {
        const float gain = std::min(remaining, 2.0f);
        steps.push_back({GammaBlend::Amplify, gain - 1.0f, 0.0f});
        remaining /= gain;
    }
    if (offset > 0.0f) {
        steps.push_back({GammaBlend::MultiplyAdd, 1.0f, clamp_unit(offset)});
    }
    return steps;
}

// PORT: drawing, what the blends do to one value (for the tests).
float apply_gamma_steps(const std::vector<GammaBlendStep>& steps, float value) {
    float v = clamp_unit(value);
    for (const GammaBlendStep& step : steps) {
        switch (step.blend) {
            case GammaBlend::MultiplyAdd: v = v * step.factor + step.offset; break;
            case GammaBlend::MultiplySubtract: v = v * step.factor - step.offset; break;
            case GammaBlend::Amplify: v = v * step.factor + v; break;
        }
        v = clamp_unit(v);
    }
    return v;
}

}  // namespace tl::core
