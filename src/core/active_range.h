// Whether a character is active, near and seen, by its distance to the point
// the camera looks at: `CCharacter::calculateActiveRange`
// and the fade `CCharacter::updateOpacity`
// runs on what it decides.
#pragma once

#include <cstdint>

namespace tl::core {

/// What `calculateActiveRange` reads of the character and the level.
struct ActiveRangeInput {
    /// The distance on the ground plane to the point the camera looks at.
    float distance = 0.0f;
    /// Active wherever it stands: a player, a boss champion, a master that is
    /// itself active, or AI state 5 (dying).
    bool forced = false;
    /// The current skill is a one-shot or running, and its animation is up or
    /// has time left.
    bool casting = false;
    /// `CSkillManager +0x50`, the running list's count, is not 0.
    bool skills_running = false;
    /// `INDOOR_UNIT_ACTIVE_RANGE` or `OUTDOOR_UNIT_ACTIVE_RANGE`, by the level
    /// template's `UNIT_LIGHT_FADE`.
    float unit_active_range = 16.0f;
    /// `UNIT_NEAR_RANGE` (`CGameGlobals +0xac`).
    float unit_near_range = 24.0f;
    /// Effect 0x2c, `PERCENT ACTIVE DISTANCE BONUS`.
    float active_distance_bonus = 0.0f;
    /// The level template's `ACTIVERANGEMOD` (`+0x98`), which a character in AI
    /// state 5 or 6 does not get (the caller passes 0 then).
    float active_range_mod = 0.0f;
    /// `+0x640` is a player (`ISA 0x1c`): the AI think band is the fastest.
    bool master_is_player = false;
    /// `+0x520` as it stands: kept when the character is not active.
    int32_t think_index = 0;

