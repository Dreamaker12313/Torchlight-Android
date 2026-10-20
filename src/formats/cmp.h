// `.cmp`, Torchlight's compiled layout format (levels, props, particles, menu backdrops).
// Container nodes carry undecoded trailing bytes; the reader resynchronises past them and
// counts it in `resyncs`.
#pragma once

#include "formats/layout_schema.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

struct CmpVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// One field of a node's stream: a dword count and the dwords themselves.
/// Raw bits, because a dword's type is positional: the caller must know the descriptor's schema.
struct CmpField {
    std::vector<uint32_t> words;

    /// False for a property a text layout never set (`CDescriptorProp::
    /// loadProperty` keeps the object's default); `CmpNode::field` answers null for it.
    bool present = true;

    size_t count() const { return words.size(); }

    /// The dword at `index` as a float, or 0 past the end (an older node is shorter: the default).
    float as_float(size_t index = 0) const;

    uint32_t as_uint(size_t index = 0) const { return index < words.size() ? words[index] : 0u; }

    bool as_bool(size_t index = 0) const { return as_uint(index) != 0u; }

    /// The field decoded as text (UTF-16LE with a NUL, padded to whole dwords), or empty.
    std::string as_text() const;
};

/// One node of a layout: a named thing with a transform and some references.
struct CmpNode {
    /// Descriptor type tag; compare against `cmp_type`.
    uint32_t type = 0;

    std::string name;

    /// Every three-dword field, in file order; `vec3s[0]` is usually the position.
    /// A convenience view: a 4-5 character string pads to three dwords and lands here, not in
    /// `strings`. A caller that knows the descriptor reads `fields` by index.
    std::vector<CmpVec3> vec3s;

    /// Text fields (uppercase archive paths for asset references). Misses 4-5 character
    /// strings; see `vec3s`.
    std::vector<std::string> strings;

    /// Every field, in file order: the authoritative view.
    std::vector<CmpField> fields;

    /// The field at `index`, or nullptr when the node is shorter than its schema (not an error).
    const CmpField* field(size_t index) const {
        return index < fields.size() && fields[index].present ? &fields[index] : nullptr;
    }

    /// Fill `vec3s` and `strings` from `fields` as the compiled reader does: a present
    /// three-dword field is a vector, any other that decodes as text a string.
    void rebuild_views();

    /// This node's 64-bit id and its parent's (the source's `ID`/`PARENTID`); -1 is the root.
    int64_t id = 0;
    int64_t parent_id = -1;

    /// True when this node sits at the root rather than inside a group.
    bool root = false;

    bool has_position() const { return !vec3s.empty(); }
    CmpVec3 position() const { return vec3s.empty() ? CmpVec3{} : vec3s.front(); }

    /// The first string ending in `extension` (case-insensitive), or empty. Paths are
    /// uppercase; resolve the result through the case-insensitive lookup.
    std::string reference(const char* extension) const;
};

/// Node type tags: a node's descriptor, by its registration order.
namespace cmp_type {
constexpr uint32_t kGroup = 0;                 ///< "Group"
constexpr uint32_t kMonster = 1;               ///< "Monster" - an NPC or creature
constexpr uint32_t kItem = 2;                  ///< "Item" - an item lying in the level; none ships
constexpr uint32_t kLayoutLink = 5;            ///< "Layout Link"
constexpr uint32_t kParticleLink = 6;          ///< "Layout Link Particle"
constexpr uint32_t kModel = 9;                 ///< "Generic Model"
constexpr uint32_t kRoomPiece = 10;            ///< "Room Piece"
constexpr uint32_t kLight = 11;                ///< "Light"
constexpr uint32_t kSound = 12;                ///< "Sound"
constexpr uint32_t kMusic = 13;                ///< "Music" - a level's own track"
constexpr uint32_t kParticle = 17;             ///< "Particle" - one billboard system
constexpr uint32_t kEmitter = 18;              ///< "Emitter" - child of a Particle
constexpr uint32_t kScaleOverTime = 25;        ///< "Scale" - child of a Particle
constexpr uint32_t kTextureRotate = 28;        ///< "Texture Rotate"
constexpr uint32_t kTextureAnimation = 29;     ///< "Texture Animation" - sheet playback
constexpr uint32_t kColorOverTime = 30;        ///< "Color" - child of a Particle
constexpr uint32_t kMarker = 45;               ///< "Property Node"
constexpr uint32_t kAnimationController = 53;  ///< "Animation Controller"
constexpr uint32_t kSkillController = 50;      ///< "Skill Controller"
/// Not placed by the port; named because timeline tracks target them.
constexpr uint32_t kLinearForce = 21;  ///< "Linear Force" - particle affector
constexpr uint32_t kSineForce = 22;    ///< "Sine Force" - particle affector
constexpr uint32_t kVortex = 23;       ///< "Vortext" (sic) - particle affector
constexpr uint32_t kGravityWell = 24;  ///< "Gravity Well" - particle affector
constexpr uint32_t kAlign = 19;        ///< "Align" - particle affector
/// Ids by `CDescriptorController::CDescriptorController`;
/// looked up by `CDescriptorManager::GetDescriptorByCreated`,
/// via `getNewDescriptorByCreatedIndex`.
constexpr uint32_t kLine = 20;             ///< "Line" - particle affector
constexpr uint32_t kBoxCollision = 31;     ///< "Box Collision" - particle affector
constexpr uint32_t kJet = 26;              ///< "Jet" - particle affector
constexpr uint32_t kGeometryRotator = 27;  ///< "Geometry Rotator" - particle affector
constexpr uint32_t kPlaneCollision = 32;   ///< "Plane Collision" - particle affector
constexpr uint32_t kSphereCollision = 33;  ///< "Sphere Collision" - particle affector
constexpr uint32_t kDamageShape = 55;      ///< "Damage Shape"
constexpr uint32_t kCameraShake = 44;      ///< "Camera Shake"
constexpr uint32_t kTeleport = 41;         ///< "Teleport"
constexpr uint32_t kDungeonObject = 58;    ///< "Dungeon Object"
constexpr uint32_t kMissile = 14;          ///< "Missile" - `media/missiles/<NAME>.layout`

/// Level script: draws nothing, mostly no position. See `CmpLogicGroup`.
constexpr uint32_t kUnitTrigger = 3;           ///< "Unit Trigger"
constexpr uint32_t kUnitSpawner = 4;           ///< "Unit Spawner"
constexpr uint32_t kTimelineLink = 7;          ///< "Layout Link Timeline"
constexpr uint32_t kTimeline = 34;             ///< "Timeline"
constexpr uint32_t kLogicGroup = 35;           ///< "Logic Group"
constexpr uint32_t kPlayerSphereTrigger = 36;  ///< "Player Sphere Trigger"
constexpr uint32_t kPlayerBoxTrigger = 37;     ///< "Player Box Trigger"
constexpr uint32_t kCounter = 38;              ///< "Counter"
constexpr uint32_t kWarper = 40;               ///< "Warper"
constexpr uint32_t kTimer = 42;                ///< "Timer"
constexpr uint32_t kCameraController = 47;     ///< "Camera Controller"
constexpr uint32_t kGameStateController = 48;  ///< "Game State Controller"
constexpr uint32_t kQuestController = 49;      ///< "Quest Controller"
constexpr uint32_t kPathing = 51;              ///< "Pathing"
constexpr uint32_t kSkipCutscene = 54;         ///< "Skip Cutscene"
constexpr uint32_t kCinematic = 56;            ///< "Cinematic"
constexpr uint32_t kWaypointActivator = 59;    ///< "Waypoint Activator"
constexpr uint32_t kMoneyTaker = 57;           ///< "Money Taker"
}  // namespace cmp_type

/// Field indices per descriptor. A compiled node carries every field, in this order,
/// defaults included, so the indices are positional.
namespace cmp_field {

/// "Light": a flat additive light-pool decal, sized in X and Z, turned about the vertical.
/// `BRIGHTNESS` is an unsigned multiplier.
namespace light {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Light", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Light", "POSITION"));
constexpr size_t kFile = layout_schema::must(layout_schema::field("Light", "FILE"));
constexpr size_t kBrightness = layout_schema::must(layout_schema::field("Light", "BRIGHTNESS"));
constexpr size_t kScaleX = layout_schema::must(layout_schema::field("Light", "SCALE X"));
constexpr size_t kScaleZ = layout_schema::must(layout_schema::field("Light", "SCALE Z"));
constexpr size_t kAngle = layout_schema::must(layout_schema::field("Light", "ANGLE"));
}  // namespace light

/// "Layout Link" and "Layout Link Particle", and the leading part of
/// "Property Node" and "Room Piece", which all share this prefix.
namespace linked {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Layout Link", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Layout Link", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Layout Link", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Layout Link", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Layout Link", "UP"));
}  // namespace linked

