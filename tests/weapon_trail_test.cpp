// Unit tests for WeaponTrail.
// Points are made up in code; no game data files are accessed.

#include "core/weapon_trail.h"

#include <cmath>
#include "test_support.h"

namespace {

using tl::core::TrailQuaternion;
using tl::core::Vec3;
using tl::core::WeaponTrail;
using tl::core::WeaponTrailNode;
using tl::core::WeaponTrailOwner;
using tl::core::WeaponTrailVertex;

bool close(float a, float b, float eps = 1e-5f) { return std::abs(a - b) < eps; }

// A node with no turn of its own under a parent with none: its derived
// position is its position, and its local Y is the world's.
struct PlainNode : WeaponTrailNode {
    Vec3 at;
    int translated = 0;
    int restored = 0;
    Vec3 derived_position() override { return at; }
    Vec3 position() override { return at; }
    void translate_local(const Vec3& d) override {
        at = at + d;
        ++translated;
    }
    void set_position(const Vec3& p) override {
        at = p;
        ++restored;
    }
};

}  // namespace

// `update`: not active and nothing in the list - nothing happens,
// not even `setVisible(true)`.
TL_TEST(weapon_trail_idle_update_does_nothing) {
    WeaponTrail trail(20, 1.0f);
    const Vec3 point{1.0f, 2.0f, 3.0f};
    const Vec3 across{0.0f, 1.0f, 0.0f};
    trail.update(0.016f, &point, &across, false, nullptr);
    TL_CHECK(!trail.drawn_visible());
    TL_CHECK(!trail.take_rebuilt());
    TL_CHECK(!trail.is_visible());
    TL_CHECK_EQ(trail.vertices().size(), static_cast<size_t>(40));
}

// The form with no node: half the width either side of the point,
// and one segment alone is V 1 with the rest of the strip folded onto it.
TL_TEST(weapon_trail_point_form_spans_half_the_width) {
    WeaponTrail trail(2, 0.4f);
    trail.set_active(true);
    const Vec3 point{1.0f, 2.0f, 3.0f};
    const Vec3 across{0.0f, 1.0f, 0.0f};
    trail.update(0.0f, &point, &across, false, nullptr);
    TL_CHECK(trail.drawn_visible());
    TL_CHECK(trail.take_rebuilt());
    TL_CHECK(!trail.take_rebuilt());
    TL_CHECK(trail.is_visible());
    TL_CHECK_EQ(trail.segments().size(), static_cast<size_t>(1));
    const std::vector<WeaponTrailVertex>& v = trail.vertices();
    TL_CHECK_EQ(v.size(), static_cast<size_t>(4));
    TL_CHECK(close(v[0].position.y, 1.8f));
    TL_CHECK(close(v[1].position.y, 2.2f));
    TL_CHECK_EQ(v[0].position.x, 1.0f);
    TL_CHECK_EQ(v[1].position.z, 3.0f);
    TL_CHECK(v[0].attributed && v[1].attributed);
    TL_CHECK_EQ(v[0].u, 0.0f);
    TL_CHECK_EQ(v[1].u, 1.0f);
    // No length at all is taken as 0.01, so the head is still V 1.
    TL_CHECK_EQ(v[0].v, 1.0f);
    TL_CHECK_EQ(v[1].v, 1.0f);
    // Padding: bare positions on the tail's near edge.
    TL_CHECK(!v[2].attributed && !v[3].attributed);
    TL_CHECK(close(v[2].position.y, 1.8f));
    TL_CHECK(close(v[3].position.y, 1.8f));
}

// The newest segment is the head: V 1 there, and the tail's
// `1 - distance / total` of 0 is held at 0.01.
TL_TEST(weapon_trail_v_falls_from_the_head_by_distance) {
    WeaponTrail trail(20, 1.0f);
    trail.set_active(true);
    const Vec3 across{0.0f, 1.0f, 0.0f};
    const Vec3 a{0.0f, 0.0f, 0.0f};
    const Vec3 b{3.0f, 0.0f, 0.0f};
    const Vec3 c{3.0f, 0.0f, 1.0f};
    trail.update(0.0f, &a, &across, false, nullptr);
    trail.update(0.0f, &b, &across, false, nullptr);
    trail.update(0.0f, &c, &across, false, nullptr);
    TL_CHECK_EQ(trail.segments().size(), static_cast<size_t>(3));
    TL_CHECK_EQ(trail.segments().front().end.z, 1.0f);
    TL_CHECK_EQ(trail.segments().back().end.x, 0.0f);
    const std::vector<WeaponTrailVertex>& v = trail.vertices();
    TL_CHECK_EQ(v.size(), static_cast<size_t>(40));
    TL_CHECK_EQ(v[0].v, 1.0f);       // c, the head
    TL_CHECK(close(v[2].v, 0.75f));  // b, 1 of 4 along
    TL_CHECK_EQ(v[4].v, 0.01f);      // a, the tail
    TL_CHECK_EQ(v[5].v, 0.01f);
    TL_CHECK(!v[6].attributed);
    TL_CHECK_EQ(v[39].position.x, 0.0f);
    TL_CHECK(close(v[39].position.y, -0.5f));
}

