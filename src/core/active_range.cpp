#include "core/active_range.h"

#include <cmath>

namespace tl::core {

ActiveRange calculate_active_range(const ActiveRangeInput& in) {
    // `CCharacter::calculateActiveRange`.
    ActiveRange out;
    out.think_index = in.think_index;
    // The percentage the think distance is divided by, 1000 for a
    // character active whatever its distance.
    float percent = 1000.0f;
    if (in.casting || in.skills_running || in.forced) {
        out.active = true;
        out.in_near_range = true;
    } else {
        float range = in.unit_active_range;
        percent = in.active_distance_bonus + in.active_range_mod;
        // `fma(percent, 0.01f, 1) * range`.
        if (percent != 0.0f) range = std::fma(percent, 0.01f, 1.0f) * range;
        if (in.distance <= range) {
            out.active = true;
            out.in_near_range = true;
        } else {
            out.active = false;
            out.in_near_range = in.distance <= in.unit_near_range;
        }
    }
    // a unit that casts shadows, with them enabled, casts one while it
    // is active, within `UNIT_SHADOW_RANGE`, not in AI state 0xc or 6, and no
    // more than 3 above or below the point looked at.
    out.casts_shadow =
        in.unit_shadows && 1 <= in.shadows_enabled && out.active &&
        !(in.unit_shadow_range <= in.distance && in.distance != in.unit_shadow_range) &&
        in.ai_state != 0xc && !(3.0f < std::fabs(in.height_difference)) && in.ai_state != 6;
    if (out.active) {
        float distance = in.distance;
        // over `percent * 0.01f`.
        if (percent != 0.0f) distance = distance / (percent * 0.01f);
        // 3 is written first, and both past 20 and past 25 it stays
        // 3. A NaN fails the first test and gets 0.
        if (!(10.0f < distance)) out.think_index = 0;
        else if (!(15.0f < distance)) out.think_index = 1;
        else if (!(20.0f < distance)) out.think_index = 2;
        else out.think_index = 3;
    }
    if (in.master_is_player) out.think_index = 0;
    return out;
}

OpacityStep update_opacity(float opacity, bool want, bool in_near_range, bool visible,
                           bool death_flight, float flight_progress, float delta, bool force) {
    // `CCharacter::updateOpacity`.
    OpacityStep step;
    step.opacity = opacity;
    if (!want || (death_flight && 0.6f < flight_progress) || !in_near_range) {
        if (0.0f < opacity || force) {
            float o = opacity;
            if (!in_near_range) o = 0.0f;
            o = death_flight ? delta * -4.0f + o : o - delta;
            if (o < 0.0f) o = 0.0f;
            step.opacity = o;
            step.written = true;
            if (!force) return step;
        }
        if (visible) step.show = OpacityStep::Show::Hide;
        return step;
    }
    if (!visible) step.show = OpacityStep::Show::Show;
    if (opacity < 1.0f || force) {
        float o = delta * 1.5f + opacity;
        if (1.0f < o) o = 1.0f;
        step.opacity = o;
        step.written = true;
    }
    return step;
}

OpacityStep update_camera_opacity(float opacity, float camera_dx, float camera_dy, float camera_dz,
                                  bool want, bool in_near_range, bool visible, bool death_flight,
                                  float flight_progress, float delta) {
    // `CCharacter::updateCameraOpacity`. Summed x, z, y, two
    // of the adds fused.
    const float distance = std::sqrt(
        std::fma(camera_dy, camera_dy, std::fma(camera_dz, camera_dz, camera_dx * camera_dx)));
    if (!(distance >= 6.0f)) {
        float near = distance - 4.0f;
        if (!(near > 0.0f)) near = 0.0f;
        return update_opacity(near * 0.5f, want, in_near_range, visible, death_flight,
                              flight_progress, 0.0f, true);
    }
    if (delta <= 0.0f) {
        OpacityStep none;
        none.opacity = opacity;
        return none;
    }
    return update_opacity(opacity, want, in_near_range, visible, death_flight, flight_progress,
                          delta, false);
}

ItemActiveRange calculate_item_active_range(const ItemActiveRangeInput& in) {
    // `CItem::calculateActiveRange`.
    ItemActiveRange out;
    out.active = in.active;
    out.in_near_range = in.in_near_range;
    if (0.0f < in.forced_time) {
        out.active = true;
        out.in_near_range = true;
        return out;
    }
    if (!in.in_level) {
        out.active = false;
        return out;
    }
    // a template without `UNIT_LIGHT_FADE` gives every item the
    // outdoor range; otherwise an interactable gets `TRIGGER_NEAR_RANGE`.
    float range = !in.light_fade    ? in.outdoor_range
                  : in.interactable ? in.trigger_near_range
                                    : in.indoor_range;
    if (range < in.distance) {
        out.active = false;
        const float near = in.interactable ? in.trigger_near_range : in.unit_near_range;
        out.in_near_range = in.distance <= near;
    } else {
        out.active = true;
        out.in_near_range = true;
    }
    // off past `UNIT_SHADOW_RANGE` or more than 4 above or below the
    // point looked at.
    if (in.casts_shadows) {
        out.shadow_set = true;
        out.casts_shadow =
            !(in.unit_shadow_range < in.distance) && !(4.0f < std::fabs(in.height_difference));
    }
    return out;
}

void update_item_opacity(ItemPresence& p, float delta, bool force, ItemPresenceChange& change) {
    // `CItem::updateOpacity`.
    if (p.used) return;
    if (p.want_visible) {
        if (!p.visible) set_unit_visible(p, true, change);
        if (1.0f <= p.opacity && !force) return;
        const float o = delta * 1.5f + p.opacity;
        p.opacity = o <= 1.0f ? o : 1.0f;
    } else {
        if (p.opacity <= 0.0f && !force) {
            if (p.visible) set_unit_visible(p, false, change);
            return;
        }
        if (!p.in_near_range) {
            p.opacity = 0.0f;
        } else {
            const float o = p.opacity - delta;
            p.opacity = 0.0f <= o ? o : 0.0f;
        }
    }
    change.opacity_written = true;
}

void set_item_visible_now(ItemPresence& p, bool on, ItemPresenceChange& change) {
    // `CItem::setVisible(bool, bool)`, the
    // immediate branch.
    if (!on && p.opacity != 0.0f) {
        p.opacity = 0.0f;
        update_item_opacity(p, 0.0f, false, change);
    }
    // `CSceneNodeObject::setVisible`.
    if (p.visible != on) change.visibility_changed = true;
    p.visible = on;
}

void set_unit_visible(ItemPresence& p, bool on, ItemPresenceChange& change) {
    // a trigger with its own layout, shown: a hide does nothing, a show shows
    // the layout; otherwise `CItem::setVisible`.
    if (p.own_layout && p.visible) {
        if (on) change.own_layout_shown = true;
        return;
    }
    set_item_visible_now(p, on, change);
}

namespace {
// PORT: the item's state handed to `calculate_item_active_range`, and its answer kept.
void apply_item_range(ItemPresence& p, ItemActiveRangeInput& range, ItemPresenceChange& change) {
    range.forced_time = p.forced_time;
    range.active = p.active;
    range.in_near_range = p.in_near_range;
    const ItemActiveRange r = calculate_item_active_range(range);
    p.active = r.active;
    p.in_near_range = r.in_near_range;
    if (r.shadow_set) {
        change.shadow_set = true;
        change.casts_shadow = r.casts_shadow;
    }
}
}  // namespace

void update_item(ItemPresence& p, ItemActiveRangeInput range, bool breakable, bool in_camera,
                 float delta, ItemPresenceChange& change) {
    // `CItem::update`.
    if (0.0f <= p.forced_time) p.forced_time = p.forced_time - delta;
    apply_item_range(p, range, change);
    update_item_opacity(p, delta, false, change);
    if (!p.active && delta != 1000.0f) {
        // `ISA 0x1d` with `+0x218`: a broken breakable leaves the level.
        if (breakable && p.used) p.deleted = true;
        p.want_visible = false;
        return;
    }
    if (delta == 1000.0f || in_camera) {
        if (!p.used) p.want_visible = true;
    } else {
        // vtable +0x2c0, `setVisible(false, true)`.
        set_unit_visible(p, false, change);
    }
}

// `CLevel::updateNearUnitList`'s item half for
// one item: its range worked out, and one out of near range hidden at once.
bool item_near_list_entry(ItemPresence& p, ItemActiveRangeInput range, ItemPresenceChange& change) {
    apply_item_range(p, range, change);
    if (p.in_near_range) return true;
    set_item_visible_now(p, false, change);
    return false;
}

}  // namespace tl::core
