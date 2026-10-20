// Unit tests for CameraShake (CCameraShake, and around; CCameraControl shake lists).
// Curves are constructed in code; no game data files are accessed.

#include "core/camera_shake.h"
#include "core/stats.h"

#include <cmath>
#include <cstring>
#include "test_support.h"

namespace {

using tl::core::CameraShake;
using tl::core::CameraShakes;
using tl::core::GraphType;
using tl::core::StatBook;
using tl::core::StatGraph;
using tl::core::Vec3f;

// Helper to construct a flat horizontal line curve y = y_val over [0, 1].
StatGraph make_flat_curve(float y_val) {
    StatGraph g(GraphType::Linear);
    g.add_point(0.0f, y_val);
    g.add_point(1.0f, y_val);
    g.process_points();
    return g;
}

// Helper to construct a linear ramp curve from (0, 0) to (1, 1).
StatGraph make_ramp_curve() {
    StatGraph g(GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(1.0f, 1.0f);
    g.process_points();
    return g;
}

float length(const Vec3f& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

bool close(float a, float b, float eps = 1e-4f) { return std::abs(a - b) < eps; }

}  // namespace

// The default constructor's values.
TL_TEST(camera_shake_default_constructor_values) {
    CameraShake shake;
    TL_CHECK_EQ(shake.duration, 0.5f);
    TL_CHECK_EQ(shake.elapsed, 0.0f);
    TL_CHECK_EQ(shake.magnitude, 1.0f);
    TL_CHECK_EQ(shake.direction.x, 0.0f);
    TL_CHECK_EQ(shake.direction.y, 1.0f);
    TL_CHECK_EQ(shake.direction.z, 0.0f);
    TL_CHECK(shake.curve == nullptr);
    TL_CHECK(shake.name.empty());
    TL_CHECK_EQ(shake.orientation, 0);
    TL_CHECK_EQ(shake.origin.x, 0.0f);
    TL_CHECK_EQ(shake.origin.y, 0.0f);
    TL_CHECK_EQ(shake.origin.z, 0.0f);
    TL_CHECK(!shake.has_origin);
    TL_CHECK(shake.falloff);
}

// A shake whose name resolves to no graph in the StatBook:
// update() returns false immediately, leaving offset and elapsed untouched.
// Exact float comparison: zero and initial values unchanged.
TL_TEST(camera_shake_unmapped_curve_returns_false_and_leaves_offset_untouched) {
    StatBook book;
    CameraShake shake;
    shake.set_name("NONEXISTENT_SHAKE", book);
    TL_CHECK(shake.curve == nullptr);

    Vec3f offset{1.0f, 2.0f, 3.0f};
    Vec3f focus{0.0f, 0.0f, 0.0f};
    const bool alive = shake.update(0.1f, focus, offset, true);

    TL_CHECK(!alive);
    TL_CHECK_EQ(shake.elapsed, 0.0f);
    TL_CHECK_EQ(offset.x, 1.0f);
    TL_CHECK_EQ(offset.y, 2.0f);
    TL_CHECK_EQ(offset.z, 3.0f);
}

// Flat curve y = 1, direction (0, 1, 0), magnitude 2, duration 1, no origin:
// update(0.25, ...) adds exactly (0, 2, 0) and returns true.
// Exact float comparison: 2.0f and 0.25f are exact in binary float.
TL_TEST(camera_shake_flat_curve_applies_offset) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShake shake;
    shake.curve = &flat;
    shake.direction = {0.0f, 1.0f, 0.0f};
    shake.magnitude = 2.0f;
    shake.duration = 1.0f;
    shake.has_origin = false;

    Vec3f offset{0.0f, 0.0f, 0.0f};
    Vec3f focus{0.0f, 0.0f, 0.0f};
    const bool alive = shake.update(0.25f, focus, offset, true);

    TL_CHECK(alive);
    TL_CHECK_EQ(shake.elapsed, 0.25f);
    TL_CHECK_EQ(offset.x, 0.0f);
    TL_CHECK_EQ(offset.y, 2.0f);
    TL_CHECK_EQ(offset.z, 0.0f);
}

// A ramp curve (0,0)-(1,1) evaluated midway (dt = 0.5s with duration 1.0s) samples 0.5.
// With magnitude 2.0 and direction (0, 1, 0), added offset is (0, 1.0, 0).
// Exact float comparison: 0.5f and 1.0f are exact in binary float.
TL_TEST(camera_shake_ramp_curve_evaluated_at_t) {
    const StatGraph ramp = make_ramp_curve();
    CameraShake shake;
    shake.curve = &ramp;
    shake.direction = {0.0f, 1.0f, 0.0f};
    shake.magnitude = 2.0f;
    shake.duration = 1.0f;
    shake.has_origin = false;

    Vec3f offset{0.0f, 0.0f, 0.0f};
    Vec3f focus{0.0f, 0.0f, 0.0f};
    const bool alive = shake.update(0.5f, focus, offset, true);

    TL_CHECK(alive);
    TL_CHECK_EQ(shake.elapsed, 0.5f);
    TL_CHECK_EQ(offset.x, 0.0f);
    TL_CHECK_EQ(offset.y, 1.0f);  // 0.5 * 2.0 = 1.0
    TL_CHECK_EQ(offset.z, 0.0f);
}

// Elapsed is clamped at duration and reset to 0 on the finishing call (t >= 1.0),
// which returns false and still adds that call's offset when apply is true.
// Exact float comparison: 0.5f, 1.0f, 0.0f are exact in binary float.
TL_TEST(camera_shake_elapsed_clamped_and_reset_on_completion) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShake shake;
    shake.curve = &flat;
    shake.direction = {0.0f, 1.0f, 0.0f};
    shake.magnitude = 1.0f;
    shake.duration = 1.0f;
    shake.has_origin = false;

