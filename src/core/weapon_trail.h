// `CWeaponTrail`: the ribbon a swung weapon drags, and a shot's tracer; `CharacterWeaponTrails` is
// that part of `CCharacter`. Without its `Ogre::ManualObject`: the strip it would have written is
// kept in `vertices()` for the renderer. No OGRE and no file I/O.
#pragma once

#include "core/animation_manifest.h"
#include "core/collision.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace tl::core {

/// `Ogre::ColourValue`, as the trail stores four of them.
struct TrailColour {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

/// `Ogre::Quaternion`, for the owner's turn.
struct TrailQuaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// `Ogre::Quaternion::operator*(const Vector3&)`.
Vec3 trail_rotate(const TrailQuaternion& q, const Vec3& v);

/// What `CWeaponTrail::update` asks of the node it follows (`+0x38`, an
/// `Ogre::Node`): a weapon entity's tag point, or a bone of the unit's own
/// skeleton.
class WeaponTrailNode {
public:
    virtual ~WeaponTrailNode() = default;
    /// `Node::_getDerivedPosition()`.
    virtual Vec3 derived_position() = 0;
    /// `Node::getPosition()`.
    virtual Vec3 position() = 0;
    /// `Node::translate(d, TS_LOCAL)`.
    virtual void translate_local(const Vec3& d) = 0;
    /// `Node::setPosition()`.
    virtual void set_position(const Vec3& p) = 0;
};

/// The owner's scene node (`CSceneNodeObject +0x58`), derived: a bone's
/// position is in the model's space, and `update` carries it into the world
/// through this - by the turn and the position alone, not the scale.
struct WeaponTrailOwner {
    TrailQuaternion orientation;
    Vec3 position;
};

/// One node of the list (`+0x10`), from its head: the newest first.
struct WeaponTrailSegment {
    Vec3 start;                ///< +0x10, the near edge
    Vec3 end;                  ///< +0x1c, the far edge
    TrailColour start_colour;  ///< +0x28
    TrailColour end_colour;    ///< +0x38
    float distance = 0.0f;     ///< +0x48, along the far edge from the head
};

/// One vertex of the strip as `ManualObject` was handed it. A padding vertex
/// is a bare `position()`: the object repeats the last texture coordinate and
/// colour it was given.
struct WeaponTrailVertex {
    Vec3 position;
    bool attributed = false;
    float u = 0.0f;
    float v = 0.0f;
    TrailColour colour;
};

class WeaponTrail {
public:
    /// `CWeaponTrail::CWeaponTrail(SceneManager*, const std::string&, int,
    /// float)`.
    WeaponTrail(int max_segments, float width);

    /// `CWeaponTrail::setActive`.
    void set_active(bool active);
    /// `CWeaponTrail::isActive`.
    bool is_active() const { return active_; }
    /// `CWeaponTrail::setVisible`.
    void set_visible(bool visible);
    /// `CWeaponTrail::isVisible`: whether the list holds anything,
    /// which is not what `set_visible` last said.
    bool is_visible() const { return !segments_.empty(); }

    /// `CWeaponTrail::setWeaponEntity`: the entity (`+0x30`) and its
    /// parent node (`+0x38`). `node` is that parent; null for no entity.
    void set_weapon_entity(WeaponTrailNode* node);
    /// `CWeaponTrail::setWeaponNode`: a bare node, no entity.
    void set_weapon_node(WeaponTrailNode* node);
    bool has_node() const { return node_ != nullptr; }
    bool has_entity() const { return entity_; }

    /// `CWeaponTrail::update`. `point` and
    /// `direction` are the form with no node: the trail's middle and the way
    /// across it.
    void update(float dt, const Vec3* point, const Vec3* direction, bool keep_updating,
                const WeaponTrailOwner* owner);
    /// `CWeaponTrail::fadeOut`.
    void fade_out(float dt, bool rebuild);

    /// `+0x88`: `CCharacter::performAttack`
    /// raises it on the tracer once its two points are in.
    void start_fading() { fading_ = true; }
    bool fading() const { return fading_; }

    /// `+0x90`: the key's `BONEOFFSET`, multiplied through the trail's own
    /// direction component by component.
    Vec3 offset;
    /// `+0x54`, `+0x64`: `CWeaponTrail::setSegmentStartInitialColourValue`,
    /// `CWeaponTrail::setSegmentEndInitialColourValue`,
    /// `CWeaponTrail::getSegmentStartInitialColourValue`,
    /// `CWeaponTrail::getSegmentEndInitialColourValue`. White; nothing sets them.
    TrailColour start_colour;
    TrailColour end_colour;
    /// `+0x8c`, the seconds a fade takes.
    float fade_time = 0.5f;

    /// `CWeaponTrail::setMaterialName`. `WeaponTrail` until
    /// set, as `init` made it.
    void set_material_name(const std::string& name) { material_ = name; }
    // PORT: an accessor of the port's own bookkeeping.
    const std::string& material() const { return material_; }

    // PORT: an accessor of the port's own bookkeeping.
    int max_segments() const { return max_segments_; }
    float width() const { return width_; }
    float fade() const { return fade_; }
    const std::list<WeaponTrailSegment>& segments() const { return segments_; }

    /// What the `ManualObject` was last told: `setVisible`, and the strip.
    // PORT: an accessor of the port's own bookkeeping.
    bool drawn_visible() const { return drawn_visible_; }
    // PORT: an accessor of the port's own bookkeeping.
    const std::vector<WeaponTrailVertex>& vertices() const { return vertices_; }
    /// True once for each rewrite of the strip.
    bool take_rebuilt();

private:
    void rebuild();

