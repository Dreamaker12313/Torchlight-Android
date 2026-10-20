// CCharacter::calculateActiveRange and updateOpacity.
#include "core/active_range.h"
#include "core/average_fps.h"
#include "test_support.h"

using tl::core::ActiveRangeInput;
using tl::core::OpacityStep;

TL_TEST(active_range_by_distance) {
    ActiveRangeInput in;
    in.unit_active_range = 16.0f;
    in.unit_near_range = 24.0f;
    in.distance = 16.0f;
    auto r = tl::core::calculate_active_range(in);
    TL_CHECK(r.active && r.in_near_range);
    TL_CHECK(r.think_index == 2);  // past 15, not past 20
    in.distance = 20.0f;
    r = tl::core::calculate_active_range(in);
    TL_CHECK(!r.active && r.in_near_range);
    in.distance = 24.5f;
    in.think_index = 2;
    r = tl::core::calculate_active_range(in);
    TL_CHECK(!r.active && !r.in_near_range);
    // An inactive character keeps its band.
    TL_CHECK(r.think_index == 2);
}

TL_TEST(active_range_think_bands) {
    ActiveRangeInput in;
    in.unit_active_range = 100.0f;
    const float at[] = {0.0f, 10.0f, 10.5f, 15.0f, 15.5f, 20.0f, 22.0f, 30.0f};
    const int band[] = {0, 0, 1, 1, 2, 2, 3, 3};
    for (int i = 0; i < 8; ++i) {
        in.distance = at[i];
        TL_CHECK(tl::core::calculate_active_range(in).think_index == band[i]);
    }
}

TL_TEST(active_range_forced_divides_by_ten) {
    // Active wherever it stands, and the band's distance over 1000 / 100.
    ActiveRangeInput in;
    in.forced = true;
    in.distance = 150.0f;
    auto r = tl::core::calculate_active_range(in);
    TL_CHECK(r.active && r.in_near_range);
    TL_CHECK(r.think_index == 1);  // 15
    in.forced = false;
    in.casting = true;
    TL_CHECK(tl::core::calculate_active_range(in).active);
    in.casting = false;
    in.skills_running = true;
    TL_CHECK(tl::core::calculate_active_range(in).active);
}

TL_TEST(active_range_percent_grows_range_and_shrinks_band) {
    // `ACTIVERANGEMOD` 50: 16 * 1.5 = 24, and the band's distance / 0.5.
    ActiveRangeInput in;
    in.unit_active_range = 16.0f;
    in.active_range_mod = 50.0f;
    in.distance = 23.0f;
    auto r = tl::core::calculate_active_range(in);
    TL_CHECK(r.active);
    TL_CHECK(r.think_index == 3);  // 46
    in.distance = 4.0f;
    TL_CHECK(tl::core::calculate_active_range(in).think_index == 0);  // 8
    in.distance = 6.0f;
    TL_CHECK(tl::core::calculate_active_range(in).think_index == 1);  // 12
}

TL_TEST(active_range_pet_of_the_player_is_fastest) {
    ActiveRangeInput in;
    in.forced = true;
    in.master_is_player = true;
    in.distance = 500.0f;
    TL_CHECK(tl::core::calculate_active_range(in).think_index == 0);
}

TL_TEST(opacity_fades_out_then_hides) {
    // Not wanted: a second to nothing, then `setVisible(false, true)`.
    OpacityStep s = tl::core::update_opacity(1.0f, false, true, true, false, 0.0f, 0.25f, false);
    TL_CHECK(s.written && s.opacity == 0.75f && s.show == OpacityStep::Show::None);
    s = tl::core::update_opacity(0.1f, false, true, true, false, 0.0f, 0.25f, false);
    TL_CHECK(s.opacity == 0.0f && s.show == OpacityStep::Show::None);
    s = tl::core::update_opacity(0.0f, false, true, true, false, 0.0f, 0.25f, false);
    TL_CHECK(!s.written && s.show == OpacityStep::Show::Hide);
    // Out of the near range: straight to nothing.
    s = tl::core::update_opacity(0.8f, true, false, true, false, 0.0f, 0.01f, false);
    TL_CHECK(s.opacity == 0.0f && s.show == OpacityStep::Show::None);
    // Forced: written and hidden in the same call.
    s = tl::core::update_opacity(0.8f, false, true, true, false, 0.0f, 0.0f, true);
    TL_CHECK(s.written && s.opacity == 0.8f && s.show == OpacityStep::Show::Hide);
}