// a full list drops its tail before the new head goes on.
TL_TEST(weapon_trail_full_list_drops_its_tail) {
    WeaponTrail trail(2, 1.0f);
    trail.set_active(true);
    const Vec3 across{0.0f, 1.0f, 0.0f};
    for (int i = 0; i < 5; ++i) {
        const Vec3 p{static_cast<float>(i), 0.0f, 0.0f};
        trail.update(0.0f, &p, &across, false, nullptr);
    }
    TL_CHECK_EQ(trail.segments().size(), static_cast<size_t>(2));
    TL_CHECK_EQ(trail.segments().front().start.x, 4.0f);
    TL_CHECK_EQ(trail.segments().back().start.x, 3.0f);
    TL_CHECK_EQ(trail.vertices().size(), static_cast<size_t>(4));
    TL_CHECK(trail.vertices()[3].attributed);
}

// The node form: the node, then the node `width` up its own Y, and
// the node put back where it was.
TL_TEST(weapon_trail_node_form_reaches_up_the_nodes_y) {
    WeaponTrail trail(20, 1.5f);
    PlainNode node;
    node.at = {2.0f, 1.0f, -1.0f};
    trail.set_weapon_entity(&node);
    trail.set_active(true);
    TL_CHECK(trail.has_node());
    TL_CHECK(trail.has_entity());
    trail.update(0.016f, nullptr, nullptr, false, nullptr);
    TL_CHECK_EQ(node.translated, 1);
    TL_CHECK_EQ(node.restored, 1);
    TL_CHECK_EQ(node.at.y, 1.0f);
    const auto& head = trail.segments().front();
    TL_CHECK_EQ(head.start.y, 1.0f);
    TL_CHECK_EQ(head.end.y, 2.5f);
    TL_CHECK_EQ(head.end.x, 2.0f);
}

// only a bare node (a bone) is carried through the owner's node; a
// weapon entity's tag point is in the world already.
TL_TEST(weapon_trail_owner_carries_a_bone_not_an_entity) {
    WeaponTrailOwner owner;
    // A quarter turn about Y: X goes to -Z.
    const float h = std::sqrt(0.5f);
    owner.orientation = TrailQuaternion{h, 0.0f, h, 0.0f};
    owner.position = {10.0f, 0.0f, 0.0f};

    PlainNode bone;
    bone.at = {1.0f, 0.0f, 0.0f};
    WeaponTrail on_bone(20, 1.0f);
    on_bone.set_weapon_node(&bone);
    on_bone.set_active(true);
    TL_CHECK(!on_bone.has_entity());
    on_bone.update(0.016f, nullptr, nullptr, false, &owner);
    const auto& moved = on_bone.segments().front();
    TL_CHECK(close(moved.start.x, 10.0f));
    TL_CHECK(close(moved.start.z, -1.0f));
    TL_CHECK(close(moved.end.x, 10.0f));
    TL_CHECK(close(moved.end.y, 1.0f));
    TL_CHECK(close(moved.end.z, -1.0f));

    PlainNode tag;
    tag.at = {1.0f, 0.0f, 0.0f};
    WeaponTrail on_entity(20, 1.0f);
    on_entity.set_weapon_entity(&tag);
    on_entity.set_active(true);
    on_entity.update(0.016f, nullptr, nullptr, false, &owner);
    TL_CHECK_EQ(on_entity.segments().front().start.x, 1.0f);
    TL_CHECK_EQ(on_entity.segments().front().start.z, 0.0f);
}