/// "Layout Link Particle". `START ON LOAD` defaults to true (`CLayout`'s constructor).
namespace particle_link {
constexpr size_t kEnabled =
    layout_schema::must(layout_schema::field("Layout Link Particle", "ENABLED"));
constexpr size_t kLayoutFile =
    layout_schema::must(layout_schema::field("Layout Link Particle", "LAYOUT FILE"));
constexpr size_t kStartOnLoad =
    layout_schema::must(layout_schema::field("Layout Link Particle", "START ON LOAD"));
}  // namespace particle_link

/// "Layout Link Timeline": a particle link's prefix, then `DURATION MODIFY`. `START ON LOAD`
/// defaults to true (`CLayout::CLayout`, `+0x1a0`).
namespace timeline_link {
constexpr size_t kLayoutFile =
    layout_schema::must(layout_schema::field("Layout Link Timeline", "LAYOUT FILE"));
constexpr size_t kStartOnLoad =
    layout_schema::must(layout_schema::field("Layout Link Timeline", "START ON LOAD"));
/// handed to the layout's `LAYOUT CONTROL` timelines, which use it by their own `TYPE`
constexpr size_t kDurationModify =
    layout_schema::must(layout_schema::field("Layout Link Timeline", "DURATION MODIFY"));
}  // namespace timeline_link

/// "Money Taker": `AMOUNT`, its one property, no position (`CMoneyTakerDescriptor::
/// CMoneyTakerDescriptor`).
namespace money_taker {
constexpr size_t kAmount = layout_schema::must(layout_schema::field("Money Taker", "AMOUNT"));
}  // namespace money_taker

/// "Item": `CItemDescriptor::CItemDescriptor`. `ITEM` is the
/// unit name, made when set (`Set_createNewEquipment` -> `CEquipment::createNewEquipment`).
namespace item {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Item", "POSITION"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Item", "ENABLED"));
constexpr size_t kItem = layout_schema::must(layout_schema::field("Item", "ITEM"));
}  // namespace item

/// "Output Incrementor": `COutputIncrementorDescriptor`'s four properties.
namespace output_incrementor {
constexpr size_t kEnabled =
    layout_schema::must(layout_schema::field("Output Incrementor", "ENABLED"));
constexpr size_t kMaxOutput =
    layout_schema::must(layout_schema::field("Output Incrementor", "MAX OUTPUT"));
constexpr size_t kLoopCount =
    layout_schema::must(layout_schema::field("Output Incrementor", "LOOP COUNT"));
constexpr size_t kLoopsForever =
    layout_schema::must(layout_schema::field("Output Incrementor", "LOOPS FOREVER"));
}  // namespace output_incrementor

/// "Generic Model": a prop placed by mesh path. `SCALE X/Y/Z` default to 1.
namespace model {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Generic Model", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Generic Model", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Generic Model", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Generic Model", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Generic Model", "UP"));
constexpr size_t kFile = layout_schema::must(layout_schema::field("Generic Model", "FILE"));
constexpr size_t kScaleX = layout_schema::must(layout_schema::field("Generic Model", "SCALE X"));
constexpr size_t kScaleY = layout_schema::must(layout_schema::field("Generic Model", "SCALE Y"));
constexpr size_t kScaleZ = layout_schema::must(layout_schema::field("Generic Model", "SCALE Z"));
constexpr size_t kSpeed = layout_schema::must(layout_schema::field("Generic Model", "SPEED"));
constexpr size_t kLoops = layout_schema::must(layout_schema::field("Generic Model", "LOOPS"));
constexpr size_t kAnimation =
    layout_schema::must(layout_schema::field("Generic Model", "ANIMATION"));
}  // namespace model

/// "Unit Trigger": a prop the player activates (chest, lever, door, portal). It draws the
/// `MODEL` unit's mesh, a `CTriggerUnit` (`unitInit`).
namespace unit_trigger {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Unit Trigger", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Unit Trigger", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Unit Trigger", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Unit Trigger", "UP"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Unit Trigger", "ENABLED"));
/// the *unit* whose mesh this prop is
constexpr size_t kModel = layout_schema::must(layout_schema::field("Unit Trigger", "MODEL"));
constexpr size_t kLoopStyle =
    layout_schema::must(layout_schema::field("Unit Trigger", "LOOP STYLE"));
constexpr size_t kRequiredQuest =
    layout_schema::must(layout_schema::field("Unit Trigger", "REQUIRED QUEST"));
constexpr size_t kRequiredItem =
    layout_schema::must(layout_schema::field("Unit Trigger", "REQUIRED ITEM"));
constexpr size_t kNumberRequired =
    layout_schema::must(layout_schema::field("Unit Trigger", "NUMBER REQUIRED"));
constexpr size_t kTakeItems =
    layout_schema::must(layout_schema::field("Unit Trigger", "TAKE ITEMS"));
}  // namespace unit_trigger

