// How brightly a unit is drawn, by its distance from the point the camera looks
// at: the light fade `CCharacter::update`
// and `CItem::update`
// compute, and what `CGenericModel::setLightOverride` makes
// of it: a per-renderable scale on the surface ambient and emissive colours. The console runs
// the fade whenever its global flags word has bit 1. No OGRE and no file I/O.
#pragma once

namespace tl::core {

struct LightFadeInput {
    /// The unit's stored position (the collision sphere's centre) and the camera's focus.
    float unit_x = 0.0f, unit_y = 0.0f, unit_z = 0.0f;
    float focus_x = 0.0f, focus_y = 0.0f, focus_z = 0.0f;
    /// The level template's `UNIT_LIGHT_FADE`, `UNIT_FADE_DISTANCE` and `UNIT_FADE_START`.
    bool unit_light_fade = true;
    float fade_distance = 14.5f;
    float fade_start = 7.0f;
    /// The game state is not the main menu's.
    bool in_play = true;
};

/// `CCharacter::update`'s light: 1 at the focus, falling linearly from
/// `UNIT_FADE_START` to 0 at `UNIT_FADE_DISTANCE` on the ground, times one less
/// a third of the height between them (capped at 1). 1 without
/// `UNIT_LIGHT_FADE`, and 1 in the menu.
float character_light(const LightFadeInput& in);

/// `CItem::update`'s light for an item that lies on its own: by its ground distance to the
/// focus, more than 1 within 1.25.
float item_light(float item_x, float item_z, float focus_x, float focus_z);

/// What `CGenericModel::setLightOverride` does with a light, per record.
struct LightOverride {
    /// Written on the record's SubEntity: at least a quarter, in sixteenths.
    float scale = 1.0f;
    /// The record's `[0]`, which hides its render-behind outline
    /// (`CGame::renderableQueued`): set unless light^4 > 0.4.
    bool outline_hidden = false;
};
LightOverride light_override(float light);

}  // namespace tl::core
