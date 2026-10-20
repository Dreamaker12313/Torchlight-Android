#include "core/missile.h"
#include "core/fused.h"
#include "core/ogre_math.h"

#include "core/runic_math.h"
#include "formats/float_convert.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>

namespace tl::core {

std::vector<int64_t> get_active_units_at_position(
    const std::vector<ActiveUnitCandidate>& characters, const Vec3& position, float radius,
    const std::vector<ActiveUnitCandidate>& items, std::vector<int64_t>* items_out) {
    // CLevel::getActiveUnitsAtPosition
    std::vector<int64_t> chars_out;
    for (const auto& u : characters) {
        const float dx = u.center.x - position.x;
        const float dy = u.center.y - position.y;
        const float dz = u.center.z - position.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= radius) {
            chars_out.push_back(u.id);
        }
    }
    if (items_out != nullptr) {
        items_out->clear();
        for (const auto& it : items) {
            const float dx = it.center.x - position.x;
            const float dy = it.center.y - position.y;
            const float dz = it.center.z - position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist <= radius) {
                items_out->push_back(it.id);
            }
        }
    }
    return chars_out;
}

// `CPositionableObject::setDirection(Vector3& direction, Vector3& up)`
// (vtable +0x80): right is rebuilt after up, each product pair one `fmsubs`.
void missile_set_direction(Missile& m, const Vec3& direction, const Vec3& up) {
    const Vec3 d = ogre_normalised(direction);
    const Vec3 u0 = ogre_normalised(up);
    const auto fms = [](float a, float b, float c) { return std::fma(a, b, -c); };
    const Vec3 r0{fms(u0.y, d.z, u0.z * d.y), fms(u0.z, d.x, u0.x * d.z),
                  fms(u0.x, d.y, u0.y * d.x)};
    const Vec3 u{fms(d.y, r0.z, d.z * r0.y), fms(d.z, r0.x, d.x * r0.z),
                 fms(d.x, r0.y, d.y * r0.x)};
    m.right = {fms(d.z, u.y, d.y * u.z), fms(d.x, u.z, d.z * u.x), fms(d.y, u.x, d.x * u.y)};
    m.up = u;
    m.forward = d;
}

// The missile's node orientation, from its matrix's columns as
// `setOrientation` hands them to the node.
OgreQuaternion missile_orientation(const Missile& m) {
    return node_orientation(m.right, m.up, m.forward);
}

namespace {

namespace mf = tl::formats::cmp_field::missile;
namespace sf = tl::formats::cmp_field::unit_spawner;

constexpr float kDegToRad = 0.017453292f;

// `Ogre::Vector3::length`, unfused as on Linux. PORT: the port's vector.
float length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// `Ogre::Vector3::normalise`: left alone below 1e-08. PORT: the port's vector.
Vec3 normalised(const Vec3& v) {
    const float n = length(v);
    if (!(1e-08f < n)) return v;
    const float inv = 1.0f / n;
    return {v.x * inv, v.y * inv, v.z * inv};
}

// The distance on the ground that `CMissile::update(float)`
// 's sums measure with: `+ 0.0 +` for the height.
float ground_distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + 0.0f + dz * dz);
}

// PORT: asset parsing, a field read with its fallback.
int word_index(const std::string& word, std::initializer_list<const char*> names) {
    int i = 0;
    for (const char* name : names) {
        if (word == name) return i;
        ++i;
    }
    // `STRINGS::getStringIndex`'s fallback, which every caller passes as 0.
    return 0;
}

// PORT: asset parsing, a field read with its fallback.
std::string text_of(const tl::formats::CmpNode& node, size_t index) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr ? f->as_text() : std::string();
}

// PORT: asset parsing, a field read with its fallback.
float float_of(const tl::formats::CmpNode& node, size_t index, float fallback) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr && f->count() == 1 ? f->as_float() : fallback;
}

// PORT: asset parsing, a field read with its fallback.
bool bool_of(const tl::formats::CmpNode& node, size_t index, bool fallback) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr && f->count() == 1 ? f->as_bool() : fallback;
}

/// `CMissile::killMissile`: dying, a second
/// to go, `DIE` started. The listeners' `missileDieing`
/// (`CSkillEvent::missileDieing`) is the renderer's, which sees
/// `killed`.
void kill(Missile& m, MissileStep& step) {
    m.dying = true;
    m.death_left = 1.0f;
    step.killed = true;
}

// CMissile::doAOEDamage
void do_aoe_damage(Missile& m, int64_t except, const MissileWorld& world) {
    if (m.spec.aoe_radius <= 0.0f) return;
    if (!world.get_active_units_at_position) return;
    std::vector<int64_t> items;
    // INVENTED: m.position stands for the derived scene-node position.
    //
    std::vector<int64_t> chars =
        world.get_active_units_at_position(m.position, m.spec.aoe_radius, &items);
    for (int64_t unit : chars) {
        if (unit == m.owner) continue;
        if (unit == except) continue;
        Vec3 target_center;
        float target_radius = 0.0f;
        bool alive = false;
        if (world.unit) {
            world.unit(unit, target_center, target_radius, alive);
        }
        // The push is in 3D, not on the ground.
        float dx = target_center.x - m.position.x;
        float dy = target_center.y - m.position.y;
        float dz = target_center.z - m.position.z;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len > 1e-08f) {
            const float inv = 1.0f / len;
            dx *= inv;
            dy *= inv;
            dz *= inv;
        }
        const Vec3 push{dx, dy, dz};
        if (world.harm) {
            world.harm(unit, push, m.spec.aoe_damage_scale, m.spec.soak_scale);
        }
    }
    // Items loop: walks item list with owner and except exclusions.
    for (int64_t item : items) {
        if (item == m.owner) continue;
        if (item == except) continue;
        // The item's vtable +0x280, `interact(nullptr)`: nobody is the breaker.
        if (world.interact_item) world.interact_item(item);
    }
}

// CMissile::ricochetMissile; count raised
Vec3 ricochet_missile(Missile& m, const Vec3& intersection, const Vec3& normal, MissileStep& step) {
    step.ricochet = true;  // CParticle::Start()
    m.ricocheted++;
    const float original_speed = length(m.velocity);

    const float normal_len = length(normal);
    Vec3 n = normal;
    if (normal_len > 1e-08f) {
        const float inv = 1.0f / normal_len;
        n = {normal.x * inv, normal.y * inv, normal.z * inv};
    }

    const float dot = m.velocity.x * n.x + m.velocity.y * n.y + m.velocity.z * n.z;
    m.velocity.x -= 2.0f * dot * n.x;
    m.velocity.y -= 2.0f * dot * n.y;
    m.velocity.z -= 2.0f * dot * n.z;

    if (normal.y == 0.0f) {
        m.velocity.y = 0.0f;
        const float xz_speed = std::sqrt(m.velocity.x * m.velocity.x + m.velocity.z * m.velocity.z);
        if (xz_speed > 1e-08f) {
            const float inv = 1.0f / xz_speed;
            m.velocity.x *= inv;
            m.velocity.z *= inv;
        }
        m.velocity.x *= original_speed;
        m.velocity.z *= original_speed;
    }

    const float clearance = m.spec.radius + 0.01f;
    return {n.x * clearance + intersection.x, n.y * clearance + intersection.y,
            n.z * clearance + intersection.z};
}