/// "Monster": an NPC or creature placed by mesh path. Field 0 is POSITION, not VISIBLE.
/// `ANIMATION` names a `.SKELETON` beside the mesh; an authored `bind` means stand still,
/// an omitted one compiles to the empty string.
namespace monster {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Monster", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Monster", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Monster", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Monster", "UP"));
/// the unit name an Animation Controller joins on
constexpr size_t kMonster = layout_schema::must(layout_schema::field("Monster", "MONSTER"));
constexpr size_t kFile = layout_schema::must(layout_schema::field("Monster", "FILE"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Monster", "ENABLED"));
constexpr size_t kInvincible = layout_schema::must(layout_schema::field("Monster", "INVINCIBLE"));
constexpr size_t kPath = layout_schema::must(layout_schema::field("Monster", "PATH"));
/// not field 0: a Monster's field 0 is its position
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Monster", "VISIBLE"));
constexpr size_t kTargetable = layout_schema::must(layout_schema::field("Monster", "TARGETABLE"));
constexpr size_t kScale = layout_schema::must(layout_schema::field("Monster", "SCALE"));
constexpr size_t kCannotWander =
    layout_schema::must(layout_schema::field("Monster", "CANNOT WANDER"));
constexpr size_t kNoGold = layout_schema::must(layout_schema::field("Monster", "NO GOLD"));
constexpr size_t kNoTreasure = layout_schema::must(layout_schema::field("Monster", "NO TREASURE"));
constexpr size_t kAnimation = layout_schema::must(layout_schema::field("Monster", "ANIMATION"));
/// playback rate, 1 unless authored
constexpr size_t kSpeed = layout_schema::must(layout_schema::field("Monster", "SPEED"));
constexpr size_t kLoops = layout_schema::must(layout_schema::field("Monster", "LOOPS"));
}  // namespace monster

/// "Animation Controller": tells a placed unit to play something; no position. `UNIT` names
/// a `Monster`'s `MONSTER`, `GROUP` its category ("MONSTERS", "PLAYER").
namespace animation_controller {
constexpr size_t kStartOnLoad =
    layout_schema::must(layout_schema::field("Animation Controller", "START ON LOAD"));
constexpr size_t kBlendTime =
    layout_schema::must(layout_schema::field("Animation Controller", "BLEND TIME"));
constexpr size_t kBlendOutTime =
    layout_schema::must(layout_schema::field("Animation Controller", "BLEND OUT TIME"));
constexpr size_t kDurationOverride =
    layout_schema::must(layout_schema::field("Animation Controller", "DURATION OVERRIDE"));
constexpr size_t kPlayIdle =
    layout_schema::must(layout_schema::field("Animation Controller", "PLAY IDLE"));
constexpr size_t kAnimation =
    layout_schema::must(layout_schema::field("Animation Controller", "ANIMATION"));
constexpr size_t kGroup =
    layout_schema::must(layout_schema::field("Animation Controller", "GROUP"));
constexpr size_t kUnit = layout_schema::must(layout_schema::field("Animation Controller", "UNIT"));
}  // namespace animation_controller

/// "Sine Force" (field 0 is the name, field 1 unsolved).
namespace sine_force {
/// `FORCE APPLICATION`: `Average` or `Add`
constexpr size_t kApplication =
    layout_schema::must(layout_schema::field("Sine Force", "FORCE APPLICATION"));
/// `FORCESX/Y/Z`, a vec3
constexpr size_t kForces = layout_schema::must(layout_schema::field("Sine Force", "FORCES"));
/// `MIN`, the frequency's low end
constexpr size_t kMin = layout_schema::must(layout_schema::field("Sine Force", "MIN"));
constexpr size_t kMax = layout_schema::must(layout_schema::field("Sine Force", "MAX"));  ///< `MAX`
}  // namespace sine_force

/// "Linear Force": `CForceAffectorDescriptor`, Sine Force's
/// first two fields.
namespace linear_force {
/// `FORCE APPLICATION`: `Average` or `Add`
constexpr size_t kApplication =
    layout_schema::must(layout_schema::field("Linear Force", "FORCE APPLICATION"));
/// `FORCESX/Y/Z`, a vec3
constexpr size_t kForces = layout_schema::must(layout_schema::field("Linear Force", "FORCES"));
}  // namespace linear_force

/// "Jet": `CJetAffectorDescriptor`, after
/// POSITION and ENABLE.
namespace jet {
/// a dynamic property
constexpr size_t kAcceleration = layout_schema::must(layout_schema::field("Jet", "ACCELERATION"));
}  // namespace jet

/// "Align": `CAlignDescriptor`, after POSITION and ENABLE.
namespace align {
constexpr size_t kToVelocity = layout_schema::must(layout_schema::field("Align", "TO VELOCITY"));
constexpr size_t kStretch = layout_schema::must(layout_schema::field("Align", "STRETCH"));
}  // namespace align

/// "Geometry Rotator": `CGeometryRotateDescriptor`, after
/// POSITION and ENABLE. An unauthored axis compiles to an empty field.
namespace geometry_rotator {
/// a dynamic property, radians a second
constexpr size_t kRotationSpeed =
    layout_schema::must(layout_schema::field("Geometry Rotator", "ROTATION SPEED"));
constexpr size_t kUseOwnRotation =
    layout_schema::must(layout_schema::field("Geometry Rotator", "USE OWN ROTATION"));
/// `AXIS OF ROTATIONX/Y/Z`, a vec3
constexpr size_t kAxis =
    layout_schema::must(layout_schema::field("Geometry Rotator", "AXIS OF ROTATION"));
}  // namespace geometry_rotator

/// "Plane Collision" and "Sphere Collision": `CColliderDescriptor`
/// after POSITION and ENABLE, then `CPlaneColliderDescriptor`
/// or `CSphereColliderDescriptor`.
namespace collider {
/// `TYPE`: `Stop`, `Bounce`, `Flow` (`gPARTICLE_COLLISION_TYPE`)
constexpr size_t kType = layout_schema::must(layout_schema::field("Box Collision", "TYPE"));
/// `INTERSECTION TYPE`: `Fast`, `Box`
constexpr size_t kIntersectionType =
    layout_schema::must(layout_schema::field("Box Collision", "INTERSECTION TYPE"));
constexpr size_t kFriction = layout_schema::must(layout_schema::field("Box Collision", "FRICTION"));
constexpr size_t kBouncyness =
    layout_schema::must(layout_schema::field("Box Collision", "BOUNCYNESS"));
/// plane: `NORMALX/Y/Z`, a vec3
constexpr size_t kNormal = layout_schema::must(layout_schema::field("Box Collision", "WIDTH"));
/// sphere: `RADIUS`
constexpr size_t kRadius = layout_schema::must(layout_schema::field("Box Collision", "WIDTH"));
/// `SHOW PLANE` / `SHOW SPHERE`: the editor's outline
constexpr size_t kShow = layout_schema::must(layout_schema::field("Box Collision", "HEIGHT"));
/// "Box Collision"'s own: `CBoxColliderDescriptor`.
constexpr size_t kWidth = layout_schema::must(layout_schema::field("Box Collision", "WIDTH"));
constexpr size_t kHeight = layout_schema::must(layout_schema::field("Box Collision", "HEIGHT"));
constexpr size_t kDepth = layout_schema::must(layout_schema::field("Box Collision", "DEPTH"));
}  // namespace collider

/// "Line": `CLineAffectorDescriptor`, after POSITION and
/// ENABLE; setters pass straight to the affector.
namespace line {
constexpr size_t kDeviation = layout_schema::must(layout_schema::field("Line", "DEVIATION"));
constexpr size_t kTimeStep = layout_schema::must(layout_schema::field("Line", "TIME STEP"));
constexpr size_t kDrift = layout_schema::must(layout_schema::field("Line", "DRIFT"));
/// a vec3
constexpr size_t kEndPosition = layout_schema::must(layout_schema::field("Line", "END POSITION"));
}  // namespace line

/// "Vortext" (sic): `CVortexAffectorDescriptor`, after POSITION and ENABLE
/// (`CAffectorDescriptor`).
namespace vortex {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Vortext", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Vortext", "ENABLE"));
/// `DIRECTIONX/Y/Z`, the axis turned about
constexpr size_t kDirection = layout_schema::must(layout_schema::field("Vortext", "DIRECTION"));
/// a dynamic property, radians a second
constexpr size_t kRate = layout_schema::must(layout_schema::field("Vortext", "RATE"));
}  // namespace vortex

/// "Gravity Well": `CGravityAffectorDescriptor`.
namespace gravity_well {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Gravity Well", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Gravity Well", "ENABLE"));
constexpr size_t kGravity = layout_schema::must(layout_schema::field("Gravity Well", "GRAVITY"));
}  // namespace gravity_well

/// "Texture Animation": plays a Particle's sprite sheet; without it the starting cell holds.
/// A near-zero `ANIMATION SPEED` with `USE RANDOM STARTING FRAME` means a random cell, held.
namespace texture_animation {
constexpr size_t kPosition =
    layout_schema::must(layout_schema::field("Texture Animation", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Texture Animation", "ENABLE"));
/// frames per second, a dynamic property
constexpr size_t kAnimationSpeed =
    layout_schema::must(layout_schema::field("Texture Animation", "ANIMATION SPEED"));
constexpr size_t kUseOwnAnimation =
    layout_schema::must(layout_schema::field("Texture Animation", "USE OWN ANIMATION"));
constexpr size_t kUseRandomStartingFrame =
    layout_schema::must(layout_schema::field("Texture Animation", "USE RANDOM STARTING FRAME"));
}  // namespace texture_animation

/// "Group": a container that can gate its subtree on quest state. Each of the five gate
/// fields names a quest; an empty one is unused.
namespace group {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Group", "POSITION"));
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Group", "VISIBLE"));
constexpr size_t kDynamic = layout_schema::must(layout_schema::field("Group", "DYNAMIC"));
constexpr size_t kRandomisation =
    layout_schema::must(layout_schema::field("Group", "RANDOMIZATION"));
constexpr size_t kNumber = layout_schema::must(layout_schema::field("Group", "NUMBER"));
constexpr size_t kChoice = layout_schema::must(layout_schema::field("Group", "CHOICE"));
/// by registration order (`CRandomGroupDescriptor::CRandomGroupDescriptor`)
///
constexpr size_t kDifficultyMode =
    layout_schema::must(layout_schema::field("Group", "DIFFICULTY MODE"));
constexpr size_t kDungeon = layout_schema::must(layout_schema::field("Group", "DUNGEON"));
constexpr size_t kClass = layout_schema::must(layout_schema::field("Group", "CLASS"));
constexpr size_t kActiveOrComplete =
    layout_schema::must(layout_schema::field("Group", "ACTIVE OR COMPLETE"));
constexpr size_t kComplete = layout_schema::must(layout_schema::field("Group", "COMPLETE"));
constexpr size_t kNotComplete = layout_schema::must(layout_schema::field("Group", "NOT COMPLETE"));
constexpr size_t kActive = layout_schema::must(layout_schema::field("Group", "ACTIVE"));
constexpr size_t kNotActiveOrComplete =
    layout_schema::must(layout_schema::field("Group", "NOT ACTIVE OR COMPLETE"));
}  // namespace group

/// "Room Piece": tileset geometry placed by GUID. Read the per-axis `X`/`Y`/`Z`, not the
/// editor's uniform `SCALE`.
namespace room_piece {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Room Piece", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Room Piece", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Room Piece", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Room Piece", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Room Piece", "UP"));
constexpr size_t kCollisionEnabled =
    layout_schema::must(layout_schema::field("Room Piece", "COLLISION ENABLED"));
constexpr size_t kBake = layout_schema::must(layout_schema::field("Room Piece", "BAKE"));
constexpr size_t kVisibleOnMap =
    layout_schema::must(layout_schema::field("Room Piece", "VISIBLE ON MAP"));
constexpr size_t kGuid = layout_schema::must(layout_schema::field("Room Piece", "GUID"));
constexpr size_t kVisual = layout_schema::must(layout_schema::field("Room Piece", "VISUAL"));
constexpr size_t kScale = layout_schema::must(layout_schema::field("Room Piece", "SCALE"));
constexpr size_t kScaleX = layout_schema::must(layout_schema::field("Room Piece", "X"));
constexpr size_t kScaleY = layout_schema::must(layout_schema::field("Room Piece", "Y"));
constexpr size_t kScaleZ = layout_schema::must(layout_schema::field("Room Piece", "Z"));
}  // namespace room_piece

/// "Particle": one billboard system; its `Emitter`, `Scale`, `Color` and `Texture Rotate`
/// nodes are its children by parent id.
namespace particle {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Particle", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Particle", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Particle", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Particle", "UP"));
constexpr size_t kRenderType = layout_schema::must(layout_schema::field("Particle", "RENDER TYPE"));
constexpr size_t kVisualScale =
    layout_schema::must(layout_schema::field("Particle", "VISUAL SCALE"));
constexpr size_t kTimeScale = layout_schema::must(layout_schema::field("Particle", "TIME SCALE"));
constexpr size_t kVelocityScale =
    layout_schema::must(layout_schema::field("Particle", "VELOCITY SCALE"));
constexpr size_t kLifeTime = layout_schema::must(layout_schema::field("Particle", "LIFE TIME"));
constexpr size_t kWorldOrientation =
    layout_schema::must(layout_schema::field("Particle", "WORLD ORIENTATION"));
constexpr size_t kNoCulling = layout_schema::must(layout_schema::field("Particle", "NO CULLING"));
constexpr size_t kIsLight = layout_schema::must(layout_schema::field("Particle", "IS LIGHT"));
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Particle", "VISIBLE"));
constexpr size_t kSorts = layout_schema::must(layout_schema::field("Particle", "SORTS"));
/// stored as 48 + the authored value
constexpr size_t kSortIndex = layout_schema::must(layout_schema::field("Particle", "SORT INDEX"));
constexpr size_t kDepthBias = layout_schema::must(layout_schema::field("Particle", "DEPTH BIAS"));
constexpr size_t kMaxVelocity = layout_schema::must(layout_schema::field("Particle", "VELOCITY"));
constexpr size_t kVisualQuota =
    layout_schema::must(layout_schema::field("Particle", "VISUAL QUOTA"));
constexpr size_t kTexture = layout_schema::must(layout_schema::field("Particle", "TEXTURE"));
constexpr size_t kRenderStyle =
    layout_schema::must(layout_schema::field("Particle", "RENDER STYLE"));
constexpr size_t kMesh = layout_schema::must(layout_schema::field("Particle", "MESH"));
constexpr size_t kDepthCheck = layout_schema::must(layout_schema::field("Particle", "DEPTH CHECK"));
constexpr size_t kRotationType =
    layout_schema::must(layout_schema::field("Particle", "ROTATION TYPE"));
constexpr size_t kOrigin = layout_schema::must(layout_schema::field("Particle", "ORIGIN"));

/// A trail (`LENGTH` to `LOCAL TRAIL`), then a sprite sheet (`FRAMES WIDE/HIGH`, a cell per
/// particle age) or a scroll (`U RATE`/`V RATE`).
constexpr size_t kLength = layout_schema::must(layout_schema::field("Particle", "LENGTH"));
constexpr size_t kWidth = layout_schema::must(layout_schema::field("Particle", "WIDTH"));
constexpr size_t kSegments = layout_schema::must(layout_schema::field("Particle", "SEGMENTS"));
constexpr size_t kFadeEnds = layout_schema::must(layout_schema::field("Particle", "FADE ENDS"));
constexpr size_t kLocalTrail = layout_schema::must(layout_schema::field("Particle", "LOCAL TRAIL"));
constexpr size_t kFramesWide = layout_schema::must(layout_schema::field("Particle", "FRAMES WIDE"));
constexpr size_t kFramesHigh = layout_schema::must(layout_schema::field("Particle", "FRAMES HIGH"));
constexpr size_t kURate = layout_schema::must(layout_schema::field("Particle", "U RATE"));
constexpr size_t kVRate = layout_schema::must(layout_schema::field("Particle", "V RATE"));

/// The bias `SORT INDEX` is stored with.
constexpr uint32_t kSortIndexBase = 48;
}  // namespace particle