    std::list<WeaponTrailSegment> segments_;  // +0x10
    bool entity_ = false;                     // +0x30
    WeaponTrailNode* node_ = nullptr;         // +0x38
    int max_segments_;                        // +0x50
    float width_;                             // +0x74
    bool active_ = false;                     // +0x78
    float fade_ = 1.0f;                       // +0x7c
    bool fading_ = false;                     // +0x88
    std::string material_ = "WeaponTrail";    // +0x48

    bool drawn_visible_ = false;
    bool rebuilt_ = false;
    std::vector<WeaponTrailVertex> vertices_;
};

/// `CCharacter::updateAnimation`, key 7's two
/// questions about a weapon in use, and key 8's: which hand a key speaks of.
/// `PARTICLECOUNT` below 2 is the right hand's trail, 2 and above the left's.
inline bool trail_key_is_left(uint32_t particle_count) {
    return !(static_cast<int>(particle_count) < 2);
}

/// The material a hand's trail is given when it is made, and by a key with no
/// `LAYOUT`: `alignment() == 1` (`GOOD`) draws `WeaponTrail`, any other side
/// `WeaponTrailEvil` (`selectAttack`,
/// `updateAnimation`).
inline const char* hand_trail_material(bool good) {
    return good ? "WeaponTrail" : "WeaponTrailEvil";
}

/// What `CCharacter`'s trail code asks of the item in one hand slot
/// (`CInventory::getEquipmentEquippedAt(slot)`).
struct TrailHand {
    bool held = false;      ///< anything in the slot
    bool no_trail = false;  ///< ISA 0x66, `NO TRAIL` (every bow is one)
    /// The item model's entity's parent node (`+0x1e0 -> +0x60`, then
    /// `getParentNode`): the tag point it hangs from. Null when it is not drawn.
    WeaponTrailNode* node = nullptr;
};

/// `CCharacter +0x498`, the weapon `selectAttack`
/// put in use: the item in slot 0, the one in
/// slot 1, or none.
enum class TrailWeaponInUse { None, Right, Left };

/// The parts of the character the trail keys read.
struct TrailCharacter {
    bool has_inventory = true;  ///< `+0x490`
    TrailWeaponInUse in_use = TrailWeaponInUse::None;
    TrailHand hands[2];  ///< slots 0 (right) and 1 (left)
    bool good = false;   ///< `alignment() == 1`
};

/// What the key's trail does about its material once the hands are dealt
/// with: every hand trail that exists is given `material`. `from_layout` says
/// it is the key's `LAYOUT` - a texture path, made into a material of that name
/// when none exists yet.
struct TrailKeyMaterial {
    std::string material;
    bool from_layout = false;
};

/// A character's three trails (`CCharacter +0x698`, `+0x6a0`, `+0x6a8`: right, left, a shot's
/// tracer), each made when first needed. The fourth, `displayPath`'s, is a developer aid.
class CharacterWeaponTrails {
public:
    /// `CCharacter::updateAnimation`, key 7
    /// (`SHOWWEAPONTRAIL`). `bone` is the key's `BONE` on the character's
    /// skeleton, null when the key names none or the skeleton lacks it.
    TrailKeyMaterial show_key(const AnimationTag& key, const TrailCharacter& who,
                              WeaponTrailNode* bone);
    /// Key 8 (`HIDEWEAPONTRAIL`).
    void hide_key(const AnimationTag& key, const TrailCharacter& who);

    /// The hands' trails off and hidden: `interrupt`, `update`,
    /// `updateActiveSkill`,
    /// `equipmentUnequipped` for a weapon.
    void hide_hands();
    /// Every trail off and hidden: `completeDeath`,
    /// `setVisible` hiding the character.
    void hide_all();

    /// `updateAnimation`: the
    /// hands' trails follow their nodes, carried by the owner; the tracer only
    /// fades.
    void update(float dt, const WeaponTrailOwner& owner);

    /// `selectAttack`: the weapon put in use
    /// takes its hand's trail, made if need be and pointed at the weapon's
    /// entity, unless it is `NO TRAIL`. With none in use, an unarmed attack
    /// chosen (`+0x3f8`) makes the right hand's trail and points it at nothing.
    void select_attack(const TrailCharacter& who, bool unarmed_attack);

    /// `performAttack` for a bow, crossbow,
    /// rifle or pistol: the tracer made if need be and started, a segment at
    /// `from` (the weapon's tag point) and one at `to` (where the shot ends),
    /// each 0.4 wide across the shot, then left to fade; `ArrowTrailReflect`
    /// for a reflected shot (flag 0x20).
    void shoot_tracer(const Vec3& from, const Vec3& to, bool reflected);

    // PORT: an accessor of the port's own bookkeeping.
    WeaponTrail* right() { return right_.get(); }
    // PORT: an accessor of the port's own bookkeeping.
    WeaponTrail* left() { return left_.get(); }
    // PORT: an accessor of the port's own bookkeeping.
    WeaponTrail* tracer() { return tracer_.get(); }

private:
    WeaponTrail& make_hand(std::unique_ptr<WeaponTrail>& slot, bool good);

    std::unique_ptr<WeaponTrail> right_;   // +0x698
    std::unique_ptr<WeaponTrail> left_;    // +0x6a0
    std::unique_ptr<WeaponTrail> tracer_;  // +0x6a8
};

}  // namespace tl::core
