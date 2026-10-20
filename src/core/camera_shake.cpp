#include "core/camera_shake.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string to_upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

}  // namespace

// CCameraShake::CCameraShake — construct with
// default 0.5s duration, (0, 1, 0) direction
CameraShake::CameraShake()
    : duration(0.5f),
      elapsed(0.0f),
      magnitude(1.0f),
      direction{0.0f, 1.0f, 0.0f},
      curve(nullptr),
      name(""),
      orientation(0),
      origin{0.0f, 0.0f, 0.0f},
      has_origin(false),
      falloff(true) {}

// CCameraShake::CCameraShake — construct with
// name, direction, duration, looking up curve in graphs
CameraShake::CameraShake(const std::string& name_in, Vec3f direction_in, float duration_in,
                         const StatBook& graphs)
    : duration(duration_in),
      elapsed(0.0f),
      magnitude(1.0f),
      direction(direction_in),
      curve(nullptr),
      orientation(0),
      origin{0.0f, 0.0f, 0.0f},
      has_origin(false),
      falloff(true) {
    set_name(name_in, graphs);
}

// CCameraShake::setCameraShakeName — store
// uppercased name and resolve curve
void CameraShake::set_name(const std::string& name_in, const StatBook& graphs) {
    name = to_upper(name_in);
    curve = graphs.find(name);
}

// CCameraShake::clone — copy settings and
// re-resolve curve; elapsed and origin are not copied
void CameraShake::clone_from(const CameraShake& other, const StatBook& graphs) {
    duration = other.duration;
    magnitude = other.magnitude;
    direction = other.direction;
    orientation = other.orientation;
    has_origin = other.has_origin;
    falloff = other.falloff;
    set_name(other.name, graphs);
}

CameraShake camera_shake_from_node(const tl::formats::CmpNode& node, const StatBook& graphs) {
    namespace f = tl::formats::cmp_field::camera_shake;
    CameraShake shake;
    // `Set_setDirection`: +0x64..+0x6c, as authored.
    if (const tl::formats::CmpField* d = node.field(f::kDirection);
        d != nullptr && d->count() == 3) {
        shake.direction = Vec3f{d->as_float(0), d->as_float(1), d->as_float(2)};
    }
    // `Set_setDuration`: +0x58.
    if (const tl::formats::CmpField* d = node.field(f::kDuration);
        d != nullptr && d->count() == 1) {
        shake.duration = d->as_float();
    }
    // `Set_setMagnitudeMult`: +0x60.
    if (const tl::formats::CmpField* m = node.field(f::kMagnitude);
        m != nullptr && m->count() == 1) {
        shake.magnitude = m->as_float();
    }
    // `Set_setDirectionOrientation`: `ABSOLUTE` 0, `RELATIVE` 1, any
    // other spelling 0.
    if (const tl::formats::CmpField* o = node.field(f::kOrientationType)) {
        shake.orientation = o->as_text() == "RELATIVE" ? 1 : 0;
    }
    // `Set_setCameraFallsOffWithDistance`: +0x99.
    if (const tl::formats::CmpField* fades = node.field(f::kFades);
        fades != nullptr && fades->count() == 1) {
        shake.falloff = fades->as_bool();
    }
    // `Set_setCameraShakeName` ->
    // `setCameraShakeName`.
    if (const tl::formats::CmpField* name = node.field(f::kName)) {
        shake.set_name(name->as_text(), graphs);
    }
    return shake;
}

