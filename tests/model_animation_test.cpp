// `CGenericModel`'s animation half (core/model_animation.h): play, blend, queue
// and the per-frame step, against the Xbox build's behaviour.

#include "core/model_animation.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

using tl::core::ActiveAnimation;
using tl::core::ModelAnimator;
using tl::core::ModelKey;

ModelAnimator two_animations(bool alternate = false) {
    ModelAnimator m(alternate);
    m.add_animation("Idle", 2.0f);
    m.add_animation("Attack1", 1.0f);
    return m;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

}  // namespace

// `CActiveAnimation::update`: a one-shot holds its end and is
// finished; a loop wraps and says so.
TL_TEST(an_active_animation_stops_at_its_end_or_wraps) {
    ActiveAnimation once;
    once.end = 1.0f;
    once.active = true;
    once.update(0.6f);
    TL_CHECK(once.active);
    once.update(0.6f);
    TL_CHECK(!once.active);
    TL_CHECK(once.finished);
    TL_CHECK(once.faded);
    TL_CHECK_EQ(once.time, 1.0f);

    ActiveAnimation loop;
    loop.end = 1.0f;
    loop.loop = true;
    loop.active = true;
    loop.update(2.5f);
    TL_CHECK(loop.active);
    TL_CHECK(loop.wrapped);
    TL_CHECK(near(loop.time, 0.5f));

    // No length: the time stays 0, a loop keeps playing.
    ActiveAnimation none;
    none.active = true;
    none.loop = true;
    none.update(0.3f);
    TL_CHECK(none.active);
    TL_CHECK_EQ(none.time, 0.0f);
}

// `playAnimation` stops everything and starts at full weight.
TL_TEST(play_replaces_what_was_playing) {
    ModelAnimator m = two_animations();
    m.play(0u, true, 1.0f);
    TL_CHECK(m.tracks()[0].enabled);
    m.play(1u, false, 1.0f);
    TL_CHECK(!m.tracks()[0].enabled);
    TL_CHECK(m.tracks()[1].enabled);
    TL_CHECK_EQ(m.active().size(), size_t{1});
    TL_CHECK(m.playing(1u));
    TL_CHECK(!m.playing(0u));
    // A duration replaces the length as the end.
    m.play(0u, false, 1.0f, 0.5f);
    TL_CHECK_EQ(m.active()[0].end, 0.5f);
}

// `blendAnimation`: the new animation fades in over the blend
// while the old one plays on, and is dropped once the blend is done.
TL_TEST(blend_cross_fades_and_drops_the_old_record) {
    ModelAnimator m = two_animations();
    m.play(0u, true, 1.0f);
    m.update(0.1f, false);
    m.blend(1u, false, 0.2f, 1.0f);
    TL_CHECK_EQ(m.active().size(), size_t{2});
    TL_CHECK_EQ(m.active()[0].index, 1u);
    TL_CHECK_EQ(m.tracks()[1].weight, 0.0f);
    TL_CHECK(m.tracks()[1].enabled);

    m.update(0.1f, false);
    // Half the blend run: weights 0.5 and 0.5, scaled by 2 (the total is 1).
    TL_CHECK(near(m.tracks()[1].weight, 1.0f));
    TL_CHECK(near(m.tracks()[0].weight, 1.0f));
    TL_CHECK_EQ(m.active().size(), size_t{2});

    // The blend runs out: the new record is faded, which marks the one behind
    // it finished, and that one is dropped the same frame.
    m.update(0.1f, false);
    TL_CHECK_EQ(m.active().size(), size_t{1});
    TL_CHECK_EQ(m.active()[0].index, 1u);
    TL_CHECK(!m.tracks()[0].enabled);
}

// No blend time, or an animation with no length, is a play.
TL_TEST(blend_with_no_time_is_a_play) {
    ModelAnimator m = two_animations();
    m.play(0u, true, 1.0f);
    m.blend(1u, false, 0.0f, 1.0f);
    TL_CHECK_EQ(m.active().size(), size_t{1});
    TL_CHECK(!m.tracks()[0].enabled);
}

// The console's own: a looping blend of an animation already behind it carries
// on from its time instead of restarting.
TL_TEST(a_looping_blend_of_the_same_animation_carries_on) {
    ModelAnimator m = two_animations();
    m.play(0u, true, 1.0f);
    m.update(0.7f, false);
    m.blend(1u, false, 0.2f, 1.0f);
    m.blend(0u, true, 0.2f, 1.0f);
    TL_CHECK_EQ(m.active()[0].index, 0u);
    TL_CHECK(near(m.active()[0].time, 0.7f));
}

