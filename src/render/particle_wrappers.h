// Runic's particle glue: how a `.LAYOUT`'s `Particle` node and its `Emitter` and affector children
// become a ParticleUniverse system (Torchlight's fork, external/particleuniverse). From
// `CParticleWrapper`, `CParticleTechWrapper`, `CParticleEmitterWrapper`, `CAffectorWrapper` and
// what `CParticle::Start`/`Stop` do to a placed system.
//
// The port builds each placement's system from the node; the original builds it once per file and
// copies it (`CParticlePreloader::LoadParticle`,
// `ParticleSystem::operator=`), not yet compared.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <OgreQuaternion.h>
#include <OgreVector.h>

namespace Ogre {
class Camera;
class MovableObject;
class SceneManager;
class SceneNode;
}  // namespace Ogre

namespace ParticleUniverse {
class ParticleAffector;
class ParticleEmitter;
class ParticleSystem;
}  // namespace ParticleUniverse

namespace tl::formats {
struct CmpNode;
struct CmpScene;
}  // namespace tl::formats

namespace tl::render {

/// What building a `Particle` node needs from the renderer.
struct ParticleBuildContext {
    Ogre::SceneManager* scene = nullptr;
    /// The player's camera: `CParticleWrapper::createParticleSystem`
    /// hands every system `PlayerCam`, and
    /// Torchlight's techniques cull against it.
    Ogre::Camera* camera = nullptr;
    /// The resource group materials and textures are made in.
    std::string resource_group;
    /// An authored texture reference to a loadable archive path, empty when the
    /// archive has none.
    std::function<std::string(const std::string&)> resolve_texture;
    /// An authored `MESH` loaded as the port loads a layout's meshes, returning OGRE's name for
    /// it, empty when it cannot be loaded. Stands for the entity renderer's hidden
    /// `CGenericModel::loadModel` (`CParticleTechWrapper::setRenderType`).
    ///
    std::function<std::string(const std::string&)> load_mesh;
    /// The render queue group this port draws a technique's queue (`CParticleTechWrapper +0x14c`,
    /// 90 and up) in: the port keeps its own base for particles.
    std::function<unsigned(unsigned)> map_render_queue;
};

/// One `Particle` node built as a ParticleUniverse system.
struct TorchlightParticle {
    ParticleUniverse::ParticleSystem* system = nullptr;
    /// The same system as the MovableObject OGRE draws.
    Ogre::MovableObject* movable = nullptr;
    /// The wrapper's own node (`CParticleWrapper +0x58`), at the node's
    /// placement; the system is attached to it.
    Ogre::SceneNode* node = nullptr;
    /// `IS LIGHT` (`CParticleTechWrapper::setAsLight`, +0x1cd): queued on the
    /// light map's groups.
    bool is_light = false;
    /// `VISIBLE` (`CParticleTechWrapper::setEnabled`, +0x82).
    bool enabled = true;
    /// What `CParticle::Start` reads off the
    /// system for the layout's stop timer: `LIFE TIME` (`getFixedTimeout`), the
    /// longest `(NUM LOOPS + 1) x EMIT DURATION + DELAY` of an emitter that
    /// does not loop forever, and whether any emitter is `NO EXPIRATION`.
    float fixed_timeout = 0.0f;
    float finite_window = 0.0f;
    bool non_expiring = false;