/// "Emitter": how a Particle releases its billboards.
namespace emitter {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Emitter", "POSITION"));
constexpr size_t kTypeOfEmitter =
    layout_schema::must(layout_schema::field("Emitter", "TYPE OF EMITTER"));
constexpr size_t kDirection = layout_schema::must(layout_schema::field("Emitter", "DIRECTION"));
constexpr size_t kFixed = layout_schema::must(layout_schema::field("Emitter", "FIXED"));
constexpr size_t kScaleOnLaunch =
    layout_schema::must(layout_schema::field("Emitter", "SCALE ON LAUNCH"));
constexpr size_t kWidth = layout_schema::must(layout_schema::field("Emitter", "WIDTH"));
constexpr size_t kHeight = layout_schema::must(layout_schema::field("Emitter", "HEIGHT"));
constexpr size_t kDepth = layout_schema::must(layout_schema::field("Emitter", "DEPTH"));
constexpr size_t kAngle = layout_schema::must(layout_schema::field("Emitter", "ANGLE"));
constexpr size_t kVelocity = layout_schema::must(layout_schema::field("Emitter", "VELOCITY"));
constexpr size_t kMass = layout_schema::must(layout_schema::field("Emitter", "MASS"));
constexpr size_t kKeepLocal = layout_schema::must(layout_schema::field("Emitter", "KEEP LOCAL"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Emitter", "ENABLED"));
constexpr size_t kNoExpiration =
    layout_schema::must(layout_schema::field("Emitter", "NO EXPIRATION"));
constexpr size_t kDelayFirst = layout_schema::must(layout_schema::field("Emitter", "DELAY FIRST"));
constexpr size_t kDelay = layout_schema::must(layout_schema::field("Emitter", "DELAY"));
constexpr size_t kParticleLife =
    layout_schema::must(layout_schema::field("Emitter", "PARTICLE LIFE"));
constexpr size_t kEmitDuration =
    layout_schema::must(layout_schema::field("Emitter", "EMIT DURATION"));
constexpr size_t kEmitRate = layout_schema::must(layout_schema::field("Emitter", "EMIT RATE"));
constexpr size_t kNumLoops = layout_schema::must(layout_schema::field("Emitter", "NUM LOOPS"));
constexpr size_t kReleaseOverDistance =
    layout_schema::must(layout_schema::field("Emitter", "RELEASE OVER DISTANCE"));
constexpr size_t kChanceToEmit =
    layout_schema::must(layout_schema::field("Emitter", "CHANCE TO EMIT"));
/// the far end of a `Line` emitter
constexpr size_t kEndPosition =
    layout_schema::must(layout_schema::field("Emitter", "END POSITION"));
constexpr size_t kBoxWidth = layout_schema::must(layout_schema::field("Emitter", "WIDTH", 1));
constexpr size_t kBoxHeight = layout_schema::must(layout_schema::field("Emitter", "HEIGHT", 1));
constexpr size_t kBoxDepth = layout_schema::must(layout_schema::field("Emitter", "DEPTH", 1));
constexpr size_t kRadius = layout_schema::must(layout_schema::field("Emitter", "RADIUS"));
constexpr size_t kMinRadius = layout_schema::must(layout_schema::field("Emitter", "MIN RADIUS"));
constexpr size_t kMaxRadius = layout_schema::must(layout_schema::field("Emitter", "MAX RADIUS"));
constexpr size_t kMinLineIncrement =
    layout_schema::must(layout_schema::field("Emitter", "MIN INCREMENT"));
constexpr size_t kMaxLineIncrement =
    layout_schema::must(layout_schema::field("Emitter", "MAX INCREMENT"));
constexpr size_t kMaxLineDeviation =
    layout_schema::must(layout_schema::field("Emitter", "MAX DEVIATION"));
/// the `Circle` emitter's step
constexpr size_t kStep = layout_schema::must(layout_schema::field("Emitter", "STEP"));
/// the `Circle` emitter's `FIXED`
constexpr size_t kCircleFixed = layout_schema::must(layout_schema::field("Emitter", "FIXED", 1));
}  // namespace emitter

/// "Scale": the size track applied to a Particle's billboards over their life.
namespace scale_over_time {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Scale", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Scale", "ENABLE"));
constexpr size_t kFixed = layout_schema::must(layout_schema::field("Scale", "FIXED"));
constexpr size_t kX = layout_schema::must(layout_schema::field("Scale", "X"));
constexpr size_t kY = layout_schema::must(layout_schema::field("Scale", "Y"));
constexpr size_t kZ = layout_schema::must(layout_schema::field("Scale", "Z"));
}  // namespace scale_over_time

/// "Color": the tint track applied to a Particle's billboards over their life.
namespace color_over_time {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Color", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Color", "ENABLE"));
constexpr size_t kColorOverTime =
    layout_schema::must(layout_schema::field("Color", "COLOR OVER TIME"));
}  // namespace color_over_time

