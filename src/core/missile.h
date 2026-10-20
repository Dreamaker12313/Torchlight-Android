// A missile: what a `Missiles` spawner fires and what flies until it hits. Nothing here draws or
// damages: it moves a missile, finds what it meets and says so (`MissileWorld`, `MissileStep`).
// Also CMissile::checkCollision, CMissile::killMissile,
// CMissile::handleDeathOfMissile,
// CMissile::getClosestTarget,
// CMissileDescriptor::Set_*.
#pragma once

#include "core/collision.h"
#include "core/ogre_math.h"
#include "core/random.h"
#include "core/route.h"
#include "formats/cmp.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// A value that may vary with a percentage: ParticleUniverse's dynamic attribute, as
/// `getDynPropFromArray(const float*, unsigned)` builds one.
struct DynamicAttribute {
    enum class Kind { Fixed, Random, Curved, RandomCurved };
    Kind kind = Kind::Fixed;
    float value = 0.0f;  ///< `Fixed`
    float min = 0.0f;    ///< `Random`
    float max = 0.0f;
    bool spline = false;  ///< spline interpolation (kinds 3 and 5)
    /// `Curved`: the control points, sorted on x. `RandomCurved`: the lower curve.
    std::vector<std::pair<float, float>> points;
    /// `RandomCurved`: the upper curve.
    std::vector<std::pair<float, float>> high_points;

    /// A fixed value, as `getDynPropFromArray` makes one from `{0, value}`.
    /// PORT: the port's attribute.
    static DynamicAttribute fixed(float value) {
        DynamicAttribute d;
        d.value = value;
        return d;
    }

    // PORT: whether `getValue` draws (the random and random-curved kinds).
    bool draws() const { return kind == Kind::Random || kind == Kind::RandomCurved; }
    /// The two ends `Math::RangeRandom` is asked between at `percent`.
    void range(float percent, float& low, float& high) const;

    /// The value at `percent`. `random01` stands for `Ogre::Math::UnitRandom`,
    /// the renderer's own generator rather than either of the game's.
    float at(float percent, float random01 = 0.5f) const;
};

/// A compiled field's words read as `getDynPropFromArray` reads the float array.
DynamicAttribute dynamic_attribute(const tl::formats::CmpField* field);

/// A `Unit Spawner`'s placement shape: the `CShape` half of `CUnitSpawner`, with
/// `CShape::CShape`'s defaults and the words of its static
/// init.
struct SpawnerShape {
    /// +0x104 `SHAPE`: `Angle`, `Line`, `Sphere`, `Point`, `Box`.
    int shape = 0;
    /// +0x100 `RELEASE ORDER`: `Clockwise`, `Counter Clockwise`, `Random`.
    int release_order = 0;
    /// +0x108 `DIRECTION`: `Forward`, `Down`, `Up`, `Outward From Center`,
    /// `Inward to Center`.
    int direction = 0;
    DynamicAttribute min_radius;                                  ///< +0x128 `MIN RADIUS`
    DynamicAttribute max_radius = DynamicAttribute::fixed(2.0f);  ///< +0x120 `MAX RADIUS`
    /// +0x110 `ANGLE`, the angle of release
    DynamicAttribute angle = DynamicAttribute::fixed(360.0f);
    DynamicAttribute angle_offset;  ///< +0x118 `ANGLE OFFSET`
    bool use_max_only = false;      ///< +0x130 `USE MAX ONLY`
    /// +0x134: `getMaxRadiusAtPercent` multiplies the radius by 1 plus it. Nothing sets it.
    float max_radius_scale = 0.0f;
    /// +0x138 `SIZEX`/`SIZEY`/`SIZEZ` (`setBoxSize`).
    Vec3 box_size{10.0f, 1.0f, 10.0f};
};

/// A spawner node's shape fields (`unit_spawner::kShape`..`kAngleOffset`).
SpawnerShape read_spawner_shape(const tl::formats::CmpNode& node);

