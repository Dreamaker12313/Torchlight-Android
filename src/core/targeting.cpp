#include "core/targeting.h"

#include <cmath>

namespace tl::core {

namespace {

constexpr float kRadToDeg = 57.295826f;  // the original's own literal

/// A candidate's distance and bearing in the character's frame: the inverse of the
/// character's yaw.
struct Local {
    float distance;
    float degrees;
};

Local to_local(const TargetCandidate& c, float player_x, float player_z, float facing) {
    const float dx = c.x - player_x;
    const float dz = c.z - player_z;
    const float sin_f = std::sin(facing);
    const float cos_f = std::cos(facing);
    // Forward is +z in the character's frame, right is +x.
    const float local_x = dx * cos_f - dz * sin_f;
    const float local_z = dx * sin_f + dz * cos_f;
    Local out;
    out.distance = std::sqrt(local_x * local_x + local_z * local_z);
    out.degrees = std::atan2(local_x, local_z) * kRadToDeg;
    return out;
}

/// Whether `candidate` should replace `best`.
///
/// "No best yet" is 9999 units, 1000 degrees, not hostile. `hysteresis` (first pass
/// only) compares the signed bearings, not magnitudes: a quirk kept, so a candidate to
/// the left always gets past it.
bool prefer(bool best_hostile, float best_distance, float best_degrees, bool hostile,
            float distance, float degrees, bool hysteresis) {
    if (!((distance < best_distance) || (!best_hostile && hostile))) return false;
    if (best_hostile && !hostile) return false;
    if (hysteresis && best_hostile && hostile && best_degrees + kTargetAngleHysteresis < degrees) {
        return false;
    }
    return true;
}

}  // namespace

// The reach of the cone.
float target_range(float weapon_range) {
    const float from_weapon = weapon_range + kTargetWeaponRangeBonus;
    return from_weapon > kTargetMinRange ? from_weapon : kTargetMinRange;
}

// CGameClient's target pick.
bool select_target(const TargetCandidate* candidates, std::size_t count, float player_x,
                   float player_z, float facing, float range, std::uint64_t* out_id) {
    bool have_best = false;
    bool best_hostile = false;
    float best_distance = 9999.0f;  // the original's own initial values
    float best_degrees = 1000.0f;
    std::uint64_t best_id = 0;

    // Pass one: the cone in front.
    for (std::size_t i = 0; i < count; ++i) {
        const TargetCandidate& c = candidates[i];
        const Local local = to_local(c, player_x, player_z, facing);

        bool accepted = false;
        if (local.distance <= kTargetTouchRadius) {
            accepted = true;
        } else if (std::fabs(local.degrees) <= kTargetConeDegrees) {
            accepted = local.distance <= range;
        } else if (local.distance < 2.0f) {
            // Retests the 30 degrees just failed, so never accepts: kept as the original's.
            accepted = std::fabs(local.degrees) < kTargetConeDegrees && local.distance <= range;
        }
        if (!accepted) continue;

        // Line of sight, except when standing on it.
        if (!c.in_sight && local.distance > kTargetTouchRadius) continue;

        if (prefer(best_hostile, best_distance, best_degrees, c.hostile, local.distance,
                   local.degrees, true)) {
            have_best = true;
            best_hostile = c.hostile;
            best_distance = local.distance;
            best_degrees = local.degrees;
            best_id = c.id;
        }
    }

    // Pass two: anything close, whatever the angle. The original runs this only
    // when the first pass found nothing, and checks no line of sight in it.
    if (!have_best) {
        best_distance = 9999.0f;
        for (std::size_t i = 0; i < count; ++i) {
            const TargetCandidate& c = candidates[i];
            const Local local = to_local(c, player_x, player_z, facing);
            if (local.distance > kTargetFallbackRadius) continue;
            if (prefer(best_hostile, best_distance, best_degrees, c.hostile, local.distance,
                       local.degrees, false)) {
                have_best = true;
                best_hostile = c.hostile;
                best_distance = local.distance;
                best_degrees = local.degrees;
                best_id = c.id;
            }
        }
    }

    if (have_best && out_id != nullptr) *out_id = best_id;
    return have_best;
}

}  // namespace tl::core