/// "Texture Rotate": how a Particle's billboards are turned.
namespace texture_rotate {
constexpr size_t kPosition =
    layout_schema::must(layout_schema::field("Texture Rotate", "POSITION"));
constexpr size_t kEnable = layout_schema::must(layout_schema::field("Texture Rotate", "ENABLE"));
constexpr size_t kRotationSpeed =
    layout_schema::must(layout_schema::field("Texture Rotate", "ROTATION SPEED"));
constexpr size_t kStartingRotation =
    layout_schema::must(layout_schema::field("Texture Rotate", "STARTING ROTATION"));
constexpr size_t kUseOwnRotation =
    layout_schema::must(layout_schema::field("Texture Rotate", "USE OWN ROTATION"));
}  // namespace texture_rotate

/// A "dynamic property": floats whose first is the kind, the rest its arguments. 0 a
/// constant, 1 a random range, 2/3 (time, value) pairs, 4/5 (time, min, max) triples.
/// `COLOR OVER TIME` is not one: see `color_key_count()`.
namespace dynamic_kind {
constexpr float kFixed = 0.0f;
constexpr float kRandomRange = 1.0f;
constexpr float kCurve = 2.0f;
constexpr float kCurveAlternate = 3.0f;
constexpr float kRandomCurve = 4.0f;
constexpr float kRandomCurveAlternate = 5.0f;
}  // namespace dynamic_kind

/// "Quest Controller": sets the quest state a `Group`'s gates read. `UNIT` is the quest's NPC;
/// `BROADCAST ON LOAD` announces the state when the level opens.
namespace quest_controller {
constexpr size_t kDisablePlayer =
    layout_schema::must(layout_schema::field("Quest Controller", "DISABLE PLAYER"));
constexpr size_t kBroadcastOnLoad =
    layout_schema::must(layout_schema::field("Quest Controller", "BROADCAST ON LOAD"));
constexpr size_t kGroup = layout_schema::must(layout_schema::field("Quest Controller", "GROUP"));
constexpr size_t kUnit = layout_schema::must(layout_schema::field("Quest Controller", "UNIT"));
constexpr size_t kQuest = layout_schema::must(layout_schema::field("Quest Controller", "QUEST"));
}  // namespace quest_controller

/// "Cinematic": the `[CINEMATIC]` it opens (`CCinematicObject +0x68`, X360 `+0x60`).
namespace cinematic {
constexpr size_t kCinematic = layout_schema::must(layout_schema::field("Cinematic", "CINEMATIC"));
}  // namespace cinematic

/// "Skill Controller": a unit starts, stops, learns or unlearns a skill.
/// `CSkillControllerDescriptor::CSkillControllerDescriptor`,
/// its `AddProperty` order after the base's `POSITION`. Comments give the object offsets.
namespace skill_controller {
/// +0x148
constexpr size_t kLearnOnStart =
    layout_schema::must(layout_schema::field("Skill Controller", "LEARN ON START"));
/// +0x149
constexpr size_t kUnlearnOnStop =
    layout_schema::must(layout_schema::field("Skill Controller", "UNLEARN ON STOP"));
/// +0x14c
constexpr size_t kForceStop =
    layout_schema::must(layout_schema::field("Skill Controller", "FORCE STOP"));
/// +0x14d
constexpr size_t kTargetPlayer =
    layout_schema::must(layout_schema::field("Skill Controller", "TARGET PLAYER"));
/// +0x14e
constexpr size_t kUseUnitTarget =
    layout_schema::must(layout_schema::field("Skill Controller", "USE UNIT TARGET"));
/// +0x150
constexpr size_t kSkillLevel =
    layout_schema::must(layout_schema::field("Skill Controller", "SKILL LEVEL"));
/// +0x120
constexpr size_t kGroup = layout_schema::must(layout_schema::field("Skill Controller", "GROUP"));
/// +0x108 (its data group)
constexpr size_t kUnit = layout_schema::must(layout_schema::field("Skill Controller", "UNIT"));
/// +0x130
constexpr size_t kSkill = layout_schema::must(layout_schema::field("Skill Controller", "SKILL"));
}  // namespace skill_controller

/// "Player Sphere Trigger": a radius about a point, tested against the player.
namespace player_sphere_trigger {
constexpr size_t kPosition =
    layout_schema::must(layout_schema::field("Player Sphere Trigger", "POSITION"));
constexpr size_t kEnabled =
    layout_schema::must(layout_schema::field("Player Sphere Trigger", "ENABLED"));
constexpr size_t kRadius =
    layout_schema::must(layout_schema::field("Player Sphere Trigger", "RADIUS"));
}  // namespace player_sphere_trigger

/// "Player Box Trigger": an axis-aligned box, tested against the player. `DIMENSIONS` is the
/// full extent, not the half-extent.
namespace player_box_trigger {
constexpr size_t kPosition =
    layout_schema::must(layout_schema::field("Player Box Trigger", "POSITION"));
constexpr size_t kEnabled =
    layout_schema::must(layout_schema::field("Player Box Trigger", "ENABLED"));
constexpr size_t kDimensions =
    layout_schema::must(layout_schema::field("Player Box Trigger", "DIMENSIONS"));
}  // namespace player_box_trigger

/// "Timeline": how to play timed keys over `DURATION` seconds; the keys are in `CmpTimeline`.
namespace timeline {
constexpr size_t kDuration = layout_schema::must(layout_schema::field("Timeline", "DURATION"));
constexpr size_t kStep = layout_schema::must(layout_schema::field("Timeline", "STEP"));
constexpr size_t kLoops = layout_schema::must(layout_schema::field("Timeline", "LOOPS"));
constexpr size_t kResetProperties =
    layout_schema::must(layout_schema::field("Timeline", "RESET PROPERTIES"));
constexpr size_t kStartOnLoad =
    layout_schema::must(layout_schema::field("Timeline", "START ON LOAD"));
constexpr size_t kDefaultInterpolation =
    layout_schema::must(layout_schema::field("Timeline", "DEFAULT INTERPOLATION TYPE"));
/// `LAYOUT CONTROL`: marks the timeline a `Layout Link Timeline` drives.
constexpr size_t kLayoutControl =
    layout_schema::must(layout_schema::field("Timeline", "LAYOUT CONTROL"));
constexpr size_t kType = layout_schema::must(layout_schema::field("Timeline", "TYPE"));
}  // namespace timeline

/// "Camera Controller": the scripted camera. It pans (`PAN TIME`), pauses (`TIME`), and only
/// then fires `Camera Stopped`; `FOLLOW UNIT` then follows `UNIT`.
namespace camera_controller {
constexpr size_t kPosition =
    layout_schema::must(layout_schema::field("Camera Controller", "POSITION"));
constexpr size_t kPanTime =
    layout_schema::must(layout_schema::field("Camera Controller", "PAN TIME"));
constexpr size_t kEaseInDistancePct =
    layout_schema::must(layout_schema::field("Camera Controller", "EASE IN DISTANCE PCT"));
constexpr size_t kEaseInPct =
    layout_schema::must(layout_schema::field("Camera Controller", "EASE IN PCT"));
constexpr size_t kTime = layout_schema::must(layout_schema::field("Camera Controller", "TIME"));
constexpr size_t kResetsAtEnd =
    layout_schema::must(layout_schema::field("Camera Controller", "RESETS AT END"));
constexpr size_t kFollowUnit =
    layout_schema::must(layout_schema::field("Camera Controller", "FOLLOW UNIT"));
constexpr size_t kType = layout_schema::must(layout_schema::field("Camera Controller", "TYPE"));
constexpr size_t kGroup = layout_schema::must(layout_schema::field("Camera Controller", "GROUP"));
constexpr size_t kUnit = layout_schema::must(layout_schema::field("Camera Controller", "UNIT"));
}  // namespace camera_controller

/// "Game State Controller": what the interface does during a scripted scene (`STATE`).
namespace game_state_controller {
constexpr size_t kEnabled =
    layout_schema::must(layout_schema::field("Game State Controller", "ENABLED"));
constexpr size_t kInvulnerable =
    layout_schema::must(layout_schema::field("Game State Controller", "INVULNERABLE"));
constexpr size_t kState =
    layout_schema::must(layout_schema::field("Game State Controller", "STATE"));
constexpr size_t kTip = layout_schema::must(layout_schema::field("Game State Controller", "TIP"));
}  // namespace game_state_controller

/// "Skip Cutscene": the way out of a scripted scene.
namespace skip_cutscene {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Skip Cutscene", "ENABLED"));
constexpr size_t kDisableSkills =
    layout_schema::must(layout_schema::field("Skip Cutscene", "DISABLE SKILLS"));
}  // namespace skip_cutscene

/// "Teleport": moves the player to it.
namespace teleport {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Teleport", "ENABLED"));
}  // namespace teleport

