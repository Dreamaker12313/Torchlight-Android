// What the renderer draws, as the game holds it: handles the game keeps beside its own state and
// hands back to the renderer (core/world.h), never looks into. Declared, not defined, so nothing
// here brings OGRE in.
#pragma once

#include <cstdint>
#include <map>

namespace Ogre {
class AnimationState;
class Entity;
class MovableObject;
class Node;
class SceneNode;
}  // namespace Ogre

namespace tl::core {

/// Every scene node a layout node produced, by its 64-bit id: what `Hide` and timelines act on.
// PORT: the renderer's handles, keyed by the layout's node ids.
using NodeMap = std::multimap<int64_t, Ogre::SceneNode*>;

/// How `place_particle_layout()` places a file. In play every kind-1 `CLayout` is a
/// `CParticle`: `CLayout::loadLayoutFile` hands it to
/// `CParticlePreloader` once `CLevel::loadRoomLayout` set
/// caching (`setCacheingParticlesForLevel`); also `createParticleSystemAt`
/// and `unitInit`.
enum class ParticleStart {
    /// A `CParticle`, started now (`CParticle::Start`).
    Now,
    /// `Now`, for a caller that leaves the systems as placed: taken down, they are kept for the
    /// file's next placement, as the preloader keeps its caches.
    Cached,
    /// A `CParticle` placed for a later `Start`: `GetParticle`, or a
    /// caller that starts its systems itself.
    Held,
    /// Not a `CParticle`: a live layout of its own, whose script runs (a kind-0
    /// `CLayout`, or a kind-2 one with a timeline).
    Live,
};

}  // namespace tl::core