// the key's `BONEOFFSET` is scaled by the trail's direction a component at a time.
TL_TEST(weapon_trail_offset_is_scaled_by_the_direction) {
    PlainNode node;
    node.at = {0.0f, 2.0f, 0.0f};
    WeaponTrail trail(20, 1.0f);
    trail.set_weapon_node(&node);
    trail.set_active(true);
    trail.offset = {4.0f, -0.5f, 4.0f};
    trail.update(0.016f, nullptr, nullptr, false, nullptr);
    const auto& head = trail.segments().front();
    TL_CHECK_EQ(head.start.x, 0.0f);
    TL_CHECK_EQ(head.start.y, 1.5f);
    TL_CHECK_EQ(head.end.y, 2.5f);
    TL_CHECK_EQ(head.end.z, 0.0f);
}

// `setActive`: the list goes, the fade is whole again, and turning
// it off lets go of the node.
TL_TEST(weapon_trail_set_active_clears) {
    PlainNode node;
    WeaponTrail trail(20, 1.0f);
    trail.set_weapon_entity(&node);
    trail.set_active(true);
    trail.update(0.016f, nullptr, nullptr, false, nullptr);
    TL_CHECK(trail.is_visible());
    TL_CHECK(trail.has_node());
    trail.set_active(true);
    TL_CHECK(!trail.is_visible());
    TL_CHECK(trail.has_node());
    trail.update(0.016f, nullptr, nullptr, false, nullptr);
    trail.set_active(false);
    TL_CHECK(!trail.is_active());
    TL_CHECK(!trail.is_visible());
    TL_CHECK(!trail.has_node());
    TL_CHECK(!trail.has_entity());
    // `setActive` hides nothing: its callers pair it with `setVisible(false)`.
    TL_CHECK(trail.drawn_visible());
    trail.set_visible(false);
    TL_CHECK(!trail.drawn_visible());
}

// The tracer (`performAttack`): each update takes `dt / 0.5` off the fade and scales
// every colour by what is left.
TL_TEST(weapon_trail_tracer_fades_over_half_a_second) {
    WeaponTrail trail(2, 0.4f);
    trail.set_active(true);
    const Vec3 across{0.0f, 0.0f, 1.0f};
    const Vec3 muzzle{0.0f, 1.0f, 0.0f};
    const Vec3 hit{5.0f, 1.0f, 0.0f};
    trail.update(0.0f, &muzzle, &across, false, nullptr);
    trail.update(0.0f, &hit, &across, false, nullptr);
    trail.start_fading();
    TL_CHECK(trail.take_rebuilt());
    TL_CHECK_EQ(trail.vertices()[0].colour.a, 1.0f);

    // `CCharacter::updateAnimation`'s call: no point, no node, not kept up.
    trail.update(0.125f, nullptr, nullptr, false, nullptr);
    TL_CHECK(trail.take_rebuilt());
    TL_CHECK_EQ(trail.fade(), 0.75f);
    TL_CHECK_EQ(trail.segments().size(), static_cast<size_t>(2));
    TL_CHECK_EQ(trail.vertices()[0].colour.r, 0.75f);
    TL_CHECK_EQ(trail.vertices()[3].colour.a, 0.75f);
    TL_CHECK_EQ(trail.vertices()[0].position.x, 5.0f);
    TL_CHECK(trail.drawn_visible());

    trail.update(0.25f, nullptr, nullptr, false, nullptr);
    TL_CHECK_EQ(trail.fade(), 0.25f);
    trail.update(0.125f, nullptr, nullptr, false, nullptr);
    TL_CHECK(!trail.drawn_visible());
    TL_CHECK(!trail.is_active());
    TL_CHECK(!trail.is_visible());
    TL_CHECK(!trail.fading());
    TL_CHECK_EQ(trail.fade(), 1.0f);
    // And an update after that is the idle one.
    trail.take_rebuilt();
    trail.update(0.125f, nullptr, nullptr, false, nullptr);
    TL_CHECK(!trail.take_rebuilt());
    TL_CHECK(!trail.drawn_visible());
}

// kept up, a fading trail still takes its new head, and the fade
// leaves the rewrite to the update.
TL_TEST(weapon_trail_fading_and_kept_up_still_grows) {
    PlainNode node;
    WeaponTrail trail(20, 1.0f);
    trail.set_weapon_entity(&node);
    trail.set_active(true);
    trail.update(0.016f, nullptr, nullptr, true, nullptr);
    trail.start_fading();
    node.at = {1.0f, 0.0f, 0.0f};
    trail.update(0.25f, nullptr, nullptr, true, nullptr);
    TL_CHECK_EQ(trail.segments().size(), static_cast<size_t>(2));
    TL_CHECK_EQ(trail.fade(), 0.5f);
    TL_CHECK_EQ(trail.vertices()[0].colour.g, 0.5f);
}