// The second set of states: a wardrobed character's blend of what it is
// already playing plays it from the second set.
TL_TEST(a_second_set_lets_an_animation_blend_into_itself) {
    ModelAnimator m = two_animations(/*alternate=*/true);
    m.play(1u, false, 1.0f);
    m.update(0.5f, false);
    m.blend(1u, false, 0.2f, 1.0f);
    TL_CHECK(m.active()[0].alternate);
    TL_CHECK(m.alternate_tracks()[1].enabled);
    TL_CHECK(m.tracks()[1].enabled);
    TL_CHECK(m.playing_alternate(1u));

    // Without the set the same blend stays on the first one.
    ModelAnimator plain = two_animations(false);
    plain.play(1u, false, 1.0f);
    plain.update(0.5f, false);
    plain.blend(1u, false, 0.2f, 1.0f);
    TL_CHECK(!plain.active()[0].alternate);
}

// `queueBlendAnimation`: the queued animation waits until the
// one in front has no more than its blend left, then starts and fades in.
TL_TEST(a_queued_animation_starts_near_the_end_of_the_one_before) {
    ModelAnimator m = two_animations();
    m.play(1u, false, 1.0f);
    m.queue_blend(0, true, 0.2f, 1.0f);
    TL_CHECK(m.queued(0u));
    TL_CHECK(!m.playing(0u));
    m.update(0.5f, false);
    TL_CHECK(m.queued(0u));
    m.update(0.25f, false);  // 0.25 left after this frame: not yet
    TL_CHECK(m.queued(0u));
    m.update(0.1f, false);  // 0.05 would be left: it starts
    TL_CHECK(!m.queued(0u));
    TL_CHECK(m.playing(0u));
    TL_CHECK(m.tracks()[0].enabled);
    // `clearQueuedAnimations` takes only queued records.
    m.queue_blend(1, false, 0.2f, 1.0f);
    m.clear_queued();
    TL_CHECK(!m.queued(1u));
    TL_CHECK(m.playing(0u));
}

// A key fires on the frame that passes it, at frame * 0.033333335.
TL_TEST(update_fires_the_keys_a_frame_passes) {
    ModelAnimator m = two_animations();
    m.add_key(1, ModelKey{0, 15.0f, 0});  // HIT at 0.5 s
    m.add_key(1, ModelKey{3, 27.0f, 1});  // PLAYSOUND at 0.9 s
    m.play(1u, false, 1.0f);
    TL_CHECK(m.update(0.4f, false).empty());
    const auto& fired = m.update(0.2f, false);
    TL_CHECK_EQ(fired.size(), size_t{1});
    TL_CHECK_EQ(fired[0].key, size_t{0});
    // A frame with no step passes nothing.
    TL_CHECK(m.update(0.0f, false).empty());
}

// A record dropped before it reached them still fires its restoring keys, and
// its hits when it was started with the flag.
TL_TEST(a_dropped_animation_fires_the_keys_that_restore) {
    ModelAnimator m = two_animations();
    m.add_key(1, ModelKey{0x17, 25.0f, 0});  // SHOWMESH at 0.83 s
    m.add_key(1, ModelKey{0, 20.0f, 1});     // HIT
    m.add_key(1, ModelKey{3, 26.0f, 2});     // PLAYSOUND: not restored
    m.blend(1u, false, 0.1f, 1.0f, /*flush_hits=*/true);
    m.update(0.2f, false);
    m.blend(0u, true, 0.1f, 1.0f);
    // The new blend runs out, which marks the record behind it finished, and
    // that record is dropped the same frame, at 0.4 s of its 1 s.
    const std::vector<tl::core::FiredModelKey> fired = m.update(0.2f, false);
    TL_CHECK_EQ(m.active().size(), size_t{1});
    size_t show = 0, hit = 0, sound = 0;
    for (const auto& k : fired) {
        if (k.animation != 1u) continue;
        if (k.key == 0) ++show;
        if (k.key == 1) ++hit;
        if (k.key == 2) ++sound;
    }
    TL_CHECK_EQ(show, size_t{1});
    TL_CHECK_EQ(hit, size_t{1});
    TL_CHECK_EQ(sound, size_t{0});
}

