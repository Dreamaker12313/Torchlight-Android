// The particles a character owns: `CCharacter +0x4a8`, twelve `CParticle`s (the
// constructor adds twelve nulls) that
// `CCharacter::loadModel` fills from five
// engine names and the unit's own `PARTICLE_*` keys. A blow, a death, a spawn
// each move one to where it happened and start it again.
#pragma once

#include <array>
#include <string>

namespace tl::core {

/// The slots of `CCharacter +0x4a8`.
namespace character_particle {
inline constexpr int kHit = 0;             ///< the engine's `HIT`: a blow landing
inline constexpr int kGetHit = 1;          ///< `PARTICLE_GETHIT`
inline constexpr int kGetHitCensor = 2;    ///< `PARTICLE_GETHIT_CENSOR`
inline constexpr int kDeath = 3;           ///< `PARTICLE_DEATH`
inline constexpr int kDeathCensor = 4;     ///< never filled (see `character_particle_names`)
inline constexpr int kSpawn = 5;           ///< `PARTICLE_SPAWN`
inline constexpr int kCritical = 6;        ///< the engine's `CRITICAL`
inline constexpr int kBlock = 7;           ///< the engine's `BLOCK`
inline constexpr int kTransform = 8;       ///< the engine's `TRANSFORM`
inline constexpr int kIceExplode = 9;      ///< the engine's `ICEEXPLODE`
inline constexpr int kExplode = 10;        ///< `PARTICLE_EXPLODE`
inline constexpr int kExplodeCensor = 11;  ///< `PARTICLE_EXPLODE_CENSOR`
inline constexpr int kCount = 12;
}  // namespace character_particle

/// A unit file's own particle names, each `""` when it authors none.
struct UnitParticles {
    std::string get_hit;         ///< `PARTICLE_GETHIT`
    std::string get_hit_censor;  ///< `PARTICLE_GETHIT_CENSOR`
    std::string death;           ///< `PARTICLE_DEATH`
    std::string death_censor;    ///< `PARTICLE_DEATH_CENSOR`
    std::string explode;         ///< `PARTICLE_EXPLODE`
    std::string explode_censor;  ///< `PARTICLE_EXPLODE_CENSOR`
    std::string spawn;           ///< `PARTICLE_SPAWN`
};

/// `CCharacter::loadModel`, the list's fill:
/// the name `CResourceManager::createParticle` is asked
/// for in each slot, `""` where the slot stays null.
///
/// Quirk kept: `PARTICLE_DEATH_CENSOR` is stored at slot 11 (tests
/// the count against 0xb, as `PARTICLE_EXPLODE_CENSOR` does), not
/// at the slot 4 that `die` reads with blood off - and an authored
/// `PARTICLE_EXPLODE_CENSOR` then replaces it there.
std::array<std::string, character_particle::kCount> character_particle_names(
    const UnitParticles& unit);

/// `CCharacter::playHitParticle`: the
/// particle of a character that takes a blow - slot 1 while
/// `KSETTINGS_SHOW_BLOOD` reads 1, slot 2 otherwise.
inline int hit_particle_slot(bool show_blood) {
    return show_blood ? character_particle::kGetHit : character_particle::kGetHitCensor;
}

/// `CCharacter::die`: the
/// particle started at the body - slot 3 while `KSETTINGS_SHOW_BLOOD` reads 1,
/// slot 4 otherwise.
inline int death_particle_slot(bool show_blood) {
    return show_blood ? character_particle::kDeath : character_particle::kDeathCensor;
}

/// `CCharacter::playStrikeParticle(from, at, radius, critical, blocked)`:
/// the attacker's own particle for the blow,
/// `BLOCK` for one that was blocked, else `CRITICAL` for a critical, else `HIT`.
/// Nothing plays when the attacker's slot 0 is null, whichever is asked for.
inline int strike_particle_slot(bool critical, bool blocked) {
    if (blocked) return character_particle::kBlock;
    if (critical) return character_particle::kCritical;
    return character_particle::kHit;
}

struct ParticlePoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// Where `playHitParticle` and
/// `playStrikeParticle` put their particle:
/// from the struck unit's position `at`, `radius` (its `+0x194`) towards `from`
/// along the ground, and 1.25 up. `from` on top of `at` (a length of 1e-08 or
/// less) leaves the direction as it was computed, the zero vector.
ParticlePoint blow_particle_position(const ParticlePoint& from, const ParticlePoint& at,
                                     float radius);

}  // namespace tl::core