TL_TEST(weapon_trail_key_hand) {
    // `PARTICLECOUNT` (`CKeyframe +0x48`): below 2 the right hand's trail.
    TL_CHECK(!tl::core::trail_key_is_left(0));
    TL_CHECK(!tl::core::trail_key_is_left(1));
    TL_CHECK(tl::core::trail_key_is_left(2));
    TL_CHECK(tl::core::trail_key_is_left(3));
}

namespace {

using tl::core::AnimationTag;
using tl::core::CharacterWeaponTrails;
using tl::core::TrailCharacter;
using tl::core::TrailWeaponInUse;

AnimationTag trail_key(const char* name, uint32_t count) {
    AnimationTag key;
    key.name = name;
    key.particle_count = count;
    return key;
}

}  // namespace

// `selectAttack`: the weapon in slot 0 gets the right hand's trail,
// on its tag point, in the side's material; `NO TRAIL` (every bow) gets none.
TL_TEST(character_trails_select_attack_makes_the_hand_trail) {
    PlainNode sword;
    TrailCharacter who;
    who.in_use = TrailWeaponInUse::Right;
    who.hands[0].held = true;
    who.hands[0].node = &sword;
    who.good = true;
    CharacterWeaponTrails trails;
    trails.select_attack(who, false);
    TL_CHECK(trails.right() != nullptr);
    TL_CHECK(trails.left() == nullptr);
    TL_CHECK(trails.right()->has_entity());
    TL_CHECK(!trails.right()->is_active());
    TL_CHECK_EQ(trails.right()->material(), std::string("WeaponTrail"));
    TL_CHECK_EQ(trails.right()->max_segments(), 20);
    TL_CHECK_EQ(trails.right()->width(), 1.0f);

    CharacterWeaponTrails bow;
    who.hands[0].no_trail = true;
    bow.select_attack(who, false);
    TL_CHECK(bow.right() == nullptr);

    // Unarmed, with an attack of its own: the right trail, on nothing, and
    // an evil side's material.
    CharacterWeaponTrails claws;
    TrailCharacter monster;
    claws.select_attack(monster, true);
    TL_CHECK(claws.right() != nullptr);
    TL_CHECK(!claws.right()->has_node());
    TL_CHECK_EQ(claws.right()->material(), std::string("WeaponTrailEvil"));
}

// Key 7 with slot 0's weapon in use: the right hand's trail starts on the weapon, unoffset.
TL_TEST(character_trails_show_key_starts_the_weapon_trail) {
    PlainNode sword;
    sword.at = {0.0f, 1.0f, 0.0f};
    TrailCharacter who;
    who.in_use = TrailWeaponInUse::Right;
    who.hands[0].held = true;
    who.hands[0].node = &sword;
    CharacterWeaponTrails trails;
    AnimationTag key = trail_key("SHOWWEAPONTRAIL", 1);
    key.offset_y = -0.5f;
    // Not made yet (no `selectAttack`): key 7 does not make it.
    trails.show_key(key, who, nullptr);
    TL_CHECK(trails.right() == nullptr);
    trails.select_attack(who, false);
    const tl::core::TrailKeyMaterial material = trails.show_key(key, who, nullptr);
    TL_CHECK(trails.right()->is_active());
    TL_CHECK_EQ(trails.right()->offset.y, 0.0f);
    TL_CHECK_EQ(material.material, std::string("WeaponTrailEvil"));
    TL_CHECK(!material.from_layout);
    trails.update(0.016f, WeaponTrailOwner{});
    TL_CHECK_EQ(trails.right()->segments().size(), static_cast<size_t>(1));
    TL_CHECK_EQ(trails.right()->segments().front().end.y, 2.0f);

    // Key 8 for the same hand: off and hidden at once.
    trails.hide_key(trail_key("HIDEWEAPONTRAIL", 1), who);
    TL_CHECK(!trails.right()->is_active());
    TL_CHECK(!trails.right()->drawn_visible());
    TL_CHECK(!trails.right()->is_visible());
}