    Vec3f offset{0.0f, 0.0f, 0.0f};
    Vec3f focus{0.0f, 0.0f, 0.0f};

    // Step 1: dt = 0.5s -> elapsed = 0.5s, alive = true, offset.y = 1.0f
    bool alive = shake.update(0.5f, focus, offset, true);
    TL_CHECK(alive);
    TL_CHECK_EQ(shake.elapsed, 0.5f);
    TL_CHECK_EQ(offset.y, 1.0f);

    // Step 2: dt = 0.6s -> elapsed clamped to min(1.0, 1.1) = 1.0s,
    // t = 1.0 >= 1.0 -> alive = false, elapsed reset to 0.0f, offset.y += 1.0f -> 2.0f
    alive = shake.update(0.6f, focus, offset, true);
    TL_CHECK(!alive);
    TL_CHECK_EQ(shake.elapsed, 0.0f);
    TL_CHECK_EQ(offset.y, 2.0f);
}

// apply = false adds nothing to offset, but still ages elapsed and still finishes the shake.
// Exact float comparison: 0.0f and 0.5f are exact in binary float.
TL_TEST(camera_shake_apply_false_ages_and_finishes_without_offset) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShake shake;
    shake.curve = &flat;
    shake.direction = {0.0f, 1.0f, 0.0f};
    shake.magnitude = 2.0f;
    shake.duration = 1.0f;
    shake.has_origin = false;

    Vec3f offset{0.0f, 0.0f, 0.0f};
    Vec3f focus{0.0f, 0.0f, 0.0f};

    // Step 1: apply = false -> offset stays 0, elapsed reaches 0.5f
    bool alive = shake.update(0.5f, focus, offset, false);
    TL_CHECK(alive);
    TL_CHECK_EQ(shake.elapsed, 0.5f);
    TL_CHECK_EQ(offset.y, 0.0f);

    // Step 2: apply = false, finish -> offset stays 0, elapsed reset to 0.0f, alive = false
    alive = shake.update(0.6f, focus, offset, false);
    TL_CHECK(!alive);
    TL_CHECK_EQ(shake.elapsed, 0.0f);
    TL_CHECK_EQ(offset.y, 0.0f);
}