/// "Dungeon Object": names a dungeon; `Clear History` forgets the player's
/// floors of it. No shipped layout places one.
namespace dungeon_object {
constexpr size_t kDungeonName =
    layout_schema::must(layout_schema::field("Dungeon Object", "DUNGEON NAME"));
}  // namespace dungeon_object

/// "Timer": a delay that fires an output when it elapses.
namespace timer {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Timer", "ENABLED"));
constexpr size_t kTime = layout_schema::must(layout_schema::field("Timer", "TIME"));
constexpr size_t kMaxTime = layout_schema::must(layout_schema::field("Timer", "MAXTIME"));
constexpr size_t kLoopCount = layout_schema::must(layout_schema::field("Timer", "LOOP COUNT"));
constexpr size_t kLoopsForever =
    layout_schema::must(layout_schema::field("Timer", "LOOPS FOREVER"));
}  // namespace timer

/// "Pathing": a route a unit walks; fires `End of Path Reached` at its end. `UNIT` names a
/// `Monster`'s `MONSTER`, or the player when `GROUP` is `PLAYER`.
namespace pathing {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Pathing", "POSITION"));
/// a flat run of vec3 waypoints
constexpr size_t kPoints = layout_schema::must(layout_schema::field("Pathing", "POINTS"));
constexpr size_t kNumberOfPoints =
    layout_schema::must(layout_schema::field("Pathing", "NUMBER OF POINTS"));
constexpr size_t kShow = layout_schema::must(layout_schema::field("Pathing", "SHOW"));
constexpr size_t kEnableOnStart =
    layout_schema::must(layout_schema::field("Pathing", "ENABLE ON START"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Pathing", "ENABLED"));
constexpr size_t kPathName = layout_schema::must(layout_schema::field("Pathing", "PATH NAME"));
constexpr size_t kRun = layout_schema::must(layout_schema::field("Pathing", "RUN"));
constexpr size_t kPathToPlayer =
    layout_schema::must(layout_schema::field("Pathing", "PATH TO PLAYER"));
constexpr size_t kGroup = layout_schema::must(layout_schema::field("Pathing", "GROUP"));
constexpr size_t kUnit = layout_schema::must(layout_schema::field("Pathing", "UNIT"));
}  // namespace pathing

/// "Random Choice": `ONE`..`FIVE` weight the outputs `One`..`Five`; `COUNT` is how many one
/// `Roll` fires.
namespace random_choice {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Random Choice", "ENABLED"));
constexpr size_t kType = layout_schema::must(layout_schema::field("Random Choice", "TYPE"));
constexpr size_t kCount = layout_schema::must(layout_schema::field("Random Choice", "COUNT"));
constexpr size_t kOne = layout_schema::must(layout_schema::field("Random Choice", "ONE"));
constexpr size_t kTwo = layout_schema::must(layout_schema::field("Random Choice", "TWO"));
constexpr size_t kThree = layout_schema::must(layout_schema::field("Random Choice", "THREE"));
constexpr size_t kFour = layout_schema::must(layout_schema::field("Random Choice", "FOUR"));
constexpr size_t kFive = layout_schema::must(layout_schema::field("Random Choice", "FIVE"));
}  // namespace random_choice

/// "Puzzle Input": a crypt lever puzzle shuffler; `NUMBER OF INPUTS` levers (2 to 5).
namespace puzzle_input {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Puzzle Input", "ENABLED"));
constexpr size_t kNumberOfInputs =
    layout_schema::must(layout_schema::field("Puzzle Input", "NUMBER OF INPUTS"));
}  // namespace puzzle_input

/// "Damage Shape": a region that hurts the characters in it (`CDamageShape`). `POSITION`,
/// `CShapeDescriptor`'s nine (no orientation), then `CDamageShapeDescriptor`'s constructor
/// order. Radii and angles are dynamic attributes.
namespace damage_shape {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Damage Shape", "POSITION"));
constexpr size_t kShape = layout_schema::must(layout_schema::field("Damage Shape", "SHAPE"));
constexpr size_t kReleaseOrder =
    layout_schema::must(layout_schema::field("Damage Shape", "RELEASE ORDER"));
constexpr size_t kDirection =
    layout_schema::must(layout_schema::field("Damage Shape", "DIRECTION"));
/// `SIZEX`/`SIZEY`/`SIZEZ`
constexpr size_t kBoxSize = layout_schema::must(layout_schema::field("Damage Shape", "SIZE"));
constexpr size_t kUseMaxOnly =
    layout_schema::must(layout_schema::field("Damage Shape", "USE MAX ONLY"));
constexpr size_t kMinRadius =
    layout_schema::must(layout_schema::field("Damage Shape", "MIN RADIUS"));
constexpr size_t kMaxRadius =
    layout_schema::must(layout_schema::field("Damage Shape", "MAX RADIUS"));
constexpr size_t kAngle = layout_schema::must(layout_schema::field("Damage Shape", "ANGLE"));
constexpr size_t kAngleOffset =
    layout_schema::must(layout_schema::field("Damage Shape", "ANGLE OFFSET"));
constexpr size_t kDamageType =
    layout_schema::must(layout_schema::field("Damage Shape", "DAMAGE TYPE"));
constexpr size_t kUnitType = layout_schema::must(layout_schema::field("Damage Shape", "UNITTYPE"));
constexpr size_t kAlignment =
    layout_schema::must(layout_schema::field("Damage Shape", "ALIGNMENT"));
constexpr size_t kUpdateInEditor =
    layout_schema::must(layout_schema::field("Damage Shape", "UPDATE IN EDITOR"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Damage Shape", "ENABLED"));
constexpr size_t kDot = layout_schema::must(layout_schema::field("Damage Shape", "DOT"));
constexpr size_t kTargetCorpse =
    layout_schema::must(layout_schema::field("Damage Shape", "TARGET CORPSE"));
constexpr size_t kMaxUnitsToHit =
    layout_schema::must(layout_schema::field("Damage Shape", "MAX UNITS TO HIT"));
constexpr size_t kMin = layout_schema::must(layout_schema::field("Damage Shape", "MIN"));
constexpr size_t kMax = layout_schema::must(layout_schema::field("Damage Shape", "MAX"));
constexpr size_t kLoops = layout_schema::must(layout_schema::field("Damage Shape", "LOOPS"));
constexpr size_t kLoopCount =
    layout_schema::must(layout_schema::field("Damage Shape", "LOOP COUNT"));
constexpr size_t kTimer = layout_schema::must(layout_schema::field("Damage Shape", "TIMER"));
constexpr size_t kPauseTimer =
    layout_schema::must(layout_schema::field("Damage Shape", "PAUSE TIMER"));
}  // namespace damage_shape

/// "Unit Spawner": where a level's monsters come from (`Spawn Units`). `GROUP` says what
/// `RESOURCE` names: `Monsters` a unit, `Spawn Class` a table, `Items` treasure.
/// `MAX ALLOWED` caps how many of its units are alive at once.
namespace unit_spawner {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Unit Spawner", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Unit Spawner", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Unit Spawner", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Unit Spawner", "UP"));
constexpr size_t kShape = layout_schema::must(layout_schema::field("Unit Spawner", "SHAPE"));
constexpr size_t kReleaseOrder =
    layout_schema::must(layout_schema::field("Unit Spawner", "RELEASE ORDER"));
constexpr size_t kDirection =
    layout_schema::must(layout_schema::field("Unit Spawner", "DIRECTION"));
/// the box a `Box` shape releases on (`CShape::setBoxSize`)
constexpr size_t kSize = layout_schema::must(layout_schema::field("Unit Spawner", "SIZE"));
constexpr size_t kUseMaxOnly =
    layout_schema::must(layout_schema::field("Unit Spawner", "USE MAX ONLY"));
constexpr size_t kMinRadius =
    layout_schema::must(layout_schema::field("Unit Spawner", "MIN RADIUS"));
constexpr size_t kMaxRadius =
    layout_schema::must(layout_schema::field("Unit Spawner", "MAX RADIUS"));
constexpr size_t kAngle = layout_schema::must(layout_schema::field("Unit Spawner", "ANGLE"));
constexpr size_t kAngleOffset =
    layout_schema::must(layout_schema::field("Unit Spawner", "ANGLE OFFSET"));
constexpr size_t kCount = layout_schema::must(layout_schema::field("Unit Spawner", "COUNT"));
constexpr size_t kMaxAllowed =
    layout_schema::must(layout_schema::field("Unit Spawner", "MAX ALLOWED"));

/// releases its units when the level is built, with no wire
constexpr size_t kSpawnOnCreate =
    layout_schema::must(layout_schema::field("Unit Spawner", "SPAWN ON CREATE"));
constexpr size_t kTargetPlayer =
    layout_schema::must(layout_schema::field("Unit Spawner", "TARGET PLAYER"));
constexpr size_t kDuration = layout_schema::must(layout_schema::field("Unit Spawner", "DURATION"));
constexpr size_t kPulseRate =
    layout_schema::must(layout_schema::field("Unit Spawner", "PULSE RATE"));
constexpr size_t kGiveXp = layout_schema::must(layout_schema::field("Unit Spawner", "GIVE XP"));
/// `CUnitSpawner +0x179`, default 1; clear, a released character drops nothing
/// (`spawnUnitByIndex`)
constexpr size_t kGiveLoot = layout_schema::must(layout_schema::field("Unit Spawner", "GIVE LOOT"));

/// everything comes out on the spawner's own point, not through its radius
constexpr size_t kSpawnInCenter =
    layout_schema::must(layout_schema::field("Unit Spawner", "SPAWN IN CENTER"));

/// a released unit performs its entrance
constexpr size_t kPlaySpawnAnim =
    layout_schema::must(layout_schema::field("Unit Spawner", "PLAY SPAWN ANIM"));

/// The rest in `CUnitSpawnerDescriptor`'s registration order.
constexpr size_t kRequiresLos =
    layout_schema::must(layout_schema::field("Unit Spawner", "REQUIRES LOS"));
constexpr size_t kMustSpawn =
    layout_schema::must(layout_schema::field("Unit Spawner", "MUST SPAWN"));
constexpr size_t kSnapToGround =
    layout_schema::must(layout_schema::field("Unit Spawner", "SNAP TO GROUND"));
constexpr size_t kUnitLevelDelta =
    layout_schema::must(layout_schema::field("Unit Spawner", "UNIT LEVEL DELTA"));
constexpr size_t kDestroyBody =
    layout_schema::must(layout_schema::field("Unit Spawner", "DESTROY BODY"));
constexpr size_t kGroup = layout_schema::must(layout_schema::field("Unit Spawner", "GROUP"));
constexpr size_t kResource = layout_schema::must(layout_schema::field("Unit Spawner", "RESOURCE"));
}  // namespace unit_spawner

/// "Missile": the one node of each `media/missiles/*.layout`. `CMissileDescriptor`'s
/// constructor order, less the three `EDITOR_ONLY` ones.
namespace missile {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Missile", "POSITION"));
constexpr size_t kRelease = layout_schema::must(layout_schema::field("Missile", "RELEASE"));
constexpr size_t kActive = layout_schema::must(layout_schema::field("Missile", "ACTIVE"));
constexpr size_t kHit = layout_schema::must(layout_schema::field("Missile", "HIT"));
constexpr size_t kDie = layout_schema::must(layout_schema::field("Missile", "DIE"));
constexpr size_t kNumRicochets =
    layout_schema::must(layout_schema::field("Missile", "NUM RICOCHETS"));
constexpr size_t kMaxDistance =
    layout_schema::must(layout_schema::field("Missile", "MAX DISTANCE"));
constexpr size_t kRadius = layout_schema::must(layout_schema::field("Missile", "RADIUS"));
/// authored `AOE RAIDUS` (sic)
constexpr size_t kAoeRadius = layout_schema::must(layout_schema::field("Missile", "AOE RAIDUS"));
constexpr size_t kMaxVelocity =
    layout_schema::must(layout_schema::field("Missile", "MAX VELOCITY"));
constexpr size_t kFriction = layout_schema::must(layout_schema::field("Missile", "FRICTION"));
constexpr size_t kForce = layout_schema::must(layout_schema::field("Missile", "FORCE"));
constexpr size_t kFullVelocity =
    layout_schema::must(layout_schema::field("Missile", "FULL VELOCITY"));
constexpr size_t kMissileName =
    layout_schema::must(layout_schema::field("Missile", "MISSILE NAME"));
constexpr size_t kHomingSpeed =
    layout_schema::must(layout_schema::field("Missile", "HOMING SPEED"));
constexpr size_t kTargetingAngle =
    layout_schema::must(layout_schema::field("Missile", "TARGETING ANGLE"));
constexpr size_t kTrackGround =
    layout_schema::must(layout_schema::field("Missile", "TRACK GROUND"));
constexpr size_t kTargetPosition =
    layout_schema::must(layout_schema::field("Missile", "TARGET POSITION"));
constexpr size_t kCollidesWithObjects =
    layout_schema::must(layout_schema::field("Missile", "COLLIDES WITH OBJECTS"));
constexpr size_t kArchHeight = layout_schema::must(layout_schema::field("Missile", "ARCH HEIGHT"));
constexpr size_t kHomeAfterSeconds =
    layout_schema::must(layout_schema::field("Missile", "HOME AFTER X SECONDS"));
constexpr size_t kPiercing = layout_schema::must(layout_schema::field("Missile", "PIERCING"));
constexpr size_t kMaxTurnRate =
    layout_schema::must(layout_schema::field("Missile", "MAX TURN RATE"));
constexpr size_t kTurnAcceleration =
    layout_schema::must(layout_schema::field("Missile", "TURN ACCELERATION"));
constexpr size_t kTurnRandomization =
    layout_schema::must(layout_schema::field("Missile", "TURN RANDOMIZATION"));
constexpr size_t kRandomizationRate =
    layout_schema::must(layout_schema::field("Missile", "RANDOMIZATION RATE"));
constexpr size_t kAoeDamageScale =
    layout_schema::must(layout_schema::field("Missile", "AOE DAMAGE SCALE"));
constexpr size_t kDamageSoakScale =
    layout_schema::must(layout_schema::field("Missile", "DAMAGE SOAK SCALE"));
constexpr size_t kVerticalAiming =
    layout_schema::must(layout_schema::field("Missile", "VERTICAL AIMING"));
constexpr size_t kMinPercentDamage =
    layout_schema::must(layout_schema::field("Missile", "MIN PERCENT DAMAGE"));
constexpr size_t kMaxPercentDamage =
    layout_schema::must(layout_schema::field("Missile", "MAX PERCENT DAMAGE"));
constexpr size_t kDamageForce = layout_schema::must(layout_schema::field("Missile", "FORCE", 1));
constexpr size_t kDamageType = layout_schema::must(layout_schema::field("Missile", "DAMAGE TYPE"));
}  // namespace missile

/// "Property Node": a marker (`TYPE`: `No Spawn Region`, `Player Start`, ...). The two
/// `Path Node Occupation` types are pathing blockers a route may not cross.
namespace property_node {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Property Node", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Property Node", "POSITION"));
constexpr size_t kForward = layout_schema::must(layout_schema::field("Property Node", "FORWARD"));
constexpr size_t kRight = layout_schema::must(layout_schema::field("Property Node", "RIGHT"));
constexpr size_t kUp = layout_schema::must(layout_schema::field("Property Node", "UP"));
constexpr size_t kType = layout_schema::must(layout_schema::field("Property Node", "TYPE"));
constexpr size_t kRadius = layout_schema::must(layout_schema::field("Property Node", "RADIUS"));
constexpr size_t kBoxWidth =
    layout_schema::must(layout_schema::field("Property Node", "BOX WIDTH"));
constexpr size_t kBoxLength =
    layout_schema::must(layout_schema::field("Property Node", "BOX LENGTH"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Property Node", "ENABLED"));
}  // namespace property_node

/// "Sound": a sound named by the `GUID` of a `media/sounds/*.DAT.adm` definition; no `VISIBLE`.
/// `ENVIRONMENTAL` with a `RADIUS` is a point source; false with `RADIUS:0` the level-wide bed.
namespace sound {
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Sound", "POSITION"));
constexpr size_t kStartOnActivate =
    layout_schema::must(layout_schema::field("Sound", "START ON ACTIVATE"));
constexpr size_t kEnvironmental =
    layout_schema::must(layout_schema::field("Sound", "ENVIRONMENTAL"));
constexpr size_t kVolume = layout_schema::must(layout_schema::field("Sound", "VOLUME"));
constexpr size_t kRadius = layout_schema::must(layout_schema::field("Sound", "RADIUS"));
constexpr size_t kGuid = layout_schema::must(layout_schema::field("Sound", "GUID"));
}  // namespace sound

/// "Camera Shake" (`CCameraShakeDescriptor`): a `CCameraShake`
/// about its layout's position. Comments give the setters (Linux) and object offsets.
namespace camera_shake {
/// `Set_setDirection`: +0x64
constexpr size_t kDirection =
    layout_schema::must(layout_schema::field("Camera Shake", "DIRECTION"));
/// `Set_setDuration`: +0x58
constexpr size_t kDuration = layout_schema::must(layout_schema::field("Camera Shake", "DURATION"));
/// `Set_setMagnitudeMult`: +0x60
constexpr size_t kMagnitude =
    layout_schema::must(layout_schema::field("Camera Shake", "MAGNITUDE"));
/// `Set_setDirectionOrientation`: +0x88
constexpr size_t kOrientationType =
    layout_schema::must(layout_schema::field("Camera Shake", "ORIENTATION TYPE"));
/// `Set_setCameraShakeName`
constexpr size_t kName =
    layout_schema::must(layout_schema::field("Camera Shake", "CAMERA SHAKE NAME"));
/// `Set_setCameraFallsOffWithDistance`: +0x99
constexpr size_t kFades = layout_schema::must(layout_schema::field("Camera Shake", "FADES"));
}  // namespace camera_shake

/// "Music": a named track a level can start besides its rules file's own.
namespace music {
constexpr size_t kFileName = layout_schema::must(layout_schema::field("Music", "FILE NAME"));
constexpr size_t kLoops = layout_schema::must(layout_schema::field("Music", "LOOPS"));
}  // namespace music

/// "Counter": fires when it has been incremented `EQUALS VALUE` times.
namespace counter {
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Counter", "ENABLED"));
constexpr size_t kEqualsValue =
    layout_schema::must(layout_schema::field("Counter", "EQUALS VALUE"));
constexpr size_t kStartingValue =
    layout_schema::must(layout_schema::field("Counter", "STARTING VALUE"));
constexpr size_t kLogic = layout_schema::must(layout_schema::field("Counter", "LOGIC"));
}  // namespace counter

/// "Warper": sends the players to another level on `Activate Warper`
/// (`CWarperDescriptor::InputLogicEvent` -> `CWarper::activate`
/// -> `CGameClient::warpLevels`).
namespace warper {
constexpr size_t kVisible = layout_schema::must(layout_schema::field("Warper", "VISIBLE"));
constexpr size_t kPosition = layout_schema::must(layout_schema::field("Warper", "POSITION"));
/// then `RIGHT` and `UP` (right = up x forward)
constexpr size_t kForward = layout_schema::must(layout_schema::field("Warper", "FORWARD"));
constexpr size_t kEnabled = layout_schema::must(layout_schema::field("Warper", "ENABLED"));
constexpr size_t kLevelDelta = layout_schema::must(layout_schema::field("Warper", "LEVEL DELTA"));
constexpr size_t kLevelAbsolute =
    layout_schema::must(layout_schema::field("Warper", "LEVEL ABSOLUTE"));
constexpr size_t kDungeonName = layout_schema::must(layout_schema::field("Warper", "DUNGEON NAME"));
constexpr size_t kWarpName = layout_schema::must(layout_schema::field("Warper", "WARP NAME"));
constexpr size_t kWaypoint = layout_schema::must(layout_schema::field("Warper", "WAYPOINT"));
}  // namespace warper

}  // namespace cmp_field

/// How a timeline key reaches its value from the one before it (`gTIMELINE_INTERP_TYPES`).
namespace cmp_interpolation {
constexpr uint32_t kLinear = 0;
constexpr uint32_t kLinearRound = 1;
constexpr uint32_t kLinearRoundDown = 2;
constexpr uint32_t kLinearRoundUp = 3;
constexpr uint32_t kSpline = 4;
constexpr uint32_t kQuaternion = 5;
constexpr uint32_t kNoInterpolation = 6;
constexpr uint32_t kUseTimelineDefault = 7;
}  // namespace cmp_interpolation

/// One wire of a `Logic Group`: a named output to a named input. `to` is the driven object's
/// local index within the group, not a node id.
struct CmpLogicLink {
    uint32_t to = 0;
    std::string output;
    std::string input;
};

/// One member of a `Logic Group`'s graph; `object_id` is the node it stands for. A group is a
/// member of its own graph (the source of `Level Activated`).
struct CmpLogicObject {
    uint32_t local = 0;
    int64_t object_id = 0;