TL_TEST(opacity_fades_in_at_one_and_a_half) {
    OpacityStep s = tl::core::update_opacity(0.0f, true, true, false, false, 0.0f, 0.2f, false);
    TL_CHECK(s.show == OpacityStep::Show::Show);
    TL_CHECK(s.written && s.opacity > 0.299f && s.opacity < 0.301f);
    s = tl::core::update_opacity(0.9f, true, true, true, false, 0.0f, 0.2f, false);
    TL_CHECK(s.opacity == 1.0f && s.show == OpacityStep::Show::None);
    s = tl::core::update_opacity(1.0f, true, true, true, false, 0.0f, 0.2f, false);
    TL_CHECK(!s.written);
}

TL_TEST(opacity_falling_fades_four_times_faster) {
    // Past 0.6 of the flight the body fades whatever else is wanted, at 4 a second.
    OpacityStep s = tl::core::update_opacity(1.0f, true, true, true, true, 0.7f, 0.1f, false);
    TL_CHECK(s.opacity > 0.599f && s.opacity < 0.601f);
    // Before it, a wanted body is not faded.
    s = tl::core::update_opacity(1.0f, true, true, true, true, 0.5f, 0.1f, false);
    TL_CHECK(!s.written);
}

TL_TEST(active_range_shadow_flag) {
    // active, within `UNIT_SHADOW_RANGE` (10, not past it), not in
    // state 0xc or 6, within 3 in height, the unit's `SHADOWS` and the setting.
    ActiveRangeInput in;
    in.distance = 10.0f;
    TL_CHECK(tl::core::calculate_active_range(in).casts_shadow);
    in.distance = 10.5f;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
    in.distance = 4.0f;
    in.height_difference = -3.5f;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
    in.height_difference = 0.0f;
    in.ai_state = 6;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
    in.ai_state = 0;
    in.shadows_enabled = 0;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
    in.shadows_enabled = 1;
    in.unit_shadows = false;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
    // Not active: none.
    in.unit_shadows = true;
    in.distance = 9.0f;
    in.unit_active_range = 8.0f;
    TL_CHECK(!tl::core::calculate_active_range(in).casts_shadow);
}

TL_TEST(average_fps_over_the_last_frames) {
    // `CGame::update`: 0 before the first frame, then the count over
    // the sum; at 60 frames the oldest goes, so the window holds 59.
    tl::core::AverageFps fps;
    TL_CHECK(fps.value() == 0.0f);
    fps.add(0.0f);
    TL_CHECK(fps.value() == 0.0f);
    for (int i = 0; i < 100; ++i) fps.add(1.0f / 60.0f);
    TL_CHECK(fps.value() > 59.9f && fps.value() < 60.1f);
    for (int i = 0; i < 59; ++i) fps.add(1.0f / 30.0f);
    TL_CHECK(fps.value() > 29.9f && fps.value() < 30.1f);
}

// CItem::calculateActiveRange, updateOpacity, update and
// the item half of CLevel::updateNearUnitList.
using tl::core::ItemActiveRangeInput;
using tl::core::ItemPresence;
using tl::core::ItemPresenceChange;

TL_TEST(item_active_range_by_kind) {
    ItemActiveRangeInput in;  // GLOBALS.DAT: 16, 24, 40, near 24
    in.distance = 20.0f;
    auto r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && r.in_near_range);  // a barrel indoors: past 16, within 24
    in.interactable = true;                  // a chest: `TRIGGER_NEAR_RANGE`
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(r.active && r.in_near_range);
    in.distance = 40.5f;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && !r.in_near_range);
    // Outdoors every item has 24, an interactable's near range still 40.
    in.light_fade = false;
    in.distance = 30.0f;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && r.in_near_range);
    in.interactable = false;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && !r.in_near_range);
    // The trigger's timer, and an item out of the level keeping its near flag.
    in.forced_time = 0.5f;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(r.active && r.in_near_range);
    in.forced_time = 0.0f;
    in.in_level = false;
    in.active = true;
    in.in_near_range = true;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && r.in_near_range && !r.shadow_set);
}

