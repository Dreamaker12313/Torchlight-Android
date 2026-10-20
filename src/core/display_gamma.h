// Brightness and contrast: the console's display gamma ramp (the PC has neither
// setting). The game client reads both keys every frame,
// truncated, and when the pair differs from the last
// sent (cache, starting at zero) fills a 256-entry 16-bit
// ramp for `D3DDevice_SetGammaRamp`, all three
// channels.
//
// The ramp is a clamped line, the identity at brightness 257 and contrast 256;
// the port's defaults are the identity (DIVERGENCE, `core/settings_menu.h`).
// The console's display then applies the TV's calibration
// (`VdGetCurrentDisplayGamma`); the port draws the line over the
// finished frame instead (`gamma_blend_steps`).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace tl::core {

struct GameSettings;

/// the ramp for one channel (the original writes the same
/// values to red, green and blue). In single precision as the original
/// computes it: `(i - 128) * (b * c / 256) + b * 128` fused, held to
/// 0 .. 65535 and truncated.
std::array<std::uint16_t, 256> gamma_ramp(int brightness, int contrast);

/// The two ints the per-frame check compares:
/// each setting truncated toward zero (`fctiwz`).
struct GammaPair {
    int brightness = 0;
    int contrast = 0;
    bool operator==(const GammaPair& o) const {
        return brightness == o.brightness && contrast == o.contrast;
    }
    bool operator!=(const GammaPair& o) const { return !(*this == o); }
};

GammaPair gamma_pair(const GameSettings& settings);

/// The per-frame check: true, and the cache
/// updated, when the pair differs from the one last sent. The cache starts at
/// zero as the original's does.
class GammaWatch {
public:
    bool changed(const GameSettings& settings);
    // PORT: drawing, an accessor.
    const GammaPair& sent() const { return sent_; }

private:
    GammaPair sent_{};
};

/// The ramp as the line it is, over a colour in 0 .. 1: `y = clamp(scale * x
/// + offset, 0, 1)`, entry `i` being `x = i / 255` and `y = ramp[i] / 65535`.
struct GammaLine {
    float scale = 1.0f;
    float offset = 0.0f;
};

GammaLine gamma_line(int brightness, int contrast);

/// One full-screen blend over the frame buffer, which clamps to 0 .. 1 after
/// each. Not the original's: how the port applies the line without a
/// hardware ramp (see `render/display_gamma.*`).
enum class GammaBlend {
    MultiplyAdd,       ///< `dst * factor + offset`  (factor and offset in 0 .. 1)
    MultiplySubtract,  ///< `dst * factor - offset`  (reverse subtract)
    Amplify,           ///< `dst * (1 + factor)`     (`dst * src + dst`, factor in 0 .. 1)
};

struct GammaBlendStep {
    GammaBlend blend = GammaBlend::MultiplyAdd;
    float factor = 1.0f;
    float offset = 0.0f;
};

/// The line as blends whose clamps compose to the line's own: a negative
/// offset is taken off before the frame is amplified, a positive one added
/// after. Empty for the identity. At most four steps over the settings'
/// range (scale up to 3.65: two amplifications).
std::vector<GammaBlendStep> gamma_blend_steps(const GammaLine& line);

/// What the steps do to one value, clamping after each as the frame buffer
/// does. For the tests.
float apply_gamma_steps(const std::vector<GammaBlendStep>& steps, float value);

}  // namespace tl::core