// Falloff, horizontal distance only: d < 2 is 1, d >= 12 is 0, and between the original's
// maxss clamps (1 - (d - 2) / 10) back up to 1 - a kept quirk.
TL_TEST(camera_shake_falloff_distance_and_quirk) {
    const StatGraph flat = make_flat_curve(1.0f);
    Vec3f focus{0.0f, 50.0f, 0.0f};  // focus Y is 50

    // Case 1: horizontal distance d = 1.0 (origin at (1, -100, 0))
    {
        CameraShake shake;
        shake.curve = &flat;
        shake.direction = {0.0f, 1.0f, 0.0f};
        shake.magnitude = 1.0f;
        shake.duration = 1.0f;
        shake.origin = {1.0f, -100.0f, 0.0f};
        shake.has_origin = true;
        shake.falloff = true;

        Vec3f offset{0.0f, 0.0f, 0.0f};
        shake.update(0.1f, focus, offset, true);
        TL_CHECK_EQ(offset.y, 1.0f);  // d < 2 -> scale = 1.0f
    }

    // Case 2: horizontal distance d = 7.0 (origin at (7, -100, 0))
    // In original binary, maxss 1.0f, (1.0f - x) clamps back to 1.0f!
    {
        CameraShake shake;
        shake.curve = &flat;
        shake.direction = {0.0f, 1.0f, 0.0f};
        shake.magnitude = 1.0f;
        shake.duration = 1.0f;
        shake.origin = {7.0f, -100.0f, 0.0f};
        shake.has_origin = true;
        shake.falloff = true;

        Vec3f offset{0.0f, 0.0f, 0.0f};
        shake.update(0.1f, focus, offset, true);
        TL_CHECK_EQ(offset.y, 1.0f);  // 2 <= d < 12 -> original bug leaves scale at 1.0f!
    }

    // Case 3: horizontal distance d = 20.0 (origin at (20, -100, 0))
    {
        CameraShake shake;
        shake.curve = &flat;
        shake.direction = {0.0f, 1.0f, 0.0f};
        shake.magnitude = 1.0f;
        shake.duration = 1.0f;
        shake.origin = {20.0f, -100.0f, 0.0f};
        shake.has_origin = true;
        shake.falloff = true;

        Vec3f offset{0.0f, 0.0f, 0.0f};
        shake.update(0.1f, focus, offset, true);
        TL_CHECK_EQ(offset.y, 0.0f);  // d >= 12 -> scale = 0.0f
    }
}

// Origin-Y overwrite: after one update() with has_origin and falloff, the shake's
// stored origin.y is overwritten with focus.y and persists.
// Exact float comparison: focus.y = 42.5f is exact in IEEE 754 float.
TL_TEST(camera_shake_origin_y_overwritten_by_focus_y) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShake shake;
    shake.curve = &flat;
    shake.origin = {5.0f, -123.0f, 5.0f};
    shake.has_origin = true;
    shake.falloff = true;

    Vec3f focus{0.0f, 42.5f, 0.0f};
    Vec3f offset{0.0f, 0.0f, 0.0f};

    shake.update(0.1f, focus, offset, true);
    TL_CHECK_EQ(shake.origin.y, 42.5f);  // origin.y now equals focus.y
}