// CCameraShake::update — age shake, apply
// curve offset, and running-clamp to length 3.5
bool CameraShake::update(float dt, const Vec3f& focus, Vec3f& offset, bool apply) {
    // If no curve is resolved, return false immediately; elapsed and offset remain untouched.
    if (curve == nullptr) {
        return false;
    }

    // minss with the duration as destination: the sum wins ties and NaNs.
    const float sum = elapsed + dt;
    elapsed = (duration < sum) ? duration : sum;
    // No guard: a zero duration makes t a NaN, as in the original (see the return below).
    const float t = elapsed / duration;
    const float v = curve->at(t);

    float scale = 1.0f;
    if (has_origin && falloff) {
        // Quirk kept: origin.y is overwritten with focus.y, and it persists.
        origin.y = focus.y;
        const float dy = focus.y - origin.y;
        const float dx = focus.x - origin.x;
        const float dz = focus.z - origin.z;
        // x squared, y and z fused on (`fmadds`);
        // a NaN goes on past the test (`blt` out); the ramp is times 0.1f.
        const float d = std::sqrt(std::fma(dz, dz, std::fma(dy, dy, dx * dx)));
        if (!(d < 2.0f)) {
            const float x = (d - 2.0f) * 0.1f;
            if (x < 1.0f) {
                // Original bug: maxss against 1.0 lifts 1 - x back to 1, so
                // there is no ramp - full strength inside 12 units, none beyond.
                const float term = 1.0f - x;
                scale = (term > 1.0f) ? term : 1.0f;
            } else {
                scale = 0.0f;
            }
        }
    }

    if (apply) {
        // (direction * (v * magnitude)) * scale, in that order.
        const float vm = v * magnitude;
        offset.x = direction.x * vm * scale + offset.x;
        offset.y = direction.y * vm * scale + offset.y;
        offset.z = direction.z * vm * scale + offset.z;
    }

    // Running-total clamp, whether or not `apply`: normalised by
    // the reciprocal, then times 3.5.: y squared, z and x
    // fused on; `Vector3::normalise` fuses x and z onto y squared.
    const float len =
        std::sqrt(std::fma(offset.x, offset.x, std::fma(offset.z, offset.z, offset.y * offset.y)));
    if (len > 3.5f) {
        const float norm = std::sqrt(
            std::fma(offset.z, offset.z, std::fma(offset.x, offset.x, offset.y * offset.y)));
        if (static_cast<double>(norm) > 1e-08) {
            const float inv = 1.0f / norm;
            offset.x *= inv;
            offset.y *= inv;
            offset.z *= inv;
        }
        offset.x *= 3.5f;
        offset.y *= 3.5f;
        offset.z *= 3.5f;
    }

    // ucomiss/jb: an unordered t counts as below 1, so a zero-duration
    // shake never finishes and is never removed.
    if (!(t >= 1.0f)) {
        return true;
    }

    elapsed = 0.0f;
    return false;
}

// CCameraShake::startCameraShake: set origin, enqueue on the owned or borrowed list
void CameraShake::start(const Vec3f& origin_pos, bool owned, CameraShakes& camera) {
    origin = origin_pos;
    has_origin = true;
    if (owned) {
        camera.add(*this);
    } else {
        camera.add(this);
    }
}

// CCameraControl::addCameraShake — enqueue shake onto owned list
void CameraShakes::add(CameraShake shake) { owned_shakes_.push_back(std::move(shake)); }

// CCameraControl::addCameraShake — enqueue shake onto borrowed list
void CameraShakes::add(CameraShake* shake) {
    if (shake != nullptr) {
        borrowed_shakes_.push_back(shake);
    }
}

// INVENTED: drop a borrowed shake its owner destroyed or stopped. The original's
// `TSafePointer<CCameraShake>` nulled the slot, pruned by the next updateGameCamera walk.
void CameraShakes::forget(const CameraShake* shake) {
    if (shake == nullptr) return;
    for (size_t i = 0; i < borrowed_shakes_.size(); ++i) {
        if (borrowed_shakes_[i] == shake) {
            borrowed_shakes_[i] = borrowed_shakes_.back();
            borrowed_shakes_.pop_back();
            --i;
        }
    }
}

// CCameraControl::updateGameCamera shake walk
//
Vec3f CameraShakes::update(float dt, const Vec3f& focus, bool no_shake_setting) {
    Vec3f offset{0.0f, 0.0f, 0.0f};
    const bool apply = !no_shake_setting;

    // List 1: owned shakes. Destroyed upon completion.
    size_t i = 0;
    while (i < owned_shakes_.size()) {
        const bool alive = owned_shakes_[i].update(dt, focus, offset, apply);
        if (!alive) {
            // Swap-with-last; the index stays, so the moved element is seen this pass.
            owned_shakes_[i] = std::move(owned_shakes_.back());
            owned_shakes_.pop_back();
        } else {
            ++i;
        }
    }

    // List 2: borrowed shakes. Kept by their owners when expired; only taken off this list.
    size_t j = 0;
    while (j < borrowed_shakes_.size()) {
        CameraShake* shake = borrowed_shakes_[j];
        if (shake == nullptr) {
            // Null slot pruned via swap-with-last without advancing index.
            borrowed_shakes_[j] = borrowed_shakes_.back();
            borrowed_shakes_.pop_back();
            continue;
        }
        const bool alive = shake->update(dt, focus, offset, apply);
        if (!alive) {
            // Swap-with-last; the index stays, so the moved element is seen this pass.
            borrowed_shakes_[j] = borrowed_shakes_.back();
            borrowed_shakes_.pop_back();
        } else {
            ++j;
        }
    }

    return offset;
}

// CCameraControl::clearCameraShakes — clear
// owned and borrowed shake lists
void CameraShakes::clear() {
    owned_shakes_.clear();
    borrowed_shakes_.clear();
}

}  // namespace tl::core