/// `CMissile::handleDeathOfMissile(float)`:
/// the second runs out, then the particles are stopped and the missile waits
/// for them to empty - which is the renderer's to judge, so `finished` means
/// "stopped".
MissileStep& run_death(Missile& m, float dt, MissileStep& step) {
    m.death_left -= dt;
    if (0.0f < m.death_left) return step;
    step.finished = true;
    return step;
}

/// `CMissile::handleMissileHitUnit(Vector3, CCharacter*)`.
///
bool hit_unit(Missile& m, int64_t unit, const Vec3& toward, const MissileWorld& world) {
    // `ISA(0x20)`: an interactable takes the missile and nothing happens to it.
    // `ISA(2)`: an item's own `interact(nullptr)` (vtable +0x280) stands for
    // the blow, and the area damage follows when it answered.
    const MissileUnitKind kind =
        world.unit_kind ? world.unit_kind(unit) : MissileUnitKind::Character;
    if (kind == MissileUnitKind::Interactable) return true;
    if (kind == MissileUnitKind::Item) {
        const bool broke = world.interact_item && world.interact_item(unit);
        if (broke) do_aoe_damage(m, unit, world);
        return broke;
    }
    // a character that reflects it sends it back instead of
    // taking it; not a hit, it flies on. The listeners' own test before it
    // is not run: the port's listeners are the harm hook.
    if (world.reflect) {
        const float share = world.reflect(unit);
        if (0.0f < share) {
            m.damage_scale = share;
            m.reflected = true;
            m.forward = {m.forward.x * -1.0f, m.forward.y * -1.0f, m.forward.z * -1.0f};
            m.right = {m.right.x * -1.0f, m.right.y * -1.0f, m.right.z * -1.0f};
            m.target = 0;
            m.velocity = {m.velocity.x * -1.0f, m.velocity.y * -1.0f, m.velocity.z * -1.0f};
            m.travelled_total = 0.0f;
            m.travelled = 0.0f;
            m.arch_travelled = 0.0f;
            return false;
        }
    }
    // The push is the direction to the unit on the ground, and the blow's own
    // push that direction plus twice the missile's heading on the ground.
    const Vec3 push = normalised({toward.x, 0.0f, toward.z});
    const Vec3 heading = normalised({m.velocity.x, 0.0f, m.velocity.z});
    const Vec3 blow = normalised(
        {heading.x + heading.x + push.x, 0.0f + 0.0f + 0.0f, heading.z + heading.z + push.z});
    // `doDamageToCharacter(unit, blow, 1.0, +0x280)`:
    // the listeners decide. INVENTED: a
    // missile with no listener (a trap's) doing the damage graph times its own
    // `MIN/MAX PERCENT DAMAGE` itself is not transcribed.
    // `+0x288 * 1.0`: a reflected missile does its share.
    const bool applied =
        world.harm && world.harm(unit, blow, m.damage_scale * 1.0f, m.spec.soak_scale);
    if (applied) {
        do_aoe_damage(m, unit, world);
    }
    return applied;
}

/// `CMissile::checkCollision(Vector3, Vector3, Vector3&, Vector3&, Vector3&,
/// CBaseUnit**)`. `to` comes back as where
/// the missile ends up.
bool check_collision(Missile& m, const Vec3& from_in, Vec3& to, int64_t& unit_out,
                     MissileStep& step, const MissileWorld& world) {
    unit_out = 0;
    Vec3 from = from_in;
    from.y += m.lift;
    Vec3 end = to;
    end.y += m.lift;
    to = end;

    // A piercing missile turns off the collision of every unit it has already
    // passed (`+0x18d`) for the length of the sweep.
    static const std::vector<int64_t> kNone;
    const std::vector<int64_t>& ignore = m.spec.piercing ? m.struck : kNone;
    Vec3 centre;
    Vec3 normal;
    int64_t unit = 0;
    const bool met =
        world.sweep && world.sweep(from, end, m.spec.radius, m.spec.collides_with_objects, ignore,
                                   centre, normal, unit);
    if (!met) {
        to.y -= m.lift;
        return false;
    }

    if (unit != 0) {
        // The missile's own owner, unless it may hit it (`+0x144`), is flown
        // through.
        if (!m.hits_owner && unit == m.owner) {
            to.y -= m.lift;
            return false;
        }
        Vec3 target_center;
        float target_radius = 0.0f;
        bool alive = false;
        Vec3 toward{0.0f, 0.0f, 1.0f};
        if (world.unit && world.unit(unit, target_center, target_radius, alive)) {
            toward = target_center - from;
        }
        const bool applied = hit_unit(m, unit, toward, world);
        if (!applied) {
            // Not harmed: flown through, at the sweep's end.
            to.y -= m.lift;
            return false;
        }
        unit_out = unit;
        if (m.spec.piercing) {
            // A piercing missile remembers the unit and flies.
            if (std::find(m.struck.begin(), m.struck.end(), unit) == m.struck.end()) {
                m.struck.push_back(unit);
            }
            to.y -= m.lift;
            return true;
        }
    } else if (m.spec.track_ground && 0.2f < normal.y) {
        // A face. One that faces up enough, for a missile that tracks the
        // ground, is slid along; anything else ends the missile there.
        to = centre;
        to.y += m.spec.radius;
        to.y -= m.lift;
        return false;
    }

    // The tail a wall and a non-piercing blow share: a ricochet
    // while any are left (N authored allow N+1 bounces), then the same test
    // decides whether the missile lives.
    to = centre;
    if (m.spec.ricochets != 0 && m.ricocheted <= m.spec.ricochets) {
        to = ricochet_missile(m, centre, normal, step);
        m.struck.clear();
        m.travelled = 0.0f;
        if (m.spec.ricochets != 0 && m.ricocheted <= m.spec.ricochets) {
            to.y -= m.lift;
            return true;
        }
    }
    if (!m.dying) {
        kill(m, step);
        if (unit == 0) do_aoe_damage(m, kNoUnitExcepted, world);
    }
    to.y -= m.lift;
    return true;
}

}  // namespace