/// Where a shape stands: its scene node's derived position and local axes
/// (`right` is the local x axis, `forward` the local z), in world space.
struct ShapeFrame {
    Vec3 center;
    Vec3 forward{0.0f, 0.0f, 1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
};

/// `CShape::getMaxRadiusAtPercent`.
float shape_max_radius(const SpawnerShape& shape, float percent);
/// `CShape::getMinRadiusAtPercent`.
float shape_min_radius(const SpawnerShape& shape, float percent);
/// `CShape::getAngleOfReleaseAtPercent`.
float shape_angle_of_release(const SpawnerShape& shape, float percent);
/// `CShape::getAngleOffsetAtPercent`: an `Angle` shape centres its
/// fan on the offset.
float shape_angle_offset(const SpawnerShape& shape, float percent);

/// `CShape::positionIntersectsAngle`.
bool shape_position_intersects_angle(const SpawnerShape& shape, const ShapeFrame& frame,
                                     float percent, const Vec3& at, float radius,
                                     float* distance = nullptr);
/// `CShape::positionWithRadiusIntersectsBox`.
bool shape_position_intersects_box(const SpawnerShape& shape, const ShapeFrame& frame,
                                   const Vec3& at, float radius, float* distance = nullptr);
/// `CShape::calculateLineSequment`: the
/// segment a `Line` shape covers at `percent`.
void shape_line_segment(const SpawnerShape& shape, const ShapeFrame& frame, float percent,
                        Vec3& start, Vec3& end);
/// `CShape::lineIntersectsPosition`.
bool shape_line_intersects_position(const SpawnerShape& shape, const ShapeFrame& frame,
                                    float percent, const Vec3& at, float radius,
                                    float* distance = nullptr);
/// `CShape::getPositionIsInShapeAtPercent`:
/// whether a body of `radius` standing at `at` is inside the shape at
/// `percent`, and (`distance`) how far it is, as the console's answer carries.
bool shape_contains_position(const SpawnerShape& shape, const ShapeFrame& frame, float percent,
                             const Vec3& at, float radius, float* distance = nullptr);

/// An orientation as the three axes `Ogre::Quaternion::FromAxes` is handed (x, y, z); axes,
/// not a quaternion, because the original builds some of them mirrored.
struct SpawnAxes {
    Vec3 x{1.0f, 0.0f, 0.0f};
    Vec3 y{0.0f, 1.0f, 0.0f};
    Vec3 z{0.0f, 0.0f, 1.0f};
};

/// `Ogre::Math::RangeRandom(lo, hi)`: the renderer's own generator, which the
/// shapes draw from, not either of the game's streams.
using RangeRandom = std::function<float(float lo, float hi)>;
/// `CShape::getRadiusAtPercent`: between the
/// two radii by the percent, or (the random order) anywhere between them.
float shape_radius(const SpawnerShape& shape, float percent, const RangeRandom& range_random);

/// Where one unit of a spawner's release comes out and which way it faces:
/// `CShape::updatePositionAndOrientation`. `percent` is the
/// spawner's clock through its `DURATION`, `fraction` the unit's place in the release (both from
/// `CUnitSpawner::spawnUnitByIndex`). A `Box` shape's random order
/// draws `volatile_rng`; everything else random draws `range_random`.
struct SpawnPlacement {
    Vec3 position;
    SpawnAxes axes;
};
SpawnPlacement shape_position_and_orientation(const SpawnerShape& shape, const ShapeFrame& frame,
                                              float percent, float fraction,
                                              const RangeRandom& range_random,
                                              RunicRandom& volatile_rng);

/// A `Missile` node's properties where `CMissile` keeps them, with
/// `CMissile::initialize`'s defaults.
struct MissileSpec {
    std::string name;            ///< +0x2c8 `MISSILE NAME`, upper-cased
    std::string release;         ///< +0x100's layout, `RELEASE`
    std::string active;          ///< +0x108's, `ACTIVE`
    std::string hit;             ///< +0x110's, `HIT`
    std::string die;             ///< +0x118's, `DIE`
    uint32_t ricochets = 0;      ///< +0x180 `NUM RICOCHETS`
    float max_distance = 25.0f;  ///< +0x1b8 `MAX DISTANCE`
    float radius = 0.2f;         ///< +0x178 `RADIUS` (`setRadiusOfMissile`)
    float aoe_radius = 0.0f;     ///< `AOE RAIDUS` (`setAOERadius`)
    float max_velocity = 1.0f;   ///< +0x158 `MAX VELOCITY`
    /// +0x15c: `Set_setFriction` stores **1 - `FRICTION`**, and the speed is
    /// multiplied by it every frame.
    float keep = 1.0f;
    float force = 0.0f;                 ///< +0x154 `FORCE`
    bool full_velocity = true;          ///< +0x140 `FULL VELOCITY`
    float homing = 0.0f;                ///< +0x250 `HOMING SPEED`
    float targeting_angle = 120.0f;     ///< +0x260 `TARGETING ANGLE`
    bool track_ground = false;          ///< +0x188 `TRACK GROUND`
    bool target_position = false;       ///< +0x186 `TARGET POSITION`
    bool collides_with_objects = true;  ///< +0x187 `COLLIDES WITH OBJECTS`
    float arch_height = 0.0f;           ///< +0x19c `ARCH HEIGHT`
    float home_after = 0.0f;            ///< +0x254 `HOME AFTER X SECONDS`
    bool piercing = false;              ///< +0x142 `PIERCING`
    float max_turn_rate = 0.0f;         ///< +0x264 `MAX TURN RATE`
    float turn_acceleration = 0.0f;     ///< +0x268 `TURN ACCELERATION`
    float turn_randomization = 0.0f;    ///< +0x26c `TURN RANDOMIZATION`
    float randomization_rate = 0.25f;   ///< +0x270 `RANDOMIZATION RATE`
    float aoe_damage_scale = 1.0f;      ///< +0x27c `AOE DAMAGE SCALE`
    float soak_scale = 1.0f;            ///< +0x280 `DAMAGE SOAK SCALE`
    /// `VERTICAL AIMING`, set by `CMissile::CMissile`.
    bool vertical_aiming = true;
    float min_damage_percent = 0.0f;  ///< +0x284 `MIN PERCENT DAMAGE`
    float max_damage_percent = 0.0f;  ///< +0x288 `MAX PERCENT DAMAGE`
    float knockback = 0.0f;           ///< +0x28c the `DAMAGE` group's `FORCE`
    std::string damage_type;          ///< +0x2d0 `DAMAGE TYPE`
    /// The node's `POSITION` y: the copy makes it `Missile::lift`, the height a launch adds.
    float height = 0.0f;
};

/// The node of a `media/missiles/*.layout` read into a spec.
MissileSpec read_missile_spec(const tl::formats::CmpNode& node);

struct Missile;
/// `CMissile::CMissile(CResourceManager*, const CMissile*)`:
/// a missile copied from its template, after `CMissile::CMissile(CResourceManager*)`
/// and `initialize` have set the defaults.
Missile missile_from_template(const MissileSpec& spec);

/// What `handleMissileHitUnit` asks of the
/// unit a missile met.
enum class MissileUnitKind {
    Character,
    Item,          ///< `ISA 2` (`ITEM`): a breakable; its `interact` decides
    Interactable,  ///< `ISA 0x20` (`INTERACTABLE`): the missile ends on it, nothing done
};

/// What a missile can ask of the level it flies through.
struct MissileWorld {
    /// A character's centre, body radius (`+0x194`) and life, by id; false
    /// when there is no such character any more.
    std::function<bool(int64_t unit, Vec3& center, float& radius, bool& alive)> unit;
    /// `CLevel::findCharacterWithinView`: the nearest character in the cone, or 0.
    std::function<int64_t(const Vec3& at, const Vec3& forward, float view_angle, float sight)>
        closest_target;
    /// `CLevel::preSortedSphereCollision`: the first thing a sphere of
    /// `radius` meets going from `from` to `to`, skipping the units in
    /// `ignore` - a unit, or a face (`unit` 0). False when the way is clear.
    std::function<bool(const Vec3& from, const Vec3& to, float radius, bool objects,
                       const std::vector<int64_t>& ignore, Vec3& centre, Vec3& normal,
                       int64_t& unit)>
        sweep;
    /// `CLevel::getActiveUnitsAtPosition`: characters (and items)
    /// within `radius` of `position`, in the level's lists order.
    std::function<std::vector<int64_t>(const Vec3& position, float radius,
                                       std::vector<int64_t>* items_out)>
        get_active_units_at_position;
    /// `CItem::interact(nullptr)` (vtable +0x280) on an item the missile met
    /// or its area damage reached: a `CBreakable` breaks (`CBreakable::interact`).
    /// True when it did.
    std::function<bool(int64_t item)> interact_item;
    /// What a unit the sweep returned is; none (a unit test) is a character.
    std::function<MissileUnitKind(int64_t unit)> unit_kind;
    /// `handleMissileHitUnit` past the
    /// missile's own tests: the listeners' `getCharacterCanBeHarmedByMissile`
    /// and `missileApplyingEffects`, with the push direction, damage scale, and
    /// the missile's soak scale. True when the blow landed.
    std::function<bool(int64_t unit, const Vec3& push, float damage_scale, float soak_scale)> harm;
    /// `CLevel::rayCollision`, the level's objects included:
    /// true with the nearest hit's point. Unset (a unit test): no ray.
    std::function<bool(const Vec3& from, const Vec3& to, Vec3& point)> ray;
    /// `g_RandVolatile`, which the turn randomisation draws from.
    RunicRandom* volatile_rolls = nullptr;
    /// The struck character's `CCharacter::getDmgToReflectFromMissile`:
    /// the share it sends back, 0 for none. Unset: none.
    std::function<float(int64_t unit)> reflect;
};

/// One missile in flight: `CMissile`'s state.
struct Missile {
    MissileSpec spec;
    int64_t owner = 0;          ///< +0x220, 0 for the player
    Vec3 owner_center;          ///< the owner's `getPosition(true)` at the launch
    float owner_radius = 0.0f;  ///< the owner's `+0x194`
    /// The unit `CMissile::setTarget` keeps.
    int64_t target = 0;