// Key 7 with no weapon in use: the key's bone, with its offset, and its
// `LAYOUT` as every hand trail's material.
TL_TEST(character_trails_unarmed_key_draws_from_its_bone) {
    PlainNode hand;
    TrailCharacter who;
    CharacterWeaponTrails trails;
    AnimationTag key = trail_key("SHOWWEAPONTRAIL", 2);
    key.bone = "tag_lefthand";
    key.layout = "media/sharedtextures/claw.dds";
    key.offset_y = -0.5f;
    const tl::core::TrailKeyMaterial material = trails.show_key(key, who, &hand);
    TL_CHECK(trails.right() == nullptr);
    TL_CHECK(trails.left() != nullptr);
    TL_CHECK(trails.left()->is_active());
    TL_CHECK(trails.left()->has_node());
    TL_CHECK(!trails.left()->has_entity());
    TL_CHECK_EQ(trails.left()->offset.y, -0.5f);
    TL_CHECK(material.from_layout);
    TL_CHECK_EQ(trails.left()->material(), std::string("media/sharedtextures/claw.dds"));

    // A key naming no bone makes the trail and starts nothing.
    CharacterWeaponTrails plain;
    plain.show_key(trail_key("SHOWWEAPONTRAIL", 1), who, nullptr);
    TL_CHECK(plain.right() != nullptr);
    TL_CHECK(!plain.right()->is_active());

    // Key 8 unarmed hides only a trail with something in it.
    trails.update(0.016f, WeaponTrailOwner{});
    trails.hide_key(trail_key("HIDEWEAPONTRAIL", 2), who);
    TL_CHECK(!trails.left()->is_active());
}

// The left hand's key: what slot 1 holds draws it for a count of exactly 2,
// whichever weapon is in use.
TL_TEST(character_trails_left_key_wants_count_two) {
    PlainNode dagger;
    TrailCharacter who;
    who.in_use = TrailWeaponInUse::Right;
    who.hands[0].held = true;
    who.hands[1].held = true;
    who.hands[1].node = &dagger;
    CharacterWeaponTrails trails;
    trails.show_key(trail_key("SHOWWEAPONTRAIL", 3), who, nullptr);
    TL_CHECK(trails.left() != nullptr);
    TL_CHECK(!trails.left()->is_active());
    trails.show_key(trail_key("SHOWWEAPONTRAIL", 2), who, nullptr);
    TL_CHECK(trails.left()->is_active());
    TL_CHECK(trails.left()->has_entity());
    // Key 8 asks for an inventory first: without one, nothing is hidden.
    who.has_inventory = false;
    trails.hide_key(trail_key("HIDEWEAPONTRAIL", 2), who);
    TL_CHECK(trails.left()->is_active());
    who.has_inventory = true;
    trails.hide_key(trail_key("HIDEWEAPONTRAIL", 2), who);
    TL_CHECK(!trails.left()->is_active());
}

// `performAttack`: two segments across the shot, then the fade;
// `hide_all` takes the tracer too, `hide_hands` does not.
TL_TEST(character_trails_tracer_spans_the_shot) {
    CharacterWeaponTrails trails;
    trails.shoot_tracer(Vec3{0.0f, 1.0f, 0.0f}, Vec3{4.0f, 1.0f, 0.0f}, false);
    tl::core::WeaponTrail* tracer = trails.tracer();
    TL_CHECK(tracer != nullptr);
    TL_CHECK_EQ(tracer->max_segments(), 2);
    TL_CHECK(tracer->fading());
    TL_CHECK_EQ(tracer->material(), std::string("ArrowTrail"));
    TL_CHECK_EQ(tracer->segments().size(), static_cast<size_t>(2));
    // Along X, across is -Z..+Z: (1,0,0) x (0,1,0) = (0,0,1).
    const auto& head = tracer->segments().front();
    TL_CHECK_EQ(head.start.x, 4.0f);
    TL_CHECK(close(head.start.z, -0.2f));
    TL_CHECK(close(head.end.z, 0.2f));
    trails.hide_hands();
    TL_CHECK(tracer->is_active());
    trails.shoot_tracer(Vec3{}, Vec3{0.0f, 0.0f, 1.0f}, true);
    TL_CHECK_EQ(tracer->material(), std::string("ArrowTrailReflect"));
    trails.hide_all();
    TL_CHECK(!tracer->is_active());
    TL_CHECK(!tracer->drawn_visible());
}