// `CMissile::updatePositionByVelocity(float)`:
// the next position.
Vec3 velocity_step(Missile& m, float dt, const MissileWorld& world) {
    // `FORCE` pushes along the stored forward, then again along the node's z axis.
    const auto push = [&](const Vec3& along) {
        m.velocity = {m.velocity.x + (along.x * m.spec.force) * dt,
                      m.velocity.y + (along.y * m.spec.force) * dt,
                      m.velocity.z + (along.z * m.spec.force) * dt};
    };
    push(m.forward);
    push(quaternion_z_axis(missile_orientation(m)));
    // The speed, and the velocity normalised in place (the length
    // in two `fmadds`, left alone at or below 1e-08).
    const float speed =
        std::sqrt(std::fma(m.velocity.z, m.velocity.z,
                           std::fma(m.velocity.x, m.velocity.x, m.velocity.y * m.velocity.y)));
    if (static_cast<double>(speed) > 1e-08) {
        const float inverse = 1.0f / speed;
        m.velocity = {m.velocity.x * inverse, m.velocity.y * inverse, m.velocity.z * inverse};
    }

    m.home_wait -= dt;
    // A target that died, or that a piercing missile has already struck, is
    // traded for the nearest in the cone.
    if (m.target != 0) {
        Vec3 c;
        float r = 0.0f;
        bool alive = false;
        const bool known = world.unit && world.unit(m.target, c, r, alive);
        const bool struck = std::find(m.struck.begin(), m.struck.end(), m.target) != m.struck.end();
        if (!known || !alive || struck) {
            m.target = world.closest_target
                           ? world.closest_target(m.position, m.forward, m.spec.targeting_angle,
                                                  m.spec.max_distance + 1.0f)
                           : 0;
        }
    }

    // Homing, on its own 30 Hz clock once `HOME AFTER X SECONDS`
    // has passed: the heading blended towards the target's centre raised by
    // twice the missile's radius (fused), each blend renormalised.
    if (!(m.home_wait > 0.0f) && m.target != 0 && m.spec.homing > 0.0f) {
        m.home_clock = m.home_clock + dt;
        constexpr float kTick = 0.033333335f;
        while (!(m.home_clock < kTick)) {
            m.home_clock = m.home_clock - kTick;
            Vec3 c;
            float r = 0.0f;
            bool alive = false;
            if (!(world.unit && world.unit(m.target, c, r, alive))) break;
            const float raised = std::fma(m.spec.radius, 2.0f, c.y);
            Vec3 toward{c.x - m.position.x, raised - m.position.y, c.z - m.position.z};
            const float length = std::sqrt(
                std::fma(toward.y, toward.y, std::fma(toward.x, toward.x, toward.z * toward.z)));
            if (static_cast<double>(length) > 1e-08) {
                const float inverse = 1.0f / length;
                toward = {toward.x * inverse, toward.y * inverse, toward.z * inverse};
            }
            const float h = m.spec.homing;
            const float keep = 1.0f - h;
            m.velocity = {m.velocity.x * keep + h * toward.x, m.velocity.y * keep + toward.y * h,
                          m.velocity.z * keep + toward.z * h};
            m.velocity = ogre_normalised(m.velocity);
        }
    }

    // The turn: its rate grows by `TURN ACCELERATION` (fused),
    // takes a volatile draw in +/-`TURN RANDOMIZATION` every `RANDOMIZATION
    // RATE` seconds (drawn even when that range is nothing), and is held to
    // `MAX TURN RATE`.
    m.turn_clock = m.turn_clock + dt;
    m.turn_rate = std::fma(m.spec.turn_acceleration, dt, m.turn_rate);
    if (m.turn_clock > m.spec.randomization_rate) {
        const float jitter = world.volatile_rolls != nullptr
                                 ? world.volatile_rolls->between(m.spec.turn_randomization * -1.0f,
                                                                 m.spec.turn_randomization)
                                 : 0.0f;
        m.turn_rate = jitter + m.turn_rate;
        m.turn_clock = 0.0f;
    }
    if (std::fabs(m.turn_rate) > std::fabs(m.spec.max_turn_rate)) {
        m.turn_rate = m.turn_rate < 0.0f ? m.spec.max_turn_rate * -1.0f : m.spec.max_turn_rate;
    }
    m.velocity = rotate_y(m.velocity, m.turn_rate * dt * kDegToRad);

    // Faced along the new heading, then moving at the old speed less the friction.
    // DIVERGENCE: friction is applied once per frame, so it is frame-rate dependent.
    const float scaled = m.spec.keep * speed;
    // The heading normalised inline (the squares summed z, y, x).
    Vec3 heading = m.velocity;
    const float heading_length = std::sqrt(
        std::fma(heading.x, heading.x, std::fma(heading.y, heading.y, heading.z * heading.z)));
    if (static_cast<double>(heading_length) > 1e-08) {
        const float inverse = 1.0f / heading_length;
        heading = {heading.x * inverse, heading.y * inverse, heading.z * inverse};
    }
    missile_set_direction(m, heading, {0.0f, 1.0f, 0.0f});
    m.velocity = m.velocity * scaled;

    if (!m.arch_built || !(m.spec.arch_height > 0.0f)) {
        // the old position plus the velocity's step; the particle
        // (+0xF4) faced along the heading.
        const Vec3 at{m.position.x + m.velocity.x * dt, m.velocity.y * dt + m.position.y,
                      m.position.z + m.velocity.z * dt};
        m.node_facing = ogre_normalised(m.velocity);
        return at;
    }

    // The arch: the missile is not moved by its velocity but
    // carried along the path, as far as its speed takes it this frame (fused).
    // Its heading and velocity above are still kept up.
    const float length =
        std::sqrt(std::fma(m.velocity.z, m.velocity.z,
                           std::fma(m.velocity.x, m.velocity.x, m.velocity.y * m.velocity.y)));
    float along = std::fma(length, dt, m.arch_travelled);
    const float path_length = m.arch.length();
    along = path_length < along ? path_length : along;
    m.arch_travelled = along;
    const Vec3 at = m.arch.spline_position_at_distance(along);
    // At the path's end the travelled counters are set so that `update`'s
    // next `MAX DISTANCE` test kills the missile.
    if (!(m.arch_travelled < path_length)) {
        m.travelled = m.spec.max_distance;
        m.travelled_total = m.target_distance;
    }
    // The particle faces along the spline: two samples 0.02 apart, the later
    // one at most the path's end.
    float ahead = m.arch_travelled + 0.01f;
    ahead = path_length < ahead ? path_length : ahead;
    float behind = ahead - 0.02f;
    behind = behind < 0.0f ? 0.0f : behind;
    const Vec3 from = m.arch.spline_position_at_distance(behind);
    const Vec3 to = m.arch.spline_position_at_distance(ahead);
    m.node_facing = ogre_normalised({to.x - from.x, to.y - from.y, to.z - from.z});
    return at;
}

