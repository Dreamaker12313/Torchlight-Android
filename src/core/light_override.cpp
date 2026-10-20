#include "core/light_override.h"

#include <cmath>
#include <cstdint>

namespace tl::core {

// `CCharacter::update`.
float character_light(const LightFadeInput& in) {
    if (!in.unit_light_fade) return 1.0f;
    const float dz = in.unit_z - in.focus_z;
    const float dx = in.unit_x - in.focus_x;
    // dx*dx fused onto dz*dz, as the console has it.
    float d = std::sqrt(std::fma(dx, dx, dz * dz)) - in.fade_start;
    if (!(d > 0.0f)) d = 0.0f;
    const float range = in.fade_distance - in.fade_start;
    if (!(d < range)) d = range;
    float height = std::fabs(in.unit_y - in.focus_y) * 0.33333334f;
    if (!(height < 1.0f)) height = 1.0f;
    const float light = (1.0f - height) * (1.0f - d / range);
    return in.in_play ? light : 1.0f;
}

// `CItem::update`.
float item_light(float item_x, float item_z, float focus_x, float focus_z) {
    const float dz = item_z - focus_z;
    const float dx = item_x - focus_x;
    float d = std::sqrt(std::fma(dx, dx, dz * dz)) - 1.25f;
    if (!(d < 10.5f)) d = 10.5f;
    return (10.5f - d) * 0.095238097f;
}

// `CGenericModel::setLightOverride`, per record. The sixteenths keep only
// the low byte of a 64-bit truncation.
LightOverride light_override(float light) {
    LightOverride out;
    const float kept = light > 0.25f ? light : 0.25f;
    const auto sixteenths = static_cast<std::uint8_t>(static_cast<std::int64_t>(kept * 16.0f));
    out.scale = static_cast<float>(sixteenths) * 0.0625f;
    const float squared = light * light;
    out.outline_hidden = !(squared * squared > 0.4f);
    return out;
}

}  // namespace tl::core