TL_TEST(item_active_range_shadow) {
    ItemActiveRangeInput in;
    in.distance = 10.0f;
    TL_CHECK(!tl::core::calculate_item_active_range(in).shadow_set);
    in.casts_shadows = true;
    auto r = tl::core::calculate_item_active_range(in);
    TL_CHECK(r.shadow_set && r.casts_shadow);  // at the range itself: on
    in.distance = 10.5f;
    TL_CHECK(!tl::core::calculate_item_active_range(in).casts_shadow);
    in.distance = 3.0f;
    in.height_difference = -4.5f;
    TL_CHECK(!tl::core::calculate_item_active_range(in).casts_shadow);
    // Set while inactive too (a character's is not).
    in.height_difference = 0.0f;
    in.distance = 20.0f;
    r = tl::core::calculate_item_active_range(in);
    TL_CHECK(!r.active && r.shadow_set && !r.casts_shadow);
}

TL_TEST(item_opacity_fade) {
    ItemPresence p;
    p.in_near_range = true;
    ItemPresenceChange c;
    // Wanted: up by 1.5 a second, to 1, then left alone.
    p.opacity = 0.0f;
    tl::core::update_item_opacity(p, 0.5f, false, c);
    TL_CHECK(c.opacity_written && p.opacity == 0.75f);
    tl::core::update_item_opacity(p, 0.5f, false, c);
    TL_CHECK(p.opacity == 1.0f);
    c = {};
    tl::core::update_item_opacity(p, 0.5f, false, c);
    TL_CHECK(!c.opacity_written);
    // Not wanted: down by 1 a second, hidden once at 0.
    p.want_visible = false;
    tl::core::update_item_opacity(p, 0.25f, false, c);
    TL_CHECK(p.opacity == 0.75f && p.visible);
    tl::core::update_item_opacity(p, 1.0f, false, c);
    TL_CHECK(p.opacity == 0.0f && p.visible);
    c = {};
    tl::core::update_item_opacity(p, 0.1f, false, c);
    TL_CHECK(!p.visible && c.visibility_changed && !c.opacity_written);
    // Out of near range it drops to 0 at once.
    p.visible = true;
    p.opacity = 1.0f;
    p.in_near_range = false;
    tl::core::update_item_opacity(p, 0.1f, false, c);
    TL_CHECK(p.opacity == 0.0f);
    // Wanted again: shown first, then faded in.
    p.visible = false;
    p.want_visible = true;
    c = {};
    tl::core::update_item_opacity(p, 0.2f, false, c);
    TL_CHECK(p.visible && c.visibility_changed && p.opacity > 0.29f && p.opacity < 0.31f);
    // A used breakable is never touched.
    p.used = true;
    p.opacity = 0.5f;
    c = {};
    tl::core::update_item_opacity(p, 0.2f, true, c);
    TL_CHECK(!c.opacity_written && p.opacity == 0.5f);
}

TL_TEST(item_update_camera_and_range) {
    ItemActiveRangeInput range;
    range.distance = 5.0f;
    ItemPresence p;
    ItemPresenceChange c;
    // Active and seen: wanted, and the constructor's 0.999 goes to 1.
    tl::core::update_item(p, range, false, true, 0.016f, c);
    TL_CHECK(p.active && p.want_visible && p.visible && p.opacity == 1.0f);
    // Off the camera: hidden at once, at 0.
    c = {};
    tl::core::update_item(p, range, false, false, 0.016f, c);
    TL_CHECK(!p.visible && p.opacity == 0.0f && c.visibility_changed);
    // Back on: the hide left the goal (`+0x208`) set, so the fade shows it and
    // brings it in from 0.
    c = {};
    tl::core::update_item(p, range, false, true, 0.1f, c);
    TL_CHECK(p.visible && p.opacity > 0.14f && p.opacity < 0.16f);
    // Past the active range: no longer wanted, fading out over a second.
    range.distance = 20.0f;
    p.opacity = 1.0f;
    tl::core::update_item(p, range, false, true, 0.1f, c);
    TL_CHECK(!p.active && !p.want_visible && p.opacity == 1.0f);
    tl::core::update_item(p, range, false, true, 0.25f, c);
    TL_CHECK(p.opacity == 0.75f && !p.deleted);
    // A broken breakable out of range is marked for deletion.
    p.used = true;
    tl::core::update_item(p, range, true, true, 0.1f, c);
    TL_CHECK(p.deleted);
    // 1000 skips the camera test and the inactive branch.
    ItemPresence q;
    q.want_visible = false;
    c = {};
    tl::core::update_item(q, range, false, false, 1000.0f, c);
    TL_CHECK(q.want_visible && q.visible);
}