    /// Where the editor drew the box; unused.
    float editor_x = 0.0f;
    float editor_y = 0.0f;

    std::vector<CmpLogicLink> links;
};

/// A `Logic Group` node's graph, from the trailing record section (the node itself has no
/// fields). See `CmpScene::logic_groups`.
struct CmpLogicGroup {
    int64_t node_id = 0;
    std::vector<CmpLogicObject> objects;

    /// The object with this local index, or nullptr.
    const CmpLogicObject* object(uint32_t local) const;

    /// The 64-bit node id the local index stands for, or 0.
    int64_t resolve(uint32_t local) const;
};

/// One key of a timeline track: a time in 0..1 and what to do at it. `words` are raw, as a
/// `CmpField`'s: their type depends on the property driven.
struct CmpTimelineKey {
    float time = 0.0f;
    uint32_t interpolation = cmp_interpolation::kLinear;
    std::vector<uint32_t> words;

    float as_float(size_t index = 0) const;
    uint32_t as_uint(size_t index = 0) const { return index < words.size() ? words[index] : 0u; }
    bool as_bool(size_t index = 0) const { return as_uint(index) != 0u; }

    /// The key as a three-float vector, or the origin if it holds fewer.
    CmpVec3 vec3() const;
};

/// One track of a timeline on one target. A property track animates the field whose index in
/// the target's descriptor is `tag` (`CDescriptor::GetPropertyByIndex`); a tag past the
/// count is dropped (`CTimeline::AddProperty`). An event track
/// (`is_event`) fires a named input at each key and carries no values.
struct CmpTimelineTrack {
    uint32_t tag = 0;
    bool is_event = false;
    std::vector<CmpTimelineKey> keys;
};

/// Everything one timeline drives on one target node.
struct CmpTimelineTarget {
    int64_t object_id = 0;
    std::vector<CmpTimelineTrack> tracks;
};

/// The input an event track fires: `tag` indexes the target descriptor's inputs. Null for a
/// pair the shipped levels never use; report it rather than guess.
const char* timeline_event_input(uint32_t descriptor, uint32_t tag);

/// A `Timeline` node's keyframes, from the trailing record section; how to play them is in
/// the node's fields (`cmp_field::timeline`).
struct CmpTimeline {
    int64_t node_id = 0;
    std::vector<CmpTimelineTarget> targets;
};

struct CmpScene {
    std::vector<CmpNode> nodes;

