#include "core/character_particles.h"

#include <cmath>

namespace tl::core {

// `CCharacter::loadModel`,
// in its order.
std::array<std::string, character_particle::kCount> character_particle_names(
    const UnitParticles& unit) {
    using namespace character_particle;
    std::array<std::string, kCount> names;
    names[kHit] = "HIT";
    names[kTransform] = "TRANSFORM";
    names[kCritical] = "CRITICAL";
    names[kBlock] = "BLOCK";
    names[kIceExplode] = "ICEEXPLODE";
    // Each of the unit's own is made only when the key holds something.
    if (!unit.get_hit.empty()) names[kGetHit] = unit.get_hit;
    if (!unit.get_hit_censor.empty()) names[kGetHitCensor] = unit.get_hit_censor;
    if (!unit.death.empty()) names[kDeath] = unit.death;
    if (!unit.death_censor.empty()) names[kExplodeCensor] = unit.death_censor;  // slot 11
    if (!unit.explode.empty()) names[kExplode] = unit.explode;
    if (!unit.explode_censor.empty()) names[kExplodeCensor] = unit.explode_censor;
    if (!unit.spawn.empty()) names[kSpawn] = unit.spawn;
    return names;
}

// `CCharacter::playHitParticle` and
// `playStrikeParticle`, the position both
// compute.
ParticlePoint blow_particle_position(const ParticlePoint& from, const ParticlePoint& at,
                                     float radius) {
    float dx = from.x - at.x;
    float dy = 0.0f;
    float dz = from.z - at.z;
    const float length = std::sqrt(dx * dx + 0.0f + dz * dz);
    if (1e-08f < length) {
        const float inverse = 1.0f / length;
        dx *= inverse;
        dy = inverse * 0.0f;
        dz *= inverse;
    }
    // `UNIT_Y * 1.25` added to the three components.
    return {dx * radius + at.x + 0.0f * 1.25f, dy * radius + at.y + 1.0f * 1.25f,
            radius * dz + at.z + 0.0f * 1.25f};
}

}  // namespace tl::core
