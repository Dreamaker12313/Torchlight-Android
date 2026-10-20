// `CWeaponTrail`: see the header.
#include "core/weapon_trail.h"
#include "core/ogre_math.h"

#include <cmath>

namespace tl::core {

Vec3 trail_rotate(const TrailQuaternion& q, const Vec3& v) {
    // `Ogre::Quaternion::operator*(const Vector3&)`, with the console's `fmsubs`.
    return quaternion_rotate(OgreQuaternion{q.w, q.x, q.y, q.z}, v);
}

WeaponTrail::WeaponTrail(int max_segments, float width)
    : max_segments_(max_segments), width_(width) {
    // `CWeaponTrail::CWeaponTrail`, then `init`:
    // `2 * max_segments` vertices, hidden until a rewrite.
    vertices_.resize(static_cast<size_t>(max_segments_ > 0 ? max_segments_ * 2 : 0));
}

void WeaponTrail::set_active(bool active) {
    // `CWeaponTrail::setActive`.
    active_ = active;
    if (!active) {
        entity_ = false;
        node_ = nullptr;
    }
    fade_ = 1.0f;
    segments_.clear();
    fading_ = false;
}

void WeaponTrail::set_visible(bool visible) {
    // `CWeaponTrail::setVisible`; shown, it is put back in render
    // queue 0x5b, where `init` had it.
    drawn_visible_ = visible;
}

void WeaponTrail::set_weapon_entity(WeaponTrailNode* node) {
    // `CWeaponTrail::setWeaponEntity`.
    entity_ = node != nullptr;
    node_ = node;
}

void WeaponTrail::set_weapon_node(WeaponTrailNode* node) {
    // `CWeaponTrail::setWeaponNode`.
    node_ = node;
    entity_ = false;
}

// PORT: whether the trail's strip was rewritten since it was last drawn.
bool WeaponTrail::take_rebuilt() {
    const bool was = rebuilt_;
    rebuilt_ = false;
    return was;
}

void WeaponTrail::rebuild() {
    // The tail `CWeaponTrail::update` and `fadeOut`
    // share: `beginUpdate(0)`, two vertices a segment, the rest of the strip
    // folded onto the tail, `end()`.
    vertices_.clear();
    int written = 0;
    if (!segments_.empty()) {
        // The running length of the far edge, from the head.
        int count = 0;
        float total = 0.0f;
        Vec3 last;
        for (WeaponTrailSegment& segment : segments_) {
            if (count == 0) {
                segment.distance = 0.0f;
            } else {
                const float dx = segment.end.x - last.x;
                const float dy = segment.end.y - last.y;
                const float dz = segment.end.z - last.z;
                total = total + std::sqrt(dx * dx + dy * dy + dz * dz);
                segment.distance = total;
            }
            last = segment.end;
            ++count;
        }
        if (total == 0.0f) total = 0.01f;
        written = count * 2;
        for (const WeaponTrailSegment& segment : segments_) {
            float v = 1.0f - segment.distance / total;
            if (!(0.01f <= v)) v = 0.01f;
            WeaponTrailVertex near_edge;
            near_edge.position = segment.start;
            near_edge.attributed = true;
            near_edge.u = 0.0f;
            near_edge.v = v;
            near_edge.colour = {segment.start_colour.r * fade_, segment.start_colour.g * fade_,
                                segment.start_colour.b * fade_, fade_ * segment.start_colour.a};
            vertices_.push_back(near_edge);
            WeaponTrailVertex far_edge;
            far_edge.position = segment.end;
            far_edge.attributed = true;
            far_edge.u = 1.0f;
            far_edge.v = v;
            far_edge.colour = {segment.end_colour.r * fade_, segment.end_colour.g * fade_,
                               segment.end_colour.b * fade_, fade_ * segment.end_colour.a};
            vertices_.push_back(far_edge);
        }
    }
    // `Vector3::ZERO` for an empty list, else the tail's near edge.
    Vec3 pad;
    if (!segments_.empty()) pad = segments_.back().start;
    for (; written < max_segments_ * 2; ++written) {
        WeaponTrailVertex vertex;
        vertex.position = pad;
        vertices_.push_back(vertex);
    }
    rebuilt_ = true;
}

void WeaponTrail::fade_out(float dt, bool rebuild_strip) {
    // `CWeaponTrail::fadeOut`.
    fade_ = fade_ - dt / fade_time;
    if (fade_ <= 0.0f) {
        set_visible(false);
        set_active(false);
        return;
    }
    drawn_visible_ = true;
    if (!rebuild_strip || !active_) return;
    rebuild();
}

void WeaponTrail::update(float dt, const Vec3* point, const Vec3* direction, bool keep_updating,
                         const WeaponTrailOwner* owner) {
    // `CWeaponTrail::update`.
    if (!active_ && segments_.empty()) return;
    if (fading_) {
        // the fade rewrites the strip itself only when this call
        // goes no further.
        fade_out(dt, !keep_updating);
        if (!keep_updating) return;
    }
    if (point == nullptr && node_ == nullptr) return;
    drawn_visible_ = true;

    // a full list drops its tail.
    if (static_cast<size_t>(static_cast<unsigned>(max_segments_)) <= segments_.size()) {
        segments_.pop_back();
    }
    if (active_) {
        WeaponTrailSegment segment;
        segment.start_colour = start_colour;
        segment.end_colour = end_colour;
        if (node_ == nullptr) {
            // half the width either side of the point.
            const Vec3 half{width_ * direction->x * 0.5f, direction->y * width_ * 0.5f,
                            direction->z * width_ * 0.5f};
            segment.start = {point->x - half.x, point->y - half.y, point->z - half.z};
            segment.end = {half.x + point->x, half.y + point->y, half.z + point->z};
        } else {
            // the node, then the node moved `width` up its own Y and
            // put back.
            segment.start = node_->derived_position();
            const Vec3 kept = node_->position();
            node_->translate_local(Vec3{0.0f, width_, 0.0f});
            segment.end = node_->derived_position();
            node_->set_position(kept);
            if (owner != nullptr && !entity_) {
                // a bare node is a bone, in the model's space.
                const Vec3 turned_end = trail_rotate(owner->orientation, segment.end);
                const Vec3 turned_start = trail_rotate(owner->orientation, segment.start);
                segment.end = {turned_end.x + owner->position.x, turned_end.y + owner->position.y,
                               turned_end.z + owner->position.z};
                segment.start = {owner->position.x + turned_start.x,
                                 owner->position.y + turned_start.y,
                                 owner->position.z + turned_start.z};
            }
            // the key's offset, each component through the same
            // component of the trail's own direction.
            Vec3 along{segment.end.x - segment.start.x, segment.end.y - segment.start.y,
                       segment.end.z - segment.start.z};
            const float length =
                std::sqrt(along.x * along.x + along.y * along.y + along.z * along.z);
            if (1e-08f < length) {
                const float inverse = 1.0f / length;
                along.x = along.x * inverse;
                along.y = along.y * inverse;
                along.z = inverse * along.z;
            }
            const Vec3 shift{offset.x * along.x, along.y * offset.y, along.z * offset.z};
            segment.end = {segment.end.x + shift.x, segment.end.y + shift.y,
                           segment.end.z + shift.z};
            segment.start = {segment.start.x + shift.x, shift.y + segment.start.y,
                             shift.z + segment.start.z};
        }
        // hooked before the head.
        segments_.push_front(segment);
    }
    rebuild();
}

WeaponTrail& CharacterWeaponTrails::make_hand(std::unique_ptr<WeaponTrail>& slot, bool good) {
    // `new CWeaponTrail(scene, uniqueName("trail_"), 20, 1.0)`, then
    // `setMaterialName` by the character's side - the same five lines in
    // `selectAttack` and `updateAnimation`.
    //
    if (!slot) {
        slot = std::make_unique<WeaponTrail>(20, 1.0f);
        slot->set_material_name(hand_trail_material(good));
    }
    return *slot;
}

void CharacterWeaponTrails::select_attack(const TrailCharacter& who, bool unarmed_attack) {
    // `CCharacter::selectAttack`, its trail half.
    if (who.in_use == TrailWeaponInUse::Right) {
        //
        if (who.hands[0].no_trail) return;
        make_hand(right_, who.good).set_weapon_entity(who.hands[0].node);
    } else if (who.in_use == TrailWeaponInUse::Left) {
        //
        if (who.hands[1].no_trail) return;
        make_hand(left_, who.good).set_weapon_entity(who.hands[1].node);
    } else if (unarmed_attack) {
        // One of the unit's own attacks (`+0x3f8`), drawn at random: the
        // right hand's trail, pointed at nothing.
        make_hand(right_, who.good);
    }
}

TrailKeyMaterial CharacterWeaponTrails::show_key(const AnimationTag& key, const TrailCharacter& who,
                                                 WeaponTrailNode* bone) {
    // `CCharacter::updateAnimation`, key 7.
    const bool left = trail_key_is_left(key.particle_count);
    const int count = static_cast<int>(key.particle_count);
    if (who.in_use == TrailWeaponInUse::None) {
        // no weapon in use. The key's hand's trail is made if need
        // be; a key naming a bone the skeleton has draws from that bone.
        WeaponTrail& trail = make_hand(left ? left_ : right_, who.good);
        if (!key.bone.empty() && bone != nullptr) {
            trail.set_weapon_node(bone);
            trail.set_active(true);
            trail.offset = Vec3{key.offset_x, key.offset_y, key.offset_z};
        }
    } else if (who.has_inventory) {
        if (!left) {
            // the right hand's, only if it is the weapon in use and
            // the trail was made (by `selectAttack`).
            if (right_ && who.in_use == TrailWeaponInUse::Right && !who.hands[0].no_trail) {
                right_->set_weapon_entity(who.hands[0].node);
                right_->set_active(true);
                right_->offset = Vec3{};
            }
        } else {
            // the left hand's is made here; what the left hand holds
            // draws it whether or not it is in use, for a count of exactly 2.
            make_hand(left_, who.good);
            const TrailHand& off = who.hands[1];
            if (count == 2 && off.held && !off.no_trail) {
                left_->set_weapon_entity(off.node);
                left_->set_active(true);
                left_->offset = Vec3{};
            }
        }
    }
    // the key's `LAYOUT`, or the side's own, on both hands.
    TrailKeyMaterial out;
    if (key.layout.empty()) {
        out.material = hand_trail_material(who.good);
    } else {
        out.material = key.layout;
        out.from_layout = true;
    }
    if (right_) right_->set_material_name(out.material);
    if (left_) left_->set_material_name(out.material);
    return out;
}

void CharacterWeaponTrails::hide_key(const AnimationTag& key, const TrailCharacter& who) {
    // `CCharacter::updateAnimation`, key 8;
    // `CSkillManager::hideWeaponTrailsOnCurrentSkill`, called first, is empty.
    if (!who.has_inventory) return;
    const bool left = trail_key_is_left(key.particle_count);
    const auto off = [](WeaponTrail& trail) {
        trail.set_active(false);
        trail.set_visible(false);
    };
    if (who.in_use == TrailWeaponInUse::None) {
        if (!left) {
            if (right_ && right_->is_visible()) off(*right_);
        } else if (left_ && left_->is_visible()) {
            off(*left_);
        }
    } else if (!left) {
        if (right_ && who.in_use == TrailWeaponInUse::Right && !who.hands[0].no_trail) {
            off(*right_);
        }
    } else if (static_cast<int>(key.particle_count) == 2 && who.hands[1].held && left_ &&
               !who.hands[1].no_trail) {
        off(*left_);
    }
}

// PORT: both hands' trails stopped and hidden.
void CharacterWeaponTrails::hide_hands() {
    for (std::unique_ptr<WeaponTrail>* slot : {&right_, &left_}) {
        if (!*slot) continue;
        (*slot)->set_active(false);
        (*slot)->set_visible(false);
    }
}

// PORT: every trail of the character stopped and hidden.
void CharacterWeaponTrails::hide_all() {
    for (std::unique_ptr<WeaponTrail>* slot : {&right_, &left_, &tracer_}) {
        if (!*slot) continue;
        (*slot)->set_active(false);
        (*slot)->set_visible(false);
    }
}

void CharacterWeaponTrails::update(float dt, const WeaponTrailOwner& owner) {
    // `CCharacter::updateAnimation`: `update(dt, 0, 0, false,
    // this)` for each hand, `update(dt, 0, 0, false, 0)` for the tracer.
    if (right_) right_->update(dt, nullptr, nullptr, false, &owner);
    if (left_) left_->update(dt, nullptr, nullptr, false, &owner);
    if (tracer_) tracer_->update(dt, nullptr, nullptr, false, nullptr);
}

void CharacterWeaponTrails::shoot_tracer(const Vec3& from, const Vec3& to, bool reflected) {
    // `CCharacter::performAttack`: made at 2
    // segments, 0.4 wide, `ArrowTrail`, and started.
    if (!tracer_) {
        tracer_ = std::make_unique<WeaponTrail>(2, 0.4f);
        tracer_->set_material_name("ArrowTrail");
    }
    tracer_->set_active(true);
    // across the shot - its level direction, normalised, crossed
    // with `UNIT_Y`.
    Vec3 along{to.x - from.x, 0.0f, to.z - from.z};
    const float length = std::sqrt(along.x * along.x + along.y * along.y + along.z * along.z);
    if (1e-08f < length) {
        const float inverse = 1.0f / length;
        along.x = along.x * inverse;
        along.y = along.y * inverse;
        along.z = along.z * inverse;
    }
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 across{along.y * up.z - along.z * up.y, along.z * up.x - along.x * up.z,
                      along.x * up.y - along.y * up.x};
    tracer_->update(0.0f, &from, &across, false, nullptr);
    tracer_->update(0.0f, &to, &across, false, nullptr);
    tracer_->start_fading();
    tracer_->set_material_name(reflected ? "ArrowTrailReflect" : "ArrowTrail");
}

}  // namespace tl::core