// `setAnimationTime` holds the time to the record's end; `getAnimationTime`
// reads the first playing one; `setAnimationSpeed` reaches every one playing.
TL_TEST(time_and_speed_reach_the_records_playing) {
    ModelAnimator m = two_animations();
    m.play(1u, false, 1.0f);
    m.set_time(5.0f);
    TL_CHECK_EQ(m.time(), 1.0f);
    m.set_time(-1.0f);
    TL_CHECK_EQ(m.time(), 0.0f);
    m.set_speed(2.0f);
    m.update(0.25f, false);
    TL_CHECK(near(m.time(), 0.5f));
    m.paused = true;
    m.update(0.25f, false);
    TL_CHECK(near(m.time(), 0.5f));
}

// By name: the upper-cased name against the name as passed.
TL_TEST(play_by_name_matches_upper_case) {
    ModelAnimator m = two_animations();
    m.play(std::string("ATTACK1"), false, 1.0f);
    TL_CHECK(m.playing(1u));
    m.play(std::string("attack1"), false, 1.0f);  // not upper-case: no match
    TL_CHECK(m.playing(1u));
    TL_CHECK(m.playing(std::string("attack1")));  // getAnimationIndex is blind to case
    TL_CHECK_EQ(m.index_of("idle"), 0);
    TL_CHECK_EQ(m.length_seconds(1), 1.0f);
    TL_CHECK_EQ(m.length_seconds(2), 0.0f);
}

// `loadAnimations`: a missing file and a duplicate name are skipped,
// `BIND` is filed but not counted.
TL_TEST(load_animation_list_follows_load_animations) {
    using tl::core::AnimationFileEntry;
    std::vector<AnimationFileEntry> entries(4);
    entries[0] = {"idle.skeleton", true, {"Idle"}, {}};
    entries[1] = {"gone.skeleton", false, {"Gone"}, {}};
    entries[2] = {"ALCHEMIST.skeleton", true, {"Pose", "Bind"}, {}};
    entries[3] = {"run.skeleton", true, {"Run"}, {}};
    const auto list = tl::core::load_animation_list(entries, "ALCHEMIST", {});
    TL_CHECK_EQ(list[0].index, 0);
    TL_CHECK(list[0].counted);
    TL_CHECK_EQ(list[1].index, -1);
    TL_CHECK_EQ(list[2].name, std::string("Bind"));
    TL_CHECK(!list[2].counted);
    TL_CHECK_EQ(list[3].index, 2);

    std::vector<AnimationFileEntry> twice(2);
    twice[0] = {"a.skeleton", true, {"Walk"}, {}};
    twice[1] = {"b.skeleton", true, {"Walk"}, {}};
    const auto dup = tl::core::load_animation_list(twice, "X", {"Walk"});
    TL_CHECK(!dup[0].duplicate);
    TL_CHECK(dup[1].duplicate);
    TL_CHECK_EQ(dup[1].index, -1);
}

// A `Monster` node's `ANIMATION`, `SPEED`, `LOOPS` setters
// replay the kept animation; out of range is the first.
TL_TEST(a_layout_nodes_animation_setters_replay_the_kept_animation) {
    ModelAnimator m = two_animations();
    TL_CHECK_EQ(m.played_index(), 0u);
    TL_CHECK(m.played_loop());
    TL_CHECK_EQ(m.played_speed(), 1.0f);
    TL_CHECK_EQ(m.descriptor_index("Attack1"), 1u);
    TL_CHECK_EQ(m.descriptor_index("ATTACK1"), 0u);
    TL_CHECK_EQ(m.descriptor_index("bind"), 0u);

    m.set_animation_playing(1);
    TL_CHECK_EQ(m.active().size(), 1u);
    TL_CHECK_EQ(m.active().front().index, 1u);
    TL_CHECK(m.active().front().loop);
    m.set_animation_speed(0.5f);
    TL_CHECK_EQ(m.active().front().speed, 0.5f);
    m.set_animation_loop(false);
    TL_CHECK(!m.active().front().loop);
    TL_CHECK_EQ(m.active().front().index, 1u);

    m.set_animation_playing(7);
    TL_CHECK_EQ(m.played_index(), 0u);
    TL_CHECK_EQ(m.active().front().index, 0u);
    TL_CHECK(!m.active().front().loop);
    TL_CHECK_EQ(m.active().front().speed, 0.5f);

    // No animations: kept, nothing played.
    ModelAnimator none;
    none.set_animation_playing(3);
    none.set_animation_speed(2.0f);
    TL_CHECK_EQ(none.played_index(), 3u);
    TL_CHECK(none.active().empty());
}