namespace {

// `ParticleUniverse::DynamicAttributeCurved::getValue(Particle*, float)`
// over control points `processControlPoints`
// has sorted.
float curve_value(const std::vector<std::pair<float, float>>& points, bool spline, float x) {
    const size_t n = points.size();
    if (n == 0) return 0.0f;
    if (spline) {
        // `+0xc`, the last x less the first; the position is `x / range`, not
        // the distance from the first point, and anything from 1 up is 1.
        const float range = points.back().first - points.front().first;
        float t = 1.0f;
        if (range != 0.0f) {
            t = x / range;
            if (1.0f <= t) t = 1.0f;
        }
        // `Ogre::SimpleSpline::interpolate(Real)` over the points (x, y, 0),
        // the y of it: the segment by index, then the Hermite blend with the
        // tangents of `recalcTangents` (closed when the first point is the last).
        const float segment = t * static_cast<float>(n - 1);
        const auto index = static_cast<unsigned>(segment);
        const float s = segment - static_cast<float>(index);
        if (index + 1 == n || s == 0.0f) return points[index].second;
        if (s == 1.0f) return points[index + 1].second;
        const bool closed = points.front() == points.back();
        const auto tangent = [&](size_t i) {
            if (i == 0) {
                return closed ? 0.5f * (points[1].second - points[n - 2].second)
                              : 0.5f * (points[1].second - points[0].second);
            }
            if (i == n - 1) {
                return closed ? 0.5f * (points[1].second - points[n - 2].second)
                              : 0.5f * (points[i].second - points[i - 1].second);
            }
            return 0.5f * (points[i + 1].second - points[i - 1].second);
        };
        const float s2 = s * s;
        const float s3 = s2 * s;
        const float w0 = s3 * 2.0f + s2 * -3.0f + s * 0.0f + 1.0f;
        const float w1 = s3 * -2.0f + s2 * 3.0f + s * 0.0f + 0.0f;
        const float w2 = s3 * 1.0f + s2 * -2.0f + s * 1.0f + 0.0f;
        const float w3 = s3 * 1.0f + s2 * -1.0f + s * 0.0f + 0.0f;
        return w0 * points[index].second + w1 * points[index + 1].second + w2 * tangent(index) +
               w3 * tangent(index + 1);
    }
    // Linear: the point before the first whose x is past `x`, and the one
    // after it; before the first point its value, past the last the last's.
    size_t before = 0;
    size_t after = 1;
    if (points[0].first <= x) {
        for (;;) {
            after = before + 1;
            if (after == n) return points[n - 1].second;
            if (!(points[after].first <= x)) break;
            before = after;
        }
    } else if (after == n) {
        return points[0].second;
    }
    const float from = points[before].first;
    if (from < x) {
        return ((points[after].second - points[before].second) * (x - from)) /
                   (points[after].first - from) +
               points[before].second;
    }
    return points[before].second;
}

// `processControlPoints`: `std::sort` by x.
// Up to 16 points - every shipped array - libstdc++'s sort is one insertion
// sort, which keeps points of equal x in their order, as this does.
void sort_control_points(std::vector<std::pair<float, float>>& points) {
    std::stable_sort(points.begin(), points.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
}

}  // namespace

void DynamicAttribute::range(float percent, float& low, float& high) const {
    if (kind == Kind::RandomCurved) {
        // `DynamicAttributeRandomCurved::getValue`:
        // each curve at x.
        low = curve_value(points, spline, percent);
        high = curve_value(high_points, spline, percent);
        return;
    }
    low = min;
    high = max;
}

// ParticleUniverse's `DynamicAttribute::getValue` for the four kinds, the random
// one `Math::RangeRandom(min, max)`. PORT: of the port's attribute.
float DynamicAttribute::at(float percent, float random01) const {
    switch (kind) {
        case Kind::Fixed: return value;
        case Kind::Random:
            // `DynamicAttributeRandom::getValue`: `Math::RangeRandom(min, max)`.
            return min + (max - min) * random01;
        case Kind::Curved: return curve_value(points, spline, percent);
        case Kind::RandomCurved: {
            // `Math::RangeRandom(low, high)`: no particle holds a roll for a
            // shape's attribute.
            float low = 0.0f;
            float high = 0.0f;
            range(percent, low, high);
            return (high - low) * random01 + low;
        }
    }
    return value;
}

DynamicAttribute dynamic_attribute(const tl::formats::CmpField* field) {
    // `getDynPropFromArray(const float*, unsigned)`.
    //
    DynamicAttribute out;
    if (field == nullptr || field->count() == 0) return out;
    const size_t count = field->count();
    const uint32_t rest = static_cast<uint32_t>(count - 1);
    const int kind = static_cast<int>(tl::formats::x86_float_to_int64(field->as_float(0)));
    switch (kind) {
        case 0: out.value = field->as_float(1); return out;
        case 1:
            if (1 < static_cast<int>(rest)) {
                out.kind = DynamicAttribute::Kind::Random;
                out.min = field->as_float(1);
                out.max = field->as_float(2);
                return out;
            }
            break;
        case 2:
        case 3:
            // `DynamicAttributeCurved(kind != 2)`: pairs of x and y.
            if (1 < static_cast<int>(rest) && (rest & 1u) == 0) {
                out.kind = DynamicAttribute::Kind::Curved;
                out.spline = kind != 2;
                for (uint32_t i = 0; i < rest; i += 2) {
                    out.points.emplace_back(field->as_float(i + 1), field->as_float(i + 2));
                }
                sort_control_points(out.points);
                return out;
            }
            break;
        case 4:
        case 5:
            // `DynamicAttributeRandomCurved(kind != 4)`: triplets of x and two
            // values, the smaller into the lower curve and the larger into the
            // upper (`addControlPoint`).
            if (1 < static_cast<int>(rest) &&
                rest == static_cast<uint32_t>((static_cast<int>(rest) / 3) * 3)) {
                out.kind = DynamicAttribute::Kind::RandomCurved;
                out.spline = kind != 4;
                for (uint32_t i = 0; i < rest; i += 3) {
                    const float x = field->as_float(i + 1);
                    const float a = field->as_float(i + 2);
                    const float b = field->as_float(i + 3);
                    out.points.emplace_back(x, a <= b ? a : b);
                    out.high_points.emplace_back(x, b <= a ? a : b);
                }
                sort_control_points(out.points);
                sort_control_points(out.high_points);
                return out;
            }
            break;
        default: break;
    }
    out.value = 0.0f;
    return out;
}

SpawnerShape read_spawner_shape(const tl::formats::CmpNode& node) {
    SpawnerShape s;
    s.shape = word_index(text_of(node, sf::kShape), {"Angle", "Line", "Sphere", "Point", "Box"});
    s.release_order =
        word_index(text_of(node, sf::kReleaseOrder), {"Clockwise", "Counter Clockwise", "Random"});
    s.direction = word_index(text_of(node, sf::kDirection),
                             {"Forward", "Down", "Up", "Outward From Center", "Inward to Center"});
    s.use_max_only = bool_of(node, sf::kUseMaxOnly, false);
    // A field the node does not carry leaves the constructor's value.
    const auto dynamic = [&node](size_t index, DynamicAttribute& into) {
        const tl::formats::CmpField* f = node.field(index);
        if (f != nullptr && f->count() != 0) into = dynamic_attribute(f);
    };
    dynamic(sf::kMinRadius, s.min_radius);
    dynamic(sf::kMaxRadius, s.max_radius);
    dynamic(sf::kAngle, s.angle);
    dynamic(sf::kAngleOffset, s.angle_offset);
    // `setBoxSize` from `SIZE`; the
    // constructor's 10, 1, 10 without it.
    const tl::formats::CmpField* size = node.field(sf::kSize);
    if (size != nullptr && size->count() >= 3) {
        s.box_size = {size->as_float(0), size->as_float(1), size->as_float(2)};
    }
    return s;
}

namespace {

/// The console's degrees-per-radian literal (0x42652eed), a
/// hair off 180 / pi.
constexpr float kShapeRadToDeg = 57.29582595825195f;

/// `atan2f(x, z)` (the console's, rounded to single) in
/// degrees, as `positionIntersectsAngle` measures a direction.
float shape_heading_raw(float x, float z) {
    return static_cast<float>(crt_atan2(static_cast<double>(x), static_cast<double>(z))) *
           kShapeRadToDeg;
}

/// whether a heading lies in an arc, a turn either way
/// included, as its branches go (`blt`, `bgt`, `ble`: a NaN heading is inside).
bool within_arc(float angle, float low, float high) {
    if (!(angle < low) && !(angle > high)) return true;
    if (!(angle < low + 360.0f) && !(angle > high + 360.0f)) return true;
    if (angle < low - 360.0f) return false;
    return !(angle > high - 360.0f);
}

}  // namespace

// `CShape::getMaxRadiusAtPercent`.
float shape_max_radius(const SpawnerShape& shape, float percent) {
    return shape.max_radius.at(percent) * (shape.max_radius_scale + 1.0f);
}

// `CShape::getMinRadiusAtPercent`: `USE MAX ONLY` answers the max.
float shape_min_radius(const SpawnerShape& shape, float percent) {
    if (shape.use_max_only) return shape_max_radius(shape, percent);
    return shape.min_radius.at(percent);
}

// `CShape::getAngleOfReleaseAtPercent`.
float shape_angle_of_release(const SpawnerShape& shape, float percent) {
    return shape.angle.at(percent);
}

// `CShape::getAngleOffsetAtPercent`.
float shape_angle_offset(const SpawnerShape& shape, float percent) {
    // one `fnmsubs`, `offset - angle * 0.5`, for a circle.
    const float offset = shape.angle_offset.at(percent);
    if (shape.shape != 0) return offset;
    // `fnmsubs` negates its rounded `angle * 0.5 - offset`: an exact zero is -0.
    return fnmsub(shape_angle_of_release(shape, percent), 0.5f, offset);
}

// `CShape::positionIntersectsAngle`: within
// the radii on the ground, then within the fan, or touching it with the body's
// edge. The distance it answers with is the centre's, or the touching side's.
bool shape_position_intersects_angle(const SpawnerShape& shape, const ShapeFrame& frame,
                                     float percent, const Vec3& at, float radius, float* distance) {
    float dx = at.x - frame.center.x;
    float dy = 0.0f;
    float dz = at.z - frame.center.z;
    const float length = std::sqrt(std::fma(dx, dx, dz * dz));
    if (static_cast<double>(length) > 1e-08) {
        const float inv = 1.0f / length;
        dx = inv * dx;
        dy = inv * dy;
        dz = inv * dz;
    }
    const auto answer = [&](bool hit, float d) {
        if (distance != nullptr) *distance = d;
        return hit;
    };
    if (length > shape_max_radius(shape, percent) + radius) return answer(false, length);
    if (length < shape_min_radius(shape, percent) - radius) return answer(false, length);
    const float release = shape_angle_of_release(shape, percent);
    if (!(release < 360.0f)) return answer(true, length);

    // The shape's facing (its absolute forward, the node's z axis) and the
    // direction, each brought into [0, 360).
    const Vec3 forward = quaternion_z_axis(node_orientation(frame.right, frame.up, frame.forward));
    float facing = shape_heading_raw(forward.x, forward.z);
    float toward = shape_heading_raw(dx, dz);
    if (facing < 0.0f) facing = facing + 360.0f;
    if (toward < 0.0f) toward = toward + 360.0f;
    const float relative = toward - facing;
    const float offset = shape_angle_offset(shape, percent);
    const float end = release + offset;
    const float low = offset < end ? offset : end;
    const float high = end < offset ? offset : end;
    if (within_arc(relative, low, high)) return answer(true, length);

    // The two sides of the body, square to the direction (`d x UNIT_Y`, scaled
    // by the radius), each normalised before its heading is taken.
    const float side_z = std::fma(1.0f, dx, -(dy * 0.0f)) * radius;
    const float side_x = std::fma(dy, 0.0f, -(1.0f * dz)) * radius;
    const Vec3 left =
        ogre_normalised({(at.x + side_x) - frame.center.x, 0.0f, (at.z + side_z) - frame.center.z});
    const Vec3 right =
        ogre_normalised({(at.x - side_x) - frame.center.x, 0.0f, (at.z - side_z) - frame.center.z});
    const auto length_of = [](const Vec3& v) {
        return std::sqrt(std::fma(v.z, v.z, std::fma(v.x, v.x, v.y * v.y)));
    };
    const float left_length =
        length_of({(at.x + side_x) - frame.center.x, 0.0f, (at.z + side_z) - frame.center.z});
    const float right_length =
        length_of({(at.x - side_x) - frame.center.x, 0.0f, (at.z - side_z) - frame.center.z});
    float left_heading = shape_heading_raw(left.x, left.z) - facing;
    float right_heading =
        std::fma(static_cast<float>(crt_atan2(right.x, right.z)), kShapeRadToDeg, -facing);
    if (left_heading < 0.0f) left_heading = left_heading + 360.0f;
    if (right_heading < 0.0f) right_heading = right_heading + 360.0f;
    if (within_arc(left_heading, low, high)) return answer(true, left_length);
    if (within_arc(right_heading, low, high)) return answer(true, right_length);
    return answer(false, length);
}

// `CShape::positionWithRadiusIntersectsBox`:
// the point into the object's frame (the inverse of its transform), then the
// body's square, as tall as the world, against the box's half widths across x
// and z, overlapping or inside. The distance it answers with is the full one
// to the centre.
bool shape_position_intersects_box(const SpawnerShape& shape, const ShapeFrame& frame,
                                   const Vec3& at, float radius, float* distance) {
    const OgreMatrix4 to_local = matrix_inverse(
        object_transform(node_orientation(frame.right, frame.up, frame.forward), frame.center));
    const Vec3 local = matrix_transform(to_local, at);
    const float half_x = shape.box_size.x * 0.5f;
    const float half_z = shape.box_size.z * 0.5f;
    const Vec3 d = frame.center - at;
    const float length = std::sqrt(std::fma(d.x, d.x, std::fma(d.z, d.z, d.y * d.y)));
    const OgreBox box = ogre_box({-half_x, -10000.0f, -half_z}, {half_x, 10000.0f, half_z});
    const OgreBox body = ogre_box({local.x - radius, -10000.0f, local.z - radius},
                                  {local.x + radius, 10000.0f, local.z + radius});
    if (distance != nullptr) *distance = length;
    return box_intersects(box, body) || box_contains(box, body);
}

// `CShape::calculateLineSequment`: the
// direction at the release plus its offset, each turned to radians by OGRE's
// `Degree`, with the console's own cosine and sine (each rounded to single);
// the two ends along it through the node's orientation, then its position.
void shape_line_segment(const SpawnerShape& shape, const ShapeFrame& frame, float percent,
                        Vec3& start, Vec3& end) {
    float low = shape_min_radius(shape, percent);
    const float high = shape_max_radius(shape, percent);
    if (high == low) low = 0.0f;
    const float release = kOgreDegToRad * shape_angle_of_release(shape, percent);
    const float offset = kOgreDegToRad * shape_angle_offset(shape, percent);
    const float theta = offset + release;
    const float c = static_cast<float>(crt_cos(theta));
    const float s = static_cast<float>(crt_sin(theta));
    const OgreQuaternion q = node_orientation(frame.right, frame.up, frame.forward);
    const Vec3 near = quaternion_rotate(q, {c * low, 0.0f * low, s * low});
    const Vec3 far = quaternion_rotate(q, {c * high, 0.0f * high, s * high});
    start = {frame.center.x + near.x, frame.center.y + near.y, frame.center.z + near.z};
    end = {frame.center.x + far.x, frame.center.y + far.y, frame.center.z + far.z};
}

// `CShape::lineIntersectsPosition`, inlined on the console in
// `getPositionIsInShapeAtPercent`: the nearest point of the
// segment, measured on the ground.
bool shape_line_intersects_position(const SpawnerShape& shape, const ShapeFrame& frame,
                                    float percent, const Vec3& at, float radius, float* distance) {
    Vec3 start;
    Vec3 end;
    shape_line_segment(shape, frame, percent, start, end);
    const Vec3 nearest = closest_point_on_line(start, end, at);
    const float dx = nearest.x - at.x;
    const float dz = nearest.z - at.z;
    const float d = std::sqrt(std::fma(dz, dz, dx * dx));
    if (distance != nullptr) *distance = d;
    return !(d > radius);  // `ble`: a NaN distance is a hit
}

// `CShape::getPositionIsInShapeAtPercent`:
// whether the body reaches the shape, and how far it is (the struct the
// console answers with).
bool shape_contains_position(const SpawnerShape& shape, const ShapeFrame& frame, float percent,
                             const Vec3& at, float radius, float* distance) {
    switch (shape.shape) {
        case 1: return shape_line_intersects_position(shape, frame, percent, at, radius, distance);
        case 0: return shape_position_intersects_angle(shape, frame, percent, at, radius, distance);
        case 2: {  // `Sphere`: the full distance, and no minimum radius.
            const Vec3 d = frame.center - at;
            const float length = std::sqrt(std::fma(d.x, d.x, std::fma(d.z, d.z, d.y * d.y)));
            if (distance != nullptr) *distance = length;
            return !(length > shape_max_radius(shape, percent) + radius);
        }
        case 3: {  // `Point`: the body must reach the centre.
            const Vec3 d = frame.center - at;
            const float length = std::sqrt(std::fma(d.x, d.x, std::fma(d.z, d.z, d.y * d.y)));
            if (distance != nullptr) *distance = length;
            return !(length > radius);
        }
        default: return shape_position_intersects_box(shape, frame, at, radius, distance);
    }
}

namespace {

// PORT: a dynamic attribute read as `getValue` reads it, a draw for the random kinds.
float attribute_at(const DynamicAttribute& a, float percent, const RangeRandom& range_random) {
    if (a.draws()) {
        float low = 0.0f;
        float high = 0.0f;
        a.range(percent, low, high);
        return range_random(low, high);
    }
    return a.at(percent);
}

}  // namespace

// `CShape::getRadiusAtPercent`: between the
// two radii by the percent, or anywhere between them for the random order.
float shape_radius(const SpawnerShape& shape, float percent, const RangeRandom& range_random) {
    const float scale = shape.max_radius_scale + 1.0f;
    const float low = shape.use_max_only
                          ? attribute_at(shape.max_radius, percent, range_random) * scale
                          : attribute_at(shape.min_radius, percent, range_random);
    const float high = attribute_at(shape.max_radius, percent, range_random) * scale;
    // one `fmadds`.
    if (shape.release_order != 2) return std::fma(high - low, percent, low);
    return range_random(low, high);
}

SpawnPlacement shape_position_and_orientation(const SpawnerShape& shape, const ShapeFrame& frame,
                                              float percent, float fraction,
                                              const RangeRandom& range_random,
                                              RunicRandom& volatile_rng) {
    SpawnPlacement out;
    // `CShape::calculatePositionFromPercent`.
    switch (shape.shape) {
        case 0: {
            // `CShape::calculatePositionBetweenAnglesAndRadius`:
            // the radius, then the release
            // and its offset, then the angle by the order.
            const float radius = shape_radius(shape, percent, range_random);
            float angle = attribute_at(shape.angle, percent, range_random);
            // `getAngleOffsetAtPercent`: `offset - angle * 0.5` in one `fnmsubs`.
            const float offset =
                fnmsub(angle, 0.5f, attribute_at(shape.angle_offset, percent, range_random));
            if (shape.release_order == 1) {
                angle = (angle - fraction * angle) + offset;
            } else if (shape.release_order == 2) {
                // the bounds picked by `blt` on each order.
                const float end = angle + offset;
                angle = range_random(end < offset ? end : offset, offset < end ? end : offset);
            } else if (shape.release_order == 0) {
                angle = fraction * angle + offset;
            }
            // (cos, 0, sin) x radius through the node's orientation, then its position;
            // `Math::AngleUnitsToRadians`, `Quaternion::operator*`. The console's own cosine and
            // sine, each rounded to single before the radius.
            const float theta = kOgreDegToRad * (angle + 90.0f);
            const float lx = static_cast<float>(crt_cos(theta)) * radius;
            const float lz = static_cast<float>(crt_sin(theta)) * radius;
            const Vec3 turned = quaternion_rotate(
                node_orientation(frame.right, frame.up, frame.forward), {lx, 0.0f, lz});
            out.position = {turned.x + frame.center.x, turned.y + frame.center.y,
                            turned.z + frame.center.z};
            break;
        }
        case 1: {
            // `CShape::calculatePositionOnLineBetweenAngles`: along the
            // segment, from its start, backwards, or anywhere on it.
            Vec3 start, end;
            shape_line_segment(shape, frame, percent, start, end);
            float t = 1.0f;
            if (shape.release_order == 1) {
                t = 1.0f - fraction;
            } else if (shape.release_order == 2) {
                t = range_random(0.0f, 1.0f);
            } else if (shape.release_order == 0) {
                t = fraction;
            }
            out.position = {(end.x - start.x) * t + start.x, (end.y - start.y) * t + start.y,
                            (end.z - start.z) * t + start.z};
            break;
        }
        case 2: {
            // `CShape::calculatePositionOnSphere`: an offset along the
            // world's own axes, three draws; the random order scales them by a
            // random reach first.
            const float radius = shape_radius(shape, percent, range_random);
            Vec3 offset;
            if (shape.release_order == 0 || shape.release_order == 1) {
                offset.x = range_random(-1.0f, 1.0f) * radius;
                offset.y = range_random(-1.0f, 1.0f) * radius;
                offset.z = range_random(-1.0f, 1.0f) * radius;
            } else if (shape.release_order == 2) {
                const float reach = range_random(0.0f, radius);
                offset.x = reach * range_random(-1.0f, 1.0f);
                offset.y = reach * range_random(-1.0f, 1.0f);
                offset.z = reach * range_random(-1.0f, 1.0f);
            }
            out.position = {offset.x + frame.center.x, offset.y + frame.center.y,
                            offset.z + frame.center.z};
            break;
        }
        case 4: {
            // `CShape::calculatePositionOnBox`: round the
            // rim by the fraction (mirrored in x counter-clockwise), or anywhere inside on the
            // volatile stream, x then y then z; then the object's transform.
            const Vec3& size = shape.box_size;
            Vec3 local;
            if (shape.release_order == 0 || shape.release_order == 1) {
                const float turn = fraction * 6.2831855f;
                const float c = static_cast<float>(crt_cos(turn));
                const float s = static_cast<float>(crt_sin(turn));
                local.x = c * size.x * (shape.release_order == 1 ? -0.5f : 0.5f);
                local.z = s * size.z * 0.5f;
            } else if (shape.release_order == 2) {
                local.x = volatile_rng.between(-size.x, size.x) * 0.5f;
                local.y = volatile_rng.between(-size.y, size.y) * 0.5f;
                local.z = volatile_rng.between(-size.z, size.z) * 0.5f;
            }
            out.position = matrix_transform(
                object_transform(node_orientation(frame.right, frame.up, frame.forward),
                                 frame.center),
                local);
            break;
        }
        default:
            // `Point`: the node's own position.
            out.position = frame.center;
            break;
    }

    // `CShape::calculateOrientation`, the
    // axes in the order `updatePositionAndOrientation` hands them to
    // `FromAxes`.
    switch (shape.direction) {
        case 0: {
            // The object's absolute right, up and forward: its node's derived
            // orientation's axes.
            const OgreQuaternion q = node_orientation(frame.right, frame.up, frame.forward);
            out.axes = {quaternion_x_axis(q), quaternion_y_axis(q), quaternion_z_axis(q)};
            break;
        }
        case 1: out.axes = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}; break;
        case 2: out.axes = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}}; break;
        case 3:
        case 4: {
            // Out from the node, or in to it, on the ground, through the console's
            // `Vector3::normalise`.
            const Vec3 z = ogre_normalised(
                shape.direction == 3
                    ? Vec3{out.position.x - frame.center.x, 0.0f, out.position.z - frame.center.z}
                    : Vec3{frame.center.x - out.position.x, 0.0f, frame.center.z - out.position.z});
            out.axes.z = z;
            out.axes.y = {0.0f, 1.0f, 0.0f};
            out.axes.x = {z.z - z.y * 0.0f, z.x * 0.0f - z.z * 0.0f, z.y * 0.0f - z.x};
            break;
        }
        default: break;
    }
    return out;
}