    /// The shadow flag's inputs: the unit's `SHADOWS`, `SHADOWS_ENABLED`,
    /// `UNIT_SHADOW_RANGE`, the height between the point looked at and the
    /// character, and its AI state.
    bool unit_shadows = true;
    int32_t shadows_enabled = 1;
    float unit_shadow_range = 10.0f;
    float height_difference = 0.0f;
    uint32_t ai_state = 0;
};

struct ActiveRange {
    bool active = false;         ///< `+0x199`
    bool in_near_range = false;  ///< `+0x19a`
    int32_t think_index = 0;     ///< `+0x520`, the AI interval's band
    bool casts_shadow = false;   ///< `+0x530`
};

/// `CCharacter::calculateActiveRange`, less
/// the editor test.
ActiveRange calculate_active_range(const ActiveRangeInput& in);

/// The fade's step: what `CCharacter::updateOpacity`
/// does to the opacity, and which `setVisible(.., true)` it asks for.
struct OpacityStep {
    float opacity = 1.0f;
    /// The opacity was written (`CGenericModel::setOpacity`).
    bool written = false;
    enum class Show { None, Show, Hide };
    Show show = Show::None;
};

/// `want` is the `setVisible(bool)` goal, `death_flight` a body flung off the
/// ground (`CCharacter::die`), `flight_progress` how
/// much of that flight is behind it; `force` is the second argument. A
/// character with no model is not stepped; the caller checks.
OpacityStep update_opacity(float opacity, bool want, bool in_near_range, bool visible,
                           bool death_flight, float flight_progress, float delta, bool force);

/// `CCharacter::updateCameraOpacity(Camera*, float)`,
/// run on the player every frame by `CGameClient::updateIngame`
/// (with a delta of 0 while paused). Within 6 of the
/// camera the character is seen through, opacity `max(d - 4, 0) * 0.5` written
/// at once; farther, the ordinary fade steps by `delta` when that is above 0.
OpacityStep update_camera_opacity(float opacity, float camera_dx, float camera_dy, float camera_dz,
                                  bool want, bool in_near_range, bool visible, bool death_flight,
                                  float flight_progress, float delta);

// ---------------------------------------------------------------------------
// The same for an item on the floor: a breakable, a dropped piece of
// equipment, gold, a `Unit Trigger`.

/// What `CItem::calculateActiveRange` reads.
struct ItemActiveRangeInput {
    /// The distance on the ground plane to the point the camera looks at.
    float distance = 0.0f;
    /// The height of that point over the item's.
    float height_difference = 0.0f;
    /// `+0x1e0`, run down by `update`; above 0 the item is active wherever it is
    /// (`CTriggerUnit::setTriggerState` sets it).
    float forced_time = 0.0f;
    /// `+0x198`, `activateUnitInLevel`.
    bool in_level = true;
    /// `ISA 0x20` (`INTERACTABLE`): a door, a chest, a lever.
    bool interactable = false;
    /// The level template's `UNIT_LIGHT_FADE`: indoor range, or for an
    /// interactable `TRIGGER_NEAR_RANGE`; without it the outdoor range.
    bool light_fade = true;
    float indoor_range = 16.0f;
    float outdoor_range = 24.0f;
    float trigger_near_range = 40.0f;
    /// `UNIT_NEAR_RANGE` (`+0xac`), an interactable's being `TRIGGER_NEAR_RANGE`.
    float unit_near_range = 24.0f;
    /// `+0x199` and `+0x19a` as they stand: an item out of the level keeps
    /// its near flag.
    bool active = false;
    bool in_near_range = false;
    /// `SHADOWS` (`+0x1a9`) and a model (vtable +0x1e0) to set the flag on.
    bool casts_shadows = false;
    /// `UNIT_SHADOW_RANGE` (`+0xa8`).
    float unit_shadow_range = 10.0f;
};

struct ItemActiveRange {
    bool active = false;         ///< `+0x199`
    bool in_near_range = false;  ///< `+0x19a`
    /// `CGenericModel::setCastsShadows` was called, with `casts_shadow`.
    bool shadow_set = false;
    bool casts_shadow = false;
};

/// `CItem::calculateActiveRange`.
ItemActiveRange calculate_item_active_range(const ItemActiveRangeInput& in);

/// An item's presence, the fields `CItem::update`
/// and its fade keep. The constructor's
/// values; `CLevel::addItem`
/// puts the opacity to 1 and writes it at
/// once.
struct ItemPresence {
    float opacity = 0.999f;      ///< `+0x204`, 0x3f7fbe77 from the constructor
    bool want_visible = true;    ///< `+0x208`, `setVisible(bool)`
    bool visible = true;         ///< `+0x81`, the node shown
    bool active = false;         ///< `+0x199`
    bool in_near_range = false;  ///< `+0x19a`
    /// `+0x218`: the model is left alone (a broken breakable stays hidden).
    bool used = false;
    /// `+0x190`: `CLevel::updateItems` deletes it instead of updating it.
    bool deleted = false;
    float forced_time = 0.0f;  ///< `+0x1e0`
    /// A `CTriggerUnit` that is a layout of its own (its `LAYOUTFILE`): its
    /// `setVisible` is `set_unit_visible`'s other branch.
    bool own_layout = false;
};

/// What changed in a step, for the caller to draw.
struct ItemPresenceChange {
    bool opacity_written = false;  ///< `CGenericModel::setOpacity(opacity)`
    bool visibility_changed = false;
    bool shadow_set = false;
    bool casts_shadow = false;
    /// `CTriggerUnit::setVisible(true, ..)` on a shown trigger with its own
    /// layout: the layout's `setVisible(true)` (vtable +0x50), the node left.
    bool own_layout_shown = false;
};

/// `CItem::updateOpacity(dt, force)`.
void update_item_opacity(ItemPresence& p, float delta, bool force, ItemPresenceChange& change);

/// `CItem::setVisible(bool, bool)` with its
/// second argument true: hidden at once at opacity 0, or shown. (False is
/// `setVisible(bool)`, the fade's goal alone.)
void set_item_visible_now(ItemPresence& p, bool on, ItemPresenceChange& change);

/// The item's `setVisible(on, true)` through its vtable: `CItem::setVisible`,
/// or for a trigger with its own layout `CTriggerUnit::setVisible(bool, bool)`
/// - while it is shown a hide is ignored and
/// a show shows the layout alone.
void set_unit_visible(ItemPresence& p, bool on, ItemPresenceChange& change);

/// `CItem::update`, less `CBaseUnit::update`,
/// the sound bank and the light override: the timer, the range, the fade, then
/// an inactive item wants to be hidden (a used breakable is marked for
/// deletion), an active one wants to be seen while the camera's frustum holds
/// its bounds and is hidden at once while it does not. A `delta` of 1000 skips
/// the camera test. `range` is filled from `p` and the caller's distances.
void update_item(ItemPresence& p, ItemActiveRangeInput range, bool breakable, bool in_camera,
                 float delta, ItemPresenceChange& change);

/// `CLevel::updateNearUnitList`'s item half
/// for one item: measured again, and one past its near range hidden at once
/// (its label too, which is the caller's). True when it stays on the near list
/// (`+0x90`).
bool item_near_list_entry(ItemPresence& p, ItemActiveRangeInput range, ItemPresenceChange& change);

}  // namespace tl::core