TL_TEST(item_near_list) {
    ItemActiveRangeInput range;
    ItemPresence p;
    ItemPresenceChange c;
    range.distance = 20.0f;
    TL_CHECK(tl::core::item_near_list_entry(p, range, c));
    TL_CHECK(!p.active && p.in_near_range && p.visible);
    range.distance = 30.0f;
    TL_CHECK(!tl::core::item_near_list_entry(p, range, c));
    TL_CHECK(!p.visible && p.opacity == 0.0f && c.visibility_changed);
    // The trigger's timer keeps it listed wherever it is.
    p.forced_time = 1.0f;
    TL_CHECK(tl::core::item_near_list_entry(p, range, c));
}

TL_TEST(camera_close_to_a_character_sees_through_it) {
    // `CCharacter::updateCameraOpacity`.
    using tl::core::update_camera_opacity;
    // 5 away: (5 - 4) * 0.5, written at once whatever the delta.
    OpacityStep s =
        update_camera_opacity(1.0f, 3.0f, 0.0f, 4.0f, true, true, true, false, 0.0f, 0.0f);
    TL_CHECK(s.written);
    TL_CHECK_EQ(s.opacity, 0.5f);
    // Within 4: nothing of it is drawn.
    s = update_camera_opacity(1.0f, 0.0f, 3.0f, 0.0f, true, true, true, false, 0.0f, 0.25f);
    TL_CHECK(s.written);
    TL_CHECK_EQ(s.opacity, 0.0f);
    // 6 or more away, paused: left as it is.
    s = update_camera_opacity(0.5f, 0.0f, 6.0f, 0.0f, true, true, true, false, 0.0f, 0.0f);
    TL_CHECK(!s.written);
    TL_CHECK_EQ(s.opacity, 0.5f);
    // ... running: the ordinary fade back, 1.5 a second.
    s = update_camera_opacity(0.5f, 0.0f, 6.0f, 0.0f, true, true, true, false, 0.0f, 0.25f);
    TL_CHECK(s.written);
    TL_CHECK_EQ(s.opacity, 0.875f);
}

// `CTriggerUnit::setVisible`: a shown trigger with its own
// layout ignores a hide - the fade's and the camera's both - and a show shows
// the layout alone; without one it is `CItem::setVisible`.
TL_TEST(a_trigger_with_its_own_layout_is_never_hidden_while_shown) {
    ItemPresence own;
    own.own_layout = true;
    own.opacity = 1.0f;
    ItemPresenceChange change;
    tl::core::set_unit_visible(own, false, change);
    TL_CHECK(own.visible);
    TL_CHECK_EQ(own.opacity, 1.0f);
    TL_CHECK(!change.visibility_changed);
    tl::core::set_unit_visible(own, true, change);
    TL_CHECK(change.own_layout_shown);

    ItemPresence plain;
    plain.opacity = 1.0f;
    ItemPresenceChange hidden;
    tl::core::set_unit_visible(plain, false, hidden);
    TL_CHECK(!plain.visible);
    TL_CHECK(hidden.visibility_changed);
    TL_CHECK(!hidden.own_layout_shown);

    // Out of the camera's view: `update`'s `setVisible(false, true)` is ignored too.
    ItemPresence far = ItemPresence{};
    far.own_layout = true;
    far.opacity = 1.0f;
    far.active = true;
    ItemPresenceChange step;
    ItemActiveRangeInput range;
    range.distance = 1.0f;
    tl::core::update_item(far, range, false, /*in_camera=*/false, 0.016f, step);
    TL_CHECK(far.visible);
}
