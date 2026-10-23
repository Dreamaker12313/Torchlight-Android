// The particle systems a level places, as the renderer keeps them, and the index script events
// find theirs by.
#pragma once

#include "render/particle_wrappers.h"

#include <OgreMovableObject.h>
#include <OgreQuaternion.h>
#include <OgreSceneNode.h>
#include <OgreVector.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tl::render {

/// A particle system placed in the world: one `Particle` node of a layout (a `CParticle`),
/// built by the glue (render/particle_wrappers.h), with the layout's timer
/// (`CParticle::updateLevelObject`).
struct ParticleSystem {
    /// The `Layout Link Particle` node this came from, or 0: the only way back from the
    /// linked file's systems to the link a wire names.
    int64_t owner = 0;
    /// The node map of the file holding that link, and the link's pose: a timeline key
    /// moving the link carries the system.
    const std::multimap<int64_t, Ogre::SceneNode*>* owner_nodes = nullptr;
    /// Part of a `Unit Trigger`'s own layout (`CTriggerUnit +0x240`, its unit's
    /// `LAYOUTFILE`, made by `unitInit`): `owner` is the trigger's node.
    bool trigger_own_layout = false;
    Ogre::Vector3 link_position = Ogre::Vector3::ZERO;
    Ogre::Quaternion link_orientation = Ogre::Quaternion::IDENTITY;
    Ogre::Vector3 offset_from_link = Ogre::Vector3::ZERO;
    Ogre::Quaternion turn_from_link = Ogre::Quaternion::IDENTITY;

    /// The `Particle` node it was built from, and the node map of the machine
    /// whose events name that node (its own file's, of the copies sharing ids).
    int64_t node = 0;
    const std::multimap<int64_t, Ogre::SceneNode*>* node_owner = nullptr;
    /// A live `CParticleTechWrapper` (its file part of the level via a `Layout Link`): its
    /// node's `Start Particle` / `Stop Particle` and affectors' `ENABLE` act on it; prepared,
    /// not started (`CParticleWrapper::createParticleSystem`).
    /// Every other system is a template copy (`CParticlePreloader::createParticleCache`).
    ///
    bool wrapper = false;

    /// What `CParticle::Start` reads for the stop timer:
    /// `LIFE TIME` (`getFixedTimeout`), the longest finite emitter run, and whether any
    /// emitter is `NO EXPIRATION`.
    float fixed_timeout = 0.0f;
    float finite_window = 0.0f;
    bool non_expiring = false;
    /// The layout's timer (`CParticle +0x120`, plus 0.1; none at first, `CParticle::CParticle`):
    /// past it `updateLevelObject` calls
    /// `Stop(true)`. Zero: no timer.
    float stop_at = 0.0f;
    /// Already given its layout's timer (an inner `Layout Link Particle` keeps its own).
    bool timer_armed = false;
    float elapsed = 0.0f;
    bool finished = false;
    /// Its holder already belongs to a holder list, which destroys the node; a placement's
    /// range can hold what a script fired meanwhile, which must not be claimed twice.
    bool claimed = false;
    /// The `CParticle` this is a template copy for (`Impl::placed_particles`), or 0: its
    /// `Start` also plays the file's sounds and camera shakes (`CParticleCache`).
    uint32_t cparticle = 0;

    /// The system itself.
    TorchlightParticle pu;
    /// `CParticle +0x128`: `Stop(false)` let it die out; stops for good once empty
    /// (`CParticle::updateLevelObject`).
    bool pu_dying = false;

    /// What OGRE draws for this system.
    Ogre::MovableObject* drawable() const { return pu.movable; }
    /// The node it hangs from.
    Ogre::SceneNode* holder() const {
        const Ogre::MovableObject* object = drawable();
        return object != nullptr ? object->getParentSceneNode() : nullptr;
    }
};

/// `particle_systems` by what script events address them by, so an event visits its own
/// systems rather than every one: by link (`owner_nodes`, `owner`), wrappers by node
/// (`node_owner`, `node`) and by node map; and by holder, the node each is made on and keeps.
/// Positions in `particle_systems`, ascending. Kept as systems are added or relinked; rebuilt
/// after any is destroyed.
struct ParticleIndex {
    using Key = std::pair<const void*, int64_t>;
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return std::hash<const void*>()(k.first) ^
                   (std::hash<int64_t>()(k.second) * 0x9e3779b97f4a7c15ull);
        }
    };
    std::unordered_map<Key, std::vector<size_t>, KeyHash> by_owner;
    std::unordered_map<Key, std::vector<size_t>, KeyHash> wrapper_by_node;
    std::unordered_map<const void*, std::vector<size_t>> wrappers_in;
    std::unordered_map<const void*, std::vector<size_t>> by_holder;
    bool stale = true;
};

/// Systems of `particle_systems` an index lookup found, in its order.
using ParticleRefs = std::vector<std::reference_wrapper<ParticleSystem>>;

}  // namespace tl::render