// A missile file's `Missile` node is its template, which
// `CMissileDescriptor::CreateObject` makes
// as `new CMissile(rm)`; the node's properties are then set on it.
MissileSpec read_missile_spec(const tl::formats::CmpNode& node) {
    MissileSpec s;
    std::string name = text_of(node, mf::kMissileName);
    for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    s.name = name;
    // `CMissile::setParticleFile`: a non-empty path is upper-cased
    // (`STRINGS::StringUpper`) before it is kept.
    const auto particle_file = [&node](size_t field) {
        std::string path = text_of(node, field);
        for (char& c : path) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return path;
    };
    s.release = particle_file(mf::kRelease);
    s.active = particle_file(mf::kActive);
    s.hit = particle_file(mf::kHit);
    s.die = particle_file(mf::kDie);
    if (const tl::formats::CmpField* f = node.field(mf::kNumRicochets); f && f->count() == 1) {
        s.ricochets = f->as_uint();
    }
    s.max_distance = float_of(node, mf::kMaxDistance, s.max_distance);
    s.radius = float_of(node, mf::kRadius, s.radius);
    s.aoe_radius = float_of(node, mf::kAoeRadius, s.aoe_radius);
    s.max_velocity = float_of(node, mf::kMaxVelocity, s.max_velocity);
    // `Set_setFriction`: `+0x15c = 1.0 - value`.
    s.keep = 1.0f - float_of(node, mf::kFriction, 0.0f);
    s.force = float_of(node, mf::kForce, s.force);
    s.full_velocity = bool_of(node, mf::kFullVelocity, s.full_velocity);
    s.homing = float_of(node, mf::kHomingSpeed, s.homing);
    s.targeting_angle = float_of(node, mf::kTargetingAngle, s.targeting_angle);
    s.track_ground = bool_of(node, mf::kTrackGround, s.track_ground);
    s.target_position = bool_of(node, mf::kTargetPosition, s.target_position);
    s.collides_with_objects = bool_of(node, mf::kCollidesWithObjects, s.collides_with_objects);
    s.arch_height = float_of(node, mf::kArchHeight, s.arch_height);
    s.home_after = float_of(node, mf::kHomeAfterSeconds, s.home_after);
    s.piercing = bool_of(node, mf::kPiercing, s.piercing);
    s.max_turn_rate = float_of(node, mf::kMaxTurnRate, s.max_turn_rate);
    s.turn_acceleration = float_of(node, mf::kTurnAcceleration, s.turn_acceleration);
    s.turn_randomization = float_of(node, mf::kTurnRandomization, s.turn_randomization);
    s.randomization_rate = float_of(node, mf::kRandomizationRate, s.randomization_rate);
    s.aoe_damage_scale = float_of(node, mf::kAoeDamageScale, s.aoe_damage_scale);
    s.soak_scale = float_of(node, mf::kDamageSoakScale, s.soak_scale);
    s.vertical_aiming = bool_of(node, mf::kVerticalAiming, s.vertical_aiming);
    s.min_damage_percent = float_of(node, mf::kMinPercentDamage, s.min_damage_percent);
    s.max_damage_percent = float_of(node, mf::kMaxPercentDamage, s.max_damage_percent);
    s.knockback = float_of(node, mf::kDamageForce, s.knockback);
    s.damage_type = text_of(node, mf::kDamageType);
    // The template is a `CPositionableObject`: its `POSITION` is its position.
    if (const tl::formats::CmpField* f = node.field(mf::kPosition); f && f->count() == 3) {
        s.height = f->as_float(1);
    }
    return s;
}