    Vec3 position;  ///< `CPositionableObject` position
    /// The orientation matrix's columns, as `setDirection` leaves them.
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 forward{0.0f, 0.0f, 1.0f};
    Vec3 velocity;                 ///< +0x148
    Vec3 previous;                 ///< +0x1a0
    float target_distance = 0.0f;  ///< +0x198, `calculateLaunchOrientation`
    float travelled_total = 0.0f;  ///< +0x1bc
    float travelled = 0.0f;        ///< +0x1c0
    float lifted = 0.0f;           ///< +0x16c, how far the launch raised it
    float lift = 0.0f;             ///< +0x170
    float active_left = 0.0f;      ///< +0x174, 0.1 from `fireMissile`
    float death_left = 999.0f;     ///< +0x1b4
    float home_wait = 0.0f;        ///< +0x258
    float home_clock = 0.0f;       ///< +0x25c
    float turn_rate = 0.0f;        ///< +0x274
    float turn_clock = 0.0f;       ///< +0x278
    uint32_t ricocheted = 0;       ///< +0x17c
    bool hits_owner = false;       ///< +0x144
    /// The scale `doDamageToCharacter` multiplies by: the share a reflecting character sent back.
    float damage_scale = 1.0f;
    bool reflected = false;       ///< a character reflected it
    bool dying = false;           ///< +0x184
    bool fired = false;           ///< +0x185
    std::vector<int64_t> struck;  ///< +0x298, the units a piercing missile has passed
    Vec3 target_point;            ///< +0x18c, `calculateLaunchOrientation`'s `to`
    /// +0x210, the arch's `CPath`: built by the first launch at a target with an `ARCH HEIGHT`
    /// and refilled by each one after, never released, so the missile keeps following it.
    Route arch;
    bool arch_built = false;
    float arch_travelled = 0.0f;  ///< +0x1c4, the distance flown along `arch`
    float arch_fraction = 0.0f;   ///< +0x218, cleared by each launch
    /// The direction the arch step turns the missile's scene node (+0x108) to:
    /// the spline's tangent. Unset without an arch.
    Vec3 node_facing{0.0f, 0.0f, 1.0f};
};

/// `CPositionableObject::setDirection` (vtable +0x80): the
/// missile faced along `direction`, `up` its up; its three axes rebuilt.
void missile_set_direction(Missile& m, const Vec3& direction, const Vec3& up);
/// The missile's node orientation, from its axes (`setOrientation`,
/// under a layout's node).
OgreQuaternion missile_orientation(const Missile& m);
/// `CMissile::updatePositionByVelocity(float)`:
/// the velocity, heading and timers moved on by `dt`, and the
/// position the missile goes to.
Vec3 velocity_step(Missile& m, float dt, const MissileWorld& world);

/// `doAOEDamage`'s excepted unit when none is (null in the original): -1, as the player's id is 0.
constexpr int64_t kNoUnitExcepted = -1;

/// A unit candidate for `CLevel::getActiveUnitsAtPosition`.
struct ActiveUnitCandidate {
    int64_t id = 0;
    Vec3 center;
};

/// `CLevel::getActiveUnitsAtPosition`: keeps each candidate whose
/// 3D distance to `position` is <= `radius` (unit's own radius not added), in
/// list order.
std::vector<int64_t> get_active_units_at_position(
    const std::vector<ActiveUnitCandidate>& characters, const Vec3& position, float radius,
    const std::vector<ActiveUnitCandidate>& items = {}, std::vector<int64_t>* items_out = nullptr);

inline std::vector<int64_t> get_active_units_at_position(
    const std::vector<ActiveUnitCandidate>& characters, const Vec3& position, float radius,
    std::vector<int64_t>* items_out) {
    return get_active_units_at_position(characters, position, radius, {}, items_out);
}

/// What one frame did, for the renderer to show.
struct MissileStep {
    bool active_visible = true;  ///< the `ACTIVE` particle still follows
    bool hit = false;            ///< start `HIT` here, along the velocity
    /// `CMissile::ricochetMissile` ran: `CParticle::Start` called
    bool ricochet = false;
    bool killed = false;    ///< `killMissile` ran: start `DIE`
    bool finished = false;  ///< `handleDeathOfMissile` let it go
};

/// `CMissile::fireMissile(owner, target position (ZERO), orientation, target,...)
/// ` as
/// `CUnitSpawner::createAndFireMissile` calls
/// it. Returns what the launch itself did - a point-blank hit is possible.
MissileStep fire_missile(Missile& missile, const Vec3& from, const Vec3& forward,
                         const MissileWorld& world);

/// `CMissile::update(float)`.
MissileStep update_missile(Missile& missile, float dt, const MissileWorld& world);

/// `CMissile::calculateLaunchOrientation`:
/// turn towards `to`, the climb clamped - with the original's own mix of `to`'s
/// x and z and `from`'s height in the clamp - and, with an `ARCH HEIGHT`, lay
/// the arch's path.
void missile_launch_orientation(Missile& missile, const Vec3& from, const Vec3& to,
                                const MissileWorld& world);

}  // namespace tl::core
