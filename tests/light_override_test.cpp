// The units' light fade: `CCharacter::update`, `CItem::update`
// and `CGenericModel::setLightOverride`,
// read from the disassembly.
#include "core/light_override.h"
#include "test_support.h"

#include <cmath>

using tl::core::LightFadeInput;

TL_TEST(character_light_by_distance) {
    LightFadeInput in;
    // Within UNIT_FADE_START: full light.
    in.unit_x = 6.0f;
    TL_CHECK(tl::core::character_light(in) == 1.0f);
    // Half way through the fade (7 + 3.75 of 7.5).
    in.unit_x = 10.75f;
    TL_CHECK(std::fabs(tl::core::character_light(in) - 0.5f) < 1e-6f);
    // At UNIT_FADE_DISTANCE and past it: none.
    in.unit_x = 14.5f;
    TL_CHECK(tl::core::character_light(in) == 0.0f);
    in.unit_x = 40.0f;
    TL_CHECK(tl::core::character_light(in) == 0.0f);
    // Height: a third a unit, up to all of it.
    in.unit_x = 0.0f;
    in.unit_y = 1.5f;
    TL_CHECK(std::fabs(tl::core::character_light(in) - 0.5f) < 1e-6f);
    in.unit_y = -4.0f;
    TL_CHECK(tl::core::character_light(in) == 0.0f);
    // Without UNIT_LIGHT_FADE, or in the menu: 1.
    in.unit_light_fade = false;
    TL_CHECK(tl::core::character_light(in) == 1.0f);
    in.unit_light_fade = true;
    in.in_play = false;
    TL_CHECK(tl::core::character_light(in) == 1.0f);
}

TL_TEST(item_light_by_distance) {
    // Within 1.25 the light is more than 1: (10.5 + 1.25) / 10.5 at the focus.
    TL_CHECK(std::fabs(tl::core::item_light(0, 0, 0, 0) - 11.75f * 0.095238097f) < 1e-6f);
    TL_CHECK(std::fabs(tl::core::item_light(1.25f, 0, 0, 0) - 1.0f) < 1e-6f);
    TL_CHECK(tl::core::item_light(0, 11.75f, 0, 0) == 0.0f);
    TL_CHECK(tl::core::item_light(0, 30.0f, 0, 0) == 0.0f);
}

TL_TEST(light_override_sixteenths_and_outline) {
    // A quarter at least, truncated to sixteenths.
    TL_CHECK(tl::core::light_override(0.0f).scale == 0.25f);
    TL_CHECK(tl::core::light_override(0.3f).scale == 0.25f);
    TL_CHECK(tl::core::light_override(0.99f).scale == 0.9375f);
    TL_CHECK(tl::core::light_override(1.0f).scale == 1.0f);
    // An item at the focus: 1.119 -> 17/16.
    TL_CHECK(tl::core::light_override(1.119f).scale == 1.0625f);
    // The outline needs light^4 above 0.4 (light above ~0.7953).
    TL_CHECK(!tl::core::light_override(1.0f).outline_hidden);
    TL_CHECK(!tl::core::light_override(0.8f).outline_hidden);
    TL_CHECK(tl::core::light_override(0.79f).outline_hidden);
    TL_CHECK(tl::core::light_override(0.0f).outline_hidden);
}