Missile missile_from_template(const MissileSpec& spec) {
    // `CMissile::CMissile(CResourceManager*)` and
    // `initialize` give the defaults the structs carry; the
    // copy takes the template's fields, its position's y as
    // the lift, and starts the turn rate at 0. `hits_owner` starts off; the spawner sets it.
    Missile m;
    m.spec = spec;
    m.lift = spec.height;
    m.lifted = 0.0f;
    m.turn_rate = 0.0f;
    return m;
}

/// `CMissile::calculateLaunchOrientation`.
void missile_launch_orientation(Missile& m, const Vec3& from, const Vec3& to,
                                const MissileWorld& world) {
    m.target_point = to;
    // The climb is held to a quarter of the length of `(to.x, from.y, to.z)` -
    // the original's own sum, kept as it is (fused, its
    // bounds picked by `blt`).
    const float k = std::sqrt(std::fma(from.y, from.y, std::fma(to.z, to.z, to.x * to.x))) * 0.25f;
    float dy = to.y - from.y;
    dy = dy < k ? dy : k;
    dy = -k < dy ? dy : -k;
    Vec3 heading{to.x - from.x, (from.y + dy) - from.y, to.z - from.z};
    // Normalised (the length's squares summed z first, then y), then faced
    // along (`setDirection(heading, UNIT_Y)`, vtable +0x80).
    const float length = std::sqrt(
        std::fma(heading.y, heading.y, std::fma(heading.z, heading.z, heading.x * heading.x)));
    if (static_cast<double>(length) > 1e-08) {
        const float inverse = 1.0f / length;
        heading = {inverse * heading.x, inverse * heading.y, heading.z * inverse};
    }
    missile_set_direction(m, heading, {0.0f, 1.0f, 0.0f});
    const Vec3 d = from - to;
    m.target_distance = std::sqrt(std::fma(d.x, d.x, std::fma(d.z, d.z, d.y * d.y)));

    // The arch: a path through the launch point, the
    // midpoint raised by `ARCH HEIGHT`, and the target dropped onto whatever
    // is below it (a ray from 100 above to 100 below; a miss leaves it be).
    if (0.0f < m.spec.arch_height) {
        Vec3 target = m.target_point;
        if (world.ray) {
            Vec3 hit;
            if (world.ray({target.x, target.y + 100.0f, target.z},
                          {target.x, target.y - 100.0f, target.z}, hit)) {
                target = hit;
            }
        }
        // `CPath::AddPoint`'s -60 and 60 (the point's angles) are not kept by
        // `Route`; nothing the missile reads uses them.
        m.arch.clear();
        m.arch_built = true;
        m.arch.add_point(from);
        m.arch.add_point({(from.x + target.x) * 0.5f + 0.0f,
                          (from.y + target.y) * 0.5f + m.spec.arch_height,
                          (from.z + target.z) * 0.5f + 0.0f});
        m.arch.add_point(target);
        m.arch_fraction = 0.0f;
    }
}