// The running total is clamped to length 3.5 after each shake, so order changes the result.
TL_TEST(camera_shake_running_total_clamp_and_order_sensitivity) {
    const StatGraph flat = make_flat_curve(1.0f);

    // Shake A: contributes (4, 0, 0) alone (exceeds 3.5)
    // Shake B: contributes (0, 2, 0) alone (does not exceed 3.5)

    // [A, B]: A clamps to (3.5, 0, 0), then the total (3.5, 2, 0) clamps again.
    CameraShakes shakes1;
    {
        CameraShake a;
        a.curve = &flat;
        a.direction = {1.0f, 0.0f, 0.0f};
        a.magnitude = 4.0f;
        a.duration = 1.0f;
        a.has_origin = false;
        shakes1.add(a);

        CameraShake b;
        b.curve = &flat;
        b.direction = {0.0f, 1.0f, 0.0f};
        b.magnitude = 2.0f;
        b.duration = 1.0f;
        b.has_origin = false;
        shakes1.add(b);
    }
    const Vec3f res1 = shakes1.update(0.1f, {0, 0, 0}, false);
    TL_CHECK(close(length(res1), 3.5f));
    TL_CHECK(close(res1.x, 3.0388f, 1e-3f));
    TL_CHECK(close(res1.y, 1.7365f, 1e-3f));

    // [B, A]: B is not clamped; the total (4, 2, 0) clamps once.
    CameraShakes shakes2;
    {
        CameraShake b;
        b.curve = &flat;
        b.direction = {0.0f, 1.0f, 0.0f};
        b.magnitude = 2.0f;
        b.duration = 1.0f;
        b.has_origin = false;
        shakes2.add(b);

        CameraShake a;
        a.curve = &flat;
        a.direction = {1.0f, 0.0f, 0.0f};
        a.magnitude = 4.0f;
        a.duration = 1.0f;
        a.has_origin = false;
        shakes2.add(a);
    }
    const Vec3f res2 = shakes2.update(0.1f, {0, 0, 0}, false);
    TL_CHECK(close(length(res2), 3.5f));
    TL_CHECK(close(res2.x, 3.1305f, 1e-3f));
    TL_CHECK(close(res2.y, 1.5652f, 1e-3f));

    // Swapping order strictly produced different vectors:
    TL_CHECK(!close(res1.x, res2.x, 1e-2f));
    TL_CHECK(!close(res1.y, res2.y, 1e-2f));
}

// A finished owned shake is deleted, a finished borrowed one only dropped, and the
// element swapped into the hole is still evaluated this pass.
TL_TEST(camera_shake_walk_owned_borrowed_and_no_skip) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShakes camera;

    // Shake 0 finishes at dt 0.25; shake 2 is swapped into its slot and must still run.
    CameraShake s0;
    s0.curve = &flat;
    s0.direction = {1.0f, 0.0f, 0.0f};
    s0.magnitude = 1.0f;
    s0.duration = 0.2f;
    s0.has_origin = false;
    camera.add(s0);

    CameraShake s1;
    s1.curve = &flat;
    s1.direction = {0.0f, 1.0f, 0.0f};
    s1.magnitude = 1.0f;
    s1.duration = 1.0f;
    s1.has_origin = false;
    camera.add(s1);

    CameraShake s2;
    s2.curve = &flat;
    s2.direction = {0.0f, 0.0f, 1.0f};
    s2.magnitude = 1.0f;
    s2.duration = 1.0f;
    s2.has_origin = false;
    camera.add(s2);

    TL_CHECK_EQ(camera.owned_count(), 3u);

    // All three contribute this frame.
    const Vec3f offset = camera.update(0.25f, {0, 0, 0}, false);
    TL_CHECK_EQ(offset.x, 1.0f);
    TL_CHECK_EQ(offset.y, 1.0f);
    TL_CHECK_EQ(offset.z, 1.0f);

    // s0 was removed, s1 and s2 remain:
    TL_CHECK_EQ(camera.owned_count(), 2u);

    // Now test borrowed shake:
    CameraShake borrowed_shake;
    borrowed_shake.curve = &flat;
    borrowed_shake.direction = {0.0f, 1.0f, 0.0f};
    borrowed_shake.magnitude = 0.5f;
    borrowed_shake.duration = 0.1f;
    borrowed_shake.has_origin = false;

    borrowed_shake.start({0, 0, 0}, false, camera);
    TL_CHECK_EQ(camera.borrowed_count(), 1u);

    // Updating finishes borrowed_shake:
    camera.update(0.2f, {0, 0, 0}, false);
    TL_CHECK_EQ(camera.borrowed_count(), 0u);
    // Object still exists and can be restarted:
    TL_CHECK_EQ(borrowed_shake.elapsed, 0.0f);
    borrowed_shake.start({0, 0, 0}, false, camera);
    TL_CHECK_EQ(camera.borrowed_count(), 1u);
}

