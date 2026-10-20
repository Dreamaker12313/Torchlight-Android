// ParticleUniverse, the particle engine Torchlight's effects run on: the port's hookup of the
// OGRECave library (external/particleuniverse) to OGRE, brought up after Ogre::Root and down
// before it. `--pu-smoke` builds one system in code to show the library running on a target.
#pragma once

#include <cstddef>

#include <OgreVector.h>

namespace Ogre {
class Camera;
class SceneManager;
class SceneNode;
}  // namespace Ogre

namespace ParticleUniverse {
class ParticleSystem;
}  // namespace ParticleUniverse

namespace tl::render {

class ParticleUniverseHost {
public:
    ParticleUniverseHost() = default;
    ParticleUniverseHost(const ParticleUniverseHost&) = delete;
    ParticleUniverseHost& operator=(const ParticleUniverseHost&) = delete;
    ~ParticleUniverseHost();

    /// Create the library's root and its factories. Needs Ogre::Root, whose
    /// MovableObject factory list and script compiler it registers with.
    bool start();

    /// Destroy every system and the library's root. Before the scene manager
    /// and Ogre::Root go: its systems are MovableObjects of theirs.
    void stop();

    bool running() const { return running_; }

    /// Where the manager's distance falloff is measured from: the player, every
    /// frame (`CGameClient::updateIngame`).
    void set_falloff_position(const Ogre::Vector3& position);

    /// What every system's time is multiplied by: the game speed times the
    /// client's speed multiplier, every frame (`CGameClient::updateIngame`
    /// manager +0x4dc).
    void set_game_speed(float speed);

    /// Whether systems are stepped this frame (`CGameClient::updateIngame`
    /// manager +0x540): paused ones are only refreshed.
    void set_game_paused(bool paused);

    /// `--pu-smoke`: one system built in code - a point emitter, a billboard
    /// renderer and a colour fade - standing at `where`'s world position and
    /// culled against `camera`, as the game's systems are against the player's.
    bool spawn_smoke(Ogre::SceneManager* scene, const Ogre::SceneNode* where, Ogre::Camera* camera);

    /// Particles alive in the `--pu-smoke` system, or 0 without one.
    size_t smoke_particles() const;

    /// What every billboard technique of `scene` really draws with, against the material its
    /// technique names: logs the ones that would draw as OGRE's blank white, and the totals.
    void log_materials(Ogre::SceneManager* scene) const;

private:
    void destroy_smoke();

    bool running_ = false;
    Ogre::SceneManager* smoke_scene_ = nullptr;
    Ogre::SceneNode* smoke_node_ = nullptr;
    ParticleUniverse::ParticleSystem* smoke_ = nullptr;
};

/// The scene's ParticleUniverse systems: all of them, those started, and
/// those attached to a node (a census for the timing log).
void count_particle_systems(Ogre::SceneManager* scene, size_t& total, size_t& started,
                            size_t& attached);

}  // namespace tl::render