MissileStep fire_missile(Missile& m, const Vec3& from_in, const Vec3& forward,
                         const MissileWorld& world) {
    MissileStep step;
    m.struck.clear();
    Vec3 from = from_in;

    // Sits the missile at least a radius above its owner's feet.
    const bool has_owner = 0.0f < m.owner_radius;
    const float owner_feet = has_owner ? m.owner_center.y - m.owner_radius : 0.0f;
    const float clearance = (from.y - m.spec.radius) - owner_feet;
    if (clearance < 0.01f) {
        m.lifted = -clearance;
        from.y += m.lifted;
    }
    m.previous = from;
    m.home_clock = 0.0f;
    m.arch_travelled = 0.0f;
    m.travelled_total = 0.0f;
    m.travelled = 0.0f;
    m.active_left = 0.1f;  // 0x3dcccccd
    m.dying = false;
    m.ricocheted = 0;
    m.home_wait = m.spec.home_after;
    m.position = from;
    // INVENTED: `fireMissile` sets the orientation from a quaternion;
    // the port's caller hands only the forward, so the axes are built from it.
    missile_set_direction(m, forward, {0.0f, 1.0f, 0.0f});

    // No target handed in: the owner's own, else - for a homing missile - the
    // nearest in the cone ahead (`CMissile::getClosestTarget`).
    // The owner's target is the renderer's to hand
    // in as `m.target`.
    if (m.target == 0 && has_owner && 0.0f < m.spec.homing && world.closest_target) {
        m.target = world.closest_target(from, m.forward, m.spec.targeting_angle,
                                        m.spec.max_distance + 1.0f);
    }

    // Aimed at a target: the heading kept on the ground, the climb set to
    // reach the target's height at its distance - only when that point is
    // further than the owner is from the launch.
    if (m.target != 0 && has_owner) {
        Vec3 c;
        float r = 0.0f;
        bool alive = false;
        if (world.unit && world.unit(m.target, c, r, alive)) {
            const float owner_distance = ground_distance(from, m.owner_center);
            const float d = ground_distance(m.owner_center, c);
            const float target_feet = c.y - r;
            const Vec3 aim = {from.x + m.forward.x * d,
                              m.forward.y * d + target_feet + (from.y - owner_feet),
                              from.z + m.forward.z * d};
            if (owner_distance + 0.1f < ground_distance(aim, m.owner_center)) {
                missile_launch_orientation(m, from, aim, world);
            }
        }
    }
    if (m.spec.full_velocity) m.velocity = m.forward * m.spec.max_velocity;

    // Point blank: anything standing between the owner and the launch point
    // is met now.
    if (has_owner && ground_distance(m.owner_center, from) < 4.0f &&
        !(m.owner_center.x == from.x && m.owner_center.z == from.z)) {
        Vec3 end = from;
        int64_t unit = 0;
        const bool met = check_collision(m, {m.owner_center.x, from.y, m.owner_center.z}, end, unit,
                                         step, world);
        if (met && unit != 0) {
            m.position = end;
            step.hit = true;
        }
    }
    m.fired = true;
    return step;
}

// `CMissile::update(float)`.
MissileStep update_missile(Missile& m, float dt, const MissileWorld& world) {
    MissileStep step;
    if (!m.fired) return step;
    if (m.dying) m.active_left -= dt;
    step.active_visible = 0.0f <= m.active_left;

    const float moved = ground_distance(m.previous, m.position);
    m.travelled_total += moved;
    m.travelled += moved;
    if (m.dying) return run_death(m, dt, step);
    if (m.spec.max_distance <= m.travelled ||
        (m.spec.target_position && m.target_distance <= m.travelled_total)) {
        kill(m, step);
        do_aoe_damage(m, kNoUnitExcepted, world);
        if (m.dying) return run_death(m, dt, step);
    }
    m.previous = m.position;
    Vec3 next = velocity_step(m, dt, world);

    // A piercing missile sweeps again after each unit it passes.
    bool met_any = false;
    for (;;) {
        int64_t unit = 0;
        Vec3 end = next;
        const bool met = check_collision(m, m.previous, end, unit, step, world);
        next = end;
        if (met) met_any = true;
        if (!(m.spec.piercing && unit != 0)) break;
    }
    m.position = next;
    if (met_any) step.hit = true;
    return step;
}

}  // namespace tl::core