// clear() empties both owned and borrowed lists. Borrowed shake objects survive.
// Exact integer comparison on list sizes.
TL_TEST(camera_shake_clear_empties_both_lists) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShakes camera;

    CameraShake owned;
    owned.curve = &flat;
    camera.add(owned);

    CameraShake borrowed;
    borrowed.curve = &flat;
    camera.add(&borrowed);

    TL_CHECK_EQ(camera.owned_count(), 1u);
    TL_CHECK_EQ(camera.borrowed_count(), 1u);

    camera.clear();

    TL_CHECK_EQ(camera.owned_count(), 0u);
    TL_CHECK_EQ(camera.borrowed_count(), 0u);
    // Borrowed object still valid:
    TL_CHECK(borrowed.curve == &flat);
}

// Clone copies duration, magnitude, direction, orientation, has_origin, falloff,
// and re-resolves curve by name, but does NOT copy elapsed or origin.
// Exact float comparison: copied values are exact.
TL_TEST(camera_shake_clone_fields_and_omissions) {
    StatBook book;
    StatGraph flat = make_flat_curve(1.0f);
    book.add("TEST_SHAKE", flat);

    CameraShake src("TEST_SHAKE", {1.0f, 0.0f, 0.0f}, 0.75f, book);
    src.magnitude = 2.5f;
    src.orientation = 42;
    src.has_origin = true;
    src.falloff = false;
    src.origin = {10.0f, 20.0f, 30.0f};
    src.elapsed = 0.35f;

    CameraShake dst;
    dst.origin = {1.0f, 2.0f, 3.0f};
    dst.elapsed = 0.1f;
    dst.clone_from(src, book);

    // Copied fields:
    TL_CHECK_EQ(dst.duration, 0.75f);
    TL_CHECK_EQ(dst.magnitude, 2.5f);
    TL_CHECK_EQ(dst.direction.x, 1.0f);
    TL_CHECK_EQ(dst.orientation, 42);
    TL_CHECK(dst.has_origin);
    TL_CHECK(!dst.falloff);
    TL_CHECK(dst.curve != nullptr);
    TL_CHECK_EQ(dst.name, "TEST_SHAKE");

    // NOT copied fields (retain dst's original values):
    TL_CHECK_EQ(dst.elapsed, 0.1f);
    TL_CHECK_EQ(dst.origin.x, 1.0f);
    TL_CHECK_EQ(dst.origin.y, 2.0f);
    TL_CHECK_EQ(dst.origin.z, 3.0f);
}

// A zero duration makes t = 0 / 0, a NaN; the original's ucomiss/jb reads an
// unordered t as below 1, so the shake reports alive for ever and elapsed stays 0.
// Exact comparisons: no arithmetic on the checked values.
TL_TEST(camera_shake_zero_duration_never_finishes) {
    const StatGraph flat = make_flat_curve(1.0f);
    CameraShake shake;
    shake.curve = &flat;
    shake.duration = 0.0f;
    Vec3f offset{0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        TL_CHECK(shake.update(0.5f, {0, 0, 0}, offset, false));
        TL_CHECK_EQ(shake.elapsed, 0.0f);
    }
}