    /// How many times the reader skipped undecoded bytes to find the next node. Non-zero is
    /// normal; callers log it, so a skipped subtree is never silent.
    size_t resyncs = 0;

    /// The level's script, from the trailing record section after the last node. Each record
    /// is keyed by its node's id and gives the next one's offset, so an unknown one is skipped.
    std::vector<CmpLogicGroup> logic_groups;
    std::vector<CmpTimeline> timelines;

    /// Records in the trailing section, and how many were of an undecoded kind. Logged.
    size_t script_records = 0;
    size_t script_undecoded = 0;

    bool ok = false;
    std::string error;

    /// The first node with this exact name, or nullptr.
    const CmpNode* find(const std::string& name) const;

    /// Every node whose parent id is `id`, in file order.
    std::vector<const CmpNode*> children_of(int64_t id) const;

    /// The node with this 64-bit id, or nullptr.
    const CmpNode* node_by_id(int64_t id) const;

    /// The graph belonging to a `Logic Group` node, or nullptr.
    const CmpLogicGroup* logic_group(int64_t node_id) const;

    /// The keyframes belonging to a `Timeline` node, or nullptr.
    const CmpTimeline* timeline(int64_t node_id) const;
};

/// A "dynamic property" (`cmp_field::dynamic_kind`) as one value: a random range is its
/// midpoint (no draw, so frames are reproducible), a curve is sampled linearly at `t` in 0..1.
/// An unrecognised kind yields `fallback`.
float dynamic_value(const CmpField& field, float t = 0.0f, float fallback = 1.0f);

/// An RGBA key of a `COLOR OVER TIME` field, which is a flat run of
/// (time, r, g, b, a) five-tuples and carries no kind tag.
struct CmpColorKey {
    float time = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

/// How many colour keys a `COLOR OVER TIME` field holds: dwords / 5, or 0 if not a multiple.
size_t color_key_count(const CmpField& field);

/// The colour a `COLOR OVER TIME` field gives at `t`, interpolated between its
/// keys. Returns `fallback` when the field holds no whole key.
CmpColorKey color_at(const CmpField& field, float t, const CmpColorKey& fallback = {});

/// The mean of a track over a particle's whole life: what a steady-state emitter's population
/// sums to under additive blending (exact for colour, approximate for size).
float dynamic_mean(const CmpField& field, float fallback = 1.0f);
CmpColorKey color_mean(const CmpField& field, const CmpColorKey& fallback = {});

/// Parse a `.cmp` from bytes the caller owns. Never throws; on failure `ok == false` and
/// `error` is set, with the nodes read so far kept.
CmpScene parse_cmp(const uint8_t* data, size_t size);

}  // namespace tl::formats
