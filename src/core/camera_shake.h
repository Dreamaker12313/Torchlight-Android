// Camera shakes (CCameraShake, and around; the
// CCameraControl shake lists). Curves come from a StatBook or as StatGraph pointers.
#pragma once

#include "core/stats.h"
#include "formats/cmp.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::core {

class CameraShakes;

/// Plain float 3D vector. Deliberately minimal to avoid heavyweight dependencies.
struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline bool operator==(const Vec3f& a, const Vec3f& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const Vec3f& a, const Vec3f& b) { return !(a == b); }

inline Vec3f operator+(const Vec3f& a, const Vec3f& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

inline Vec3f operator-(const Vec3f& a, const Vec3f& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline Vec3f operator*(const Vec3f& a, float s) { return {a.x * s, a.y * s, a.z * s}; }

inline Vec3f operator*(float s, const Vec3f& a) { return {a.x * s, a.y * s, a.z * s}; }

/// One camera shake curve instance (`CCameraShake`, Linux binary).
///
class CameraShake {
public:
    // CCameraShake::CCameraShake — construct
    // with default 0.5s duration, (0, 1, 0) direction
    CameraShake();

    // CCameraShake::CCameraShake — construct
    // with name, direction, duration, looking up curve in graphs
    CameraShake(const std::string& name, Vec3f direction, float duration, const StatBook& graphs);

    // CCameraShake::setCameraShakeName —
    // store uppercased name and resolve curve
    void set_name(const std::string& name, const StatBook& graphs);

    // CCameraShake::clone — copy settings and
    // re-resolve curve; elapsed and origin are not copied
    void clone_from(const CameraShake& other, const StatBook& graphs);

    // CCameraShake::update — age shake, apply
    // curve offset, and running-clamp to length 3.5
    bool update(float dt, const Vec3f& focus, Vec3f& offset, bool apply);

    // CCameraShake::startCameraShake: set origin, enqueue on a list
    void start(const Vec3f& origin, bool owned, CameraShakes& camera);

    float duration = 0.5f;
    float elapsed = 0.0f;
    float magnitude = 1.0f;
    Vec3f direction = {0.0f, 1.0f, 0.0f};
    const StatGraph* curve = nullptr;
    std::string name;
    int32_t orientation = 0;
    Vec3f origin = {0.0f, 0.0f, 0.0f};
    bool has_origin = false;
    bool falloff = true;
};

/// A `Camera Shake` node's object: `CCameraShake::CCameraShake`
/// 's defaults under
/// `CCameraShakeDescriptor`'s setters.
CameraShake camera_shake_from_node(const tl::formats::CmpNode& node, const StatBook& graphs);

/// Camera shake list manager (`CCameraControl` shake lists and update walk).
class CameraShakes {
public:
    // CCameraControl::addCameraShake — enqueue shake onto owned list
    void add(CameraShake shake);

    // CCameraControl::addCameraShake — enqueue shake onto borrowed list
    void add(CameraShake* shake);

    // INVENTED: drop a borrowed shake its owner destroyed or stopped. The original's
    // `TSafePointer<CCameraShake>` nulled the slot, pruned by the next
    // updateGameCamera walk.
    void forget(const CameraShake* shake);

    // CCameraControl::updateGameCamera shake
    // walk
    Vec3f update(float dt, const Vec3f& focus, bool no_shake_setting);

    // CCameraControl::clearCameraShakes —
    // clear owned and borrowed shake lists
    void clear();

    size_t owned_count() const { return owned_shakes_.size(); }
    size_t borrowed_count() const { return borrowed_shakes_.size(); }
    size_t size() const { return owned_shakes_.size() + borrowed_shakes_.size(); }
    const std::vector<CameraShake>& owned() const { return owned_shakes_; }
    const std::vector<CameraShake*>& borrowed() const { return borrowed_shakes_; }

private:
    std::vector<CameraShake> owned_shakes_;
    std::vector<CameraShake*> borrowed_shakes_;
};

}  // namespace tl::core