// The falloff's far edge is where (d - 2) / 10 reaches 1, computed as the original
// does: exactly 12 units away is silenced, just inside is full strength.
TL_TEST(camera_shake_falloff_edge_at_twelve) {
    const StatGraph flat = make_flat_curve(1.0f);
    for (float d : {12.0f, 11.5f}) {
        CameraShake shake;
        shake.curve = &flat;
        shake.duration = 1.0f;
        shake.origin = {d, 0.0f, 0.0f};
        shake.has_origin = true;
        Vec3f offset{0.0f, 0.0f, 0.0f};
        shake.update(0.1f, {0, 0, 0}, offset, true);
        TL_CHECK_EQ(offset.y, d == 12.0f ? 0.0f : 1.0f);
    }
}

namespace {

tl::formats::CmpField word_field(std::vector<uint32_t> words) {
    tl::formats::CmpField f;
    f.words = std::move(words);
    return f;
}

uint32_t float_bits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return bits;
}

/// A string field as the compiled form stores it: UTF-16LE with a NUL, padded
/// to whole dwords.
tl::formats::CmpField string_field(const std::string& text) {
    std::vector<uint8_t> payload(((text.size() + 1) * 2 + 3) / 4 * 4, 0);
    for (size_t i = 0; i < text.size(); ++i) payload[2 * i] = static_cast<uint8_t>(text[i]);
    std::vector<uint32_t> words(payload.size() / 4);
    for (size_t i = 0; i < words.size(); ++i) {
        words[i] = static_cast<uint32_t>(payload[4 * i]) |
                   (static_cast<uint32_t>(payload[4 * i + 1]) << 8) |
                   (static_cast<uint32_t>(payload[4 * i + 2]) << 16) |
                   (static_cast<uint32_t>(payload[4 * i + 3]) << 24);
    }
    return word_field(words);
}

}  // namespace

// `CCameraShakeDescriptor`'s six fields (solved over the archive's
// `Camera Shake` nodes): the trap explosion's `GOBLINEXPLODE`, 0.25 s at 0.9, here turned
// relative and not fading.
TL_TEST(camera_shake_from_node_reads_the_descriptor_fields) {
    StatBook graphs;
    graphs.add("GOBLINEXPLODE", make_flat_curve(1.0f));
    tl::formats::CmpNode node;
    node.type = tl::formats::cmp_type::kCameraShake;
    node.fields = {
        word_field({float_bits(1.0f), float_bits(0.0f), float_bits(0.0f)}),
        word_field({float_bits(0.25f)}),
        word_field({float_bits(0.9f)}),
        string_field("RELATIVE"),
        string_field("goblinExplode"),
        word_field({0u}),
    };
    const CameraShake shake = tl::core::camera_shake_from_node(node, graphs);
    TL_CHECK_EQ(shake.direction.x, 1.0f);
    TL_CHECK_EQ(shake.direction.y, 0.0f);
    TL_CHECK_EQ(shake.duration, 0.25f);
    TL_CHECK_EQ(shake.magnitude, 0.9f);
    TL_CHECK_EQ(shake.orientation, 1);
    TL_CHECK(!shake.falloff);
    TL_CHECK(shake.name == "GOBLINEXPLODE");
    TL_CHECK(shake.curve != nullptr);
}

// A node with no fields keeps `CCameraShake::CCameraShake`'s values.
TL_TEST(camera_shake_from_node_keeps_the_constructor_defaults) {
    StatBook graphs;
    tl::formats::CmpNode node;
    node.type = tl::formats::cmp_type::kCameraShake;
    const CameraShake shake = tl::core::camera_shake_from_node(node, graphs);
    TL_CHECK_EQ(shake.duration, 0.5f);
    TL_CHECK_EQ(shake.magnitude, 1.0f);
    TL_CHECK_EQ(shake.direction.y, 1.0f);
    TL_CHECK_EQ(shake.orientation, 0);
    TL_CHECK(shake.falloff);
    TL_CHECK(shake.curve == nullptr);
}