    /// The player's camera, which `CParticleWrapper::start`
    /// hands the system again.
    Ogre::Camera* camera = nullptr;
    /// Each affector node's affector (`CAffectorWrapper +0x110`), by the node's id.
    std::vector<std::pair<int64_t, ParticleUniverse::ParticleAffector*>> affectors;
    /// Each `Emitter` node's emitter (`CParticleEmitterWrapper +0x110`), by the
    /// node's id.
    std::vector<std::pair<int64_t, ParticleUniverse::ParticleEmitter*>> emitters;
    /// `CParticleWrapper +0x130`: started, and not stopped for good since.
    bool playing = false;
    /// `CParticleTechWrapper +0x208`: stopped with particles alive, and on the
    /// level's update list until the last of them is gone.
    bool dying = false;
};

/// Whether every part of this node - its render type, render style, emitter
/// shapes and affectors - is one the fork has transcribed. `why` names the
/// first that is not.
bool can_build_torchlight_particle(const tl::formats::CmpScene& layout,
                                   const tl::formats::CmpNode& node, std::string* why);

/// Build one `Particle` node, as the layout loader builds a
/// `CParticleTechWrapper` and its children, at `position`/`orientation`
/// (world). The system is prepared and not started.
bool build_torchlight_particle(const ParticleBuildContext& context,
                               const tl::formats::CmpScene& layout,
                               const tl::formats::CmpNode& node, const Ogre::Vector3& position,
                               const Ogre::Quaternion& orientation, TorchlightParticle& out);

/// `CParticle::Start` for one system: every
/// emitter may release again, the camera is refreshed, and the system starts
/// over.
void start_torchlight_particle(TorchlightParticle& particle);

/// `CParticle::Stop(false)` for one system:
/// emitters whose particles expire release nothing more and what is alive dies
/// out; a system with a `NO EXPIRATION` emitter is stopped outright.
void stop_torchlight_particle_emission(TorchlightParticle& particle);

/// `CParticle::Stop(true)`: the system stops.
void stop_torchlight_particle(TorchlightParticle& particle);

/// `CAffectorWrapper::setEnabled` on the affector built from the
/// node `affector_node`: the affector's own switch, the negation of `ENABLE`.
/// False when the system has no such affector; `changed` says whether the
/// switch moved.
bool set_torchlight_affector_enabled(TorchlightParticle& particle, int64_t affector_node,
                                     bool enabled, bool* changed = nullptr);

/// `CParticleEmitterWrapper::setEnabled` on the emitter built from
/// the node `emitter_node`: its emission stopped while disabled. False when
/// the system has no such emitter.
bool set_torchlight_emitter_enabled(TorchlightParticle& particle, int64_t emitter_node,
                                    bool enabled);

/// A `Particle` node that is part of the level (its file linked by a `Layout
/// Link Timeline`, not a particle link's `CParticle`) is a live
/// `CParticleTechWrapper`. `CParticleTechWrapper::start`
/// for an enabled one: stopped first if it is
/// playing, shown, every emitter released, then `CParticleWrapper::start`.
///
void start_torchlight_wrapper(TorchlightParticle& particle);

/// `CParticleTechWrapper::stop`: with
/// particles alive and no `NO EXPIRATION` emitter, the emitters stop and the
/// system waits for the last particle (`dying`); otherwise
/// `CParticleWrapper::stop`.
void stop_torchlight_wrapper(TorchlightParticle& particle);

/// `CParticleTechWrapper::updateLevelObject`, every level update
/// while `dying`: once no particle is updating, the system stops and is hidden.
void update_torchlight_wrapper(TorchlightParticle& particle);

/// `CParticleWrapper::setVisible`: the node and the system.
void set_torchlight_wrapper_visible(TorchlightParticle& particle, bool visible);
/// Whether any technique of the system is `NO CULLING` (`ParticleTechnique +0x5b1`): what
/// `CParticle::Start` keeps in the particle's `+0x127` (X360
/// `+0x123`) and `CLevel::findParticles` asks of a link's nodes.
bool torchlight_particle_no_cull(const TorchlightParticle& particle);
/// `CParticle::forceParticleUpdate(float)` (inline in
/// `CLevel::updateVisibleParticles` on the console) for one
/// system: `_update(0.1)` until `seconds`, counted down by 0.1, is not above 0.
void force_torchlight_particle_update(TorchlightParticle& particle, float seconds);

/// `CParticle::getNumberOfParticlesUpdating(true)`
/// for one system: whether its technique
/// still has particles of emitters that expire.
bool torchlight_particle_updating(const TorchlightParticle& particle);

/// `CParticleWrapper::isPlaying()` and
/// `CParticle::getNumberOfParticlesUpdating(false)`, their first test: the
/// system's technique 0 has emitted particles still alive.
bool torchlight_particle_has_particles(const TorchlightParticle& particle);

/// `CParticle::looping` for one system: an
/// emitter of one of its techniques that does not expire or loops for ever.
bool cparticle_system_loops(const TorchlightParticle& particle);

/// `CParticle::Pause` for one of its systems: paused when
/// shown. False when it was not.
bool pause_torchlight_particle(TorchlightParticle& particle);

/// Particles alive in the system's techniques.
size_t torchlight_particle_count(const TorchlightParticle& particle);

/// How many of the system's emitters are `Line` emitters: what
/// `CParticle::getAllEmitters` hands `CSkillEvent::startEvent`,
/// which keeps a `CLineEmitterWrapper` for
/// each whose type is `"Line"`.
size_t torchlight_particle_line_emitters(const TorchlightParticle& particle);

/// `ParticleUniverse::LineEmitter::setEnd` on every `Line` emitter
/// of the system, as `CSkillEvent::updateEvent`
/// does each frame.
void set_torchlight_particle_line_end(TorchlightParticle& particle, const Ogre::Vector3& end);

/// Destroy the system and its node.
void destroy_torchlight_particle(Ogre::SceneManager* scene, TorchlightParticle& particle);

}  // namespace tl::render
