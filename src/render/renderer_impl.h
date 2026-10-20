// The shared declarations of the renderer's implementation files (renderer*.cpp).
#pragma once

#include "render/renderer.h"

#include "core/attack_button.h"
#include "core/camera_shake.h"
#include "core/character.h"
#include "core/character_sheet.h"
#include "core/cinematics.h"
#include "core/combine.h"
#include "core/hotbar.h"
#include "core/howto_menu.h"
#include "core/inventory.h"
#include "core/inventory_screen.h"
#include "core/item_drop.h"
#include "core/item_use.h"
#include "core/merchant.h"
#include "core/model_animation.h"
#include "core/stats_page.h"
#include "render/automap.h"
#include "render/cinematic_menu.h"
#include "render/death_screen.h"
#include "render/display_gamma.h"
#include "render/game_hud.h"
#include "render/gl_thread.h"
#include "render/gpu_skinning.h"
#include "render/gpu_timer.h"
#include "render/hud.h"
#include "render/inventory_menu.h"
#include "render/layout_screen.h"
#include "render/menu.h"
#include "render/particle_universe.h"
#include "render/particle_wrappers.h"
#include "render/quest_dialog.h"
#include "render/render_system_choice.h"
#include "render/touch_overlay.h"
#include "render/unit_opacity.h"

#include "core/active_range.h"
#include "core/affix_book.h"
#include "core/animation_manifest.h"
#include "core/asset_path.h"
#include "core/audio.h"
#include "core/average_fps.h"
#include "core/breakable.h"
#include "core/character_effects.h"
#include "core/collision.h"
#include "core/context_tips.h"
#include "core/death.h"
#include "core/dungeon.h"
#include "core/enchanting.h"
#include "core/fishing.h"
#include "core/game_speed.h"
#include "core/game_ui.h"
#include "core/gamepad.h"
#include "core/input.h"
#include "core/item_effects.h"
#include "core/item_label.h"
#include "core/item_name.h"
#include "core/item_roll.h"
#include "core/item_save.h"
#include "core/item_sets.h"
#include "core/light_override.h"
#include "core/log.h"
#include "core/missile.h"
#include "core/music_object.h"
#include "core/navigation.h"
#include "core/no_spawn_region.h"
#include "core/passability.h"
#include "core/pet.h"
#include "core/pet_commands.h"
#include "core/platform.h"
#include "core/player.h"
#include "core/populate.h"
#include "core/quest.h"
#include "core/random_groups.h"
#include "core/retirement.h"
#include "core/script.h"
#include "core/settings_menu.h"
#include "core/skill_animation.h"
#include "core/skill_cast.h"
#include "core/skill_text.h"
#include "core/skills.h"
#include "core/skills_tab.h"
#include "core/sound_data.h"
#include "core/spawn_class.h"
#include "core/spell_remove.h"
#include "core/stats.h"
#include "core/targeting.h"
#include "core/text.h"
#include "core/text_event.h"
#include "core/town_portal.h"
#include "core/tray_moves.h"
#include "core/tray_press.h"
#include "core/unit_observers.h"
#include "core/unit_resource.h"
#include "core/unit_themes.h"
#include "core/unit_types.h"
#include "core/wardrobe.h"
#include "core/warp.h"
#include "core/waypoints.h"
#include "core/weapon_procs.h"
#include "core/weapon_trail.h"
#include "core/weapons.h"
#include "formats/adm.h"
#include "formats/cmp.h"
#include "formats/ogg.h"
#include "formats/wav.h"

#include <array>
#include <fstream>

#include <OgreAnimation.h>
#include <OgreAnimationState.h>
#include <OgreAnimationTrack.h>
#include <OgreBillboard.h>
#include <OgreBillboardSet.h>
#include <OgreBone.h>
#include <OgreCamera.h>
#include <OgreCodec.h>
#include <OgreControllerManager.h>
#include <OgreDataStream.h>
#include <OgreEntity.h>
#include <OgreFrameListener.h>
#include <OgreFrustum.h>
#include <OgreGpuProgramManager.h>
#include <OgreHardwareBufferManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreImage.h>
#include <OgreKeyFrame.h>
#include <OgreLogManager.h>
#include <OgreManualObject.h>
#include <OgreMaterialManager.h>
#include <OgreMatrix3.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreMeshSerializer.h>
#include <OgrePass.h>
#include <OgreRectangle2D.h>
#include <OgreRenderQueueListener.h>
#include <OgreRenderTargetListener.h>
#include <OgreRenderTexture.h>
#include <OgreRenderWindow.h>
#include <OgreResourceGroupManager.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSkeletonInstance.h>
#include <OgreSkeletonManager.h>
#include <OgreStaticGeometry.h>
#include <OgreSubEntity.h>
#include <OgreSubMesh.h>
#include <OgreTagPoint.h>
#include <OgreTechnique.h>
#include <OgreTextureManager.h>
#include <OgreTextureUnitState.h>
#include <OgreTimer.h>
#include <OgreViewport.h>

#include <OgreShaderGenerator.h>

// Static plugin registration: no plugins.cfg and no dlopen (the Switch has no dynamic
// loader). Only the render system differs: the console's OGRE has GLES2 alone, unless
// built with TL_SWITCH_GL3PLUS (render/render_system_choice.h).
#if defined(TL_PLATFORM_SWITCH) || defined(TL_PLATFORM_ANDROID)
#include <OgreGLES2Plugin.h>
#if defined(TL_SWITCH_GL3PLUS)
#include <OgreGL3PlusPlugin.h>
#endif
#else
#include <OgreGL3PlusPlugin.h>
#include <OgreGLPlugin.h>
#endif
#include <OgreSTBICodec.h>

#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#ifndef TL_OGRE_MEDIA_DIR
#define TL_OGRE_MEDIA_DIR ""
#endif

namespace tl::render {
namespace renderer_detail {}
using namespace renderer_detail;
namespace renderer_detail {
class RenderBehindMaterials;  // the render-behind outline's materials, below

/// The default when a level authors no `RIMLIGHT` (`CLevel::addCharacter`).
constexpr const char* kDefaultRimLight = "media/sharedtextures/rimlight.dds";

/// The game's own data, kept out of the default group so its scripts are not parsed
/// (mount_resources()).
constexpr const char* kGameGroup = "Torchlight";

/// Where place_mesh()'s increasing paint order for opaque layout meshes stops:
/// every later one shares this priority.
constexpr Ogre::ushort kLayoutDrawOrderCeiling = 90;

/// Path tests via stat() rather than std::filesystem: console paths carry a device prefix
/// ("sdmc:/..."), which std::filesystem::path is not specified for.
inline bool is_dir(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

inline bool is_file(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/// Join two path components with '/', which every platform here accepts.
inline std::string join_path(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    const char back = base.back();
    return (back == '/' || back == '\\') ? base + leaf : base + "/" + leaf;
}

/// Forwards OGRE's log through ours, so the console's file sink captures it too.
class OgreLogBridge : public Ogre::LogListener {
public:
    void messageLogged(const Ogre::String& message, Ogre::LogMessageLevel lml, bool /*maskDebug*/,
                       const Ogre::String& /*logName*/, bool& /*skipThisMessage*/) override {
        // OGRE indexes resources by basename and Torchlight reuses filenames across
        // directories: these are expected, and would bury the real errors.
        if (message.find("because it already exists in resource group") != Ogre::String::npos) {
            ++duplicate_names;
            return;
        }

        tl::core::LogLevel level = tl::core::LogLevel::Info;
        switch (lml) {
            case Ogre::LML_TRIVIAL: level = tl::core::LogLevel::Debug; break;
            case Ogre::LML_NORMAL: level = tl::core::LogLevel::Info; break;
            case Ogre::LML_WARNING: level = tl::core::LogLevel::Warning; break;
            case Ogre::LML_CRITICAL: level = tl::core::LogLevel::Error; break;
        }
        tl::core::log_write(level, "OGRE", 0, message);
    }

    size_t duplicate_names = 0;
};

/// Generates a shader technique whenever a material has none for the RTSS scheme: the
/// game's materials are all fixed-function, which neither GL3Plus nor GLES2 has. Mirrors
/// OgreBites::SGTechniqueResolverListener (the Bites component is not built).
class ShaderResolver : public Ogre::MaterialManager::Listener {
public:
    ShaderResolver(Ogre::RTShader::ShaderGenerator* generator, GpuSkinning* skinning)
        : generator_(generator), skinning_(skinning) {}

    Ogre::Technique* handleSchemeNotFound(unsigned short /*schemeIndex*/,
                                          const Ogre::String& scheme_name, Ogre::Material* original,
                                          unsigned short /*lodIndex*/,
                                          const Ogre::Renderable* /*rend*/) override {
        if (!generator_->hasRenderState(scheme_name)) return nullptr;

        if (!generator_->createShaderBasedTechnique(*original, Ogre::MSN_DEFAULT, scheme_name)) {
            // Never silently: a null here draws the fixed-function technique, which stops
            // the frame, so it gets the blank white's. `createShaderBasedTechnique` refuses
            // when a redeclared material left an entry built from another source technique
            // (parse_material_script()).
            TL_LOG_WARN("no shader technique for material '%s' (group '%s'); it draws blank white",
                        original->getName().c_str(), original->getGroup().c_str());
            return blank_technique(scheme_name, original);
        }
        if (skinning_ != nullptr) skinning_->regenerating(*original);
        generator_->validateMaterial(scheme_name, *original);

        for (auto* technique : original->getTechniques()) {
            if (technique->getSchemeName() == scheme_name) return technique;
        }
        // The generator answered for the name and put nothing on this object:
        // its entry is another `Material` of the same name.
        TL_LOG_WARN(
            "the shader technique of material '%s' (group '%s') is another object's; it "
            "draws blank white",
            original->getName().c_str(), original->getGroup().c_str());
        return blank_technique(scheme_name, original);
    }

    bool afterIlluminationPassesCreated(Ogre::Technique* technique) override {
        if (!generator_->hasRenderState(technique->getSchemeName())) return false;
        Ogre::Material* material = technique->getParent();
        generator_->validateMaterialIlluminationPasses(technique->getSchemeName(),
                                                       material->getName(), material->getGroup());
        return true;
    }

    bool beforeIlluminationPassesCleared(Ogre::Technique* technique) override {
        if (!generator_->hasRenderState(technique->getSchemeName())) return false;
        Ogre::Material* material = technique->getParent();
        generator_->invalidateMaterialIlluminationPasses(technique->getSchemeName(),
                                                         material->getName(), material->getGroup());
        return true;
    }

private:
    /// OGRE's unlit blank white, with shaders: what a material with no shader technique
    /// draws with (its fixed-function one would throw).
    Ogre::Technique* blank_technique(const Ogre::String& scheme_name,
                                     const Ogre::Material* original) {
        const Ogre::MaterialPtr blank =
            Ogre::MaterialManager::getSingleton().getDefaultMaterial(false);
        if (!blank || blank.get() == original) return nullptr;
        blank->load();
        for (int attempt = 0; attempt < 2; ++attempt) {
            for (auto* technique : blank->getTechniques()) {
                if (technique->getSchemeName() == scheme_name) return technique;
            }
            if (!generator_->createShaderBasedTechnique(*blank, Ogre::MSN_DEFAULT, scheme_name))
                break;
            generator_->validateMaterial(scheme_name, *blank);
        }
        return nullptr;
    }

    Ogre::RTShader::ShaderGenerator* generator_;
    GpuSkinning* skinning_;
};

}  // namespace renderer_detail

/// One thing a layout asks to be placed, with everything the layout says about it. Scale
/// and brightness are apart from the transform: they come from named per-descriptor fields
/// (tl::formats::cmp_field).
struct MeshPlacement {
    std::string mesh;

    /// A `Unit Trigger`'s prop: its unit's `LAYOUT_STATE_1`, hung on it once
    /// it stands (`Renderer::Impl::set_trigger_state_layout`).
    std::string trigger_resting_layout;

    /// The layout node this came from (its 64-bit id, how wires and timelines address it);
    /// zero for anything not placed from a node.
    int64_t node_id = 0;

    /// For a collision placement: its `CollisionWorld` group, so it can be switched off;
    /// zero (the level) for all but a `Unit Trigger`'s prop.
    int64_t collision_group = 0;
    /// The unit's `BLOCK`, for a unit's own collision model
    /// (`CollisionWorld::set_group_blocks`).
    bool collision_blocks = false;

    Ogre::Vector3 position = Ogre::Vector3::ZERO;
    Ogre::Quaternion orientation = Ogre::Quaternion::IDENTITY;
    Ogre::Vector3 scale = Ogre::Vector3::UNIT_SCALE;

    /// What the layout asked this thing to play: a `.SKELETON` stem beside the mesh (empty
    /// or `bind`: the bind pose). `unit` is a `Monster`'s `MONSTER` name, the join an
    /// `Animation Controller` uses.
    std::string animation;
    std::string unit;
    float animation_rate = 1.0f;
    bool loops = true;

    /// True when `animation` is the default idle rather than named: a mesh without
    /// `Idle.SKELETON` is then no fault.
    bool idle_fallback = false;

    /// Whether this mesh is somebody rather than something: units are skipped by
    /// `ground_under()`, so one placed on a marker is not ground for the next.
    bool is_unit = false;

    /// `VISIBLE:false` at placement: placed and registered, started invisible, so a later
    /// `Show` finds a scene node.
    bool start_hidden = false;

    /// A `Room Piece`: its materials take the light map's `LIGHTPASS` (`CLevel::bake`
    /// into `CLevel +0x290`; `CLevel::setProjectorPass`).
    ///
    bool room_piece = false;
    /// The piece's `PRERENDER` (-1) or `POSTRENDER` (+1) in its tileset table, 0 for neither:
    /// `CLevel::bake` batches them into `stgeompre_` on queue 0x30 and
    /// `stgeompost_` on 0x59, the rest into `stgeom_` on 0x31.
    int render_phase = 0;
    /// A room piece's `BAKE` (`CRoomPieceDescriptor::Set_setIsBakeable`, X360 +0x14f):
    /// whether `CLevel::bake` batches it; true by default (`CRoomPiece::CRoomPiece`).
    bool bake = true;

    /// A layout `Light`'s `LIGHT DENSITY` (`CLight +0x100`, 1 from the constructor),
    /// else 0: drawn into the light map only, that many times over.
    unsigned light_density = 0;
};

/// The animation a unit plays when the layout names nothing else (an engine name, not data).
constexpr const char* kUnitIdleAnimation = "Idle";

/// How `place_particle_layout()` places a file. In play every kind-1 `CLayout` is a
/// `CParticle`: `CLayout::loadLayoutFile` hands it to
/// `CParticlePreloader` once `CLevel::loadRoomLayout` set
/// caching (`setCacheingParticlesForLevel`); also `createParticleSystemAt`
/// and `unitInit`.
enum class ParticleStart {
    /// A `CParticle`, started now (`CParticle::Start`).
    Now,
    /// A `CParticle` placed for a later `Start`: `GetParticle`, or a
    /// caller that starts its systems itself.
    Held,
    /// Not a `CParticle`: a live layout of its own, whose script runs (a kind-0
    /// `CLayout`, or a kind-2 one with a timeline).
    Live,
};

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

/// `CMissile::killMissile`: `+0x1b4`, how long a dead
/// missile's particles keep releasing before `handleDeathOfMissile` stops them.
inline constexpr float kMissileParticleStopSeconds = 1.0f;

struct Renderer::Impl {
    RendererConfig config;

    Ogre::LogManager* log_manager = nullptr;
    std::unique_ptr<OgreLogBridge> log_bridge;

    std::unique_ptr<Ogre::Root> root;
#if defined(TL_PLATFORM_SWITCH) || defined(TL_PLATFORM_ANDROID)
    // One of the two per Root, never both: each makes its own EGL display.
    std::unique_ptr<Ogre::GLES2Plugin> gles2_plugin;
#if defined(TL_SWITCH_GL3PLUS)
    std::unique_ptr<Ogre::GL3PlusPlugin> gl3plus_plugin;
#endif
#else
    std::unique_ptr<Ogre::GL3PlusPlugin> gl3plus_plugin;
    std::unique_ptr<Ogre::GLPlugin> gl_plugin;
#endif

    /// ParticleUniverse, up while Root is. See `render/particle_universe.h`.
    ParticleUniverseHost particle_universe;

    Ogre::RenderWindow* window = nullptr;
    Ogre::SceneManager* scene = nullptr;
    /// See `TransparentsWithScenery`.
    std::unique_ptr<Ogre::RenderQueueListener> transparents_listener;
    Ogre::Camera* camera = nullptr;
    Ogre::SceneNode* camera_node = nullptr;
    Ogre::Light* key_light = nullptr;
    Ogre::SceneNode* key_light_node = nullptr;
    Ogre::SceneNode* sky_node = nullptr;
    /// The template's `LEVELPARTICLE` layout (`CGameClient +0x1080`), put on the player
    /// every frame (`advance`).
    Ogre::SceneNode* level_particle_node = nullptr;

    Ogre::SceneNode* subject_node = nullptr;
    Ogre::Entity* subject = nullptr;

    /// The archive directory the subject's mesh came from, with a trailing slash; the
    /// mesh's references are qualified against it.
    std::string subject_dir;

    /// Everything a layout placed, for the ground query below.
    std::vector<Ogre::Entity*> layout_meshes;

    /// Which of `layout_meshes` are units. See `MeshPlacement::is_unit`.
    std::set<Ogre::Entity*> unit_meshes;

    /// `stgeom_`, the room pieces' main batch (bake_static_scenery()); `stgeompre_` and
    /// `stgeompost_` are the scene manager's.
    Ogre::StaticGeometry* static_scenery = nullptr;
    /// The room pieces bake_static_scenery() may batch: bakeable, with the
    /// render phase their tileset gives them.
    struct BakeablePiece {
        Ogre::Entity* entity = nullptr;
        int render_phase = 0;
    };
    std::vector<BakeablePiece> bakeable_room_pieces;
    /// Numbers the sets' names, which a scene manager keeps unique.
    unsigned baked_sets = 0;
    /// The scene nodes a wire or a timeline can address; see place_layout().
    std::set<const Ogre::SceneNode*> scripted_scene_nodes;

    /// The room pieces placed, whose materials take the light map's
    /// `LIGHTPASS` (see MeshPlacement::room_piece and add_light_pass()).
    std::vector<Ogre::Entity*> room_piece_meshes;

    /// The layout `Light`s' copies (MeshPlacement::light_density): light map only (`CLevel::bake`'s
    /// `stgeomLight_`; entities here).
    std::vector<Ogre::Entity*> light_pool_meshes;

    /// The layout's particle systems; see ParticleSystem.
    std::vector<ParticleSystem> particle_systems;

    /// The archive path of the layout whose `Particle` nodes are being placed,
    /// for `RendererConfig::pu_particle_layouts` (`--pu-particles`).
    std::string placing_layout_path;

    /// Everything in the scene with an animation running: the level's NPCs and animated
    /// props. Apart from animated() (the subject), which runs at the player's pace.
    struct SceneAnimation {
        Ogre::Entity* entity = nullptr;
        float rate = 1.0f;
        /// A `Room Piece`'s model: `CRoomPiece::updateAnimation`
        /// runs it only while the camera sees
        /// its box (`room_piece_in_view`).
        bool room_piece = false;
    };
    /// `CRoomPiece::updateAnimation`'s test: position plus model box
    /// (`CGenericModel +0x1d4..+0x1e8`) inside the frustum (`Ogre::Frustum::isVisible`).
    ///
    bool room_piece_in_view(const Ogre::Entity* entity) const;
    std::vector<SceneAnimation> scene_animations;

    /// `CGenericModel`'s animation half (core/model_animation.h) per entity
    /// `play_scene_animation()` drives; a track's index is this entity's own.
    struct EntityAnimation {
        tl::core::ModelAnimator animator;
        std::vector<Ogre::AnimationState*> states;
        /// What was last written to each state: only changes are written, so a state
        /// another path switched off stays off.
        std::vector<tl::core::ModelTrack> written;
    };
    std::map<Ogre::Entity*, EntityAnimation> entity_animations;
    /// The track of `state` on `entity`'s animator, added on first use.
    unsigned entity_track(EntityAnimation& animation, Ogre::AnimationState* state);
    /// Copy the animator's tracks onto the states, where they changed.
    void write_entity_animation(EntityAnimation& animation);
    /// `CGenericModel::updateAnimation` for an entity with an animator:
    /// false when it has none, and the caller steps its states itself.
    bool step_entity_animation(Ogre::Entity* entity, float delta);
    /// Whether `state` is one of the entity's animator's tracks.
    bool animator_owns(Ogre::Entity* entity, const Ogre::AnimationState* state) const;

    /// Every quest under `media/Quests/` and the log the player carries; the log owns the
    /// gate state (`QuestLog::state()` is what a `Group` node's gate tests).
    tl::core::QuestBook quest_book;
    tl::core::QuestLog quest_log;

    /// The window an NPC offers a quest in (`questdialogmenu_xenon.layout`): a
    /// `CQuestDialogMenu`, apart from a `FLOATYTEXT` line's `CTextEvent`.
    QuestDialogScreen quest_dialog;

    /// See `render/death_screen.h`.
    DeathScreen death_screen;
    /// The fishing ring (`media/UI/fishing_xenon.layout`, `CFishingMenu`, `CGameUI +0x3e4`),
    /// shown in AI state 0x21. `CFishingMenu::CFishingMenu` (X360 inline in
    /// `CGameUI::create`) starts shown (`+0x30`, X360 `+0x18`);
    /// `createMenus` builds it.
    LayoutScreen fishing_screen;
    /// Load Character (`media/UI/characterload.uilayout`, `CContinueGameMenu`),
    /// built when the screen is first asked for and drawn from
    /// `tl::core::ContinueMenu` by `show_load_character`.
    LayoutScreen character_load_screen;
    void show_load_character(const tl::core::ContinueMenu& state, const std::string& shown_file);
    /// `highlight`'s authored corner; `CContinueGameMenu +0x188` keeps it less
    /// `Player1`'s, which `show_load_character` takes off.
    float load_highlight_x = 0.0f, load_highlight_y = 0.0f;
    /// The modal message (`media/UI/modalmenu_xenon.layout`, `CModalMenu`, `CGameUI +0x3cc`)
    /// as `CGameUI::openModalDialog` opens it: a title, a line and `Ok`.
    /// `CModalMenu::CModalMenu`; no callbacks here.
    LayoutScreen modal_screen;

    /// Which quest the open window is offering, and who is offering it.
    const tl::core::QuestDefinition* offered_quest = nullptr;
    std::string offered_by;
    /// The speaker's node, and whether the offered line is
    /// `MAKE_PET_ON_ACCEPT` (see accept_offered_quest()).
    int64_t offered_by_node = 0;
    bool offered_makes_pet = false;
    /// The line the window shows (`CQuestDialogMenu +0x120`'s), for
    /// `hand_dialog_items` when it closes.
    const tl::core::DialogLine* offered_line = nullptr;
    /// `CQuestDialog::giveOrRemoveItems`: `[TAKEITEM]`s,
    /// then `[GIVEITEM]`s; false when already handed over (`+0x77`).
    bool hand_dialog_items(const tl::core::QuestDefinition& quest,
                           const tl::core::DialogLine& line);
    /// The offered line's section (`CQuestDialog +0x70`): `OK` on a
    /// `[COMPLETE]` line is what completes its quest.
    tl::core::QuestSection offered_section = tl::core::QuestSection::Intro;
    /// The menu's unit (`CQuestDialogMenu +0xc0`) for a line the player's own
    /// talk opened: the machine that runs it and its node.
    int talk_menu_machine = kNoMachine;
    int64_t talk_menu_node = 0;
    /// A floaty line the player's talk showed: `CQuestDialogMenu::update`
    /// calls `onClick(10)` on its next pass
    /// (`fresh` until then).
    struct TalkFloatyClose {
        bool pending = false;
        bool fresh = false;
        int machine = kNoMachine;
        int64_t node = 0;
        bool complete = false;
        std::string quest;
    };
    TalkFloatyClose talk_floaty_close;
    /// `onClick` on the menu's unit for the
    /// player's talk: its flag back and the broadcasts on it; `OK` on a
    /// `[COMPLETE]` line completes the quest between them.
    void talk_menu_closed(int machine, int64_t node, int32_t menu_event, bool complete_line,
                          const std::string& quest);
    /// The player's talk window closing with `menu_event` (2 `OK`, 3 `Accept`, 4 `Decline`):
    /// put away, `onClick`'s broadcasts, and `OK` on `[COMPLETE]` completes the quest.
    void finish_talk_window(int32_t menu_event);
    /// The player's forward, gait and whether they are going anywhere, for a
    /// machine whose characters include the player's pets.
    void tell_player_motion(tl::core::LevelScript& machine);
    /// The placed or spawned unit called `unit` nearest the player, or 0.
    int64_t nearest_unit_node(const std::string& unit) const;

    /// Build the offer window for `unit` and show it; false when it has nothing to say or
    /// the console interface data is not mounted.
    bool open_quest_offer(const std::string& unit);
    /// `CCharacter::approachAI`'s choice for the unit reached, and what
    /// `CGameUI::updateIngameUI` opens for it.
    void interact_with(const std::string& unit);
    void open_vendor_screen(const std::string& unit, VendorScreen screen);
    void close_quest_offer();
    bool open_script_dialog(const tl::core::ScriptEvent& event, const std::string& text);
    /// False when the log is full and the quest was not taken.
    bool accept_offered_quest(const tl::core::QuestDefinition& quest);
    /// The quest window is showing a line a `Quest Controller` opened, and
    /// answering it answers the script.
    bool script_dialog_window = false;

    /// Finish a quest the player is carrying and pay what it owes.
    bool complete_quest(const std::string& name);
    /// `completeQuest` as the dialogue's `OK` on a `[COMPLETE]` line runs it:
    /// once more on a quest already complete (`QuestLog::complete_by_dialog`).
    bool complete_quest_by_dialog(const std::string& name);
    /// What follows any completion here: the payout, the gates, the icons.
    void quest_completed(const std::string& name);

    /// Carry out every queued `CQuestManager::completeQuest`:
    /// journal, `[REWARD]` (`rewardPlayer`), "Quest Complete", take-backs.
    void pay_quest_completions();
    /// `QuestLog::take_notices` handed to every machine of the level
    /// (`LevelScript::quest_notice`), their events applied.
    void deliver_quest_notices();
    /// `CQuestDialog::cleanUp` for a completed quest's
    /// lines: the pets they made leave the player.
    void release_quest_pets(const tl::core::QuestCompletion& done);

    /// `CQuestRewards::rewardPlayer` for one completion.
    void reward_player(const tl::core::QuestCompletion& done);

    /// A quest's reward in `calculateRewards`' order:
    /// `[UNITREWARD]` units, then `TREASURECLASS` (`tl::core::quest_treasure_roll`).
    std::vector<std::string> quest_reward_items(const tl::core::QuestDefinition& quest, int level,
                                                int32_t seed);

    /// Raise the two-line banner and play the narrator, on accept or on completion.
    void announce_quest(const tl::core::QuestDefinition& quest, bool complete);

    /// The first item `quest_reward_items` hands over (what the windows draw); empty: none.
    std::string quest_reward_item(const tl::core::QuestDefinition& quest, int level, int32_t seed);

    /// A quest's seed from an NPC (`CQuest +0x1c`, drawn by `initializeQuestWithNPC`
    /// on the volatile stream, `QuestLog::offered`).
    /// INVENTED: a quest no NPC initialised gets a seed from its GUID.
    int32_t offer_seed(const tl::core::QuestDefinition& quest) const {
        const tl::core::QuestLog::Offered* rolled = quest_log.offered(quest);
        if (rolled != nullptr && rolled->seed != -1) return rolled->seed;
        return static_cast<int32_t>(static_cast<uint32_t>(quest.guid) ^ 0x9e3779b9u);
    }
    /// The level a quest's rewards are worked at (`CQuest +0x1ec`, `QuestLog::offer_level`),
    /// else `quest_level_for_offer`.
    int quest_level_of(const tl::core::QuestDefinition& quest) const {
        return quest_log.offer_level(quest, quest_level_for_offer);
    }
    /// The quest given to the player unless its log is full for it.
    bool take_quest(const tl::core::QuestDefinition& quest, int level, uint32_t seed) {
        return !quest_log.log_full_for(quest) && quest_log.accept(quest, level, seed, stat_graphs);
    }
    /// What `reinitializeQuest`'s floor roll reads.
    tl::core::QuestDungeonSource quest_dungeon_source();
    tl::core::QuestPlayerSource quest_player_source();
    /// What `CQuestRewards::calculateRewards` makes reward units with.
    tl::core::QuestRewardWorld quest_reward_world();

    /// Fill an accepted quest's `reward_item` / `reward_icon`.
    void resolve_reward_item(tl::core::QuestEntry& entry);

    /// Say so when a quest that a *gate in this level* was waiting on changes.
    void report_gates_waiting_on(const std::string& quest);
    std::set<std::string> reported_gates;

    /// The `PORTRAIT` a unit authors (a name in `media/UI/itemicons/`); empty for none.
    std::string portrait_of(const std::string& unit);
    tl::core::LevelScript script;

    /// Where a character can walk in this level and how to get anywhere (core/navigation.h).
    tl::core::NavGrid nav;

    /// Every scene node a layout node produced, by its 64-bit id: what `Hide` and
    /// timelines act on.
    using NodeMap = std::multimap<int64_t, Ogre::SceneNode*>;
    NodeMap scripted_nodes;

    /// One machine per placed nested layout with a script of its own (a wire never crosses
    /// a file); each has its own node map, as copies share their layout's node ids.
    struct NestedScript {
        std::string reference;
        int64_t owner = 0;  ///< the `Layout Link` node in the parent
        std::string owner_name;
        /// The node map of the file holding that link (`Play` on the link is
        /// `CLayout::start` on its layout).
        const NodeMap* owner_nodes = nullptr;
        /// In the level's coordinates (`LevelScript::set_frame` before it is built); its
        /// characters join the level's list (`set_level_machines`).
        tl::core::LevelScript script;
        NodeMap nodes;

        bool running = false;

        /// Where its nodes sit as `CLogicNodeState` records them: the room layout's index in
        /// `CLevel+0x10` (+0x30) and the parent hierarchy hash (+0x40) (`placing_scene`).
        uint32_t scene = 0;
        int64_t hierarchy = 0;

        /// Whether its first action has been logged (once, not every frame).
        bool reported = false;
    };
    /// A deque: `node_sink` points into one while its layout is placed, and a push_back
    /// part-way must not move it.
    std::deque<NestedScript> nested_scripts;

    /// The level's list of characters (`CLevel +0x98`) as their machines: the level's own,
    /// then each room's in placing order (`LevelScript::set_level_machines`).
    std::vector<tl::core::LevelScript*> level_machines{&script};

    /// The level template's `ACTIVERANGEMOD` and unit active range, which every
    /// machine's characters use (see apply_environment()).
    float level_active_range_mod = 0.0f;
    float level_unit_active_range = 16.0f;
    /// `GLOBALS.DAT` `UNIT_NEAR_RANGE` (`CGameGlobals +0xac`, 24 by default).
    float level_unit_near_range = 24.0f;
    /// `GLOBALS.DAT` `UNIT_SHADOW_RANGE` (`CGameGlobals +0xa8`, 10 by default).
    float level_unit_shadow_range = 10.0f;
    /// What an item's range is chosen from (`CItem::calculateActiveRange`):
    /// `UNIT_LIGHT_FADE`, the active ranges, `TRIGGER_NEAR_RANGE` (40).
    bool level_light_fade = true;
    /// The template's UNIT_FADE_DISTANCE and UNIT_FADE_START: where a unit's
    /// light falls to 0 and where it starts to (update_unit_lights()).
    float level_unit_fade_distance = 14.5f;
    float level_unit_fade_start = 7.0f;
    float level_indoor_range = 16.0f;
    float level_outdoor_range = 24.0f;
    float level_trigger_near_range = 40.0f;
    /// `KSETTINGS_F_AVERAGE_FPS`, kept as `CGame::update`
    /// keeps it.
    tl::core::AverageFps average_fps;
    /// The camera test `CCharacter::update` makes on the
    /// culling bounds (`culling_box`); no bounds counts as in view.
    bool unit_in_view(const tl::core::LevelScript& machine, int64_t node);
    /// A unit's culling bounds in the world (`CBaseUnit +0x1c0`, a
    /// `CCullingBounds`), sized from its model or the constructor's default.
    Ogre::AxisAlignedBox culling_box(Ogre::SceneNode* node, bool from_model) const;
    /// What every machine of the level is told about its characters' ranges
    /// and the camera (`set_unit_*_range`, `set_view_test`).
    void configure_unit_ranges(tl::core::LevelScript& machine);

    /// Where place_mesh() registers what it places while a scripted nested layout is
    /// placed; null: the level's own map.
    NodeMap* node_sink = nullptr;

    /// The `Layout Link` node whose file is currently being placed, and its
    /// name. Saved and restored around the recursive call, like `particle_owner`.
    int64_t nested_link = 0;
    std::string nested_link_name;

    /// While a generated floor's room pieces are placed, their machines wait unactivated:
    /// `CLevel::populate` activates each room scene
    /// (`CEditorScene::setActivated`, vtable +0x218) only after population and
    /// `CQuestManager::populate`.
    bool defer_room_activation = false;
    std::vector<std::pair<size_t, std::string>> deferred_room_machines;
    /// `level_activated` on a placed room's machine, and its events applied.
    void activate_room_machine(size_t index, const std::string& path);

    /// The loaded level as `CLevel` keys it: `+0x280` (upper-cased dungeon `NAME`), `+0x1a4`
    /// (depth), `IS_TOWN` (+0x86), `DONTSTORE` (+0x5a). Empty for a plain layout.
    std::string level_dungeon;
    int32_t level_depth = 0;
    /// `CLevel +0x1a8`, the floor's monster level (`place_dungeon_floor`).
    /// INVENTED: 1 for a level no floor roll set.
    int32_t level_monster_level = 1;
    /// The level of the player a warp brings in, 0 for none: `CGameClient::loadLevel`
    /// reads `CPlayer +0x100`, but the sheet arrives later here.
    int32_t arriving_player_level = 0;
    bool level_is_town = false;
    bool level_dont_store = false;
    /// The template's `ALLOW_PORTALS` (+0x84); false for a plain layout.
    bool level_allows_portals = false;
    /// The template's `ALLOW_PET_RETURN` (+0x85); false for a plain layout.
    bool level_allows_pet_return = false;

    // The town portal (`core/town_portal.h`, renderer_portal.cpp).
    /// `CLevel +0x14c` and `+0x158`: the last `Property Node` typed `Town
    /// Portal` (7) and `Waypoint Start` (15) the layout walk met.
    Ogre::Vector3 town_portal_spot = Ogre::Vector3::ZERO;
    bool has_town_portal_spot = false;
    Ogre::Vector3 waypoint_spot{-1.0f, -1.0f, -1.0f};
    /// Entry type 6's start (`loadRoomLayout`, cases 14 and 15):
    /// the last `Waypoint` (14) or `Waypoint Start` (15) `Property Node`.
    Ogre::Vector3 waypoint_entry_spot = Ogre::Vector3::ZERO;
    bool has_waypoint_entry_spot = false;
    /// The portal props `CPlayer::createPortals` added to the level - the items
    /// `CLevel::deleteOpenPortals` takes away (ISA `TOWNPORTAL` or `WAYPOINTNODE`).
    struct OpenPortal {
        std::string unit;          ///< the `PROPS` unit's `NAME`
        std::string display_name;  ///< its floor label
        std::string unittype;
        Ogre::Vector3 at = Ogre::Vector3::ZERO;
        /// A `MAP PORTAL`'s `WARP` (0x71) effect: the dungeon it leads to,
        /// upper-cased (`CPlayer::openMapPortal`). Empty for the others.
        std::string warp;
        /// `CSceneNodeObject::setEnabled` (vtable +0x40): `CTriggerUnit::trigger` turns a
        /// used map portal off; off draws nothing and is not offered to the pad.
        bool enabled = true;
        std::vector<Ogre::SceneNode*> holders;  ///< its `LAYOUTFILE`'s particles
        /// Its layout's `Sound` nodes that start on activation and loop, held
        /// as voices under `voice` and re-mixed every frame.
        struct HeldSound {
            std::string voice;
            std::string definition;
            Ogre::Vector3 at = Ogre::Vector3::ZERO;
            float volume = 1.0f;
            float radius = 0.0f;
        };
        std::vector<HeldSound> sounds;
    };
    std::vector<OpenPortal> open_portals;
    /// `CSoundObjectDescriptor::descriptorSceneActivated`
    /// for a portal's layout: `START ON ACTIVATE` sounds play.
    void start_portal_sounds(OpenPortal& portal);
    /// The held portal sounds' gain and pan, from where the listener stands.
    void step_portal_sounds();
    /// The gain and pan `play_sound` gives a definition heard at `at`; false
    /// when it is out of earshot.
    bool positional_mix(float volume, float radius, const Ogre::Vector3& at, float& gain,
                        float& pan) const;
    int portal_targeted = -1;
    /// `warpLevels("Town", -99)` from a portal used (`CTriggerUnit::trigger`).
    bool portal_warp_pending = false;
    tl::core::PortalLevel portal_level() const;
    /// The level a warp leaves (`core/warp.h`): its dungeon, that dungeon's
    /// `PARENT_DUNGEON` and the depth.
    tl::core::WarpOrigin warp_origin() const;
    void delete_open_portals();
    void create_portals();
    /// `CCharacter::applyInstantEffect`'s `OPEN TOWN PORTAL` on the player.
    void open_town_portal();
    /// `CCharacter::applyInstantEffect`'s `OPEN PORTAL` on the player:
    /// `CPlayer::openMapPortal`.
    void open_map_portal(const std::string& dungeon);
    /// `CResourceManager::createUnit(L"PROPS", L"MAP PORTAL")`, `CLevel::addItem` and its
    /// `WARP` effect: from `openMapPortal` and from a stored level state.
    bool add_map_portal(const Ogre::Vector3& at, const std::string& warp, bool enabled);
    /// What `setEnabled(false)` takes away from a portal prop: its particles
    /// and held sounds.
    void disable_portal(OpenPortal& portal);
    /// The level's portal props as `core/town_portal.h`'s two walks read them,
    /// one entry per `open_portals` entry and in its order.
    std::vector<tl::core::TemporaryPortal> temporary_portals() const;
    /// A on the targeted portal: `CTriggerUnit::trigger`.
    ///
    void use_targeted_portal();

    // Waypoints (`core/waypoints.h`, renderer_portal.cpp).
    /// `CWaypointActivator::activate`: this
    /// level's depth a waypoint, and "Waypoint Activated" when it is new.
    void activate_waypoint();
    /// The Xbox `waypointmenu_xenon.layout`, and what `CWaypointMenu::setOpen`
    /// put in it.
    LayoutScreen waypoint_screen;
    /// Empty until `setOpen`: `CWaypointMenu::CWaypointMenu`
    /// clears its flags (X360 `+0x108`, `+0x109`) and row lists (`+0x29c`, `+0x2ac`).
    std::vector<tl::core::WaypointRow> waypoint_rows;
    int waypoint_selected = 0;
    bool waypoint_stick_held = false;
    /// `CGameUI::toggleWaypointMenu` ->
    /// `CWaypointMenu::setOpen`.
    void toggle_waypoint_menu();
    void open_waypoint_menu();
    void close_waypoint_menu();
    void refresh_waypoint_menu();
    /// The pad on the open menu: the stick moves the highlight, A is
    /// `onClick` on the row, B closes it.
    void drive_waypoint_menu(const tl::core::InputState& in, bool confirm_edge, bool skip_edge);
    // The Pause Menu and Options (`core/settings_menu.h`, renderer_options.cpp).
    /// `COptionsMenu` on the Xbox `options_Xenon.layout`: Start in the game.
    LayoutScreen pause_screen;
    tl::core::OptionsMenu pause_menu;
    bool pause_stick_held = false;
    /// `CSettingsMenuXenon` on `settingsmenu_xenon.layout`.
    LayoutScreen settings_screen;
    tl::core::SettingsMenu settings_menu;
    bool settings_stick_held = false;
    bool settings_dpad_held = false;
    bool settings_y_held = false;
    /// Y on Options: Help once the close has been carried out.
    bool help_after_settings = false;
    /// Help, `CHowToMenu` (`CGameUI +0x18b8`) on the Xbox `howto_xenon.layout`
    /// (`core/howto_menu.h`).
    LayoutScreen howto_screen;
    tl::core::HowToMenu howto_menu;
    bool howto_stick_held = false;
    /// The settings when the caller keeps none (`RendererConfig::settings`).
    tl::core::GameSettings own_settings;
    tl::core::GameSettings& game_settings();
    void create_options_screens();
    /// `CGameUI::toggleOptions`, opening.
    void open_pause_menu();
    void close_pause_menu();
    void refresh_pause_menu();
    /// The pad on the Pause Menu: the tab list, A is `onClick` on the row,
    /// Start (and B) close it.
    void drive_pause_menu(const tl::core::InputState& in, bool start_edge);
    /// `CGameUI::toggleSettings`, opening.
    void open_settings_menu();
    void refresh_settings_menu();
    void drive_settings_menu(const tl::core::InputState& in);
    /// `CSettingsMenuXenon::vfunc_3` once a frame: carries a close out, writes
    /// the file after Done and brings the Pause Menu back.
    void step_settings_menu();
    /// `CGameUI`'s Help toggle.
    void toggle_help();
    /// `CHowToMenu::setOpen`, and what its close leads to.
    void set_help_open(bool open);
    /// The pad on Help and its `vfunc_9`.
    void drive_howto_menu(const tl::core::InputState& in, float dt, bool start_edge,
                          bool back_edge);
    /// Any of the screens Start leads to is up - the Pause Menu, Options or the
    /// debug menu - and the level stands still under it.
    bool start_menu_up() const {
        return debug_menu_open || pause_menu.open || settings_menu.open || !settings_menu.finished;
    }

    // Context tips (`core/context_tips.h`, renderer_tips.cpp).
    /// `CTipMenu` (`CGameUI +0x550`) on the Xbox `tipmenu_xenon.layout`.
    LayoutScreen tip_screen;
    /// `CGameUI +0x1960`.
    tl::core::TipQueue tip_queue;
    /// `CGameGlobals +0xf0`: `[CONTEXTTIPS]`, the console's wording when its
    /// data is mounted.
    tl::core::ContextTipTexts tip_texts;
    bool tip_texts_loaded = false;
    /// `CTipMenu +0x30`; `CTipMenu::CTipMenu`.
    bool tip_open = false;
    /// `CGameClient::getIsPaused` on the console: a full-screen menu
    /// covers a side (`rightCovered`, `leftCovered`) or a modal menu
    /// is up. The PC's needs both sides covered.
    bool game_paused() const { return full_screen_menu_up() || modal_dialog_open(); }
    /// `CGameUI +0x1798`, the seven left-side menus `CGameUI::create` lists:
    /// `CInventoryMenuXenon`, `CSkillMenuXenon`, `CQuestMenuXenon`,
    /// `CMerchantMenuXenon`, `CEnchantMenuXenon`, `CStashMenuXenon`, `CStatsMenuXenon`; the
    /// port's master menu is all of them.
    bool full_screen_menu_up() const { return inventory_menu.visible(); }
    /// Which of them, for the log.
    const char* modal_dialog_name() const;
    bool level_was_paused = false;
    /// The player's target is an interactive character (`ISA 0x83`) through its
    /// conversation: paused, `CGameClient::updateIngame`
    /// keeps the particles beside it running. A quest controller's window too
    /// (`CQuestController::interactWithUnit` -> `CGameUI::forceInteractiveDialog`
    /// `CCharacter::setTarget`).
    bool talking_to_unit() const {
        return talk_target_machine != kNoMachine ||
               (quest_dialog.visible() &&
                (!script_dialog_window || conversation_speaker() != nullptr));
    }
    /// That, or the quest dialog having its NPC: what `updateIngame` still
    /// updates while paused - the unit, and the player beside it.
    bool conversation_open() const {
        return talk_target_machine != kNoMachine || quest_dialog.visible() || script_dialog_open();
    }
    /// The unit the open quest window is with, or null.
    Ogre::Entity* conversation_speaker() const;
    /// `updateIngame`'s paused branch: the speaker's and the player's poses.
    void step_conversation_poses(float delta, float player_rate);
    void create_tip_screen();
    void load_context_tips();
    /// `CGameUI::queueTip`.
    void queue_tip(tl::core::ContextTip tip);
    /// The queue's turn in `CGameUI::updateIngameUI`, once a frame.
    void step_tips();
    /// A button on the open tip, one of the `kMenuPad*`
    /// codes: B closes, Y turns tips off.
    void press_tip_menu(int button);
    /// `CGameUI::modalDialogOpenPartial`: a window of the
    /// modal list (`CGameUI +0x1948`) is open. On the console also `CGameUI::modalDialogOpen`:
    /// no console window is ever still closing.
    ///
    bool modal_dialog_open() const;
    /// `CPlayer::update`'s three tips, once a frame.
    void queue_player_update_tips();
    /// The console screens' tips: the bag, the merchant and the stash as they
    /// open, the stats and skills tabs while points are unspent.
    void queue_menu_tips();
    bool tip_bag_was_open = false;
    /// `CCharacter::equipmentPickedUp` for the pet: the
    /// tip when its bag is full.
    void queue_pet_bag_tip();
    uint32_t tip_pet_pickups = 0;

    /// A row's caption: the depth's template's `LEVELNAME`, with ` - Floor N`
    /// when it shows floor numbers (`setOpen`).
    std::string waypoint_label(const tl::core::WaypointRow& row,
                               const tl::core::DungeonFiles& files);
    /// `warpLevels(dungeon, 0, depth, true, L"", false)`.
    void warp_to_waypoint(const tl::core::WaypointRow& row);

    /// `CLevel+600` (a remembered state exists while the level is built) and
    /// `+0x1a1` (the level was put back from it).
    bool level_has_saved_state = false;
    bool level_restored = false;

    /// The level template's `CINEMATIC` (`CLevelTemplateData +0x698`), the
    /// stratum's laid over it; empty for a level loaded as a plain layout.
    std::string level_cinematic;
    /// `CCinematics` (`media/cinematics/*.dat`), read on first use.
    tl::core::Cinematics cinematics;
    bool cinematics_loaded = false;
    void load_cinematics();
    /// `CCinematicMenu +0x30`: the cinematic screen is up.
    bool cinematic_open = false;
    /// `CCinematicMenu +0xf8`: the music's share, a quarter under a cinematic; 1 from
    /// `CCinematicMenu::CCinematicMenu`.
    float cinematic_music = 1.0f;
    /// `CCinematicMenu::update`, the music
    /// half, every frame.
    void step_cinematic_music(float delta);

    /// The room layout being placed: its index (`CLevel+0x10`) and the hash of the
    /// `Layout Link` chain above. INVENTED, the hash:
    /// `CEditorBaseObject::calculateParentHierarchyHashCode`
    /// ORs only links with +0x30 set; +0x30 is unread, so all are ORed.
    uint32_t placing_scene = 0;
    int64_t placing_hierarchy = 0;

    /// A unit's themes (`CBaseUnit +0x108..+0x150`) and the particles they hang on it
    /// (`CCharacter +0x738`), with what this engine drew for each trigger's particle.
    struct UnitThemeState {
        bool initialised = false;
        tl::core::UnitThemeSet themes;
        tl::core::ThemeParticleTriggers triggers;
        struct Drawn {
            std::vector<Ogre::SceneNode*> holders;
            /// `CParticleAnimationTrigger +0x38`: the node under the unit's for a particle
            /// attached on a bone, or the world node of one that does not attach.
            Ogre::SceneNode* tag = nullptr;
            bool configured = false;
        };
        std::vector<Drawn> drawn;
        /// `CCharacter::die` has stopped its particles.
        bool stopped_by_death = false;
        /// `loadModel`'s triggers (`load_model_triggers`) have been made.
        bool model_loaded = false;
        /// `+0x81` as the trigger list last saw it; false from `CSceneNodeObject`'s
        /// constructor, so the first show starts what the unit wears.
        bool shown = false;
        /// Whether `makeChampion`'s theme has been looked at for this unit.
        bool champion_checked = false;
    };

    struct SceneUnit {
        Ogre::Entity* entity = nullptr;
        std::string mesh;
        float rate = 1.0f;

        /// The layout node this unit was placed from (a unit name covers several).
        int64_t node = 0;

        /// What the layout put this unit on, so a reset can put it back.
        std::string animation;
        bool loops = true;

        /// What it is playing now (the script's last start, e.g. a swing), and the state:
        /// the manifest times `HIT` by animation name.
        std::string playing;
        Ogre::AnimationState* playing_state = nullptr;

        /// How far into `playing` the last frame left it, to spot the `HIT` frame once.
        float last_time = 0.0f;

        /// The unit's `ARMED` flag, which picks between two sets of animation files.
        bool armed = true;

        /// The rolled hands (`sync_unit_equipment`) are on it, or it has none.
        bool equipment_synced = false;

        /// The sound cursor, apart from `last_time` (which only `step_monster_swings()`
        /// advances, for hunting units).
        float last_sound_time = 0.0f;
        /// The animation `last_sound_time` was read in: a new one starts before
        /// its first key, not as a wrap of the last one's loop.
        std::string sound_animation{};

        /// Its themes and their particles; filled on first use.
        UnitThemeState themes{};

        /// The animation and time the particle keys were last looked at
        /// (`step_unit_particle_keys`).
        std::string key_animation{};
        float key_time = -1.0f;
    };
    std::multimap<std::string, SceneUnit> scene_units;

    /// One-shot animations a script controller started, so the machine is told when one
    /// ends (`Animation Stopped`): only the renderer knows how long one runs.
    struct PendingAnimation {
        int64_t controller = 0;
        Ogre::AnimationState* state = nullptr;
    };
    std::vector<PendingAnimation> pending_animations;

    /// A `Unit Trigger` prop part-way through its opening transition:
    /// `CTriggerUnit::setTriggerState` blends it once, keeps the unit active for
    /// its length plus 0.3 s (`+0x1e0`), then queues the state's own animation.
    struct PendingTriggerLoop {
        Ogre::AnimationState* transition = nullptr;
        int64_t node = 0;
        std::string unit;
        std::string next;
        bool loops = false;
    };
    std::vector<PendingTriggerLoop> pending_trigger_loops;

    /// `CTriggerUnit +0x2b0`: each state's layout (`LAYOUT_STATE_n`, `unitInit`)
    /// on the unit's node; `setTriggerState`
    /// shows the current one. DIVERGENCE: only the current one exists.
    struct TriggerStateLayout {
        int state = 0;
        std::vector<Ogre::SceneNode*> holders;
    };
    std::map<Ogre::Entity*, TriggerStateLayout> trigger_state_layouts;
    /// Placed trigger props whose unit authors `LAYOUT_STATE_1`, waiting: a layout cannot
    /// be placed inside `place_layout`, so the script's step hangs them.
    std::vector<std::pair<Ogre::Entity*, std::string>> trigger_layout_props;
    std::vector<std::pair<Ogre::Entity*, std::string>> pending_trigger_layouts;

    /// The state each animation this engine started runs on, by entity and lower-cased
    /// `.SKELETON` stem, so `Stop` can switch off one animation (a skeleton's internal
    /// animation name need not match its file).
    std::map<std::pair<Ogre::Entity*, std::string>, Ogre::AnimationState*> unit_states;

    // PORT: the OGRE animation state a unit's animation was last given, kept.
    void remember_state(Ogre::Entity* entity, const std::string& animation,
                        Ogre::AnimationState* state) {
        if (entity == nullptr || state == nullptr) return;
        std::string key = animation;
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        unit_states[{entity, key}] = state;
    }

    // PORT: the OGRE animation state a unit's animation was last given.
    Ogre::AnimationState* remembered_state(Ogre::Entity* entity,
                                           const std::string& animation) const {
        std::string key = animation;
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const auto it = unit_states.find({entity, key});
        return it == unit_states.end() ? nullptr : it->second;
    }

    /// What every quest has its characters say, and how long for (core/dialog.h): filled
    /// by load_quest_book(), handed to every machine by wire_script().
    tl::core::DialogBook dialog_book;

    /// The units this level places, with what their own `.DAT` authors (core/units.h).
    tl::core::UnitBook unit_book;

    /// What each `Unit Spawner` has released and not taken back, by the spawner's node id
    /// (`Destroy Spawned Units` and `MAX ALLOWED` are per spawner).
    struct SpawnedUnit {
        Ogre::SceneNode* node = nullptr;
        Ogre::Entity* entity = nullptr;
        std::string unit;

        /// The unit's own id, from a range no layout uses (kSpawnedNodeBase).
        int64_t id = 0;
    };
    std::map<int64_t, std::vector<SpawnedUnit>> spawned;
    unsigned spawn_serial = 0;

    /// Skill `NAME`, lower-cased, to what it plays and how far it reaches, from every
    /// `.DAT.adm` under `media/skills/` (files are not named after their skills).
    std::map<std::string, tl::core::AttackSkill> skills;
    bool skill_files_loaded = false;
    void ensure_skill_index();

    /// The player's skills: every skill file by upper-cased `NAME` (a `SkillDef` points
    /// into it) and the screen's book. The Xbox copies under `xbox/media/skills/` win.
    struct PlayerSkillFile {
        tl::formats::AdmFile file;
        tl::core::SkillDef def;
    };
    std::map<std::string, std::unique_ptr<PlayerSkillFile>> player_skill_files;
    bool player_skill_files_loaded = false;
    void ensure_player_skill_files();
    tl::core::SkillBook player_skills;
    /// Where the class's `[SKILL]` blocks were read from, for the log.
    std::string player_skills_source;

    /// `media/EffectsList.dat` and every affix file (core/affix_book.h); console copies
    /// under `xbox/media/affixes/` win.
    tl::core::EffectTypeTable effect_types;
    tl::core::AffixBook affixes;
    /// `CSets` (`media/sets/`), loaded with the affixes it names.
    tl::core::ItemSetBook item_sets;
    /// `CUnitThemes` (`media/unitthemes/`), read by `load_unit_themes()`.
    tl::core::UnitThemeBook unit_themes;
    bool unit_themes_loaded = false;
    /// The player's themes and their particles.
    UnitThemeState player_themes;
    /// The cinematic state `update_npc_icons` last saw (`gCinematicActive`).
    bool npc_icons_cinematic = false;
    /// `CLevel::updateNPCIcons` is owed once the player is in control.
    bool npc_icons_dirty = true;
    bool effect_data_loaded = false;
    void ensure_effect_data();
    /// `GLOBALS.DAT`'s item-roll figures (`CGameGlobals`, `item_roll_context()`).
    tl::core::ItemRollGlobals item_roll_globals;
    bool item_roll_globals_loaded = false;
    /// What `CEquipment::unitInit`'s rolls read (`core/item_roll.h`).
    tl::core::ItemRollContext item_roll_context();
    /// What an item's own effect manager is built with: the effect and graph
    /// tables and no owner (an item is not a character).
    const tl::core::EffectManager::Context& item_effect_context();
    tl::core::EffectManager::Context item_effect_context_;
    /// `CResourceManager::createItem`:
    /// a copy through `unitInit`'s rolls (`init_item_copy`). `from_save`
    /// rolls combat stats and requirements but does not enchant.
    tl::core::ItemStats create_item(const tl::core::ItemStats& data, bool from_save,
                                    bool random_enchant_allowed, bool force_enchant = false);
    /// Hands the player's inventory (and the pet's bag) the tables its effect
    /// half reads, and recalculates (`Inventory::set_effect_context`).
    void refresh_inventory_effects();
    /// `CCharacter::getEffectValue`'s two halves for the player: its own
    /// manager and its inventory's totals (`core/character_effects.h`).
    tl::core::CharacterEffects player_total() const {
        return tl::core::CharacterEffects(&player_effects, &player_inventory);
    }
    /// Rebuilt whenever the player's stats change: what `skill_stats_text` and
    /// `skill_stat_bonuses` read. Holds `std::function`s that capture `this`.
    tl::core::EffectDisplayContext skill_effects;
    void refresh_skill_effects();
    /// The player's own `CEffectManager` (`CCharacter +0x1b8`), kept in step by
    /// `refresh_skill_effects`, ticked by `update_player_effects` (core/effect_manager.h).
    tl::core::EffectManager player_effects;
    /// `GLOBALS.DAT`'s regeneration rates; see load_recharge_rates().
    tl::core::RechargeRates recharge_rates;
    bool recharge_rates_loaded = false;
    void load_recharge_rates();
    /// `CCharacter::updateHP` and
    /// `updateMana` for the player.
    void update_player_meters(float dt);
    /// `CCharacter::modifyMana` on the player: god mode
    /// sets `maxMana()`; else added, capped at `maxMana()`, floored at 0.
    void modify_player_mana(float delta);
    /// `player_health.max` and the sheet's mana maximum from `maxHP()` and
    /// `maxMana()` (`CharacterProgression::max_hp_of`/`max_mp_of`).
    void refresh_player_maxima();
    /// An item's `RESPEC`: `CPlayer::resetSkills`.
    ///
    void respec_player();
    /// The player as the item use path reads it.
    tl::core::ItemUseTarget player_use_target();
    /// `CGameUI::useItem` on the player's bag slot;
    /// `from_tray` speaks a `Town Portal Scroll` refusal.
    bool use_player_item(size_t slot, bool from_tray = false);
    /// A on the bag's own tray (past the vendor pages): gem, fish, useable
    /// item, else `wear_player_item`.
    void bag_default_action(int slot);
    /// The console's default A on any other bag row: identify, take off,
    /// or put on in place of what is in the way. True when anything changed.
    bool wear_player_item(size_t slot);
    /// the first Identify Scroll of the player's
    /// bag, then of a pet's that is not in town, used on the item at `slot`.
    bool identify_player_item(size_t slot);
    /// The same on the copy at `slot` of `bag` (the pet's, on its tray).
    bool identify_item_in(tl::core::Inventory& bag, size_t slot);

    // ------------------------------------------- the pet's items and spells --
    // renderer_pet_items.cpp: A on the pet's tray, the learn and its
    // replace dialog (`CMessageSpellRemoveXenon`), the pet's effects and
    // regeneration (`CCharacter::updateEffects`, `updateHP`).

    /// `CCharacter::getEffectValue`'s two halves for the pet.
    tl::core::CharacterEffects pet_total() const {
        return tl::core::CharacterEffects(&pet.effects(), &pet.bag());
    }
    /// The pet's manager's tables, and its spell book's effect totals.
    void refresh_pet_effects();
    /// The pet as the item use path reads it.
    tl::core::ItemUseTarget pet_use_target();
    /// What asks of a tray's owner about `item`.
    /// `CGameUI +0x14c4`: item compare, toggled by the D-pad's left and right.
    /// Off, details come 0.65 s after the selection settles
    /// (`+0x1e4`); on, at once with the worn items beside.
    bool item_compare = false;
    bool item_compare_held = false;
    float item_tip_timer = 0.0f;
    /// The tray's `+0x1e8`: the item the timer runs for, as tray * 1000 + slot,
    /// -1 for none.
    int item_tip_key = -1;
    bool item_tip_shown = false;
    void toggle_item_compare();
    void step_item_tooltips(float dt);
    void show_item_tooltips();
    /// What every tray's row fill asks of the player.
    tl::core::TrayRowContext tray_row_context();
    tl::core::TrayCaptionContext tray_caption_context(const tl::core::ItemStats& item,
                                                      bool pet_tray);
    /// A on the pet's tray: with the pet as the owner.
    void pet_tray_press();
    /// `canUse` and `useOnTarget` of the pet's copy at `slot`
    /// on the pet, then (the copy away when spent).
    bool use_pet_item(size_t slot);
    /// `canUse(item, user, pet)` and `useOnTarget(item, user, pet)`
    /// (Linux `CEquipment::canUseOnTarget`, `useOnTarget`) on
    /// the first pet: a spell learned, an instant `TRANSFORM` (`LevelScript::transform_unit`).
    /// False when `canUse` refuses; `used` is the caller's to spend by.
    bool use_item_on_pet(const tl::core::ItemStats& item, int stack, bool by_player,
                         tl::core::ItemUseResult& used);
    /// the function's ISA 0x7c branch for the fish at `slot` of `holder`: with a pet
    /// (`+0x5ac`), used on it, then.
    bool feed_pet_from(tl::core::Inventory& holder, size_t slot);
    /// The pet's half of a `Transform` event: `convertCharacter`
    /// re-made it as `unit`.
    void pet_converted(const std::string& unit);
    /// the function's equipment branch on the pet's tray.
    bool wear_pet_item(size_t slot);
    /// for the spell scroll at `slot`: the learn, or with the slots full
    /// (two for a pet, four else) the replace dialog.
    void learn_spell_from_tray(bool pet_owner, size_t slot);
    /// The replace dialog's state (`CMessageSpellRemoveXenon`): whose book, the
    /// scroll's slot in that owner's bag, the choice (`+0x38`).
    bool spell_remove_pet = false;
    size_t spell_remove_slot = 0;
    int spell_remove_choice = 0;
    bool spell_remove_stick_x_held = false;
    bool open_spell_remove(bool pet_owner, size_t slot);
    /// `vfunc_5` for the owner as it is now.
    void refresh_spell_remove();
    /// `vfunc_2` on the pad, and the tray's poll once it is down:
    /// the chosen slot unlearned and the scroll tried again.
    void handle_spell_remove(const tl::core::InputState& in, bool x_edge, bool y_edge,
                             bool start_edge, bool back_edge, bool other_edge);
    /// The pet's spells relearned into an emptied book (`learnSpell`,
    /// as `applySaveState` does).
    void relearn_pet_spells(const std::array<std::string, 4>& names,
                            const std::array<int32_t, 4>& levels);

    /// `CGameUI::activateItemSlot` and the use it leads to.
    void activate_item_slot(int index);
    /// `CCharacter::castSkill` for the player (`--cast` and
    /// the hotbar). `report` logs a refusal; a held button does not.
    bool cast_player_skill(const tl::core::KnownSkill& skill, bool report);
    /// The Xbox in-game dispatch's slot loop over the six hotbar
    /// buttons, each given as pressed this frame and held since the last.
    void step_hotbar(const std::array<bool, 7>& pressed, const std::array<bool, 7>& held,
                     float stick_x = 0.0f, float stick_y = 0.0f);
    /// The dispatch's release test: with no face button pressed and a skill performed, it
    /// is let go when its button is up (`core::hotbar_lets_go`).
    void release_held_skill(const std::array<bool, 7>& pressed, const std::array<bool, 7>& held,
                            bool face_pressed);
    /// `CPlayer::attemptToStopPlayerSkill(bool)`.
    void attempt_to_stop_player_skill(bool stop_path);
    /// `CCharacter::performingSkill` for the player.
    bool player_performing_skill();
    /// where the pad aims a skill (stick direction or
    /// facing; distance by `RANGEMIN`..`RANGE`, or a trigger's pull).
    tl::core::Vec3 pad_skill_point(const tl::core::KnownSkill& skill, float stick_x, float stick_y,
                                   float left_trigger, float right_trigger) const;
    /// The point stores before it casts (X360 `CCharacter +0x1c0`,
    /// Linux `+0x228`); unset, `--cast` aims as the pad at rest.
    std::optional<tl::core::Vec3> pad_skill_target;
    /// The player's `+0x228` (X360 `+0x1c0`): the last aimed-at point, from
    /// `setTargetDirection`, `castSkill` and `turnTowardPosition`;
    /// read by `step_player_skill_turn`.
    tl::core::Vec3 player_turn_point{};
    /// The player's `+0x234` (X360 `+0x1cc`): its last aim direction, normalised
    /// (`setTargetDirection`, `turnToward`).
    tl::core::Vec3 player_turn_direction{0.0f, 0.0f, 1.0f};
    /// `CCharacter::setTargetDirection`: the point
    /// on the ground (height 0) and the level direction to it.
    void set_player_target_direction(const tl::core::Vec3& point);
    /// `CCharacter::turnTowardPosition` for the player,
    /// with `turnToward`.
    void turn_player_toward(const tl::core::Vec3& at, float dt);
    /// The console's turn in a skill or holding attack (X360 `CCharacter::update`
    /// `core::player_skill_turn`), before the motion.
    void step_player_skill_turn(float dt);
    /// `CCharacter::castSkill` for a position skill (types 1, 5,
    /// 6, 11): grounded, target let go, within `RANGE`, walked back for `REQUIRES_PATHABLE`.
    /// Writes `caster.target_position`.
    void aim_position_skill(const tl::core::KnownSkill& skill, tl::core::SkillCaster& caster);
    /// `CPlayer::unitInit`'s default item slots.
    void bind_default_item_slots();
    /// `CSkillManager::startPassiveSkills` for the player,
    /// when the book is built and when a skill's level changes.
    void start_player_passives();
    /// The skills tab's description column as text beside its PNG, for regression checks.
    void write_skill_text_sidecar(const std::string& png_path);
    /// `CCharacter::update`'s effect calls, in order:
    /// affix durations, effect ages, the sweep.
    void update_player_effects(float dt);
    /// `CCharacter::updateCameraOpacity` on the player (`tl::core::update_camera_opacity`):
    /// `delta` is the frame's, or 0 while the game is paused.
    void step_player_camera_opacity(float delta);
    /// The player's `+0x528` (X360 `+0x508`): 1 until the camera comes within 6.
    float player_opacity = 1.0f;
    tl::core::SkillCastContext player_cast_context();
    /// The player's running skills and cooldowns (`CSkillManager::update`).
    ///
    tl::core::SkillRunner player_skill_runner;
    /// The worn items' skill managers by equip location (`CItem +0x1c8`, X360 `+0x184`),
    /// keyed to the item's name and emptied when it changes.
    std::array<tl::core::SkillRunner, 12> worn_skill_runners;
    std::array<std::string, 12> worn_skill_runner_items;
    /// `CCharacter::updateSkill(float)` -> `CInventory::updateSkillManagers`,
    /// inline in `CCharacter::updateAnimation`:
    /// each location's item runs `CSkillManager::update`.
    ///
    void update_worn_skill_managers(float delta);
    /// The runner a skill of that name runs on (the player's, else a worn item's), where its
    /// later events go.
    tl::core::SkillRunner& skill_runner_running(const std::string& name);
    /// What the gate asks about the caster: mana off the sheet, the two
    /// equipped weapons' unit types, and the effects that can silence them.
    tl::core::SkillCaster player_caster() const;
    /// Outlives `player_cast_context`, which hands out a pointer to it.
    tl::core::SkillCaster cast_caster;
    /// Rebuild `player_skills` for `player_class` at the class's starting
    /// levels, points and mapping cleared.
    void build_player_skills();
    /// The buttons the skills tab hears this frame: pressed (`GamepadButtonDown`)
    /// and, for the four a skill goes on, released (`GamepadButtonUp`).
    struct SkillsTabPad {
        bool dpad_up = false;
        bool dpad_down = false;
        bool lt = false;
        bool rt = false;
        bool y = false;
        bool x = false;
        bool start = false;
        bool lt_up = false;
        bool rt_up = false;
        bool y_up = false;
    };
    /// The pad on the skills tab: `CSkillMenuXenon`'s handler and
    /// its map sheet's; `core/skills_tab.*` decides.
    void handle_skills_tab(const tl::core::InputState& in, const SkillsTabPad& pad);
    bool skill_stick_held = false;
    /// The master menu closed as its housing's `setOpen(false)`: `InventoryClose` (0x42)
    /// except the skills tab (`core::housing_plays_close_sound`); Stats points placed.
    void close_master_menu();
    /// LT / RT on the master menu: the housing's close, then
    /// the next one's open (`InventoryOpen`, 0x16).
    void step_master_menu_tab(int direction);

    /// Unit `NAME`, lower-cased, to its `.DAT.adm` (files are not named after units); built
    /// once, on the first level that needs it.
    std::map<std::string, std::string> monster_files;
    bool monster_files_loaded = false;

    /// The letterbox bars and the dialogue box a script shows (render/hud.h).
    Hud hud;

    /// The game screen's interface (orbs, skill slots, experience bar) from
    /// `hud_xenon.layout`; `hud` is the script's.
    GameHud game_hud;

    /// What the chosen class authors about itself, read once out of its `.DAT`
    /// chain by `load_player_stats()` and handed to `player_character`.
    tl::core::ClassProfile class_profile;

    /// The player character's progression and attributes (stats, level, exp, journal).
    tl::core::CharacterProgression player_character;
    /// What the last creation screen was left holding (`creation_names()`).
    std::string created_player_name;
    std::string created_pet_name;

    /// The player character's inventory (equipment, potions, scrolls, items).
    tl::core::Inventory player_inventory;
    /// The console's stash, `CPlayer +0x9f4`.
    tl::core::Inventory player_stash = tl::core::make_player_stash();
    /// A saved item made again (`createUnit` with `from_save`, then its
    /// `applySaveState`); nothing for a name the item index does not know.
    std::optional<tl::core::ItemStats> item_from_save(const tl::formats::ItemSaveState& item);
    /// One held item's `CItemSaveState` (`CEquipment::fillSaveState`,
    /// core/item_save.h); `+0x5c`: the slot is an equip location.
    tl::formats::ItemSaveState item_save_state(const tl::core::InventorySlot& held, int32_t slot);
    /// `CSharedStash::loadSharedStash` / `saveSharedStash`,
    /// the file in `directory`.
    void load_shared_stash(const std::string& directory);
    void save_shared_stash(const std::string& directory);

    /// The Xbox 360 Menu (`media/UI/inventorymenu_xenon.layout` etc.).
    InventoryMenu inventory_menu;

    /// Input edge detection flags for menu navigation.
    bool inventory_button_held = false;
    bool inventory_stick_x_held = false;
    bool death_stick_held = false;
    bool inventory_stick_y_held = false;
    bool menu_prev_held = false;
    bool menu_next_held = false;
    bool tab_prev_held = false;
    bool tab_next_held = false;
    bool action_y_held = false;
    /// `in.attack` (X) read as the inventory screen's own X while it is open.
    bool action_x_held = false;
    bool pet_stance_held = false;
    bool pet_town_held = false;
    /// Hotbar buttons already down when the hotbar got the pad back (e.g. the B that closed
    /// a menu), ignored until let go. `hotbar_had_pad`: the hotbar read the pad last frame.
    std::array<bool, 7> hotbar_wait_release{};
    bool hotbar_had_pad = false;

    /// The level's authored `LEVELNAME`, which the HUD prints in the corner.
    std::string level_display_name;

    /// The level's own `RIMLIGHT`, which every character in it carries
    /// (`CLevel::addCharacter`).
    std::string rim_light_texture = kDefaultRimLight;

    /// The layout this level was loaded from, as asked for (which level this is: the
    /// pet's town trip and the town timer read it).
    std::string level_reference;

    /// Whether the next main menu offers `ContinueLast`.
    bool continue_available = false;

    std::string chosen_dungeon;
    /// The floor that goes with it: the picked global floor number.
    int chosen_dungeon_floor = 1;
    /// The biome the Choose Level list returned `ChooseLevelBiome` for (`run_main_menu()`).
    std::string chosen_biome;

    /// The floor `place_dungeon_floor()` last built and where its exit is, polled by `run()`
    /// every frame; cleared by `load_layout()` (`Renderer::DungeonTransition`).
    bool dungeon_active = false;
    std::string dungeon_active_name;
    int dungeon_active_floor = 0;
    int dungeon_active_floors_total = 1;
    Ogre::Vector3 dungeon_exit_point = Ogre::Vector3::ZERO;
    float dungeon_exit_radius = 4.0f;
    /// The floor's `ENTRANCE_NODE`: its `Stairs Up` Unit Trigger activates a
    /// Warper with `LEVEL DELTA -1` and no dungeon name.
    Ogre::Vector3 dungeon_entrance_point = Ogre::Vector3::ZERO;
    /// The `Warper`s of the floor's `ENTRANCE_NODE` and `EXIT_NODE` layouts:
    /// where each stands and its `getForwardAbsolute()`.
    Ogre::Vector3 dungeon_entrance_warp_at = Ogre::Vector3::ZERO;
    Ogre::Vector3 dungeon_entrance_warp_forward = Ogre::Vector3::UNIT_Z;
    Ogre::Vector3 dungeon_exit_warp_at = Ogre::Vector3::ZERO;
    Ogre::Vector3 dungeon_exit_warp_forward = Ogre::Vector3::UNIT_Z;
    float dungeon_entrance_radius = 3.0f;
    bool dungeon_transition_pending = false;

    /// `Renderer::set_passability_dump()`; empty writes nothing (Renderer::dungeon_transition).
    std::string passability_dump_path;

    bool warp_pending = false;
    std::string warp_dungeon;
    int warp_floor = 1;
    /// `CGameClient +0x10a0` and `+0x10a4` as `warpLevels` left them: level delta and
    /// absolute depth, for `performWarp`'s entry type.
    int32_t warp_level_delta = 0;
    int32_t warp_level_absolute = 0;
    /// `ELevelEntryType`, which `CLevel::loadRoomLayout`
    /// picks the start by: 3 from `setGameState`, else `core::warp_entry_type`.
    int level_entry_type = 3;
    /// The warp's `WAYPOINT` (`CGameClient +0x10ba`): a Warper that authors it,
    /// or a row of the waypoint menu.
    bool warp_waypoint = false;

    /// A `CFormationNode` waiting for the player: `CFormationNode::setLayoutFile`
    /// loads its monsters out of the level, `updateLevelObject`
    /// brings them in within `kFormationWakeDistance`.
    struct PendingFormation {
        std::string path;
        Ogre::Vector3 at;
        int monster_level = 0;
        bool woken = false;
        /// Its layout holds `Unit Spawner`s: they spawn as it wakes.
        bool spawners = false;
        /// The formations file's name for the layout (`CFormationNodeSaveAndLoad +0x18`).
        std::string reference;
    };
    /// A `CFormationNode` over `reference` at `at` (`setLayoutFile`);
    /// false when the layout is not in the archive.
    bool add_pending_formation(const std::string& reference, const Ogre::Vector3& at,
                               int monster_level);
    std::vector<PendingFormation> pending_formations;

    /// Spawned units playing their entrance, held out of their AI until it stops
    /// (`LevelScript::set_spawning`).
    std::map<int64_t, Ogre::AnimationState*> spawning_units;
    static constexpr float kFormationWakeDistance = 25.0f;  ///< `distance < 25.0`
    void wake_formations();

    /// The two banners: `CGameUI`'s `setBigText` (`+0x1980`)
    /// and `setSmallText` (`+0x1990`). Each holds, then
    /// `CGameUI::updateIngameUI` fades it by 2.0 a second.
    struct Banner {
        std::string text;
        float left = 0.0f;
        float alpha = 0.0f;
    };
    Banner big_banner;
    Banner small_banner;
    /// The Xbox `+` over the bag and its glow (`core/game_ui.h`).
    tl::core::AvailablePointsIcon available_points;
    void set_big_text(const std::string& text, float seconds);
    void set_small_text(const std::string& text, float seconds);

    /// The level's `LEVELNAME` (template `+0x770`) and `SHOWFLOORNUMBER` (`+0x76c`), what
    /// `CLevel::getName` (`level_display_name`) and the arrival banner are made of.
    std::string level_name_authored;
    bool level_shows_floor = true;
    bool level_has_rules = false;
    /// `CGameClient::loadLevel`: the level's name big and "Floor N" small, for 1.0
    /// second.
    void raise_arrival_banner();

    /// `CPlayer::levelUp` after `CCharacter::levelUp`:
    /// `PLAYER_LEVELUP` particle, chime and narrator, the banners for 2.0 seconds, full
    /// health and mana.
    void player_levelled_up();
    /// `CCharacter::awardExperience` on the player: pets
    /// first (`LevelScript::award_pet_experience`); true when the player rose a level.
    bool award_player_experience(int amount, bool dying);

    /// `CParticlePreloader::GetParticle` by name: its `FILE` in
    /// `media/particles/preload.dat`, empty for none.
    std::string preloaded_particle_file(const std::string& name);
    std::map<std::string, std::string> preloaded_particles;
    bool preloaded_particles_read = false;

    /// The level-up particle's nodes under the player's (`CPlayer +0x778`,
    /// `CPlayer::unitInit`), replaced each level-up.
    std::vector<Ogre::SceneNode*> level_up_particle_nodes;
    void clear_level_up_particle();

    /// Fill the HUD in from what the engine knows (`HudState`).
    void refresh_game_hud();

    /// The main menu, built from the game's own interface files. Empty until
    /// run_main_menu() puts one up.
    MainMenu menu;

    /// The loading screen, from `media/UI/loading.layout`.
    LoadingScreen loading;

    /// The character creation screen, from `media/UI/charactercreate.layout`.
    CharacterCreate create_screen;

    /// Paint the loading screen and leave it on the display: one frame, as loading is
    /// synchronous and what was presented last stays.
    void paint_loading_screen() {
        if (config.headless || window == nullptr || root == nullptr) return;
        if (!loading.show(kGameGroup, config.width, config.height)) return;
        // Hidden while painted: the outgoing level's line must not show through.
        hud.set_line(std::string(), std::string());
        root->renderOneFrame();
        // After the frame: the boost lowers the GPU clock.
        tl::core::set_loading_boost(true);
    }

    /// Whether a level's music and ambience start when it loads (off for a frame dump).
    bool audio_enabled = true;

    /// The line on screen, for the debug overlay and for the frame dumps.
    std::string dialog_line;
    std::string dialog_speaker;

    /// A `FLOATYTEXT` line and who says it, while it fades (a `CTextEvent`, apart from the
    /// box). This countdown drives the fade only; the machine's decides when it is over.
    std::string floaty_line;
    std::string floaty_unit;
    float floaty_remaining = 0.0f;

    /// The `CTextEvent`s in the air (`CGameUI +0x1468`), oldest first (core/text_event.h).
    std::vector<tl::core::TextEvent> text_events;
    /// The player's `+0x1ec` / `+0x1f0`: the damage taken and not yet shown.
    tl::core::DamageDisplay player_damage_shown;
    /// `displayDamageAbsorbed(delta)` on the player: the red number, when due.
    void display_player_damage(float delta);

    /// Who each hunting unit is going for, by attacker node: 0 the player, else the
    /// target's node (from `Hunt` events, for `step_monster_swings()`).
    std::map<int64_t, int64_t> hunt_target;

    /// The `Pathing` node walking the player, or 0 (only the renderer knows it arrived).
    int64_t scripted_walk_path = 0;

    /// `--logic-trace`: every wire the level script delivers, in the trace line format.
    std::FILE* logic_trace = nullptr;
    float logic_trace_next_sample = 0.0f;
    Ogre::Vector3 logic_trace_last{1e9f, 1e9f, 1e9f};

    /// The script has the player disabled (`CSceneNodeObject::setEnabled(false)`): only a
    /// scripted path moves the character.
    bool script_player_disabled = false;

    /// Units a script put on a one-shot, back to idle when it has run
    /// (`CCharacter::setAIPlayAnimation`'s `queueBlendAnimation("IDLE")`; also `die`'s
    /// `DEATH_LAND` behind a flight).
    struct PendingIdle {
        int64_t node = 0;
        std::string unit;
        Ogre::AnimationState* state = nullptr;
        std::string animation = kUnitIdleAnimation;
        bool loops = true;
        /// What is queued behind `animation` in turn, each with whether it loops.
        std::vector<std::pair<std::string, bool>> after;
        /// `animation` queued on the animator (`queueBlendAnimation("IDLE", true, 0.2,
        /// speed)`, starting 0.2 s before the end).
        Ogre::AnimationState* queued = nullptr;
    };
    std::vector<PendingIdle> pending_idles;

    /// The unit the player pressed A on and is walking to, or empty:
    /// `CGameUI::updateIngameUI` opens its talk at state 0x1e, after the walk; dropped if
    /// the walk fails.
    std::string pending_talk;

    /// The player's target (`CCharacter +0x340`) while it is someone to talk to; the script
    /// reads it through `set_player_target` (`idleAI`).
    int64_t talk_target_node = 0;
    /// The stash whose lid is up while its screen is (`CStashMenu::setOpen`).
    std::string stash_opened_unit;
    void animate_stash(const std::string& unit, bool open);
    /// Where a frame's time goes, summed over the fps line's window (us).
    unsigned long long prof_select_us = 0;
    unsigned long long prof_drive_us = 0;
    unsigned long long prof_advance_us = 0;
    unsigned long long prof_render_us = 0;
    /// The part of `prof_render_us` before the swap (OGRE walking the scene, GL calls); the
    /// rest is the swap, so the two split CPU from GPU.
    unsigned long long prof_submit_us = 0;
    /// The level's machines - the monsters' AI, paths and fights - inside
    /// `prof_advance_us` (step_script()).
    unsigned long long prof_script_us = 0;
    /// GpuSkinning::step(), inside `prof_submit_us`.
    unsigned long long prof_skin_us = 0;

    /// `prof_submit_us` split at OGRE's seams, summed over every viewport and the window:
    /// `controllers` (frame start to the first scene-graph update: particles, animations),
    /// `scene_graph`, `cull` (also skeletons and particle geometry) and `draw` (GL calls);
    /// `cull` and `draw` are also kept per viewport.
    struct FramePhases final : Ogre::SceneManager::Listener, Ogre::RenderQueueListener {
        enum Target { kView = 0, kTexture, kMinimap, kTargets };
        Ogre::Timer* timer = nullptr;
        const Ogre::RenderTarget* window = nullptr;
        /// The scene listened to, for the viewport being drawn (the minimap's has no
        /// preFindVisibleObjects, Automap::attach).
        const Ogre::SceneManager* scene = nullptr;
        unsigned long long frame_start = 0;
        unsigned long long mark = 0;
        bool first = false;
        int target = kView;
        unsigned long long controllers = 0;
        unsigned long long scene_graph = 0;
        unsigned long long cull = 0;
        unsigned long long draw = 0;
        unsigned long long cull_by[kTargets] = {};
        unsigned long long draw_by[kTargets] = {};

        unsigned long long now() const { return timer != nullptr ? timer->getMicroseconds() : 0; }
        void start_frame() {
            frame_start = now();
            first = true;
        }
        void reset() {
            controllers = scene_graph = cull = draw = 0;
            for (int i = 0; i < kTargets; ++i) cull_by[i] = draw_by[i] = 0;
        }
        void preUpdateSceneGraph(Ogre::SceneManager*, Ogre::Camera*) override {
            mark = now();
            if (first) controllers += mark - frame_start;
            first = false;
        }
        void postUpdateSceneGraph(Ogre::SceneManager*, Ogre::Camera*) override {
            scene_graph += now() - mark;
        }
        int classify(const Ogre::Viewport* viewport) const {
            return viewport == nullptr || viewport->getTarget() != window ? kTexture
                   : viewport->getZOrder() != 0                           ? kMinimap
                                                                          : kView;
        }
        void preFindVisibleObjects(Ogre::SceneManager*, Ogre::SceneManager::IlluminationRenderStage,
                                   Ogre::Viewport* viewport) override {
            target = classify(viewport);
            mark = now();
        }
        void postFindVisibleObjects(Ogre::SceneManager*,
                                    Ogre::SceneManager::IlluminationRenderStage,
                                    Ogre::Viewport*) override {
            const unsigned long long spent = now() - mark;
            cull += spent;
            cull_by[target] += spent;
        }
        void preRenderQueues() override {
            if (scene != nullptr) target = classify(scene->getCurrentViewport());
            mark = now();
        }
        void postRenderQueues() override {
            const unsigned long long spent = now() - mark;
            draw += spent;
            draw_by[target] += spent;
        }
    };
    FramePhases frame_phases;

    /// Two jobs on OGRE's frame events: `frameStarted` ends the loading boost
    /// (paint_loading_screen()), and `frameRenderingQueued` (Root::_updateAllRenderTargets)
    /// splits `prof_submit_us` from the swap. It also counts frames for
    /// `step_stats_overlay`, in every loop.
    struct FrameClock final : Ogre::FrameListener {
        Ogre::Timer* timer = nullptr;
        Impl* owner = nullptr;
        unsigned long long queued_us = 0;
        bool frameStarted(const Ogre::FrameEvent&) override {
            tl::core::set_loading_boost(false);
            if (owner != nullptr) owner->step_stats_overlay();
            if (owner != nullptr) owner->touch_overlay.step();
            if (owner != nullptr) owner->frame_phases.start_frame();
            if (owner != nullptr) {
                owner->gpu_timer.frame_started(
                    owner->scene,
                    {owner->window, owner->light_map_target(), owner->shadow_map_target()});
            }
            return true;
        }
        bool frameRenderingQueued(const Ogre::FrameEvent&) override {
            if (owner != nullptr) owner->gpu_timer.frame_queued();
            // Every draw of the frame is issued: materials can change now consistently.
            if (owner != nullptr && timer != nullptr) {
                const unsigned long long s0 = timer->getMicroseconds();
                owner->gpu_skinning.step(owner->scene);
                owner->prof_skin_us += timer->getMicroseconds() - s0;
            }
            if (timer != nullptr) queued_us = timer->getMicroseconds();
            return true;
        }
    };
    FrameClock frame_clock;
    TouchOverlay touch_overlay;
    int talk_target_machine = kNoMachine;
    void set_talk_target(const std::string& unit);
    void clear_talk_target();

    /// Which script machine drives a unit: `kLevelMachine`, an index into `nested_scripts`,
    /// or `kNoMachine`; found through the scene node, as rooms share node ids.
    static constexpr int kLevelMachine = -1;
    static constexpr int kNoMachine = -2;
    int unit_machine(const std::string& unit, int64_t& node) const;
    /// Whether the machine that runs node `node`, drawn at `where`, lets it be targeted
    /// (found through the scene node, as in unit_machine()).
    bool node_targetable(int64_t node, const Ogre::SceneNode* where) const;
    tl::core::LevelScript* machine_script(int machine);
    void apply_machine_events(int machine, const std::vector<tl::core::ScriptEvent>& events);
    /// The machine whose `Quest Controller` has a line up, or `kNoMachine` (rooms' too).
    int dialog_machine() const;
    bool script_dialog_open() const { return dialog_machine() != kNoMachine; }
    /// `LOOK_AT_PLAYER` (`CQuestDialogMenu::setNPC`).
    void speaker_looks_at_player(const std::string& unit);
    /// `CQuestDialogMenu::updateDialogTags`: the line's
    /// `<LOOKAT=NAME>` made the speaker's target.
    void update_dialog_tags(const std::string& unit, const std::string& text);

    /// The confirm button was down last frame: a dialog goes on the press, not the hold.
    bool confirm_held = false;
    bool attack_held = false;  ///< the pad's Attack (X), for its press edge
    /// The console's X (core/attack_button.h): hold position (`+0x1fe`), "down at the last
    /// dispatch" (`+0x55`) and the 0.1 s repeat clock (`+0x40`).
    tl::core::AttackButton attack_button;
    /// The X button's share of the in-game dispatch.
    ///
    void step_attack_button(float delta, bool down, bool pressed, float move_x, float move_y);
    /// `step_attack_button` ran this frame (the pad's dispatch); `advance`
    /// clears it, and steps the button itself for `auto_attack` without it.
    bool attack_button_stepped = false;
    /// `CCharacter::setDestination` for the held player:
    /// stopped, `attack()`, the point kept (`+0x1c0`). (0, 0) is the point already there.
    void set_player_destination_held(float x, float z);

    /// Whether the skip button was already down last frame.
    bool skip_held = false;

    /// DIVERGENCE (2026-09-30): a debug menu, as a pad cannot type into `CConsole`;
    /// drawn on `modalmenu_xenon.layout`, a row of the Pause Menu (`core::options_row`).
    bool debug_menu_open = false;
    /// The title's main menu is running (`run_main_menu`): Options' close test
    /// (`+0x1404`), out of a game the Pause Menu does not come back.
    bool title_menu_running = false;
    int debug_menu_row = 0;
    bool quit_held = false;
    bool debug_stick_held = false;
    void open_debug_menu();
    void close_debug_menu();
    void refresh_debug_menu();
    /// The menu's pad: up and down, A, and B or + to close.
    void drive_debug_menu(const tl::core::InputState& in);
    /// `CPlayer +0x790` (`CPlayer::getIsInGodMode`): `CCharacter::modifyHP`
    /// changes nothing while set.
    bool god_mode = false;
    /// `CConsole::executeCommandLine`'s `GODSPEED`: `GOD`
    /// flips `+0x790`, `SPEED` (`addSpeedToPlayer`) toggles
    /// `PLAYER_SPEED_UP`. DIVERGENCE: both set to `on`, and no `CPlayer::setCheater`.
    ///
    void set_godspeed(bool on);
    /// DIVERGENCE (2026-10-03, for testing skills): the LEVEL 100 row:
    /// `CConsole::executeCommandLine`'s `LEVELUP` per level
    /// (`CCharacter::awardExperience`) up to `level`.
    void level_up_to(int level);
    /// DIVERGENCE (2026-10-11, for testing skills): the port's own rest of the row:
    /// the best droppable staff (ISA 0x3d) for the level, its requirements met from the
    /// unspent points, worn as A wears it (`wear_item`).
    void outfit_debug_staff();
    /// The LEVEL row whole: `level_up_to`, then `outfit_debug_staff`.
    void debug_level_row(int level);

    /// The port's performance line (`Hud::set_stats`): frames over half-second windows,
    /// and the window's size; toggled from the debug menu.
    bool stats_overlay_on = false;
    unsigned stats_frames = 0;
    unsigned long long stats_window_us = 0;
    void set_stats_overlay(bool on);
    void step_stats_overlay();

    /// Which path each unit is walked by, so the log can say where a path picked it up.
    std::map<int64_t, std::string> unit_path;

    /// A sound definition: `CSoundData` (`core/sound_data.h`); layouts and lines name the
    /// definition, not the file.
    using SoundDefinition = tl::core::SoundData;

    /// `CSoundBankDataInformation`, every `media/sounds/` table: a bank reaches a
    /// definition by `NAME`, a layout's `Sound` node or a particle's sound by `GUID`.
    tl::core::SoundDataInformation sound_info;
    bool sound_files_loaded = false;

    void ensure_sound_index();
    const SoundDefinition* sound_by_name(const std::string& name);
    const SoundDefinition* sound_by_guid(const std::string& guid);

    /// Decoded effects, by archive path, shared with every voice playing one; dropped with
    /// the archive's caches on remount (`mount_resources()`).
    std::map<std::string, tl::core::SamplePtr> samples;

    /// Definitions already reported on, so each is said once.
    std::set<std::string> reported_sounds;

    /// Play a sound definition by name, positioned in the world (authored files, `VOLUME`,
    /// `VOLUMEVARIATION`, `PLAYCHANCE`, `RADIUS`). False when not played.
    bool play_sound(const std::string& name, const Ogre::Vector3& at);
    /// The same for a layout's `Sound` node (`CSoundObject::play`):
    /// its `VOLUME` scales the definition's, its `RADIUS` fills in
    /// (`CSoundBank::addSample`), and its last sound stops first.
    /// `volume_given`: `CSoundBank::playSample` handed a
    /// volume, standing in for the definition's (as `CParticleCache::playSounds` does).
    bool play_sound(const std::string& name, const Ogre::Vector3& at, float volume_scale,
                    float radius_if_none, const std::string& voice_key, bool volume_given = false);
    /// A sample whose draws the level machine made (`LevelScript::play_unit_sample`).
    bool play_sound_drawn(const std::string& name, const std::string& recording, float variation,
                          const Ogre::Vector3& at);
    /// Which recording of a definition sounds: the sound system's choice, not a game draw.
    // PORT: the port's own generator, apart from the game's two; seeded from the clock.
    const std::string& pick_recording(const SoundDefinition& definition);
    std::minstd_rand recording_picker{static_cast<std::minstd_rand::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count())};

    /// Say one of the character's voice lines (`CSoundBank::queueGlobalSample`). The slot is
    /// the player's bank's even for the pet's lines, as `modifyHP` reads it.
    /// `delay` in seconds; `volume` 0: the sound's own. False when the class authors nothing
    /// for the slot or the line is not queued.
    bool play_voice(tl::core::SoundSlot slot, float delay = 0.0f, float volume = 0.0f);
    /// `CSoundBank::queueGlobalSample(slot, volume, delay)`
    /// on a bank whose `slot` names `name`.
    bool queue_global_sample(tl::core::SoundSlot slot, const std::string& name, float volume,
                             float delay);

    /// Play one of the interface's own sounds (banks the menus build, named by definition;
    /// core/sound_bank.h).
    bool play_ui(tl::core::InterfaceSound sound);

    /// The definition name in a slot, or "-", for the log.
    const char* voice_or_none(tl::core::SoundSlot slot) const;

    /// Advance `play_voice()`'s queue (`CSoundManager::update`).
    ///
    void step_voices(float delta);

    /// `CSoundManager`'s queue of global samples.
    tl::core::VoiceQueue voice_queue;

    /// Slots already reported as unauthored, one line each.
    std::set<int> reported_voices;

    /// ... and without a position, for something that happens to the player.
    bool play_sound_2d(const std::string& name, const char* label = "voice");
    /// `CQuestDialog::playDialogSound` / `stopDialogSound`: the one dialogue
    /// recording that is sounding, by its voice key.
    void play_dialog_sound(const std::string& name);
    void stop_dialog_sound();
    std::string dialog_sound_key;

    /// The decoded sample behind one archive path, decoding it the first time.
    tl::core::SamplePtr sample_for(const std::string& archive_path);

    /// Start the music (the rules' `MUSIC`) and the ambience (a layout `Sound` node) this
    /// level authors, after it is placed.
    void start_level_audio(const std::string& layout_path);

    /// The channels a layout's `Sound` nodes play on (core/sound_object.h): `CSoundManager`'s
    /// 64 (`FMOD::System::getChannel`), each a mixer voice; a positional one keeps its spot.
    struct NodeChannel {
        bool used = false;
        bool positional = false;
        Ogre::Vector3 at = Ogre::Vector3::ZERO;
        float gain = 0.0f;
        float pan = 0.0f;
        /// Held with no voice, a run with no audio device: sounding while it
        /// loops, over at once otherwise.
        bool silent = false;
        bool silent_loop = false;
    };
    std::array<NodeChannel, 64> node_channels;
    bool node_channel_sounding(size_t index) const;
    /// What a machine's sound banks ask of the mixer.
    tl::core::SoundBankHost node_sound_host();
    /// The pan of each positional node channel, kept to the listener and the
    /// camera every frame.
    void step_node_channels(float delta);
    float node_channel_pan(const Ogre::Vector3& at) const;

    /// Start (or stop, for an empty name) a track the data names.
    bool play_music(const std::string& relative, bool loop, const char* because);

    /// What is on each channel, so a re-load does not restart the right track.
    std::string music_track;

    /// The track the level authors, for `Play Level Music` to put back.
    std::string level_music;

    /// The level's `Music` nodes (`CMusicObject`) by node id, made on first input and updated
    /// every frame (`CMusicObjectDescriptor::update`).
    std::map<int64_t, tl::core::MusicObject> music_objects;
    /// What a node's `CMusicObject` asks of the sound manager and its level.
    tl::core::MusicHost music_host(int64_t node);
    void music_node_input(const tl::core::ScriptEvent& event);
    void step_music_objects(float delta);

    tl::formats::CmpScene script_layout;
    std::set<int64_t> script_gated;
    tl::core::QuestState initial_quests;
    bool script_ready = false;

    /// The scripted camera, when a `Camera Controller` has taken it: the controller moves
    /// the point looked at, and the camera keeps its offset (as follow_subject()). `follow`
    /// is the node whose position is that focus.
    struct ScriptedCamera {
        bool active = false;
        Ogre::Vector3 position = Ogre::Vector3::ZERO;
        int64_t follow = 0;
        bool have_follow_position = false;
        Ogre::Vector3 follow_position = Ogre::Vector3::ZERO;

        /// The pan over `PAN TIME`.
        Ogre::Vector3 from = Ogre::Vector3::ZERO;
        float elapsed = 0.0f;
        float pan_time = 0.0f;
        float ease_at = 1.0f;
    };
    ScriptedCamera scripted_camera;

    /// `CGameUI +0x178c`, the interactive menus open (`setInteractiveMenuVisible`):
    /// the bars (`CInteractiveMenu`, `+0x570`) are up while above
    /// 0. Counted by `INTERACTIVE UI` and the quest dialog (`CQuestDialogMenu::setOpen`).
    ///
    int interactive_menus = 0;
    /// The quest dialog's share of `interactive_menus`.
    bool quest_dialog_interactive = false;
    void set_interactive_menu_visible(bool on);
    /// The quest dialog's share, taken or given back.
    void set_quest_dialog_interactive(bool on);
    /// `CCameraControl +0x98` and `+0x94` (`setCinematicMode`).
    tl::core::CinematicCamera camera_cinematic;
    /// The player's `+0x52e`, `INVINCIBLE` (only a `Game State Controller`'s `INVULNERABLE`,
    /// `performStateControl`): `modifyHP` holds the health at 1.
    bool player_invincible = false;
    /// The player's `+0x70d`, the mesh hidden on purpose (`setMeshVisible`):
    /// `HIDE PLAYER`.
    bool player_mesh_hidden = false;
    /// `CCharacter::setMeshVisible(visible, true)`
    /// on the player.
    void set_player_mesh_visible(bool visible);
    /// `CGameStateController::doActualGameState`, the
    /// interface's half (the level's is `LevelScript::game_state_do`).
    void game_state_interface(uint32_t type, bool on);
    /// State 6, `CLOSE INGAME UI`: `CGameUI::closeLeft`, `closeRight`,
    /// `closeMenus` and `closeAll`.
    void close_ingame_ui();
    /// What `CCameraController::startCamera` closes:
    /// `CGameUI::closeAll` and `CGameUI::hideModalDialogs`.
    void close_for_camera();
    /// `CGameStateController::healPlayer`:
    /// `modifyHP(maxHP())`.
    void heal_player();

    /// Player Start's world position, and whether the layout had one.
    Ogre::Vector3 player_start = Ogre::Vector3::ZERO;
    /// The facing the level stands the player in (`CLevel +0x164`), as the player's yaw.
    float player_start_yaw = 0.0f;
    bool have_player_start = false;

    /// The dynamic light map (setup_light_map()): `ProjectorRTTLight` (`CGameClient +0x210`),
    /// its `ProjectorCam` (`CCameraControl +0x28`), and the quads `ProjectorLightQuad`
    /// (`+0x220`, `+0x230`) and `ProjectorLightMask` (`+0x228`, `+0x238`), in the scene only
    /// while it renders (`preRenderTargetUpdate`). Torn down before Root.
    struct LightMap {
        Ogre::TexturePtr texture;
        Ogre::Viewport* viewport = nullptr;
        Ogre::Camera* camera = nullptr;
        Ogre::SceneNode* camera_node = nullptr;
        Ogre::SceneNode* quad_node = nullptr;
        std::unique_ptr<Ogre::Rectangle2D> light_quad;
        std::unique_ptr<Ogre::Rectangle2D> light_mask;
        Ogre::MaterialPtr light_material;
        Ogre::MaterialPtr mask_material;
        std::unique_ptr<Ogre::RenderTargetListener> listener;
        /// `CGameClient +0x2a8` and the frame clock `gTorchlightUpdate`; see
        /// step_light_map().
        float flicker_angle = 0.0f;
        float flicker_clock = 0.0f;
    } light_map;

    /// The shadow map (setup_shadow_map()).
    struct ShadowMap {
        Ogre::TexturePtr texture;
        Ogre::Viewport* viewport = nullptr;
        Ogre::Camera* camera = nullptr;
        Ogre::SceneNode* camera_node = nullptr;
        Ogre::SceneNode* quad_node = nullptr;
        std::unique_ptr<Ogre::Rectangle2D> background;
        Ogre::MaterialPtr background_material;
        std::unique_ptr<Ogre::RenderTargetListener> listener;
    } shadow_map;

    /// The `Layout Link Particle` whose subtree is being placed, or 0
    /// (ParticleSystem::owner).
    int64_t particle_owner = 0;
    /// What kind of file is being placed (ParticleSystem::wrapper): under a layout link,
    /// and a particle file. Saved and restored around the recursion.
    bool placing_under_link = false;
    bool placing_particle_file = false;
    /// Set by a layout link just before it places its file, taken by
    /// place_layout() as it starts.
    bool link_places_next_layout = false;
    /// Set by whatever places a file as a `CParticle`, taken by place_layout(): loaded as
    /// `CParticlePreloader::LoadParticle` does, only its
    /// `Particle`, `Sound` and `Camera Shake` objects; no script runs.
    bool cparticle_places_next_layout = false;

    /// A particle file's `CParticleCache` extras (`LoadParticle`,
    /// copied by `createParticleCache`): one-shot `Sound`s by GUID
    /// with `VOLUME` (+0x40, +0x58) and `Camera Shake`s (+0xc0), in file order, by path.
    /// `CParticleCache::CParticleCache(SceneManager*, CSoundBank*)`.
    ///
    struct ParticleFileExtras {
        struct Sound {
            std::string guid;
            float volume = 1.0f;
        };
        std::vector<Sound> sounds;
        std::vector<tl::core::CameraShake> shakes;
    };
    std::map<std::string, ParticleFileExtras> particle_file_extras;
    const ParticleFileExtras* particle_extras(const std::string& reference);

    /// One placed `CParticle`: its extras, and where it stands from its first system's
    /// holder (ParticleSystem::cparticle).
    struct PlacedParticle {
        const ParticleFileExtras* extras = nullptr;
        std::string reference;
        Ogre::Vector3 from_holder = Ogre::Vector3::ZERO;
        /// On the level's list (`CLevel +0x260`, X360 `+0x1bc`) that `CLevel::findParticles`
        /// fills: start-on-load links with no `NO CULLING`
        /// technique, which `update_visible_particles` shows and hides.
        bool listed = false;
        /// `CSceneNodeObject +0x81` of the link (X360 `+0x6d`): shown.
        bool visible = true;
    };
    std::map<uint32_t, PlacedParticle> placed_particles;
    uint32_t next_placed_particle = 0;
    /// How many placements are `listed`.
    size_t listed_particles = 0;
    /// `updateVisibleParticles`' static `mLastPos`: the camera focus the last
    /// pass was made for.
    Ogre::Vector3 visible_particles_last{-9999.0f, -9999.0f, -999.0f};
    /// `CLevel::updateVisibleParticles(Ogre::Camera*, const Ogre::Vector3&, bool)`:
    /// a listed link is shown while across the screen's width
    /// (half a screen spare), within aspect times 15 of the focus, or paused (CLayout::setVisible).
    void update_visible_particles(bool paused);
    /// Make `particle_systems[first, end)` that `take` accepts one `CParticle` over
    /// `reference` at `origin`; 0 when none was taken.
    uint32_t register_cparticle(const std::string& reference, size_t first,
                                const Ogre::Vector3& origin,
                                const std::function<bool(const ParticleSystem&)>& take);
    /// `CParticle::Start` besides the systems:
    /// `CParticleCache::playSounds` and `playCameraShakes`.
    void cparticle_started(uint32_t id);
    void play_particle_extras(const ParticleFileExtras& extras, const Ogre::Vector3& at,
                              const std::string& reference);
    /// Drop the placements none of whose systems is left.
    void forget_dead_cparticles();

    /// Log each `Puzzle Input`'s order as its machine is built (`CPuzzleRandomizer` +0x6c,
    /// shuffled on the volatile generator).
    void log_puzzles(const tl::core::LevelScript& script);

    /// Increasing paint order for layout meshes; see place_mesh().
    unsigned layout_draw_order = 1;
    /// Blended passes on alpha-less textures that place_mesh() made solid.
    size_t blends_drawn_solid = 0;
    /// Cut-out meshes place_mesh() queued after the other depth-writing blends.
    size_t cut_outs_drawn_last = 0;
    /// What a texture's alpha holds (`formats::dds_alpha_census`), by name:
    /// 255 throughout or absent, and the share an alpha rejection removes.
    struct TextureAlpha {
        bool opaque = false;
        float clear_share = 0.0f;
    };
    std::unordered_map<std::string, TextureAlpha> texture_alphas;
    const TextureAlpha& texture_alpha(Ogre::TextureUnitState* unit);
    /// The layout meshes given an order of their own below that counter's
    /// ceiling; bake_static_scenery() leaves them out.
    std::set<const Ogre::Entity*> paint_ordered;

    /// GUID to mesh paths, from the tileset index: every `FILE` of the record, in order (the
    /// variants one of which each copy rolls, `CRoomPiece::initRoomPiece`).
    std::map<int64_t, std::vector<std::string>> tileset;
    bool tileset_loaded = false;

    /// GUID to collision mesh paths (`COLLISIONFILE`, in order), from the same index;
    /// not drawn (src/core/collision.h).
    std::map<int64_t, std::vector<std::string>> tileset_collision;

    /// Each piece's `PRERENDER` (-1) / `POSTRENDER` (+1), MeshPlacement::render_phase.
    std::map<int64_t, int> tileset_render_phase;
    /// Every `PIECE` GUID the index holds, drawn or not.
    std::set<int64_t> tileset_known;
    /// `CRoomPieceDataInformation +0x10` (console `+8`): the last piece named `ERROR`
    /// (`ParseRoomPieceData`), drawn for an unknown GUID; 0
    /// with none (`CRoomPieceDataInformation::CRoomPieceDataInformation`).
    ///
    int64_t tileset_default_guid = 0;

    /// Brightness and contrast, drawn over the finished frame.
    DisplayGamma display_gamma;

    /// The in-game minimap and automap system.
    Automap automap;
    bool automap_initialized = false;
    bool map_toggle_held = false;

    /// The level's collision geometry, in world space.
    tl::core::CollisionWorld collision;
    size_t collision_meshes = 0;
    size_t collision_skipped = 0;

    /// The level's pathable grid (`CLevel::bakeCollision`, core/passability.h).
    tl::core::PassabilityMap passability;
    /// One box per placed room layout, grown by its pieces' collision bounds,
    /// and which of them the room piece being placed belongs to (-1: none).
    std::vector<tl::core::PassabilityMap::Box> collision_sections;
    int collision_section = -1;

    /// The `Unit Trigger` nodes placed with their own collision: candidates for
    /// `CBaseUnit::addToAvoidanceMap` once `passability.bake()` stands; drained after it.
    std::vector<int64_t> door_trigger_nodes;

    /// Which of `door_trigger_nodes` marked the map (for `CBaseUnit+0x19b`), to take back out
    /// on opening (`CBaseUnit::removeFromAvoidanceMap`).
    std::set<int64_t> door_avoidance_added;
    /// The `Unit Trigger`s a `Not Collidable` switched off (`CBaseUnit +0x18d`
    /// clear), which the level state carries in `CItemSaveState +0x5e`.
    std::set<int64_t> trigger_not_collidable;

    /// The level's own file's `Path Node Occupation` nodes whose cells are in
    /// the map (`CPropertyNode +0x150`); `apply_path_blockers` starts it over.
    std::set<int64_t> level_path_blockers_on;
    /// A path blocker node's box as `updatePathNodes` hands it to the level, for the linked
    /// layouts' and rooms' `Path Node Occupation` regions.
    struct PathBlocker {
        bool circle = false;
        tl::core::Vec3 centre;
        /// `+0x120`, `updateBoundingBox`'s bounds of the turned box.
        tl::core::Vec3 low;
        tl::core::Vec3 high;
        /// The node's own scale X (`BOX WIDTH`, else `RADIUS`): a circle's
        /// radius, and what `killOrMoveBaseUnits` measures with.
        float scale_x = 0.0f;
    };
    struct MachinePathBlocker {
        PathBlocker box;
        /// `CPropertyNode +0x150`: its cells are in the map; `updatePathNodes`
        /// adds only while clear and removes only while set.
        bool applied = false;
    };
    std::map<int64_t, MachinePathBlocker> machine_path_blockers;
    /// Put every enabled one on the navigation grid, once the grid stands.
    void apply_machine_path_blockers();

    /// The `No Spawn Region` `Property Node`s as boxes, in the order `CLevel::loadRoomLayout`
    /// (case 3) adds them to `CLevel +0xd8`.
    std::vector<tl::core::NoSpawnRegion> no_spawn_regions;
    /// `CLevel::populate`'s sections: one box
    /// per collision-enabled room piece, in the order the pieces were placed.
    std::vector<tl::core::PopulateSection> population_sections;
    /// `CLevel +0x28`, `populate`'s room records: one per placed room layout, the union of
    /// its sections (`getRoomIndexThatPositionIsIn`).
    std::vector<tl::core::LevelRoomBox> level_rooms;

    /// The player character, when --play is on (off: a frame dump wants a fixed pose).
    tl::core::Player player;
    bool player_control = false;

    /// The camera's compass heading (radians about +Y) and subject-to-camera vector, from
    /// the layout's authored camera (follow_subject()).
    float camera_yaw = 0.0f;
    Ogre::Vector3 camera_offset = Ogre::Vector3::ZERO;
    bool camera_follows = false;

    /// `CCameraControl`'s state across frames (`updateGameCamera`):
    /// smoothed focus (+0x4c), smoothed offset (+0x3c), `gbWasInZoomIn`.
    struct GameCamera {
        Ogre::Vector3 focus = Ogre::Vector3::ZERO;
        Ogre::Vector3 offset = Ogre::Vector3::ZERO;
        bool was_zoomed_in = false;
        bool have_focus = false;
        bool close_up = false;  ///< the state last logged
    };
    GameCamera game_camera;
    /// The right stick's zoom, -1 to 1 (`InputState::zoom`), and the dolly (`CCameraControl +0x38`,
    /// X360 `+0x18`), reset each level. `CCameraControl::CCameraControl`
    /// (X360 inline in `CGame::createCamera`): dolly 28.5, the rest zero.
    float camera_zoom_input = 0.0f;
    float camera_dolly = 0.0f;
    /// The frame's delta for the camera (`step_player`); 0 for any other re-aim.
    float camera_delta = 0.0f;
    /// The unit talked to in the quest window (target in AI state 0x1f), and the dialog's
    /// `CAMERAMODX/Y/Z` (`CQuestDialog +0x88`).
    std::string talk_unit;
    Ogre::Vector3 talk_camera_mod = Ogre::Vector3::ZERO;
    /// The same for a script's line (`CGameUI::forceInteractiveDialog`):
    /// its target character, 0 for a prop's line.
    int64_t script_talk_node = 0;
    Ogre::Vector3 script_talk_camera_mod = Ogre::Vector3::ZERO;

    /// Whether the loaded layout aimed the camera itself (only the main-menu layouts).
    bool layout_has_camera = false;

    /// Where that authored camera was aimed, in world space.
    Ogre::Vector3 layout_camera_target = Ogre::Vector3::ZERO;

    /// What the follow camera looks at, relative to the subject's feet.
    Ogre::Vector3 camera_look_offset = Ogre::Vector3::ZERO;

    /// CAMERAMULT from the level rules; see apply_environment().
    float camera_mult = 1.0f;

    /// The quit input was pressed (`+`, ESC); `run()` ends its loop, and `main.cpp`'s
    /// `play()` returns to the main menu (`Renderer::quit_to_menu_requested()`).
    bool quit_requested = false;

    /// The animation the player controller last asked for, applied once per change.
    std::string player_animation;

    // ---------------------------------------------------------------- combat --

    /// Everything the player character's own `.DAT` chain authors (`[EQUIPMENT]`,
    /// `WEAPON_SCALE`, speeds), from load_player_stats().
    tl::core::UnitStats player_stats;

    /// The weapon whose attack is in use (`CCharacter +0x3ec`, Linux `+0x498`), and the
    /// entity drawing the right hand's (the left's when the right is empty).
    tl::core::ItemStats weapon;
    bool has_weapon = false;
    Ogre::Entity* weapon_entity = nullptr;
    /// Where `weapon` is equipped: 0 the right hand, 1 the left, -1 bare hands.
    int attack_location = -1;
    /// `CCharacter +0x2b2` (Linux `+0x37e`): flipped by every `performAttack`;
    /// with both hands armed the hands swing in turn.
    bool alternate_hand = false;
    /// The left hand's item when `weapon_entity` is the right's (one of
    /// `worn_attachments`), for its weapon trail.
    Ogre::Entity* worn_left_hand = nullptr;
    /// The two hands as `attackRange`, `selectAttack` and `attack` read them.
    tl::core::HeldHands player_held_hands();
    /// `CCharacter::selectAttack(mode)` for the player;
    /// nothing while a swing has more than 0.2 s left.
    void select_player_attack(int mode);
    /// `CCharacter::attack`: the hand
    /// a swing is taken with when one is ranged and the other not.
    void choose_player_attack_hand();
    /// The other equipped items `CEquipment::attachToGivenLocation` puts on the character
    /// (core/equipment_attach.h), rebuilt by `sync_worn_attachments`.
    std::vector<Ogre::Entity*> worn_attachments;

    /// The hand nodes, `CCharacter +0x2e8` (right) and `+0x2f8` (left): children of the
    /// unit's node scaled by `WEAPON_SCALE` (`CCharacter::getBones`),
    /// kept on `tag_righthand` / `tag_lefthand` by `updateBonePositions`.
    /// Made when a weapon first wants one.
    Ogre::SceneNode* hand_nodes[2] = {nullptr, nullptr};
    /// The held weapon's element particle (`CEquipment +0x3c8`), under its hand
    /// node, and the hand it rides.
    std::vector<Ogre::SceneNode*> weapon_element_particles;
    int weapon_element_hand = 0;
    /// `CEquipment::createParticles` + `attachToGivenLocation`
    /// for the weapon just put in a hand.
    void attach_weapon_element(bool right);
    void clear_weapon_element();

    /// The animation class that weapon swings on ("Polearm", "Rslash", "Attack"; core/weapons.h).
    std::string attack_class = "Attack";

    /// A swing in progress; the hit lands on the manifest's `HIT` frame
    /// (core/animation_manifest.h).
    struct Swing {
        bool active = false;
        std::string animation;  ///< the `.SKELETON` stem being played
        float elapsed = 0.0f;   ///< seconds into it, at the played rate
        float hit_at = 0.0f;    ///< when the blow lands
        float length = 0.0f;    ///< how long the whole thing runs
        bool landed = false;
        int64_t target = 0;  ///< the node aimed at, 0 for "whatever is in reach"
    };
    Swing swing;

    /// The animation a cast queued: `CCharacter::castSkill`
    /// blends it in for `duration`, then `IDLE` or the loop; its keys fire `EVENT_TRIGGER`
    /// (`CCharacter::updateSkillKeys`).
    struct SkillPlayback {
        bool active = false;
        std::string animation;  ///< the `.SKELETON` stem playing
        int index = -1;         ///< its place in the model's list
        float time = 0.0f;      ///< the animation's own clock, in seconds
        float length = 0.0f;
        float speed = 1.0f;
        float held = -1.0f;  ///< frame time left on the cue; -1 loops
        int then_loop = -1;  ///< queued after it, looping
    };
    SkillPlayback skill_playback;
    /// `CCharacter +0x398` and the cast flags beside it.
    tl::core::ActiveSkill player_active_skill;
    /// The character's animations in manifest order with lengths (`CGenericModel +0x1e0 ->
    /// +0x28`), built once per mesh.
    tl::core::AnimationCatalog player_catalog;
    std::string player_catalog_mesh;
    void build_player_catalog();
    /// Put a cast's cue on the character.
    void play_skill_cue(const tl::core::SkillAnimationCue& cue);
    /// The shadow and light maps brought up to date for a frame dump.
    void update_light_maps(const std::string& png_path);
    void step_skill_animation(float delta);
    /// The skill animation's own keys, before the skill's: trails, sounds, particles.
    void fire_model_keys(const std::vector<tl::core::FiredKey>& keys,
                         const tl::core::AnimationManifest& manifest);
    /// `CCharacter::updateActiveSkill` for
    /// the player.
    void step_active_skill(float dt);

    /// The node the character is fighting (`CCharacter::setTarget`, `+0x340`); outlives a
    /// swing, read by `CMonster::idleAI` for the pet.
    int64_t player_target_node = 0;
    /// The target item (`CCharacter +0x350`) as a skill sees it: a breakable `castSkill`
    /// found and aimed at; 0 for none. Kept until `setTarget`,
    /// a position aim, or the breakable leaving (`notifyOfDeletion`).
    int64_t player_skill_item = 0;
    /// `CCharacter::setTarget` for the living
    /// player: the target, and the target item let go.
    void set_player_target(int64_t node) {
        player_target_node = node;
        player_skill_item = 0;
    }

    /// How much life something has left, by layout node id (one unit name covers several);
    /// seeded on the first hit.
    struct Health {
        float current = 0.0f;
        float max = 0.0f;
        bool dead = false;
    };
    std::map<int64_t, Health> unit_health;
    Health player_health;

    /// The player's own death: `CCharacter::alive` false (AI state 5, then 6) and
    /// `CPlayer +0x884`, the delay before the death screen.
    struct PlayerDeath {
        bool dying = false;
        std::string animation;  ///< the `DIE` animation `findRandomAnimation` chose
        float timer = tl::core::kDeathTimerStart;
    };
    PlayerDeath player_death;
    /// `CDieMenu +0x30` (open), and the pad's highlighted choice. `CDieMenu::CDieMenu`
    /// starts closed, nothing chosen.
    bool death_screen_up = false;
    int death_selected = 0;
    /// `setGameState(5)` with choice 2 away from a town: `warpLevels("Town", -99)`.
    bool death_town_pending = false;

    /// A spawned instance's own level and champion-ness, by node, set at `spawn_unit()`;
    /// `level == 0`: none (readers use `effective_monster_level()`).
    struct SpawnLevel {
        int level = 0;
        bool champion = false;
    };
    std::map<int64_t, SpawnLevel> unit_spawn_level;

    // PORT: the level a placed unit was made at (`CLevel +0x1a8` for a populated floor's,
    // `spawn_unit()`), else the player's `character_level`.
    int effective_monster_level(int64_t node) const {
        const auto found = unit_spawn_level.find(node);
        if (found != unit_spawn_level.end() && found->second.level > 0) return found->second.level;
        return character_level;
    }

    /// Whether this node was promoted at spawn through `CHAMPIONSPAWNCLASS`
    /// (`apply_champion_curves()`; apart from `UnitStats::is_champion`).
    bool spawn_marked_champion(int64_t node) const {
        const auto found = unit_spawn_level.find(node);
        return found != unit_spawn_level.end() && found->second.champion;
    }

    /// The authored stat curves under `media/graphs/stats/`; see `core/stats.h`.
    tl::core::StatBook stat_graphs;
    bool stat_graphs_loaded = false;
    /// `media/graphs/camerashakes/`: the curves a `CCameraShake` names.
    tl::core::StatBook camera_shake_graphs;
    bool camera_shake_graphs_loaded = false;
    /// `CCameraControl`'s two shake lists (`+0x58`, `+0x70`), walked by
    /// update_game_camera() and cleared with the level.
    tl::core::CameraShakes camera_shakes;
    /// `CGameSpeed`, from `CMasterResourceManager::CMasterResourceManager`
    /// with its own `DecelerateGameSpeed.dat`
    /// (`CGraph::loadGraph`); stepped by advance() (load_game_speed()).
    tl::core::StatGraph game_speed_curve;
    tl::core::GameSpeed game_speed{&game_speed_curve, &game_speed_curve};
    bool game_speed_loaded = false;
    /// The lowest speed of the slow-down under way, for the log.
    float game_speed_lowest = 1.0f;
    void load_game_speed();
    /// `CCharacter::rollAttack`: a player blow leaving the target dying or dead
    /// slows the game for half a second.
    void slow_on_player_kill(bool target_dead);

    /// Every item in the game, by name, from `media/masterresourceunits.dat`.
    tl::core::ItemBook items;
    bool items_loaded = false;
    /// The master table's `UNIT_GUID`s (`CResourceManager::getUnitDataByGuid`,
    /// `getUnitGuidByDataGroup`), by `NAME`; filled by `ensure_item_index`.
    std::map<int64_t, std::string> master_unit_by_guid;
    std::map<std::string, int64_t> master_guid_by_unit;  ///< by upper-cased `NAME`
    /// -1 for a unit the table has not got.
    int64_t unit_guid_of(const std::string& name);
    /// Empty for a guid the table has not got.
    std::string unit_of_guid(int64_t guid);

    /// The `.ANIMATION` manifest beside each character mesh, by mesh directory.
    std::map<std::string, tl::core::AnimationManifest> manifests;

    /// The attack button held with the stick at rest for the whole scene
    /// (`step_attack_button`), to verify a fight headlessly (Renderer::set_auto_attack()).
    bool auto_attack = false;

    /// `--pad`: step the console's target selector in a headless run.
    bool headless_targeting = false;

    /// What the console's selector picked and last frame's, so a change is logged once.

    std::string pad_selected;

    bool pad_selected_hostile = false;

    int64_t pad_selected_node = 0;

    /// The entity lit up and whether as hostile; checked against the scene before clearing.

    Ogre::Entity* pad_highlighted = nullptr;

    bool pad_highlighted_hostile = false;

    /// The `dropped_items` entry targeted for pickup (`CCharacter::setTargetItem`,
    /// `+0x350`), sought only with no unit target (`+0x340`).
    int item_targeted = -1;
    Ogre::Entity* item_highlighted = nullptr;
    /// The second pass's other pick: a `Unit Trigger` by machine and node
    /// (`trigger_targeted_machine` `kNoMachine` for none).
    int trigger_targeted_machine = -2;
    int64_t trigger_targeted_node = 0;
    void interact_targeted_trigger();
    /// The sleeping mimic the same pass points at, by machine and node. DIVERGENCE: the PC
    /// targets it under the pointer (`CGameClient::updateIngame`); a pad cannot.
    int mimic_targeted_machine = -2;
    int64_t mimic_targeted_node = 0;
    /// A on it: `CCharacter::attack` on a `MIMICIDLE`
    /// target calls `reactToDamage(player, true)`. DIVERGENCE: ranged, the original walks to
    /// it instead.
    void disturb_targeted_mimic();
    /// The selection came from the console's second pick (`+0x1c4`).
    bool item_second_pick = false;
    /// The target item as A sets it (`CCharacter::setTargetItem`, `+0x350`, AI state 7):
    /// what `getItemAI` walks to and uses (`step_item_ai`).
    struct PlayerItemTarget {
        /// `Mimic`: a sleeping mimic as the target (`+0x340`) for `attackAI`.
        enum class Kind { None, Dropped, Trigger, Portal, Mimic };
        Kind kind = Kind::None;
        Ogre::SceneNode* dropped = nullptr;  ///< the floor item's node
        int machine = kNoMachine;            ///< a `Unit Trigger`'s (or mimic's) machine and node
        int64_t node = 0;
        std::string portal;  ///< an open portal's unit
        Ogre::Vector3 portal_at = Ogre::Vector3::ZERO;
    };
    PlayerItemTarget player_item;
    /// A on the current item selection: `setTargetItem` with what it points at.
    void target_selected_item();
    /// `CCharacter::getItemAI` for the player, each frame.
    void step_item_ai(float delta);
    /// `CCharacter::attackAI` on a sleeping mimic.
    void step_mimic_attack(float delta);
    /// The enchant shrine a shrine's page works on (`CEnchantMenuXenon +0xf4`, set
    /// from the target item): its machine and `Unit Trigger` node.
    int enchant_shrine_machine = -2;
    int64_t enchant_shrine_node = 0;
    /// The shrine's `+0x19c` (Linux `CTriggerUnit +0x1f0`) cleared: a one-shot
    /// trigger at its last state (the machine's `UnitTriggerView::armed`).
    bool enchant_shrine_spent() const;
    /// `CGameUI::updateIngameUI` for AI state 0x16.
    void open_enchant_shrine(int machine, int64_t node);
    /// Whether a `Unit Trigger`'s prop is an `ENCHANTSHRINE` (ISA 0x9d).
    bool is_enchant_shrine(const std::string& unit);
    /// The prop a trigger draws, if it draws one.
    Ogre::Entity* trigger_entity(int64_t node, const std::string& unit, const NodeMap& nodes) const;
    /// The renderer's handle on one placed copy of a node (a `Unit Trigger`'s presence and
    /// collision group, a pathing blocker): copies share node ids, so the key mixes in the
    /// room and link hierarchy `CLogicNodeState` saves (+0x30, +0x40). Level nodes keep ids.
    int64_t instance_key(int64_t node, const NodeMap& nodes) const;
    int64_t instance_key(int64_t node, int machine) const;
    static int64_t instance_key(int64_t node, uint32_t scene, int64_t hierarchy);

    /// Fishing (`core/fishing.h`): the player's AI states 0x20..0x22 and the
    /// bobber (`CPlayer +0x850..+0x860`).
    tl::core::FishingState fishing_state = tl::core::FishingState::Off;
    tl::core::FishingBobber bobber;
    /// The fishing animation playing, and its speed (`blendAnimation`'s).
    std::string fishing_animation;
    float fishing_animation_rate = 1.0f;
    /// A one-shot fishing animation, and whether it has run out.
    bool fishing_animation_once = false;
    float fishing_animation_clock = 0.0f;
    /// The level's fish slot class (`CLevelTemplateData +0x5a8`, slot 6 of
    /// `chooseUnitSpawners`); empty with no template.
    std::string level_fish_spawn_class;
    /// `CPlayer +0x828`, `+0x830`, `+0x840`/`+0x848`: the rod on the right
    /// hand, the fish on the hook, the line.
    Ogre::Entity* fishing_rod = nullptr;
    Ogre::Entity* hooked_fish = nullptr;
    Ogre::AnimationState* hooked_fish_idle = nullptr;
    Ogre::ManualObject* fishing_line = nullptr;
    Ogre::SceneNode* fishing_line_node = nullptr;
    /// Where the hole is: the lure never sinks below it (`updateFishingLine`).
    Ogre::Vector3 fishing_hole_at = Ogre::Vector3::ZERO;
    /// Whether a `Unit Trigger`'s prop is a `FISHING HOLE` (ISA 0x7a).
    bool is_fishing_hole(const std::string& unit);
    /// `CPlayer::startFishing` at the hole
    /// `at`.
    void start_fishing(const Ogre::Vector3& at);
    /// `setAIState(2)` out of any fishing state (`CPlayer::setAIState` hides the rod).
    void stop_fishing();
    /// Play a fishing animation (`blendAnimation` / `queueBlendAnimation`).
    void play_fishing_animation(const std::string& name, float rate, bool once,
                                const std::string& then);
    /// What `queueBlendAnimation` put behind the one-shot fishing animation.
    std::string fishing_queued;
    /// `animationPlayingSubstring` for the one-shot fishing animation.
    bool fishing_animation_playing(const std::string& name) const;
    /// One frame of AI states 0x20..0x22, and the two buttons in 0x21.
    void step_fishing(float delta, bool catch_press, bool stop_press);
    /// `CPlayer::catchFish`.
    void catch_fish();
    /// `CPlayer::fishingAICatch` once
    /// `FISHING_CATCH` has run out.
    void land_catch();
    /// `CPlayer::updateAnimation`'s fishing half and
    /// `updateFishingLine`: rod, line and hooked fish.
    void update_fishing_gear();
    void build_fishing_gear();
    void destroy_fishing_gear();
    /// The ring's size and colour (`CFishingMenu::update`).
    ///
    void refresh_fishing_screen();
    /// `CGameUI::openModalDialog`.
    void open_modal(const std::string& title, const std::string& text);
    /// The name a quest objective prints (`CQuestUnitData::getUnitName`),
    /// off the item and unit tables.
    tl::core::QuestUnitNamer quest_unit_namer();
    /// What `CQuestUnitData` reaches outside the quest for (`tl::core::QuestUnitWorld`).
    tl::core::QuestUnitWorld quest_unit_world();

    /// The player's own level (`award_experience()`), and what a placed monster with no
    /// `unit_spawn_level` scales its curves at.
    int character_level = 1;

    /// A standing unit's fidgets: `CMonster::idleAI` under `animationExistsSubstring("FIDGET")`:
    /// while idle, `randomIntegerBetweenVolatile(0, 1000) <= 9` per AI tick, not within
    /// `kFidgetCooldown` (`+0x284`), one `findRandomAnimation("FIDGET")` played once with
    /// `IDLE` queued. DIVERGENCE (frame rate): the per-tick roll is a rate per second at
    /// 60 Hz, applied as `1 - exp(-rate * delta)`.
    struct Fidget {
        /// Seconds since this unit last fidgeted - the original's `+0x284`.
        float since = 0.0f;
        /// The state a fidget is playing on, or null when none is.
        Ogre::AnimationState* state = nullptr;
        /// What to go back to when it runs out.
        std::string back_to;
        /// What is playing, so a scene unit can be put back on its own idle.
        std::string playing;
        /// A scene unit's idle queued behind the fidget (`queueBlendAnimation("IDLE", true,
        /// 0.2, 1)`), taking over 0.2 s before it ends.
        Ogre::AnimationState* queued = nullptr;
        /// The mesh's `.SKELETON`s named "fidget", asked once; empty until then.
        std::vector<std::string> available;
        bool looked_up = false;
    };

    /// Keyed by entity, and cleared whenever an entity is torn down (`clear_subject()`).
    std::map<Ogre::Entity*, Fidget> fidgets;

    /// How fast a swing plays, as a multiple of real time (the blow's frame sets the cadence).
    // INVENTED: `ATTACKSPEED` over 100; the console's attack animation speed is not
    // transcribed here.
    float swing_rate() const {
        return player_stats.attack_speed > 0.0f ? player_stats.attack_speed / 100.0f : 1.0f;
    }

    /// Link one `.SKELETON` beside the character's mesh onto it and what it wears
    /// (Renderer::link_animation()).
    bool link_subject_animation(const std::string& name);

    /// Play one of them, exclusively, on the character and what it wears
    /// (Renderer::apply_animation()); `quiet` for the per-frame caller.
    bool apply_subject_animation(const std::string& name, float time, bool quiet);

    /// Put the character on whatever wanted_player_animation() asks for.
    void follow_player_animation();

    /// What the character should be playing this frame: its swing while one is
    /// running, and otherwise whatever its movement says.
    std::string wanted_player_animation() const {
        if (player_death.dying) {
            return player_death.animation.empty() ? player_animation : player_death.animation;
        }
        if (fishing_state != tl::core::FishingState::Off && !fishing_animation.empty()) {
            return fishing_animation;
        }
        // An `Animation Controller` holding the player (`setAIPlayAnimation`,
        // state 0x10): the scene's animation, over whatever it was doing.
        if (!scripted_player_animation.empty()) return scripted_player_animation;
        if (skill_playback.active) return skill_playback.animation;
        if (swing.active) return swing.animation;
        return player.animation();
    }
    /// What a `PLAYER` `Animation Controller` has the character play
    /// (`CAnimationPlayer::playAnimation`), and whether it loops.
    std::string scripted_player_animation;
    bool scripted_player_loops = false;

    void load_stat_graphs();

    void load_camera_shake_graphs();

    size_t load_graph_directory(const std::string& directory, tl::core::StatBook& book);

    void start_critical_shake();
    /// A shake `CCharacter::die` starts at a body, along
    /// the up vector (`+0x9c`), from `centre`.
    void start_death_shake(const char* name, float duration, float magnitude,
                           const Ogre::Vector3& centre);
    /// `die`'s branches before the fall: the burst or the death flight.
    /// True when one took the death (no `DIE` animation).
    bool die_burst_or_flight(const tl::core::ScriptEvent& event, SceneUnit& unit, NodeMap& nodes);

    /// Read every quest under `media/Quests/` into `quest_book`, once.
    void load_quest_book();
    bool quest_book_loaded = false;

    /// A quest no NPC initialised is worth 1 level (`CQuest +0x1ec` from the constructor);
    /// `--quest-level` moves it for `--accept-quest`.
    int quest_level_for_offer = 1;
    void ensure_item_index();
    /// A unit's data group: the file over its `BASEFILE` chain
    /// (`CUnitResourceList::parseChildFile`, core/unit_resource.h); a missing base is empty.
    /// `first_root` is tried before each `BASEFILE` (`xbox/`). With a `group`, also the four
    /// values `CUnitResourceList::loadFile` appends.
    tl::formats::AdmFile read_unit_file(const std::string& path,
                                        const std::string& first_root = std::string(),
                                        const tl::core::ResourceGroup* group = nullptr);
    const tl::core::AnimationManifest& manifest_for(const std::string& mesh_path);
    void load_player_stats();

    /// Read the playable classes, once.
    void load_player_classes();

    /// Destroy the subject and everything hanging off it.
    void clear_subject();

    /// The three classes a new character can be, in the order the creation
    /// screen shows them. Empty until load_player_classes() has run.
    std::vector<tl::core::PlayerClass> player_classes;

    /// The character's class by `NAME`, or empty (a `--mesh` run): this, not the mesh's
    /// filename, identifies the class.
    std::string player_class;
    bool equip_starting_weapon();
    /// `location` is where the inventory holds it (0 the right hand, 1 the
    /// left); -1 puts a bow in the left and anything else in the right.
    bool equip_weapon_item(const tl::core::ItemStats& item, int location = -1);
    /// Hang `right` and `left` (item names, empty for none) on a unit's
    /// `tag_righthand`/`tag_lefthand` bones (its `[EQUIPMENT]`).
    void equip_unit_items(Ogre::Entity* entity, const std::string& unit, const std::string& right,
                          const std::string& left);
    /// `CEquipment::calculateCombatStats` for the weapon in
    /// a hand: its attack (`+0x238` right, `+0x23c` left), or none.
    tl::core::UnitStats::WeaponAttack weapon_attack_of(const std::string& item_name,
                                                       tl::core::Hand hand = tl::core::Hand::Right);
    /// `CCharacter::createDefaultEquipment`, spawn-class
    /// half: `SPAWNRIGHTHAND` then `SPAWNLEFTHAND`, one `g_Rand` roll each.
    void roll_unit_equipment(const std::string& unit, tl::core::LevelScript::UnitCreation& made);
    /// What `CCharacter::createDefaultEquipment` makes a machine's
    /// characters' equipment with.
    tl::core::LevelScript::UnitEquipment unit_equipment_hooks();
    /// Hang on each drawn character the hands its roll gave it, once made.
    void sync_unit_equipment();
    void unequip_weapon();
    void sync_equipped_weapon();
    /// `CEquipment::equipped` for every location but the held weapon's.
    void sync_worn_attachments(int weapon_location);
    void clear_worn_attachments();

    /// An item on the floor (dropped, a monster's, gold, a quest's). Picked up by
    /// `CCharacter::getItemAI`/`getItem`
    /// via `setTargetItem`, chosen by
    /// core/targeting.h. Drawn as the console's (`step_ground_labels`,
    /// core/item_label.h); not transcribed: `CItem::update`'s light override.
    ///
    struct DroppedWorldItem {
        tl::core::ItemStats item;
        int count = 0;
        Ogre::SceneNode* node = nullptr;
        Ogre::Entity* entity = nullptr;

        /// The toss (`CEquipment +0x250` path, `+0x258` distance along it,
        /// `+0x25c` still flying) and the orientation it lands in (`+0x2f0`).
        tl::core::DropArc arc;
        float travelled = 0.0f;
        bool flying = false;
        bool spins = false;
        Ogre::Quaternion rest = Ogre::Quaternion::IDENTITY;

        /// `CBaseUnit +0x170/+0x178` for an item a quest placed; empty otherwise.
        tl::core::QuestUnitTag quest_tag;

        /// The id a `Unit Spawner` knows it by, or 0 for an item no spawner released.
        int64_t unit_id = 0;

        /// A `CItemGold`, not a `CEquipment`: amount, turn and size (`pile`), its growing in
        /// (`+0x234`, from 0.35) and `GOLD_SPARKLE`.
        bool is_gold = false;
        tl::core::GoldPile pile;
        float grow_left = 0.0f;
        std::vector<Ogre::SceneNode*> sparkle;

        /// `CEquipment +0x3d0` and `+0x3c8`: the drop particle and a weapon's element
        /// (`createParticles`, core/item_label.h).
        std::vector<Ogre::SceneNode*> particles;

        /// Its range and fade (`CItem +0x199`, `+0x204`, ...), and whether it is on the near
        /// item list (`CLevel +0x90`).
        tl::core::ItemPresence presence;
        bool listed = true;
    };
    /// `CEquipment::update` and `CItemGold::update`
    /// after `CItem::update`: the toss, gold's sparkle and growth.
    void step_dropped_item(DroppedWorldItem& dropped, float delta);
    /// `CEquipment::setActiveInLevel(true)` ->
    /// `createParticles`.
    void start_item_particles(DroppedWorldItem& dropped);
    /// A ground item leaving the level: its particles, then its model.
    void remove_ground_item(DroppedWorldItem& dropped);
    /// The console's floor labels for the frame: landed items and usable
    /// `Unit Trigger`s on screen (core/item_label.h), handed to the HUD.
    void step_ground_labels();

    /// `CUnitThemes::reload`, once
    /// (`CUnitThemes::CUnitThemes(folder)`).
    void load_unit_themes();
    /// `CBaseUnit::unitInitThemes` the first
    /// time a unit's themes are asked for.
    void init_unit_themes(UnitThemeState& state, const tl::core::UnitStats& stats);
    /// `CCharacter::unitThemeUpdated` on a
    /// drawn unit: its theme particles made, placed and started, or stopped.
    void unit_theme_updated(UnitThemeState& state, Ogre::Entity* entity, uint32_t unit_type,
                            const tl::core::UnitTheme* theme, bool removed);
    /// What `ThemeParticleTriggers` asks of a drawn unit; `light_map` marks a theme's
    /// particles.
    tl::core::ThemeParticleUnit trigger_unit(UnitThemeState& state, Ogre::Entity* entity,
                                             uint32_t unit_type, const std::string& label,
                                             bool light_map);
    /// `configureParticleTrigger`, the node
    /// half, for the triggers just made, and their offsets (+0x28).
    void configure_trigger_nodes(UnitThemeState& state, Ogre::Entity* entity);
    /// `CCharacter::loadModel`, the triggers
    /// of the model's base skeleton, once per drawn unit.
    void load_model_triggers(UnitThemeState& state, Ogre::Entity* entity, const std::string& mesh,
                             uint32_t unit_type);
    /// `CParticle::Start` / `Stop` / the park for steps `setVisible`
    /// and `die` hand back.
    bool cparticle_looping(const UnitThemeState& state, int particle) const;
    void apply_trigger_steps(UnitThemeState& state,
                             const std::vector<tl::core::ThemeParticleStep>& steps);
    /// `CCharacter::updateBonePositions`, the
    /// triggers' half.
    void update_theme_particle_positions(UnitThemeState& state, Ogre::Entity* entity);
    /// Each frame: `CBaseUnit::update`'s theme half for the player and every
    /// placed unit, then their theme particles moved onto their bones.
    void step_unit_themes();
    /// `CLevel::updateNPCIcons` over the
    /// placed units.
    void update_npc_icons();
    /// `CGameUI::getUIIsInCinematic`, the
    /// part the port has.
    bool ui_in_cinematic() const;
    /// Take a unit's theme particles down and forget its themes.
    void clear_unit_themes(UnitThemeState& state);
    /// `GLOBALS.DAT`'s label colours (`core/item_label.h`), read with the item
    /// figures.
    tl::core::ItemLabelColours item_label_colours;
    /// Level-local: `restart()` replaces `Impl`, so a dropped item does not survive leaving.
    /// INVENTED: the original keeps its items in `CLevelState`; player drops have no field.
    std::vector<DroppedWorldItem> dropped_items;

    /// A `CBreakable` in the level: a barrel, an urn, a basket (core/breakable.h).
    struct BreakableWorld {
        std::string unit;  ///< its `NAME`
        tl::core::BreakableData data;
        tl::core::Breakable state;
        Ogre::SceneNode* node = nullptr;
        Ogre::Entity* entity = nullptr;
        /// Its collision group, and the id the `Unit Spawner` that released it
        /// knows it by.
        int64_t id = 0;
        bool has_collision = false;  ///< its collision model loaded (`+0x238`)
        /// Its range and fade, and whether it is on the near item list.
        tl::core::ItemPresence presence;
        bool listed = true;
    };
    /// In `CLevel::addItem` order, newest first. Level-
    /// local. INVENTED: no `CLevelState` field, so a restored level has none.
    std::vector<BreakableWorld> breakables;
    /// What the last item near list logged: listed, active, of all.
    std::array<size_t, 3> item_near_logged{static_cast<size_t>(-1), 0, 0};
    /// The level's items as `CLevel::getActiveUnitsAtPosition`
    /// walks them (`+0x90`): the breakables still standing, at their nodes.
    std::vector<tl::core::ActiveUnitCandidate> breakable_item_candidates() const;
    /// Items a missile's area reached (`CMissile::doAOEDamage`,
    /// `CItem::interact(nullptr)`), broken after the missiles step.
    struct PendingItemInteract {
        int64_t item = 0;
        bool by_player = false;  ///< the breaker `interact` is handed
    };
    std::vector<PendingItemInteract> pending_item_interacts;
    void flush_item_interacts();
    /// Queue `item` for `flush_item_interacts`: true for a standing, unqueued breakable.
    bool queue_item_interact(int64_t item, bool by_player = false);
    /// The level's items as the machines get them (`set_level_items`), offered to the damage
    /// shapes each frame (`CLevel::updateItems`).
    tl::core::LevelScript::LevelItems level_items;
    void step_breakable_shapes();
    /// The standing breakable with this id, or null.
    const BreakableWorld* standing_breakable(int64_t id) const;
    /// Whether `unit` is made as a `CBreakable` (`CREATEAS: BREAKABLE` along `BASEFILE`;
    /// `CResourceManager::createNewItem`).
    bool is_breakable_unit(const std::string& unit);
    bool non_selectable_prop(const std::string& unit);
    std::map<std::string, bool> breakable_units;
    /// `NONSELECTABLE` by prop name, once each (`non_selectable_prop()`).
    std::map<std::string, bool> non_selectable_units;
    /// `createUnit` for a breakable, then `CLevel::addItem(b, at, false)`:
    /// `unitInit`, `setActiveInLevel(true)`,
    /// then `settle`. False with no file or model.
    bool place_breakable(const std::string& unit, const Ogre::Vector3& at,
                         const Ogre::Quaternion& facing, int64_t id, const Ogre::Vector3* settle,
                         const float* drawn_scale = nullptr);
    /// `CBreakable::interact(CCharacter*)` on
    /// `breakables[index]`, by the player (`by_player`) or by nobody.
    bool break_breakable(size_t index, bool by_player);
    void cast_prop_trap(const std::string& unit, const std::string& name, uint32_t level,
                        const Ogre::Vector3& skill_at, const Ogre::Quaternion& facing,
                        bool by_player);
    /// `CLevel::findItemWithinView` from the player: the
    /// first breakable within `max_angle` and `max_distance` in clear sight, or -1.
    int breakable_within_view(float max_angle, float max_distance) const;

    /// The point `CLevel::update` runs the items from: a
    /// scripted camera's, else the player's.
    Ogre::Vector3 item_view_point() const;
    tl::core::ItemActiveRangeInput item_range_input(const Ogre::Vector3& at, bool interactable,
                                                    bool casts_shadows) const;
    /// `Camera::isVisible` on the item's culling bounds (`+0x1c0`).
    bool item_in_camera(Ogre::SceneNode* node, bool from_model = false) const;
    /// A step's changes drawn on the item's node.
    void draw_item_presence(Ogre::SceneNode* node, const tl::core::ItemPresence& p,
                            const tl::core::ItemPresenceChange& change);
    /// `CLevel::updateNearUnitList`'s item
    /// half, on the characters' tick (`LevelScript::set_item_near_list`).
    void item_near_list();
    /// `CLevel::updateItems` over the near list: each
    /// `update` (`CBreakable::update`, floor items,
    /// `CTriggerUnit::update`); out-of-range broken ones deleted.
    void step_level_items(float delta);

    /// A `Unit Trigger`'s range and fade (a `CItem`), by instance key, made as `addItem`
    /// makes one the first time the walk meets it.
    struct TriggerPresence {
        tl::core::ItemPresence presence;
        bool listed = true;
        bool interactable = false;  ///< `ISA 0x20`
        bool shadows = true;        ///< `SHADOWS`
    };
    std::map<int64_t, TriggerPresence> trigger_presence;
    /// The collision groups the range took out (`CTriggerUnit::update`), so it puts back
    /// only what it took.
    std::set<int64_t> trigger_range_off;
    /// One `Unit Trigger` as the item walk sees it.
    struct TriggerItem {
        int64_t key = 0;
        int machine = kLevelMachine;  ///< `kLevelMachine` or a `nested_scripts` index
        int64_t node = 0;             ///< its `Unit Trigger` node in that machine
        /// its prop's nodes (none for a bare collision model)
        std::vector<Ogre::SceneNode*> nodes;
        const NodeMap* node_map = nullptr;  ///< its machine's nodes
        Ogre::Vector3 at = Ogre::Vector3::ZERO;
        TriggerPresence* state = nullptr;
    };
    /// Every `Unit Trigger` of every machine of the level.
    std::vector<TriggerItem> level_trigger_items();
    /// `CTriggerUnit::update`'s collision rule on one: in while active, out of
    /// range out (only while `COLLIDEABLE`, i.e. not made `Not Collidable`).
    void trigger_range_collision(int64_t key, bool active);

    /// A merchant: its inventory (`CCharacter +0x490`) and clock (`+0x388`,
    /// `CCharacter::update`), by node (core/merchant.h).
    struct MerchantState {
        std::string unit;
        tl::core::Inventory inventory = tl::core::make_merchant_inventory();
        bool gambler = false;
        float clock = 0.0f;
    };
    std::map<int64_t, MerchantState> merchants;
    /// INVENTED: a `CItemSaveState` here has no affixes, sockets or rolled armour, so the
    /// stock copies are kept by level (`dungeon:depth`) and node.
    std::map<std::string, std::map<int64_t, MerchantState>> remembered_merchants;
    /// `CCharacter::unitInit`'s Merchant branch for any
    /// unstocked merchant, then every merchant's clock.
    void step_merchants(float delta);
    /// `generateMerchantInventory` into a
    /// fresh merchant-shaped inventory for `node`.
    void stock_merchant(MerchantState& state, int64_t node, const tl::core::UnitStats& stats);
    /// The merchant as `core/merchant.h`'s stock roll reads it.
    tl::core::MerchantCharacter merchant_character(MerchantState& state, int64_t node,
                                                   const tl::core::UnitStats& stats);
    /// Run `fn` with the stock roll's context (its pointers live only as long).
    void with_merchant_stock_context(
        const std::function<void(const tl::core::MerchantStockContext&)>& fn);
    /// `CLevel::populateNewMerchantsInventory`, as
    /// `CGameClient::restoreLevelState` ends.
    void populate_new_merchants();
    /// The merchant a vendor screen was opened on, by its unit name; null for
    /// none.
    MerchantState* merchant_named(const std::string& unit);
    /// The merchant whose screen is open.
    int64_t trading_with = 0;
    /// X on a vendor's screen: buy from the merchant's tray,
    /// sell from the bag.
    void trade_selected();
    /// `CCharacter::applySaveState`'s
    /// merchant half, for a merchant the level state brings back.
    void restore_merchant(const tl::formats::CharacterSaveState& saved);
    /// Placed characters already asked whether they are merchants.
    std::set<int64_t> merchant_checked;

    /// A Transmuter's own inventory (`CCharacter +0x490`), by node: the combine's slots 15-18
    /// are its, and stay when the screen closes.
    struct CombinerState {
        std::string unit;
        tl::core::Inventory inventory;
    };
    std::map<int64_t, CombinerState> combiners;
    /// INVENTED, as `remembered_merchants`: the copies the level state cannot
    /// carry, by level and node.
    std::map<std::string, std::map<int64_t, CombinerState>> remembered_combiners;
    /// The combiner whose screen is open.
    int64_t combining_with = 0;
    /// The Transmuter placed as `unit` (ISA 0x86); null for none.
    CombinerState* combiner_named(const std::string& unit);
    /// `CRecipes` (`media/recipes/`), read by `load_recipes()`.
    tl::core::RecipeBook recipes;
    bool recipes_loaded = false;
    void load_recipes();
    /// Y on a Transmuter's screen:.
    void transmute_selected();
    /// A on the bag on an enchanter's or a shrine's screen;
    /// true when the press is the tray's own A instead (an unidentified item).
    bool enchant_selected();
    /// A on the bag on a gem service's page: (mode 2, gems
    /// back, item destroyed) or (mode 3, gems destroyed).
    void gem_service_selected();
    /// A on the retirement page (mode 4): the item becomes the heirloom and
    /// the character retires.
    void retire_selected();
    /// `CGameClient +0x3900` / `+0x3908`: the heirloom for the next new character, taken by
    /// `Renderer::take_heirloom` before the rebuild.
    tl::core::HeirloomHandOff heirloom_handoff;
    /// The retirement's `requestSetGameState(0, 2)`: New Game in retirement mode.
    bool retirement_requested = false;
    /// A on a gem in the bag: the item-select
    /// dialog, for a player with a pet. Returns false when it did not open.
    bool open_gem_socketing();
    /// The dialog's input (`CMessageItemSelectXenon::vfunc_2`); on closing the
    /// tray's poll sets the gem into the chosen item.
    void handle_item_select(const tl::core::InputState& in);
    void socket_selected_gem();
    bool item_select_stick_y_held = false;
    bool item_select_stick_x_held = false;
    /// Start on the inventory tab (code 0xd): the pet commands
    /// dialog (`core/pet_commands.h`). False when it did not open.
    bool open_pet_commands();
    /// The dialog's codes this frame: Y (8), Start (0xd), Back (0xe), and `other` (codes 1..4
    /// and 9..12, which close it).
    struct PetCommandEdges {
        bool y = false;
        bool start = false;
        bool back = false;
        bool other = false;
    };
    /// The dialog's input while it is up (`CMessagePetCommandsXenon::vfunc_2`).
    ///
    void handle_pet_commands(const tl::core::InputState& in, const PetCommandEdges& edges);
    /// `vfunc_5` for the pet as it is now.
    void refresh_pet_commands();
    /// What reads off the player's level.
    tl::core::PetDepartureLevel pet_departure_level() const;
    bool pet_commands_stick_x_held = false;
    /// X on a Transmuter's or a stash's screen: bag to their slots, their
    /// tray back.
    void swap_selected();
    /// X on the bag's screen: the stack to the other tray
    /// (`core::move_to_inventory`); into the pet's only when allows.
    void swap_bag_and_pet();
    /// Y on the pet's tray: the stack onto the level at the pet's feet.
    void drop_selected_pet_item();
    /// The inventory the vendor tray shows on a Transmuter's or a stash's
    /// page, which X moves into and out of; null on any other.
    tl::core::Inventory* housing_inventory_for(VendorScreen screen);
    tl::core::Inventory* housing_inventory_of(VendorScreen screen);
    /// `CCharacter::applySaveState`'s inventory half for a
    /// restored Transmuter.
    void restore_combiner(const tl::formats::CharacterSaveState& saved);
    /// `CBaseUnit::addToAvoidanceMap` / `removeFromAvoidanceMap`
    /// for a breakable: radius + 0.4 on the grid.
    void breakable_avoidance(BreakableWorld& b, bool add);
    /// A breakable taken out of the level: its model, its collision, its place
    /// on the map grid.
    void remove_breakable(BreakableWorld& b);

    void drop_item_near_player(const tl::core::ItemStats& item, int count);
    /// `CLevel::addItem(item, at, true)` for a `CEquipment`:
    /// grounded, then tossed (`CEquipment::drop`).
    void drop_item(const tl::core::ItemStats& item, int count, const Ogre::Vector3& at);
    /// A layout's `Item` node: `CItemDescriptor::CreateObject`
    /// (`CEquipment::createNewEquipment`), then
    /// `CItemDescriptor::descriptorSceneActivated`.
    void place_layout_item(const tl::formats::CmpNode& node, const Ogre::Vector3& at);
    /// `CLevel::addItem(gold, at, true)` for a `CItemGold`: `setActiveInLevel`
    /// grounds it and its sparkle; the drop sound when `drop_sound`.
    void place_gold(const tl::core::GoldPile& pile, const Ogre::Vector3& at,
                    const tl::core::ItemStats* item = nullptr, bool drop_sound = true);
    /// `CItem::snapToGround`: down from 100 to -100; a miss
    /// retries at `randomOpenPosition(p, 4)`, 30 times.
    Ogre::Vector3 snap_item_to_ground(const Ogre::Vector3& at);
    /// `CCharacter::getItem`, the gold half,
    /// for `dropped_items[index]`.
    void take_gold(size_t index);
    /// `CGameClient::autoPickupGold`.
    void auto_pickup_gold();
    /// The treasure at the end of `CCharacter::die`;
    /// `attacker` as `hurt_unit` takes it.
    void drop_death_loot(int64_t node, const std::string& unit, int64_t attacker);
    void drop_death_loot_at(const Ogre::Vector3& at, int64_t node, const std::string& unit,
                            int64_t attacker);
    /// What the loot rolls hand core/item_roll.h: placement tests, `addUnit`, `createUnit`.
    tl::core::LootPlacement loot_placement_at(const Ogre::Vector3& at);
    tl::core::CreateLootUnit loot_unit_maker();
    /// The killer or breaker as the loot roll reads it: `attacker` 0 is the
    /// player, `LevelScript::kNoAttacker` nobody (null).
    const tl::core::LootCharacter* loot_character(int64_t attacker, tl::core::LootCharacter& out);
    /// Units a `Unit Spawner` released with `GIVE LOOT` clear (+0x703, +0x704).
    std::set<int64_t> no_loot_units;
    /// The item's own mesh as an entity, its materials bound; null when it names none.
    Ogre::Entity* create_item_entity(const tl::core::ItemStats& item);
    /// An item at rest on the ground (`CLevel::addItem`, `CItem::snapToGround`): no toss.
    void place_ground_item(const tl::core::ItemStats& item, int count, const Ogre::Vector3& at,
                           const Ogre::Quaternion& orientation, const tl::core::QuestUnitTag& tag);

    /// The `Quest Item` and `Quest Boss` `Property Node`s, in the order
    /// `CLevel::loadRoomLayout` (cases 12 and 13) adds them
    /// to `CLevel +0x108` and `+0xf0`.
    struct QuestMarker {
        Ogre::Vector3 position;
        Ogre::Quaternion orientation;
        /// `CPropertyNode +0x114`: how many of `populate`'s NPCs stand on it.
        int32_t taken = 0;
    };
    std::vector<QuestMarker> quest_item_markers;
    std::vector<QuestMarker> quest_boss_markers;
    /// What each monster placed for a quest carries (`CBaseUnit +0x170/+0x178`), by node.
    std::map<int64_t, tl::core::QuestUnitTag> quest_unit_tags;
    /// A character's own name (`CCharacter +0x4c0`) by node: a quest champion's
    /// (`CQuestUnitData::spawnUnit`) or `UNIQUE_MONSTER_NAME`
    /// (`createUnit`); shown over `DISPLAYNAME`.
    std::map<int64_t, std::string> unit_names;
    /// `CRandomNames` (`media/RANDOMNAMES.DAT`), read with the quests.
    tl::core::RandomNames random_names;
    /// `CQuestManager::populate` for the level just built.
    void populate_quests();
    /// `addCharacter`'s `dropToGround(4.0)` (`CQuestUnitData::createUnit`;
    /// `spawnUnit`).
    Ogre::Vector3 dropped_to_ground(const Ogre::Vector3& at) const;
    void create_quest_unit(const tl::core::QuestObjective& objective,
                           const tl::core::QuestUnitTag& tag, int64_t quest_guid);
    /// `CQuestUnitData::spawnUnit` past its checks; the
    /// character's node, or 0.
    int64_t spawn_quest_unit(const tl::core::QuestObjective& objective,
                             const tl::core::QuestUnitTag& tag, int64_t quest_guid,
                             const Ogre::Vector3& at, const Ogre::Quaternion& facing);
    /// A unit as a quest event sees it (`questEventFire`'s third argument): its
    /// name, its `UNITTYPE` for `ISA`, where it stands and what it carries.
    tl::core::QuestEventUnit quest_event_unit(const std::string& name, const std::string& unittype,
                                              const Ogre::Vector3& at,
                                              const tl::core::QuestUnitTag& tag);
    /// An item's `+0x170` / `+0x178` as a tag; empty for -1.
    tl::core::QuestUnitTag quest_tag_of_item(const tl::core::ItemStats& item);
    /// The `QUEST_GUID` of the quest a tag names, -1 for none.
    int64_t quest_guid_of(const tl::core::QuestUnitTag& tag);
    /// What the bags tell the quest log: `CEquipment::addedToInventory`
    /// raises event 0 for the player's and the pet's bags,
    /// `removedFromInventory` event 5 from any (`housing_item_events`).
    tl::core::InventoryItemEvents bag_item_events;
    tl::core::InventoryItemEvents housing_item_events;
    void wire_bag_item_events();
    void bag_item_added(tl::core::ItemStats& item);
    void bag_item_removed(const tl::core::ItemStats& item);
    /// Where an event's item is: where it lay (`bag_event_at`), else the player.
    Ogre::Vector3 bag_event_position() const;
    bool bag_event_at_set = false;
    Ogre::Vector3 bag_event_at = Ogre::Vector3::ZERO;
    /// Carry out what completed quests take back (`QuestLog::take_item_removals`):
    /// the player's bag, then the pet's.
    void apply_quest_item_removals();
    /// A pickup or a kill reported to the quests, and the banner for each one it finished.
    void quest_event(const std::vector<const tl::core::QuestDefinition*>& finished);
    /// `CCharacter::getItemAI`'s targeting half: which
    /// `dropped_items` entry is in reach; sets `item_targeted`/`item_highlighted` only.
    bool update_item_selection(bool active);
    /// The other half of `getItemAI`/`getItem`: takes whatever
    /// `update_item_selection` is currently pointing at.
    void pick_up_targeted_item();

    /// Release the particle systems the character always carries: the manifest's keys on
    /// the base skeleton (e.g. the gauntlet glow). Animation `SPAWNPARTICLE` keys are the
    /// swing's.
    size_t equip_carried_particles();

    /// A `SPAWNPARTICLE` key on a player animation: a `CParticleAnimationTrigger`, made at
    /// model load in the original (`CCharacter::loadModel`,
    /// `configureParticleTrigger`), here on first firing.
    struct ParticleTrigger {
        std::string animation;
        size_t tag = 0;
        std::string bone;
        Ogre::Vector3 offset = Ogre::Vector3::ZERO;
        bool attaches = true;
        /// `SPAWNPARTICLE_STOP_ON_DEATH` (+0x20).
        bool stop_on_death = false;
        /// The key's `UNITTHEME` (+0x50), upper-cased; empty for none.
        std::string unit_theme;
        /// The node that follows the bone (+0x38), for a trigger that attaches.
        Ogre::SceneNode* follower = nullptr;
        /// What the layout placed, and where each holder sits inside it.
        std::vector<Ogre::SceneNode*> holders;
        std::vector<Ogre::Vector3> inside;
    };
    std::vector<ParticleTrigger> particle_triggers;
    /// The player's unit themes, upper-cased (`CCharacter::unitThemeUpdated`).
    ///
    std::set<std::string> player_theme_names;

    /// A `SPAWNPARTICLE` key fired: `CCharacter::updateAnimation`,
    /// keyframe types 4 and 5.
    void fire_particle_trigger(const std::string& animation, size_t index,
                               const tl::core::AnimationTag& tag);
    /// Keep every attached trigger's node on its bone: `CCharacter::
    /// updateBonePositions`.
    void update_particle_triggers();
    void clear_particle_triggers();

    /// The same on a placed unit: `CCharacter::updateAnimation`
    /// types 4, 5, 0xc (`REMOVEPARTICLES`) and 0xd (`REMOVEANIMATIONPARTICLES`).
    struct UnitParticleTrigger {
        Ogre::Entity* entity = nullptr;
        std::string layout;  ///< the key's `LAYOUT`, upper-cased, slashes forward
        ParticleTrigger trigger;
    };
    std::vector<UnitParticleTrigger> unit_particle_triggers;
    /// Every placed unit's particle keys this frame, and attached triggers on their bones
    /// (`updateBonePositions`).
    void step_unit_particle_keys();

    /// A placed unit's weapon element: `CEquipment::createParticles`
    /// under the hand's node (`attachToGivenLocation`),
    /// kept on the bone by `updateBonePositions`.
    struct UnitWeaponElement {
        Ogre::Entity* entity = nullptr;
        std::string bone;
        Ogre::SceneNode* hand = nullptr;
        std::vector<Ogre::SceneNode*> holders;
    };
    std::vector<UnitWeaponElement> unit_weapon_elements;

    // ---------------------------------------------------------- weapon trails --

    /// An `Ogre::Node` as `CWeaponTrail::update` asks of it:
    /// a weapon's tag point, or a skeleton bone.
    struct OgreTrailNode : tl::core::WeaponTrailNode {
        Ogre::Node* node = nullptr;
        tl::core::Vec3 derived_position() override {
            const Ogre::Vector3 p = node->_getDerivedPosition();
            return {p.x, p.y, p.z};
        }
        tl::core::Vec3 position() override {
            const Ogre::Vector3 p = node->getPosition();
            return {p.x, p.y, p.z};
        }
        void translate_local(const tl::core::Vec3& d) override {
            node->translate(Ogre::Vector3(d.x, d.y, d.z), Ogre::Node::TS_LOCAL);
        }
        void set_position(const tl::core::Vec3& p) override {
            node->setPosition(Ogre::Vector3(p.x, p.y, p.z));
        }
    };
    /// A character's `CWeaponTrail`s (`CCharacter +0x698..+0x6a8`) and the `ManualObject` each
    /// draws into (`CWeaponTrail +0x28`, under its own root node, `+0x40`).
    struct DrawnTrails {
        tl::core::CharacterWeaponTrails trails;
        OgreTrailNode hands[2];  ///< the tag points of slots 0 and 1's items
        OgreTrailNode bones[2];  ///< a key's bone, for the right and left trails
        struct Drawn {
            Ogre::ManualObject* object = nullptr;
            Ogre::SceneNode* node = nullptr;
            std::string material;
        };
        Drawn drawn[3];  ///< right, left, tracer
    };
    DrawnTrails player_trails;
    /// A placed unit's, by its entity; made by its first trail key.
    std::map<Ogre::Entity*, DrawnTrails> unit_trails;
    /// What `equip_unit_items` hung in a placed unit's hands, slots 0 and 1:
    /// the item's entity and its `UNITTYPE`.
    struct UnitHeldItems {
        Ogre::Entity* held[2] = {nullptr, nullptr};
        std::string unittype[2];
    };
    std::map<Ogre::Entity*, UnitHeldItems> unit_held_items;

    /// The character as `CCharacter`'s trail code reads it.
    tl::core::TrailCharacter player_trail_character();
    tl::core::TrailCharacter unit_trail_character(Ogre::Entity* entity, DrawnTrails& drawn,
                                                  bool good, int64_t node = 0);
    /// `updateAnimation`, keys 7 and 8, for
    /// `tag` crossed on `entity`'s animation.
    void weapon_trail_key(DrawnTrails& drawn, Ogre::Entity* entity,
                          const tl::core::AnimationTag& tag, const tl::core::TrailCharacter& who);
    /// The frame's `CWeaponTrail::update`s, before the frame's keys.
    void step_weapon_trails(float player_dt, float world_dt);
    /// Each trail's strip onto its `ManualObject`, once the keys are done.
    void draw_weapon_trails();
    void draw_character_trails(DrawnTrails& drawn);
    void destroy_character_trails(DrawnTrails& drawn);
    /// A placed unit's trails and held items, when its entity goes.
    void forget_unit_trails(Ogre::Entity* entity);
    /// `CGame::createMaterials`'s four trail materials, or a
    /// key's `LAYOUT` made into one (`updateAnimation`), by name.
    void ensure_trail_material(const std::string& name);
    void attach_unit_weapon_element(Ogre::Entity* entity, const tl::core::ItemStats& item,
                                    const std::string& bone, float scale);
    /// `entity`'s, or every one whose unit has left the scene when null.
    void clear_unit_weapon_elements(Ogre::Entity* entity);

    /// Start a swing, if the character is not already in one.
    bool begin_swing(int64_t target);

    /// Advance the player's swing and land it when the authored frame passes.
    void step_swing(float delta);

    /// Advance every hunting monster's swing and land those.
    void step_monster_swings(float delta);

    /// The player's blow on `target`: `rollAttack`, held weapon or bare-handed; sets
    /// `last_blow_critical`. `damage_scale` and `soak_scale` are `rollAttack`'s two floats.
    float player_blow(int64_t target, float damage_scale = 1.0f, float soak_scale = 1.0f);
    /// `player_blow` with no weapon in use: the character's own attack.
    float player_unarmed_blow(int64_t target, float damage_scale, float soak_scale);
    /// The two blows' shared half: the defender, the roll, the interrupt and
    /// `applyDamageEffects`.
    float player_strike(int64_t target, const tl::core::Strike& strike);
    /// What `CCharacter::rollAttack` puts on the one struck
    /// after `applyDamageEffects`: `KNOCKBACK`, `DEGRADEARMOR` and the attacker's
    /// `TRANSFER` effects. Null `target_machine` and `target_node` 0: the player. `from`/`to`:
    /// the knock-back's direction.
    void roll_attack_effects(const tl::core::AttackEffectSources& sources, float knock_back,
                             float degrade, const tl::core::Vec3& from, const tl::core::Vec3& to,
                             tl::core::LevelScript* target_machine, int64_t target_node,
                             const tl::core::EffectSource& attacker);
    /// The same for a placed character's blow: its manager, the pet's bag for the player's
    /// pet, and its `KNOCK BACK` and `DEGRADE ARMOR`.
    void unit_attack_effects(int64_t attacker_node, tl::core::LevelScript* target_machine,
                             int64_t target_node);
    /// A placed character as `CCharacter::isEffectValidForUnit`
    /// reads it.
    tl::core::ItemUseTarget unit_use_target(tl::core::LevelScript* machine, int64_t node);
    /// `CCharacter::getDmgToReflectFromMissile` of the player (0), the pet,
    /// or a placed character: the share sent back.
    float missile_reflect_share(int64_t unit);
    /// `CCharacter::performUnknownSkill(CSkill*)` on the
    /// player, for a used item's skill.
    void perform_unknown_skill(const std::string& name, uint32_t level);
    /// `applyInstantEffect`'s `WARP` on the player: `warpLevels(dungeon, 0,
    /// absolute)`, carried out as a Warper's is.
    void warp_by_effect(const std::string& dungeon, int32_t absolute);
    /// A skill event's caster's level (X360 `+0xf0`) for `CEffect::setOwner`'s
    /// `USEOWNERLEVEL`: the placed character's, or the player's for null; empty if unrun.
    std::optional<int32_t> event_caster_level(tl::core::LevelScript* machine, int64_t node) const {
        if (machine == nullptr) return player_character.level();
        int level = 0;
        if (!machine->unit_level(node, level)) return std::nullopt;
        return level;
    }
    bool last_blow_critical = false;
    /// The last blow's direction (from the player to the target, turned by
    /// `rollAttack`'s jitter), knock-back force and critical, for `die`.
    tl::core::KillingBlow last_killing_blow;
    /// `rollAttack`'s glancing flag (`applyDamage`'s 7th argument) and `blocked()`.
    bool last_blow_glancing = false;
    bool last_blow_blocked = false;
    /// `rollAttack` interrupted the target of the last blow.
    bool last_blow_interrupts = false;

    /// How much life a placed unit has, creating the entry the first time.
    Health& health_of(int64_t node, const std::string& unit);

    /// Take `amount` off a placed unit, killing it if that empties it. `owner`: its machine
    /// if known. `blow`: what `applyDamage` hands `die`. `struck`: `applyDamage`'s 8th
    /// argument, rolling `CAST SKILL ON STRUCK` (`LevelScript::damage_unit`).
    void hurt_unit(int64_t node, const std::string& unit, float amount, bool is_crit = false,
                   int64_t attacker = 0, tl::core::LevelScript* owner = nullptr,
                   const tl::core::KillingBlow* blow = nullptr, bool struck = false);
    /// The machine whose characters include `node` (the level's own first).
    tl::core::LevelScript* script_owning(int64_t node);
    /// A unit's animations in its manifest's order with their lengths; see
    /// `LevelScript::set_animation_catalog`.
    std::vector<std::pair<std::string, float>> animation_catalog(const std::string& unit);
    std::map<std::string, std::vector<std::pair<std::string, float>>> animation_catalogs;

    /// A placed unit's life has run out: fall over, stop hunting, stop being hunted.
    void kill_unit(int64_t node, const std::string& unit, bool fall = true, int64_t attacker = 0);
    /// A floor-rolled level (`unit_spawn_level`) handed to the machine's `setLevel`.
    void apply_spawn_level(int64_t node) {
        const auto found = unit_spawn_level.find(node);
        if (found != unit_spawn_level.end() && found->second.level > 0) {
            script.set_unit_level(node, found->second.level);
        }
    }

    /// Take `amount` off the player. `attacker` (`applyDamage`'s 6th argument, `kNoAttacker`
    /// none) is answered by reflection; `struck` rolls `CAST SKILL ON STRUCK` first.
    /// `shown`: add to the number over the character.
    void hurt_player(float amount, const std::string& from,
                     int64_t attacker = tl::core::LevelScript::kNoAttacker, bool struck = false,
                     bool shown = true);
    /// `CCharacter::applyDamage`, its tail: a
    /// living victim answers with `DAMAGE REFLECTION` (0x30) plus `PERCENT DAMAGE REFLECTED`
    /// (0x20), through the attacker's armour. Node 0 is the player.
    void reflect_damage(const tl::core::CharacterEffects& victim, int64_t victim_node,
                        const std::string& victim_name, float amount, int64_t attacker);
    int reflect_depth = 0;
    /// `CCharacter::applyDamageEffects` on a placed unit or
    /// the player (null): the elemental part may leave a `DAMAGEEFFECT*` affix.
    void apply_damage_status(tl::core::LevelScript* machine, int64_t node, float amount,
                             const std::array<int, tl::core::kDamageTypeCount>& by_type, int parts,
                             int first_type, bool forced, int attacker_level,
                             const tl::core::EffectSource& attacker);
    /// The character whose skill event is applying effects right now (an
    /// effect's `+0x48`): a placed unit, or with a null machine the player.
    tl::core::LevelScript* instant_caster_machine = nullptr;
    int64_t instant_caster_node = 0;
    /// The direction that event hands `applyAffixesAndEffects`
    /// for `CAffix::setDirection`; unset is `UNIT_Z`.
    bool effect_direction_set = false;
    std::array<float, 3> effect_direction{};
    /// `CCharacter::applyInstantEffect`, case
    /// `DAMAGE`, on the player.
    void player_instant_damage(const tl::core::Effect& effect);
    /// What a blow meets on the player: `AC()`, `maxHP()`, `getBlockChance()`
    /// and `damageDefense(type)`.
    tl::core::Defender player_defender();
    /// `CCharacter::modifyDamage` when a `SHIELD BUFFER` took the hit:
    /// `Absorbed!` over the character and `SHIELDBLOCK` (slot 7).
    void show_absorbed(const Ogre::Vector3& at, const tl::core::UnitStats& stats);
    /// The same over a placed unit, by node.
    void show_unit_absorbed(tl::core::LevelScript* machine, int64_t node);
    /// `attacker`: whoever dealt the last blow (`die`'s 2nd argument), for the
    /// `CAST SKILL ON DEATH` procs; `kNoAttacker` for none.
    void kill_player(int64_t attacker = tl::core::LevelScript::kNoAttacker);
    bool save_character(const std::string& path, bool force);
    void step_player_death(float delta);
    void open_death_screen();
    void refresh_death_screen();
    void choose_resurrection(tl::core::ResurrectChoice choice);
    void level_resetting_player(tl::core::ResurrectChoice choice);

    /// The nearest hostile placed unit within `reach` of the character, or 0.
    int64_t hostile_in_reach(float reach) const;

    /// How long the last load_layout() took, in seconds.
    float load_seconds = 0.0f;

    /// Layout files being loaded, by path, so a self-reference cannot recurse forever.
    std::set<std::string> layouts_in_progress;

    /// `--rng-trace`: where every seeding and draw of `g_Rand` (`core::game_random()`) goes.
    std::FILE* rng_trace_file = nullptr;
    tl::core::RandomTrace rng_trace;
    /// Takes the trace off `g_Rand`, which outlives this renderer.
    struct RngTraceGuard {
        ~RngTraceGuard() { tl::core::game_random().set_trace(nullptr); }
    } rng_trace_guard;
    /// `CResourceManager+0x42` (X360 `+0x26`): set by `CLevel::loadRoomLayout` around each
    /// room piece, making `CLayout::loadLayoutFile` reseed; clear at first.
    ///
    bool layout_seeding = false;
    int32_t layout_level_seed = 0;  ///< `CResourceManager::getCurrentLevelSeed()`
    /// `loadLayoutFile`'s static `mRecursiveFix` count (layouts being loaded)
    /// and `iChildCount`, reset whenever an outermost load starts.
    uint32_t layout_stack_depth = 0;
    uint32_t layout_child_count = 0;

    /// Wardrobe pieces worn on the subject, animated with the body (Renderer::equip()).
    std::vector<Ogre::Entity*> equipment;

    /// `CWardrobe` (core/wardrobe.h): the class's outfit (`wear_base_outfit`), each slot's
    /// entity, and what the slots last held; `wardrobe_ready` once the outfit is read.
    tl::core::WardrobeBase wardrobe_base;
    std::string wardrobe_class;
    std::string wardrobe_stem;
    bool wardrobe_ready = false;
    std::array<Ogre::Entity*, tl::core::kWardrobeSlotCount> worn_pieces{};
    std::string worn_signature;

    /// The main menu's debug wardrobe editor (render/renderer_wardrobe.cpp), the engine's
    /// own tool. A look is one way a slot can be drawn, with its first item and a count.
    struct WardrobeLook {
        const tl::core::ItemStats* item = nullptr;  ///< null: the class's own / an empty hand
        std::string mesh;
        std::string texture;
        int sharing = 1;
    };
    /// One row: a wardrobe slot (`tl::core::WardrobeSlot`), or -1 for the weapon.
    struct WardrobeRow {
        std::string label;
        int slot = -1;
        std::vector<WardrobeLook> looks;
        int index = 0;
    };
    struct WardrobeEditor {
        bool open = false;
        std::vector<WardrobeRow> rows;
        /// What the menu had before, put back on the way out.
        std::string previous_class;
        tl::core::Inventory saved_inventory;
        bool had_weapon = false;
        tl::core::ItemStats saved_weapon;
        Ogre::Vector3 camera_position = Ogre::Vector3::ZERO;
        Ogre::Quaternion camera_orientation = Ogre::Quaternion::IDENTITY;
        Ogre::Quaternion subject_orientation = Ogre::Quaternion::IDENTITY;
        /// The camera's distance from the character, as a share of the
        /// backdrop's own (1 = where the menu put it).
        float zoom = 1.0f;
    };
    WardrobeEditor wardrobe_editor;

    /// The main menu's debug bestiary (render/renderer_bestiary.cpp), the engine's own tool:
    /// every unit under `media/units/monsters/`, placed in turn where the menu's pet stands.
    struct BestiaryUnit {
        std::string name;    ///< `NAME`, what `spawn_unit()` takes
        std::string file;    ///< its `.DAT.adm`
        std::string folder;  ///< the file's directory under `media/units/monsters/`
    };
    struct Bestiary {
        bool open = false;
        /// Ordered by file path, so a folder's units are together.
        std::vector<BestiaryUnit> units;
        /// Each folder's first index into `units`, in order.
        std::vector<int> folders;
        int unit = 0;
        /// The `.SKELETON` stems beside the unit's mesh other than its own
        /// skeleton; the screen's 0 is the unit's idle as placed.
        std::vector<std::string> animations;
        int animation = 0;
        /// What stands there now: null when the unit could not be placed, and
        /// `status` says why.
        Ogre::Entity* entity = nullptr;
        int64_t id = 0;
        std::string mesh;
        std::string status;
        /// The spot, and the turn the right stick has added to its facing.
        Ogre::Vector3 where = Ogre::Vector3::ZERO;
        Ogre::Quaternion facing = Ogre::Quaternion::IDENTITY;
        float turn_degrees = 0.0f;
        /// The camera's distance as a share of the one that frames the whole
        /// unit (1).
        float zoom = 1.0f;
        /// What the menu had before, put back on the way out.
        Ogre::Vector3 camera_position = Ogre::Vector3::ZERO;
        Ogre::Quaternion camera_orientation = Ogre::Quaternion::IDENTITY;
        std::vector<Ogre::SceneNode*> hidden_pets;
    };
    Bestiary bestiary;
    void place_bestiary_unit();
    void play_bestiary_animation();
    void place_bestiary_camera();
    std::string bestiary_text() const;
    void refresh_bestiary_screen();
    std::vector<WardrobeLook> wardrobe_armour_looks(int slot);
    std::vector<WardrobeLook> wardrobe_weapon_looks();
    int wardrobe_weapon_location(const tl::core::ItemStats& item);
    void wear_wardrobe_row(const WardrobeRow& row);
    std::string wardrobe_editor_text() const;
    void redress_wardrobe_editor();
    void place_wardrobe_camera();
    /// Load a wardrobe mesh onto the subject (materials, skeleton, linked animations), pushed
    /// on `equipment`; null when it does not load.
    Ogre::Entity* attach_worn_mesh(const std::string& mesh_path);
    /// `set_render_behind()` on the subject and its wardrobe pieces (on its node, not its
    /// bones, so the entity's own walk misses them).
    void set_subject_render_behind(bool on, bool hostile);
    /// `CWardrobe::update(CInventory*)` for the player: five
    /// slots from outfit and worn items, the skin composited, each slot's submeshes; nothing
    /// when the slots are unchanged, unless `force`.
    void dress_player(bool force = false);

    /// Archive paths of the animation skeletons linked so far, for pieces equipped later.
    std::vector<std::string> linked_paths;

    /// The subject and everything worn on it (subject may be null before show_mesh()).
    std::vector<Ogre::Entity*> animated() const {
        std::vector<Ogre::Entity*> all;
        if (subject) all.push_back(subject);
        all.insert(all.end(), equipment.begin(), equipment.end());
        return all;
    }

    /// Re-parse the subject's material scripts even if cached: set only around a creation
    /// class swap, as every class declares `Material_#1/<slot>` (g_parse_counts).
    bool force_subject_materials = false;

    // -------------------------------------------------------------- the pet --

    /// The player's animal (core/pet.h); its behaviour is the level script's (a character of
    /// the level's machine). This side owns the model, the voice lines and the map icon.
    tl::core::Pet pet;

    Ogre::Entity* pet_entity = nullptr;
    Ogre::SceneNode* pet_node = nullptr;
    std::string pet_mesh;

    /// The pet's id in the level script (`kSpawnedNodeBase`), new with each model.
    int64_t pet_node_id = 0;

    /// Last frame's town flag and health, for the hide and the low-health line.
    bool pet_was_in_town = false;
    float pet_last_health = 0.0f;

    /// The pet has been reported missing (once).
    bool pet_reported = false;

    /// Put the chosen animal beside the character, a character of the level script's; false
    /// when the unit or mesh is missing.
    bool spawn_pet(const std::string& unit);

    /// The learned spells onto the script's pet (`learnSpell`).
    void give_pet_spells_to_script();

    /// Stand the animal beside the character, wherever it now is (`place_player_at()`).
    void place_pet_beside_player();

    /// Destroy the model and the script's character, for another animal or a level left.
    void destroy_pet_entity();

    /// Take a unit `spawn_unit()` placed out of every list and destroy its model, held items,
    /// node and particles; `id` from `spawn_unit()` (0: unknown). The script is not told.
    void destroy_spawned_unit(Ogre::Entity* entity, int64_t id);
    /// Every per-entity list `place_mesh()` and `spawn_unit()` used, emptied of the entity.
    void forget_unit_entity(Ogre::Entity* entity);
    /// The archive path of a unit's mesh (`UnitStats::mesh`); empty for none.
    std::string unit_mesh_path(const std::string& unit);
    /// The scene's half of `CCharacter::convertCharacter`:
    /// `deleteUnitSpecificData`, `unitInit`'s model for
    /// `unit` on the same node, then the `TRANSFORM` particle.
    void convert_unit_body(tl::core::LevelScript* machine, int64_t node, const std::string& unit);

    /// One frame of the pet: script mirror, the town hide, the voices and the icon.
    void step_pet();

    /// Fire the trigger tags this frame's step of `entity`'s animation passed (footfalls,
    /// swings, barks); `unit` supplies a bare tag's sound (`UnitStats::attack_sound`).
    void fire_animation_sounds(const std::string& mesh, const std::string& animation,
                               const tl::core::UnitStats& unit, float was, float now,
                               const Ogre::Vector3& at);

    /// Play the trigger tags every animated unit crossed this frame.
    void step_unit_sounds();

    /// Send the pet shopping (`CCharacter::sendToTown`); false when away or not allowed.
    bool send_pet_to_town();

    /// Copy what the level script says about the pet into `pet`.
    void sync_pet_mirror();

    /// `pet`'s stance onto the script's character, and its target dropped.
    void apply_pet_stance();

    /// Whether this level is the town, where the pet has nowhere to go.
    bool in_town() const;

    /// The pets that stood on the backdrop's `Point of Interest`, by unit name: kept hidden,
    /// not destroyed, as `scene_units`, `scene_animations` and `spawned` hold them.
    std::map<std::string, Ogre::Entity*> menu_pets;

    /// Where a main-menu backdrop's `Point of Interest` stands the pet and its turn; false
    /// without one.
    bool menu_pet_spot(Ogre::Vector3& where, Ogre::Quaternion& facing);

    /// The animation enabled and where it was parked, for equip() to match.
    std::string playing;
    float playing_time = 0.0f;

    Ogre::RTShader::ShaderGenerator* shader_generator = nullptr;
    std::unique_ptr<ShaderResolver> shader_resolver;
    GpuSkinning gpu_skinning;
    /// The GPU's own time over a sampled frame and its passes, for the fps line.
    GpuTimer gpu_timer;
    /// The light map's and the shadow map's render targets, or null.
    Ogre::RenderTarget* light_map_target() const {
        return light_map.texture ? light_map.texture->getBuffer()->getRenderTarget() : nullptr;
    }
    Ogre::RenderTarget* shadow_map_target() const {
        return shadow_map.texture ? shadow_map.texture->getBuffer()->getRenderTarget() : nullptr;
    }
    /// The characters' render-behind outline (set_render_behind()).
    std::unique_ptr<renderer_detail::RenderBehindMaterials> render_behind_materials;

    // Character preview for console inventory
    Ogre::TexturePtr preview_texture;
    Ogre::Camera* preview_camera = nullptr;
    Ogre::SceneNode* preview_camera_node = nullptr;
    Ogre::SceneNode* preview_node = nullptr;
    Ogre::Light* preview_light = nullptr;
    Ogre::SceneNode* preview_light_node = nullptr;
    Ogre::Entity* preview_subject = nullptr;
    Ogre::Entity* preview_weapon = nullptr;
    std::vector<Ogre::Entity*> preview_equipment;
    float preview_idle_time = 0.0f;

    void init_character_preview();
    void update_character_preview(float delta);
    void shutdown_character_preview();
    void update_menu();
    /// The Stats column (`CStatsXenon`): pending points, the selection.
    tl::core::StatsPage stats_page;
    /// `CStatsMenuXenon::vfunc_7` (`setOpen`): opens with the Stats tab,
    /// placing its pending points when it closes.
    void sync_stats_page();

    bool mount_resources();
    void setup_scene();
    void advance(float delta);
    bool place_layout(const std::string& reference, const Ogre::Vector3& origin,
                      const Ogre::Quaternion& basis, int depth, uint32_t load_seed_arg = 0);

    /// Build and place one procedurally-generated dungeon floor.
    bool place_dungeon_floor(const std::string& dungeon_name, int floor_number);
    /// `CLevel::populate(true)`: formations, counts, the five
    /// unit phases and gold. `placed_pieces` takes the formations placed.
    void populate_level(const tl::core::LevelTemplate& level_template, int level,
                        std::vector<std::pair<std::string, Ogre::Vector3>>& placed_pieces);
    /// One unit phase of `CLevel::populate`: monsters..
    /// props, sections picked on `g_Rand`, each a
    /// `populateSectionOfLevel`. `owner`: the characters' list; `label`: the log's word.
    struct PopulatePhase {
        int64_t owner = 0;
        const char* label = "";
        bool champion = false;      ///< `populateSectionOfLevel`'s fourth argument
        bool markers = false;       ///< its fifth: the NPCs stand on `CLevel +0xf0`
        bool empty_counts = false;  ///< an empty roll counts as one: all but the monsters
        bool props = false;         ///< the props phase, for the log
    };
    int populate_units(const std::string& spawn_class, uint32_t wanted, int level,
                       tl::core::Randomizer& sections, const PopulatePhase& phase);
    tl::core::Vec3 populate_open_position(const tl::core::Vec3& at, float radius);
    /// A unit's `MINION_SPAWNCLASS` rolled on `g_Rand` as the level takes it in.
    std::vector<std::string> roll_unit_minions(const std::string& unit, int level);
    /// ... and each of them made beside `master`, added to the level under
    /// `owner` and made its pet.
    void place_unit_minions(int64_t master, int64_t owner, const std::vector<std::string>& minions);
    /// Both, for a character already in the level.
    void spawn_unit_minions(int64_t master, int64_t owner);
    std::vector<tl::core::FormationChoice> formation_choices(const std::string& table);
    /// A `CREATEAS: TRIGGER` prop made outside a layout (`populate`'s chests:
    /// `CResourceManager::createUnit`, `CLevel::addItem`),
    /// as a one-node layout so it acts as a placed one. False for a unit
    /// that is not a trigger prop. `scale`: what making it drew (`CBaseUnit::unitInit`).
    bool place_populated_trigger(const std::string& unit, const Ogre::Vector3& at,
                                 const Ogre::Quaternion& facing, int64_t id, float scale = 1.0f);
    struct PopulatedTrigger {
        std::string unit;
        Ogre::Quaternion facing = Ogre::Quaternion::IDENTITY;
    };
    /// The trigger props `place_populated_trigger` made, by node (stored as created).
    std::map<int64_t, PopulatedTrigger> populated_triggers;
    /// The scale `populate` drew for a trigger prop, by its coming node; `place_layout`
    /// draws any other.
    std::map<int64_t, float> made_trigger_scale;
    /// `CGameClient::loadLevel`'s quest pass for a dungeon depth.
    void apply_floor_quests(const tl::core::DungeonDef& def, uint32_t depth);

    /// Find a `Property Node` of `wanted_type` (`"exit"` or `"entrance"`, without case) in a
    /// placed chunk's `.cmp` (`EXIT_NODE.LAYOUT` / `ENTRANCE_NODE.LAYOUT` via a `Layout Link`).
    /// What else the marker's layout says: its flattened forward (`CLevel +0x164`) and the
    /// `Warper` beside it.
    struct ChunkMarkerExtra {
        Ogre::Vector3 forward = Ogre::Vector3::UNIT_Z;
        bool has_warper = false;
        Ogre::Vector3 warper_position = Ogre::Vector3::ZERO;
        Ogre::Vector3 warper_forward = Ogre::Vector3::UNIT_Z;
    };
    bool find_chunk_marker(const std::string& chunk_cmp_path, const Ogre::Vector3& chunk_origin,
                           const std::string& wanted_type, Ogre::Vector3& world_position,
                           float& radius, ChunkMarkerExtra* extra = nullptr);

    void load_tileset();
    /// `group` tags the triangles so they can be switched off (`CollisionWorld::set_group()`);
    /// zero is the level's own. `piece_at`: the owning `Room Piece`'s position, making it a
    /// `CLevel::populate` section (`population_sections`).
    void add_collision_mesh(const std::string& path, const Ogre::Matrix4& to_world,
                            int64_t group = 0, const Ogre::Vector3* piece_at = nullptr);

    /// A collision mesh as add_collision_mesh() folds it in, in its own space, read once per
    /// level; each placement transforms its points with the same `Matrix4 * Vector3`.
    /// Defaults: `CCollisionModel::CCollisionModel`.
    struct CollisionModel {
        enum class State { Loaded, Threw, Missing } state = State::Missing;
        /// The load's exception, repeated for every placement as it was before.
        std::string error;
        struct Submesh {
            /// `tl::core::collision_material` of the submesh's name and index.
            uint32_t material = 0;
            std::vector<Ogre::Vector3> points;
            /// Three per triangle, each already checked against `points`.
            std::vector<uint32_t> corners;
        };
        std::vector<Submesh> submeshes;
        bool has_bounds = false;
        tl::core::Vec3 low, high;
    };
    /// By the path add_collision_mesh() was handed; per level.
    std::unordered_map<std::string, CollisionModel> collision_models;
    const CollisionModel& collision_model(const std::string& path);

    /// A layout file as parse_cmp() reads it, once per level. Null when it does not read; a
    /// parse failure is kept (`ok` false). By pointer, so a nested placement cannot move a
    /// layout its parent is walking.
    std::unordered_map<std::string, std::unique_ptr<const tl::formats::CmpScene>> layout_files;
    const tl::formats::CmpScene* layout_file(const std::string& path);

    /// How long a `SOUND` definition plays, in seconds; 0 when it names nothing.
    float sound_length(const std::string& name);

    /// `CGameUI::addTextEvent`: nothing with "Show Damage
    /// Info" off or out of the camera's sight.
    void add_text_event(const tl::core::TextEvent& event);
    /// One of the combat and loot lines over a unit standing at `unit_at`.
    void add_combat_text(tl::core::CombatText what, const Ogre::Vector3& unit_at, int amount = 0);
    /// Where a unit of the level stands; false when nothing draws it.
    bool unit_node_position(int64_t node, Ogre::Vector3& at);
    /// One of a character's own particles (`CCharacter +0x4a8`) put at `at`
    /// and started. Nothing where the slot is null.
    void start_character_particle(const tl::core::UnitStats& unit, int slot,
                                  const Ogre::Vector3& at);
    /// A unit as `CSkillManager::getPositionForSkill` reads
    /// one: centre, radius, facing. A null machine is the player.
    bool skill_position_unit(tl::core::LevelScript* machine, int64_t node,
                             tl::core::SkillPositionUnit& out);
    /// `CCharacter::castSkill`: the
    /// point a unit's position skill is cast at, into `unit_cast_caster`.
    void aim_unit_position_skill(tl::core::LevelScript& machine, int64_t node,
                                 const tl::core::KnownSkill& skill);
    /// `CEffect::playFX`: each asking effect's `PARTICLE_FX` 0.1 above `centre`,
    /// started. `who` is for the log.
    void play_effect_fx(tl::core::EffectManager& effects, const Ogre::Vector3& centre,
                        const std::string& who);
    /// `CCharacter::playHitParticle` on the
    /// unit struck.
    void play_hit_particle(const tl::core::UnitStats& victim, const Ogre::Vector3& from,
                           const Ogre::Vector3& at);
    /// `CCharacter::playStrikeParticle` of
    /// the attacker; `radius` is the struck unit's.
    void play_strike_particle(const tl::core::UnitStats& attacker, const Ogre::Vector3& from,
                              const Ogre::Vector3& at, float radius, bool critical, bool blocked);
    /// `CCharacter::die`: the particle at the body.
    void play_death_particle(const tl::core::UnitStats& unit, const Ogre::Vector3& at);
    /// What `rollAttack` shows of the player's blow besides
    /// the damage. `damage` is what `applyDamage` was handed.
    void show_player_blow(int64_t target, const std::string& unit, float damage);
    /// `CCharacter::awardFame` for the
    /// player, with its text.
    bool award_player_fame(int amount);
    void step_floaties(float delta);
    void step_pending_talk();

    /// The HUD and the minimap, out of the way of a scene or a conversation.
    void step_hud_visibility();
    bool hud_hidden_for_scene = false;
    bool hud_hidden_game_hud = false;
    bool hud_hidden_automap = false;

    /// Write the frame just drawn to the file a pad script's `shot=` asked for,
    /// if it asked (`tl::core::take_scripted_shot`).
    void save_scripted_shot();
    /// Beside a shot: the material, technique and passes each dropped item in
    /// view is drawn with.
    void log_dropped_item_materials();
    /// Beside a shot: each unit in view, its height over the ground and what
    /// it is playing.
    void log_shot_units();

    /// What the buttons act on: the unit the Xbox build's target selector picks.
    ///
    bool selected_unit(std::string& unit, bool& hostile, int64_t& node,
                       Ogre::Entity*& picked_entity);

    /// Whether this entity is still a placed or spawned unit (the highlight's raw pointer).
    bool entity_still_placed(const Ogre::Entity* entity) const;

    /// Run that pick for this frame, remember it and log it when it changes.
    /// `active` false clears it.
    bool update_pad_selection(bool active);

    /// Where a placed unit stands, by lower-cased substring (`ratlin`).
    bool unit_position(const std::string& name, Ogre::Vector3& at, std::string& found) const;
    /// The unit itself, by the same match.
    Ogre::Entity* find_unit_entity(const std::string& name, std::string& found) const;

    /// Block or unblock one `Path Node Occupation` region on the nav grid (nothing for
    /// another node). `nodes`: the switching machine's map. `live`: also clear characters off
    /// the cells (`clear_path_blocker_area`).
    void apply_path_blocker(int64_t node_id, bool on, const NodeMap* nodes = nullptr,
                            bool live = false);
    /// `CPropertyNode::killOrMoveBaseUnits()` over every
    /// machine: a box `width` by `length` about `centre`.
    void clear_path_blocker_area(const PathBlocker& b);
    bool path_blocker_of(const tl::core::LevelScript::NodeRecord& n, PathBlocker& b) const;
    bool path_blocker_of(const tl::formats::CmpNode& node, PathBlocker& b) const;
    size_t set_path_blocker_cells(const PathBlocker& b, bool on);
    /// Every blocker that is on taken off the map (a level reset).
    void lift_path_blockers();

    /// Apply every enabled pathing blocker the layout places.
    void apply_path_blockers(const tl::formats::CmpScene& layout, const std::set<int64_t>& gated);

    /// `CBaseUnit::addToAvoidanceMap` for every
    /// `ISA(INTERACTABLE)` `Unit Trigger` placed (`door_trigger_nodes`, drained): its collision
    /// model into the map grid; recorded in `door_avoidance_added` for `TriggerState`.
    void register_door_avoidance();

    /// Load the authored numbers of every unit this layout places.
    void load_unit_stats(const tl::formats::CmpScene& layout, const std::set<int64_t>& gated);

    /// Read one unit's authored numbers into the book; false when nothing is called that.
    bool index_unit(const std::string& name);

    /// This unit's authored numbers, or the defaults when nothing loaded them.
    tl::core::UnitStats stats_for_unit(const std::string& unit) const {
        if (const tl::core::UnitStats* found = unit_book.find(unit)) return *found;
        return tl::core::UnitStats{};
    }
    /// `CCharacter::alignment()` for a placed character:
    /// its machine's `+0x338` (`setAlignment`), else the data's.
    tl::core::UnitStats::Alignment unit_side(int64_t node, const tl::core::UnitStats& stats) const;
    /// `CCharacter::setAlignment`: red outline and highlight for a hostile
    /// (`+0x280` 2 or 4) unless a mimic (`ISA 0xa7`).
    bool outline_hostile(const tl::core::UnitStats& stats) const {
        return stats.hostile() && !unit_types.isa(stats.unittype, tl::core::unit_type::kMimic);
    }
    /// A character whose `+0x640` master is the player, in any machine.
    bool node_is_player_pet(int64_t node) const;
    // PORT: whether a unit is on the hostile side (`unit_side`).
    bool unit_hostile(int64_t node, const tl::core::UnitStats& stats) const {
        return unit_side(node, stats) == tl::core::UnitStats::Alignment::Hostile;
    }

    /// Index every monster unit by its `NAME`, once.
    void ensure_monster_index();

    /// Resolve a `Unit Spawner`'s `RESOURCE` to a unit name (a unit, or a `Spawn Class` table
    /// rolled on `g_Rand`); empty when it cannot.
    std::string resolve_spawn_resource(const std::string& group, const std::string& resource,
                                       int depth = 0, int32_t level = 0, uint32_t flags = 0);

    /// The spawn tables a roll reads and its context, read on first use.
    struct SpawnTables {
        std::map<std::string, tl::core::SpawnClass> classes;
        std::map<std::string, tl::core::SpawnUnitData> units;
        tl::core::SpawnContext context;
    };
    /// Fill `tables.context`. `items_by_type` backs a `UNITTYPE` pick with the item index
    /// (`CUnitResourceList::getDataGroupsByUnitType`).
    void build_spawn_context(SpawnTables& tables, bool items_by_type);
    /// The spawn class whose `NAME` is `name` (`spawn_class_paths`), parsed
    /// against `context`; empty when there is none.
    std::optional<tl::core::SpawnClass> load_spawn_class(const std::string& name,
                                                         const tl::core::SpawnContext& context);

    /// The spawn tables the drops roll (`drop_death_loot`), kept for the level.
    SpawnTables loot_tables;
    bool loot_tables_built = false;

    /// Put one unit into the scene at a point, remembered under `owner`; null when the unit or
    /// its mesh is missing. `level`: the rolled monster level (0: none, `character_level`).
    /// `champion`: resolved through `CHAMPIONSPAWNCLASS`, given `CCharacter::makeChampion`'s
    /// stats (`apply_champion_curves()`).
    Ogre::Entity* spawn_unit(const std::string& unit, const Ogre::Vector3& at, int64_t owner,
                             int level = 0, bool champion = false, bool spawn_anim = false,
                             const Ogre::Quaternion& facing = Ogre::Quaternion::IDENTITY,
                             float scale = 0.0f);
    /// The node of a unit the script made takes its drawn scale (`LevelScript::unit_scale`).
    void apply_unit_scale(int64_t node);

    /// Play a released unit's entrance, if it has one and the spawner asked
    /// (`UnitStats::spawn_anim`); whether anything started.
    bool begin_spawn_animation(Ogre::Entity* entity, const std::string& mesh,
                               const tl::core::UnitStats& stats);

    /// A `Unit Spawner` as the level sees it (core/unit_spawner.h): the renderer's half of
    /// `CUnitSpawner` for one spawner; its outputs go back to its machine.
    class SpawnerHost : public tl::core::UnitSpawnerHost {
    public:
        SpawnerHost(Impl& impl, tl::core::LevelScript* machine, int64_t node)
            : impl_(impl), machine_(machine), node_(node) {}
        int32_t level_monster_level() const override;
        tl::core::Vec3 random_open_position(const tl::core::Vec3& at, float radius,
                                            bool avoid_no_spawn) override;
        bool ray_collision(const tl::core::Vec3& from, const tl::core::Vec3& to,
                           tl::core::Vec3& normal) override;
        float range_random(float lo, float hi) override;
        tl::core::RunicRandom& volatile_random() override;
        void create_particles(const std::string& file, const tl::core::Vec3& at,
                              const tl::core::SpawnAxes& axes, uint32_t index) override;
        void fire_missile(const std::string& resource, const tl::core::Vec3& at,
                          const tl::core::SpawnAxes& axes) override;
        tl::core::SpawnClassPick roll_spawn_class(const std::string& name, int32_t level) override;
        tl::core::SpawnedUnitRef create_unit(const tl::core::SpawnRelease& release,
                                             const std::string& name, bool enchant) override;
        bool unit_exists(int64_t id) const override;
        bool character_alive(int64_t id) const override;
        void kill_character(int64_t id) override;
        void hide_character(int64_t id, bool hide) override;
        void run_particles(bool run) override;
        void output(uint32_t event) override { outputs_.push_back(event); }
        void unit_spawned(const tl::core::SpawnedUnitRef& unit) override;

        /// Hand the outputs raised so far to the spawner's machine.
        void deliver();

    private:
        Impl& impl_;
        tl::core::LevelScript* machine_;
        int64_t node_;
        std::vector<uint32_t> outputs_;
    };

    /// An input reaching a spawner (`CUnitSpawnerDescriptor::InputLogicEvent`).
    void spawner_input(const tl::core::ScriptEvent& event);
    /// `CUnitSpawnerDescriptor::update` for every machine's spawners.
    void step_spawners(float delta);
    /// The nodes a machine's events act on.
    NodeMap* nodes_of(tl::core::LevelScript* machine);
    /// A unit a spawner released: which spawner, for its `unitStateChange`.
    struct SpawnerUnit {
        tl::core::LevelScript* machine = nullptr;
        int64_t spawner = 0;
    };
    std::map<int64_t, SpawnerUnit> spawner_units;
    /// A `Particle` spawner's layouts (`CUnitSpawner +0x1d0`), by spawner and index.
    std::map<int64_t, std::map<uint32_t, std::vector<Ogre::SceneNode*>>> spawner_particles;
    /// Units released with `GIVE XP` clear (`+0x454` = 0).
    std::set<int64_t> no_xp_units;
    /// `CUnitSpawner::unitStateChange` for a released item:
    /// 3 when picked up.
    void spawned_item_state(int64_t id, int state);
    /// Ids for released items, from the same range as spawned characters.
    int64_t next_spawned_id();

    /// Index every prop unit by its `NAME`, once (`media/units/props/`, apart from monsters).
    void ensure_prop_index();
    std::map<std::string, std::string> prop_files;
    bool prop_files_loaded = false;

    /// A `CSkillEvent` hung on a skill event's damage shapes (`CSkillEvent::addListeners`),
    /// its hooks as callbacks.
    struct SkillEventListener final : tl::core::DamageShapeListener {
        std::function<bool(const tl::core::DamageShape&, const tl::core::DamageShapeUnit&)> can;
        std::function<bool(const tl::core::DamageShape&, const tl::core::DamageShapeUnit&)> act;
        /// `itemEffectedByDamageShape`; unset: nothing
        /// against the shape's own `interact`.
        std::function<bool(const tl::core::DamageShape&, const tl::core::DamageShapeItem&)> item;
        // `itemEffectedByDamageShape`,
        // through the listener's hook; unset, nothing against it.
        bool affect_item(const tl::core::DamageShape& shape,
                         const tl::core::DamageShapeItem& which) override {
            return !item || item(shape, which);
        }
        bool can_affect(const tl::core::DamageShape& shape,
                        const tl::core::DamageShapeUnit& who) override {
            return can && can(shape, who);
        }
        bool affect(const tl::core::DamageShape& shape,
                    const tl::core::DamageShapeUnit& who) override {
            return act && act(shape, who);
        }
    };
    std::vector<std::unique_ptr<SkillEventListener>> skill_event_listeners;

    /// Place a unit whose body is a particle layout, not a mesh (`LAYOUTFILE` and no
    /// `MESHFILE`, e.g. the phase beast's portal), which spawn_unit() cannot place.
    bool place_layout_bodied_unit(const std::string& unit, const Ogre::Vector3& at);
    /// A prop's `RESOURCEDIRECTORY` + `LAYOUTFILE` + `.LAYOUT`; empty for a
    /// prop with no layout body.
    std::string prop_layout_path(const std::string& unit);

    /// What a `Unit Trigger`'s `MODEL` (a unit, `cmp_field::unit_trigger`) is made of.
    /// `ANIMATION_STATE_n` is played once and held, not looped (`CTriggerUnit::setTriggerState`
    /// `blendAnimation(idx, false, 0.1, 1.0, -1.0)`); `_LOOP_` loops; a transition
    /// runs first.
    struct TriggerProp {
        std::string mesh;            ///< `RESOURCEDIRECTORY` + `MESHFILE`
        std::string collision;       ///< + `COLLISIONFILE`, when it has one
        std::string resting;         ///< `ANIMATION_STATE_1`, played once
        std::string resting_loop;    ///< `ANIMATION_STATE_LOOP_1`, looped
        std::string activated;       ///< `ANIMATION_STATE_2`, played once
        std::string activated_loop;  ///< `ANIMATION_STATE_LOOP_2`, looped
        std::string opening;         ///< `ANIMATION_TRANSITION_STATE_2`
        std::string closing;         ///< `ANIMATION_TRANSITION_STATE_1`
        /// `CTriggerUnit::unitInit`: `MAXSTATES` (2 unless
        /// authored), and `LOOPTYPE`'s index in `gTRIGGER_LOOP_TYPE_NAMES`, -1 for none.
        int32_t max_states = 2;
        int32_t loop_type = -1;
        /// `addSoundsToState`: `SOUND_STATE_ONE` / `_TWO`,
        /// else `INTERACT_SOUND`.
        std::string state_sound[2];
        /// A props unit file names it, so `setUnitModelName`
        /// runs `unitInit`; it may draw nothing.
        bool found = false;
        bool collideable = true;  ///< `COLLIDEABLE`
        /// `BLOCK` (`CBaseUnit +0x156`, `unitInit`): whether a ray meets the
        /// collision model (`CLevel::objectRayCollision`).
        bool block = false;
        /// `LAYOUT_STATE_1` and `LAYOUT_STATE_2` (`CTriggerUnit +0x2b0`).
        std::string state_layout[2];
        /// `OCCUPIESNODES`, `CBaseUnit +0x18f`: without it `addToAvoidanceMap`
        /// leaves the grid alone.
        bool occupies_nodes = true;
        /// `SHADOWS` (`CBaseUnit +0x1a9`, true by default).
        bool shadows = true;
        /// `CItem +0x210`, the floor label: `DISPLAYNAME`, `NAME` or `ITEM`
        /// (`CItem::unitInit`).
        std::string display_name;
        std::string unittype;  ///< `UNITTYPE`, for the label's two type tests
        /// `CTriggerUnit::unitInit`: `[TREASURE]`'s
        /// `SPAWNCLASS` (`+0x2d0`), `MINCOUNT`, `MAXCOUNT` (1 each; constructor).
        ///
        std::string treasure_class;
        int32_t treasure_min = 1;
        int32_t treasure_max = 1;
        /// `CBaseUnit::unitInit`: `[SKILL]` blocks in order.
        std::vector<std::string> skills;
        std::vector<uint32_t> skill_levels;
        /// `+0x194`, `COLLISION_RADIUS` (`CTriggerUnit::unitInit`), 0.3 by default
        /// (`CBaseUnit::CBaseUnit`): the trap is cast that far below.
        float collision_radius = 0.3f;
        /// `SCALE` (1) + `randomBetweenVolatile(0, SCALE_VARIATION)` (`CBaseUnit::unitInit`).
        ///
        float scale = 1.0f;
        float scale_variation = 0.0f;
    };

    /// Resolve a `Unit Trigger`'s `MODEL` to its geometry, or false when
    /// nothing under `media/units/props/` is called that.
    bool trigger_prop(const std::string& unit, TriggerProp& out);
    /// The layout half of `CTriggerUnit::setTriggerState`;
    /// `state` is 1 or 2.
    void set_trigger_state_layout(Ogre::Entity* entity, const TriggerProp& prop, int state);
    /// `CTriggerUnit::trigger` at a one-shot's last state:
    /// treasure and trap. `mine`: the firing machine's nodes (only its copy opens); empty,
    /// the first copy found.
    void finish_trigger_unit(int64_t node, const std::string& unit, const TriggerProp& prop,
                             bool by_player, const std::vector<const Ogre::SceneNode*>& mine,
                             bool treasure, bool skill);
    /// `unitInit`'s states for a new machine's `Unit
    /// Trigger`s (`LevelScript::init_unit_trigger`).
    void init_unit_triggers(tl::core::LevelScript& machine);
    std::map<std::string, TriggerProp> trigger_props;

    /// `CCharacter::die`'s skill half for a placed unit: `stopAllSkills(true, true, false)` and
    /// `CSkillManager::fireSkillsOnDeath`.
    void fire_unit_skills_on_death(tl::core::LevelScript& machine, int64_t node);
    void fire_unit_skills_on_create(tl::core::LevelScript& machine, int64_t node);
    /// A player skill's `[EVENT_*]` block as its layouts remember it: the `CSkillEvent` a
    /// `Missiles` spawner fires for (`CSkillEvent::missileApplyingEffects`).
    struct SkillEventSource {
        std::string skill;
        uint32_t level = 0;
        const tl::formats::AdmSection* event = nullptr;     ///< the block (`+0xa0`)
        const tl::formats::AdmSection* property = nullptr;  ///< its level (`+0x90`)
        /// That level's `DURATIONOVERRIDEMS` in seconds (`+0xa4`, inherited).
        float duration_override = 0.0f;
        tl::core::SkillTargetFilter unauthored = tl::core::SkillTargetFilter::Enemy;
        int32_t target_alignment = 2;  ///< the skill's `TARGET_ALIGNMENT`
        tl::core::SkillEventSpec spec;
        int group = 0;            ///< `+0x48`, the event's `EVENT_*` group
        int32_t target_type = 0;  ///< the level property's `TARGET_TYPE` (`+0x70`)
        /// The skill's owner when a placed unit; null is the player.
        tl::core::LevelScript* caster_machine = nullptr;
        int64_t caster_node = 0;
        /// The copy of the skill that fired the event (`SkillRunState::copy`).
        uint32_t run = 0;
    };
    /// The placed unit whose skill is running, set around its `SkillRunner` calls; null
    /// machine: the player.
    struct UnitEventCaster {
        tl::core::LevelScript* machine = nullptr;
        int64_t node = 0;
        Ogre::SceneNode* scene = nullptr;
        float radius = tl::core::kDefaultCollisionRadius;
    };
    UnitEventCaster unit_event_caster;
    /// `DAMAGE_REQUIRES_LOS` (`CSkillEvent +0x14a`): the level stands between caster and
    /// target (`machine` null: the player).
    bool skill_event_sight_blocked(const SkillEventSource& src, tl::core::LevelScript* machine,
                                   int64_t node);
    UnitEventCaster unit_event_caster_for(tl::core::LevelScript* machine, int64_t node);
    /// A placed unit's cast animation, stepped for its keys as the player's is
    /// (`step_skill_animation`).
    struct UnitSkillPlayback {
        tl::core::LevelScript* machine = nullptr;
        int64_t node = 0;
        std::string animation;
        int index = -1;
        float time = 0.0f;
        float length = 0.0f;
        float speed = 1.0f;
        float held = -1.0f;
        int then_loop = -1;
    };
    std::vector<UnitSkillPlayback> unit_skill_playbacks;
    /// Each unit model's animations in manifest order, by mesh.
    std::map<std::string, tl::core::AnimationCatalog> unit_catalogs;
    /// The caster a unit's `SkillCastContext` points at, kept alive here.
    tl::core::SkillCaster unit_cast_caster;
    /// The hooks every machine's characters cast through (`LevelScript::UnitSkills`).
    tl::core::LevelScript::UnitSkills unit_skill_hooks();
    /// `CBaseUnit::unitInit`: a unit's
    /// `[SKILL]` blocks.
    std::vector<tl::core::LevelScript::UnitSkill> unit_skill_list(const std::string& unit);
    const tl::core::AnimationCatalog* unit_catalog(const std::string& mesh);
    /// `CGenericModel +0x1cc`'s list for a mesh as `loadAnimations` builds it from the
    /// `.ANIMATION` file (`tl::core::load_animation_list`); `missing` counts absent files.
    tl::core::AnimationCatalog model_catalog(const std::string& mesh, const std::string& dir,
                                             size_t* missing = nullptr);
    tl::core::SkillCaster unit_caster(tl::core::LevelScript& machine, int64_t node,
                                      tl::core::LevelScript* target_machine, int64_t target_node,
                                      bool has_target);
    tl::core::SkillCastContext unit_cast_context(tl::core::LevelScript& machine, int64_t node,
                                                 tl::core::LevelScript* target_machine,
                                                 int64_t target_node, bool has_target);
    /// `CCharacter::castSkill`'s `performSkill`, mana and animation for a unit.
    bool cast_unit_skill(tl::core::LevelScript& machine, int64_t node,
                         tl::core::LevelScript* target_machine, int64_t target_node,
                         bool has_target);
    /// Every placed unit's cast: its animation's keys, `updateActiveSkill` and
    /// its manager's `update`.
    void step_unit_skills(float delta);
    /// A placed unit's skill by name, for its runner's lookups.
    const tl::core::KnownSkill* unit_skill_named(tl::core::LevelScript& machine, int64_t node,
                                                 const std::string& name);
    /// The entity a machine's unit is drawn with, or null.
    SceneUnit* scene_unit_of(tl::core::LevelScript* machine, int64_t node);
    /// Every `Unit Spawner` of a placed skill layout by node id (the file's own, so the latest
    /// cast's is kept), with its event and shape.
    struct SkillSpawner {
        SkillEventSource source;
        tl::core::SpawnerShape shape;
    };
    std::map<int64_t, SkillSpawner> skill_spawners;
    /// `CSkillEvent::startEvent`, the layout half: `FILE`
    /// on the caster (or target, `PLACEONTARGET`) facing its way, timelines played.
    void start_skill_event(const tl::core::KnownSkill& skill, uint32_t level, int group,
                           const tl::formats::AdmSection& event,
                           const tl::core::SkillEventTarget& target);
    /// A character `findCharacterWithinView` can answer: the player, or a
    /// machine's node.
    struct ViewUnit {
        bool player = false;
        tl::core::LevelScript* machine = nullptr;
        int64_t node = 0;
        /// A breakable by id when nonzero (`SkillEventTarget::item`).
        int64_t item = 0;
    };
    /// A player skill's `CSkillEvent` while it lives (`CSkill +0xb8`): its layout's nested
    /// machines, `first`..`end`.
    struct LiveSkillEvent {
        std::string skill;
        /// The copy of the skill that started it (`SkillRunState::copy`).
        uint32_t run = 0;
        size_t first = 0;
        size_t end = 0;
        /// `ATTACHOWNER`: the caster follows `shape` (`+0x40`, the first damage shape) of the
        /// layout placed at `at`, `facing`.
        bool attach_owner = false;
        bool ignores_units = false;
        bool ignores_obstructions = false;
        bool scale_distance = false;
        tl::core::LevelScript* shape_machine = nullptr;
        int64_t shape = 0;
        Ogre::Vector3 at = Ogre::Vector3::ZERO;
        Ogre::Quaternion facing = Ogre::Quaternion::IDENTITY;
        /// `+0x1fc`: origin to target on the ground, and `+0x160`, its length (within range).
        tl::core::Vec3 to_target{};
        float distance = 0.0f;
        int32_t skill_target_type = 0;
        /// Who cast it (`+0x50`): a placed unit, or the player (null machine).
        tl::core::LevelScript* caster_machine = nullptr;
        int64_t caster_node = 0;
        /// The skill loops (`CSkill +0xec` not -1): `stopAllParticles`' flag.
        bool loop = false;
        /// The particle systems its layout placed, by the nodes they hang from.
        std::vector<Ogre::SceneNode*> holders;
        /// Still in the skill's list (`CSkill +0xc0`); `stopSkill` resets it either way.
        bool listed = true;
        /// `updateEvent`'s line half: the systems with a `Line` emitter (`+0xa8`), driven with
        /// `ATTACH_LINE_EMITTER` (+0x147) or `..._TO_MAX_DISTANCE` (+0x148).
        std::vector<Ogre::SceneNode*> line_holders;
        bool attach_line_emitter = false;
        bool to_max_distance = false;
        bool collides = false;  ///< +0x149 `LINE_EMITTER_COLLIDES`
        /// `ATTACHTOTARGET` with no target or +0x148: the layout follows `far_point`.
        bool far_target = false;
        Ogre::Vector3 far_point = Ogre::Vector3::ZERO;
        int fire_from = 0;          ///< +0x108 `FIREFROMBONE`
        int fire_at = 0;            ///< +0x10c `FIREATBONE`
        float range = 0.0f;         ///< `CSkill::getRange` at the run's level
        float target_ahead = 0.0f;  ///< +0x164 `TARGET_AHEAD_OF_UNIT`
        /// The owner whose bones the line leaves from (`getOwnerCharacter`), and the target
        /// (`+0x80`).
        ViewUnit owner;
        bool has_target = false;
        ViewUnit target;
        bool line_reported = false;
    };
    /// `CSkillEvent::updateEvent`, the `ATTACHOWNER` half,
    /// once a frame.
    void update_live_skill_events(float delta);
    /// `updateEvent`, the `ATTACHTOTARGET` and line halves.
    void update_skill_event_lines(LiveSkillEvent& event);
    /// `CSkillEvent::getBonePosition(unit, attachment, true)`
    /// in the world; false when the unit is gone.
    bool skill_bone_world(const ViewUnit& unit, int attachment, Ogre::Vector3& out);
    /// The unit's facing on the ground (vtable +0x130). False when it is gone.
    bool skill_unit_forward(const ViewUnit& unit, Ogre::Vector3& out);
    /// A placed character's `CEffectManager` (`+0x1b8`), made on first use (owner the player,
    /// gate `unit_effect_on_unit`).
    tl::core::EffectManager* unit_effect_manager(tl::core::LevelScript* machine, int64_t node);
    /// `CCharacter::applyEffectOnUnit` for a placed
    /// character; true copies the effect in.
    bool unit_effect_on_unit(tl::core::LevelScript* machine, int64_t node,
                             tl::core::Effect& effect);
    /// `CCharacter::applyInstantEffect` for a
    /// placed character.
    void unit_instant_effect(tl::core::LevelScript* machine, int64_t node,
                             const tl::core::Effect& effect, float value);
    /// `updateEffects`' `KNOCK BACK EFFECT` case (`CCharacter::updateEffects`;
    /// `updateHP`): this frame's push, forces
    /// decayed; `resistance` is `PERCENT KNOCK BACK RESISTANCE` (0x35).
    tl::core::Vec3 knock_back_push(tl::core::EffectManager& effects, float resistance, float dt,
                                   const std::string& who);
    void update_unit_effects(float dt);
    /// `CCharacter::moveToPosition` for the player.
    bool move_player_to(const Ogre::Vector3& to, bool ignore_units, bool ignore_obstructions);
    bool move_unit_to(tl::core::LevelScript* machine, int64_t node, const Ogre::Vector3& to,
                      bool ignore_units, bool ignore_obstructions);
    std::vector<LiveSkillEvent> live_skill_events;
    /// `CSkillEvent +0x15c`: what is left of `DURATIONOVERRIDEMS` (`+0x158`), set by
    /// `startEvent`, run down by `updateEvent`;
    /// `canStop` waits on it.
    struct SkillEventHold {
        std::string skill;
        uint32_t run = 0;
        tl::core::LevelScript* caster_machine = nullptr;
        int64_t caster_node = 0;
        float left = 0.0f;
    };
    std::vector<SkillEventHold> skill_event_holds;
    /// `CSkillEvent::shouldStop`, negated, over one copy's
    /// live events (`SkillRunState::copy`).
    bool skill_events_alive(const std::string& skill, uint32_t run,
                            tl::core::LevelScript* caster_machine, int64_t caster_node);
    /// `CSkillEvent::canStop` over them, for
    /// `CSkill::attemptToStopSkill`.
    bool skill_events_can_stop(const std::string& skill, uint32_t run,
                               tl::core::LevelScript* caster_machine, int64_t caster_node);
    /// `CSkill::stopSkill`'s `resetEvent` over
    /// the copy's events: `CSkillEvent::stopEvent`, and
    /// `CSkillEvent::reset` detaching flying missiles
    /// (`FlyingMissile::detached`).
    void detach_event_missiles(const std::string& skill, uint32_t run,
                               const tl::formats::AdmSection& block,
                               tl::core::LevelScript* caster_machine, int64_t caster_node);
    /// A placed item's collision group and avoidance entry switched (`TriggerCollidable`,
    /// `ENABLECOLLISION` / `DISABLECOLLISION`).
    void set_trigger_collidable(int64_t key, bool on);
    bool skill_event_particles_alive(const LiveSkillEvent& event) const;
    bool level_stores_unit(int64_t id);
    void reset_skill_events(const std::string& skill, uint32_t run,
                            tl::core::LevelScript* caster_machine, int64_t caster_node);
    /// `CSkillEvent::addListeners`, the damage-shape half:
    /// shapes of machines `first`.. take `TARGET_CORPSES` and `MAX_UNITS_HIT` and report.
    void hang_player_skill_event(const SkillEventSource& source, size_t first);
    /// `CSkillEvent::applyEffects` on one character (null
    /// machine: the player); whether anything was applied. (`CSkillEvent::invokeHitSkills`,
    /// is apart.)
    bool skill_event_effects_on(const SkillEventSource& source, tl::core::LevelScript* machine,
                                int64_t node);
    /// `CSkillEvent::applyWeaponDamage` on a placed
    /// character: `WEAPONDAMAGEPCT` of the player's blow; the damage dealt.
    float skill_event_weapon_blow(const SkillEventSource& source, tl::core::LevelScript* machine,
                                  int64_t node, float damage_scale, float soak_scale);
    /// A skill event striking one character (`characterEffectedByDamageShape`,
    /// `startEvent`'s `APPLYEFFECTS`).
    bool skill_event_strike(const SkillEventSource& source, tl::core::LevelScript* machine,
                            int64_t node);
    /// `CLevel::findCharacterWithinView` from `at` turned by
    /// `facing`, optionally for a skill (`CSkill::getTargetIsValid`); true with a unit.
    bool find_character_within_view(const tl::core::Vec3& at, const Ogre::Quaternion& facing,
                                    float near, float angle, float range, int32_t alignment,
                                    bool nearest, const ViewUnit& ignore, ViewUnit& found,
                                    const std::function<bool(const ViewUnit&)>& valid = nullptr);
    /// `CCharacter::castSkill`: a
    /// targetless cast with `FINDTARGETANGLE` takes the nearest valid character in the cone,
    /// else the first breakable (`findItemWithinView`).
    void find_player_skill_target(const tl::core::KnownSkill& skill);
    /// `CSkill::getTargetIsValid` for a player skill.
    bool player_skill_target_valid(const tl::core::KnownSkill& skill, const ViewUnit& unit);
    /// The standing breakable with this id, by index; -1 for none.
    int breakable_index(int64_t id) const;
    /// Skills a player skill's events executed (`EXECUTE_SKILL`), added hidden by
    /// `CSkillManager::addSkill`, by upper-cased name.
    std::map<std::string, tl::core::KnownSkill> executed_skills;
    const tl::core::KnownSkill* add_executed_skill(const std::string& name, uint32_t level);
    /// The player's skill of that name: the class's, else an executed one.
    const tl::core::KnownSkill* player_skill(const std::string& name);
    /// While another unit's executed skill starts: its node, which the layouts ride.
    Ogre::SceneNode* event_caster_override = nullptr;
    /// The same unit by machine and node.
    tl::core::LevelScript* event_caster_override_machine = nullptr;
    int64_t event_caster_override_node = 0;
    /// `CSkillEvent::startEvent`'s tail: each `EXECUTE_SKILL`
    /// via `CExecuteSkillProps::executeSkill`.
    void execute_event_skills(const SkillEventSource& source, const tl::core::SkillEventSpec& spec,
                              const ViewUnit* unit, const tl::core::Vec3& origin,
                              const Ogre::Quaternion& facing, const tl::core::Vec3& point);
    void invoke_hit_skills(const SkillEventSource& source, int group,
                           tl::core::LevelScript* machine, int64_t node);
    /// `CLevel +0x200`: who watches which unit (core/unit_observers.h); per level.
    tl::core::UnitObservers unit_observers;
    /// `CSkill`'s `iUnitObserver` half: one copy of a caster's skill (null machine: player's)
    /// listening for a death its effect may cause.
    struct SkillObserver final : tl::core::UnitObserver {
        Impl* impl = nullptr;
        tl::core::LevelScript* caster_machine = nullptr;
        int64_t caster_node = 0;
        std::string skill;
        uint32_t run = 0;
        void unit_state_change(const void* machine, int64_t unit, int state) override;
    };
    std::map<std::tuple<tl::core::LevelScript*, int64_t, std::string, uint32_t>,
             std::unique_ptr<SkillObserver>>
        skill_observers;
    /// The observer for one copy of a caster's skill, made on first asking.
    SkillObserver* skill_observer_for(tl::core::LevelScript* caster_machine, int64_t caster_node,
                                      const std::string& skill, uint32_t run);
    /// `CSkill::unitStateChange`.
    void skill_unit_state_change(SkillObserver& observer, tl::core::LevelScript* machine,
                                 int64_t node, int state);
    /// Set while the player's `updateHP` takes effect damage: death broadcasts 4, then 1.
    bool player_hurt_by_effects = false;
    /// Who the possibly-lethal effects credit, while `player_hurt_by_effects` is set.
    int64_t player_effect_killer = tl::core::LevelScript::kNoAttacker;
    /// `CSkillEvent::unitSpawnedFromUnitSpawner` for a
    /// character a player skill's spawner released.
    void skill_unit_spawned(const SkillEventSource& source, tl::core::LevelScript& machine,
                            int64_t node);
    /// `CCharacter::addPet` on the player: past
    /// `MAXIMUM_PET_INSTANCES` the oldest of the name die, then `setMaster`.
    void add_player_pet(tl::core::LevelScript& machine, int64_t node);
    /// The same with a placed master (a monster's `MAKEPET` summon).
    void add_monster_pet(tl::core::LevelScript& machine, int64_t node,
                         tl::core::LevelScript& master_machine, int64_t master_node);
    /// The compiled layout at `reference`, parsed once (via layout_file()).
    const tl::formats::CmpScene* cached_layout(const std::string& reference);
    std::map<std::string, const tl::formats::CmpScene*> cached_layouts;

    /// One missile in flight (`core/missile.h`) and what draws it.
    struct FlyingMissile {
        tl::core::Missile missile;
        SkillEventSource source;
        /// Fired by a spawner of the level's own layout (a trap): no owner or listener.
        bool level_placed = false;
        /// Fired by the player's weapon (`CEquipment::fireMissiles`): the weapon listens.
        bool by_weapon = false;
        /// Its event restarted (`CSkillEvent::reset`): no listener left, so
        /// `doDamageToCharacter`'s own blow and no death event.
        bool detached = false;
        Ogre::SceneNode* node = nullptr;  ///< carries `ACTIVE`
        std::vector<Ogre::SceneNode*> active;
    };
    /// A deque: a missile's blow can fire more (`step_missiles` holds one by reference).
    std::deque<FlyingMissile> missiles;
    /// Particle layouts placed for a moment (a missile's `RELEASE`, `HIT`, `DIE`):
    /// `CMissile::killMissile` gives them 1.0 s (`+0x1b4`),
    /// then `handleDeathOfMissile` stops each.
    struct TransientParticles {
        std::vector<Ogre::SceneNode*> holders;
        float left = 0.0f;
        bool stopped = false;
        /// A character's own particle: taken down once its layout's timer stopped it.
        bool self_timed = false;
    };
    std::vector<TransientParticles> transient_particles;
    /// `CUnitSpawner::createAndFireMissile` /
    /// `CMissile::fireMissile`: launched from `at` along
    /// `forward`, drawn, kept in `missiles`. `who` names the firer in the log.
    void launch_missile(FlyingMissile flying, const Ogre::Vector3& at, const Ogre::Vector3& forward,
                        const std::string& who);
    /// `CEquipment::fireMissiles(owner, target)` for the
    /// player's weapon (needs a `MISSILE`); `target` a character, `breakable` an index (-1).
    bool fire_weapon_missiles(int64_t target, int breakable);
    /// The same for a placed character's weapon, from its hand (`left`), at `target_node` of
    /// `target_machine` when `aimed`, else along its facing.
    void fire_unit_weapon_missile(tl::core::LevelScript* machine, int64_t node,
                                  const std::string& missile, bool left, bool aimed,
                                  tl::core::LevelScript* target_machine, int64_t target_node);

    // ---- procs and a weapon's own skills (`core/weapon_procs.h`) ----------
    /// The player's known list as procs ask of it (`CSkillManager +0x60`): what
    /// `CBaseUnit::activateEffect` taught; one copy per
    /// name. `CCharacter::deactivateEffect`'s delete is
    /// not carried.
    tl::core::SkillCopies player_proc_skills{true};
    /// `CCharacter +0x708`: 1.0 when an own proc other than `CAST SKILL AT TARGET` casts
    /// (`executeProcs`), run down by `CCharacter::update`,
    /// waited out by `CAST SKILL ON STRUCK`.
    float player_proc_lockout = 0.0f;
    /// The skill files by name, for `SkillCopies::add`.
    tl::core::SkillFileLookup skill_file_lookup();
    /// What a proc or weapon skill is cast at: a character, a breakable (by id), or nothing,
    /// with its centre and radius taken when made (a breakable's procs come after it broke).
    struct ProcTarget {
        tl::core::LevelScript* machine = nullptr;
        int64_t node = 0;
        int64_t breakable_id = 0;
        bool placed = false;
        tl::core::Vec3 centre{};
        float radius = 0.0f;
        bool any() const { return placed; }
    };
    ProcTarget proc_target_unit(int64_t node);
    ProcTarget proc_target_breakable(int index);
    /// `CCharacter::executeProcs(type, target)` for the
    /// player: its own effects, then its worn items'.
    void player_execute_procs(int type, const ProcTarget& target);
    /// `CCharacter::doWeaponSkill(weapon)`: true when a
    /// skill started, which ends the blow.
    bool player_weapon_skill(const ProcTarget& target);
    /// `CCharacter::doWeaponProcs(weapon, struck)`.
    void player_weapon_procs(const ProcTarget& struck);
    /// The procs' target for an attacker id: a placed unit, or none.
    ProcTarget proc_target_attacker(int64_t attacker);

    /// `CBaseUnit::activateEffect` on a placed unit: a
    /// cast-skill effect adds its skill to the unit's list.
    void unit_activate_effect(tl::core::LevelScript* machine, int64_t node,
                              tl::core::Effect& effect);
    /// `CCharacter::executeProcs(type, target)`, placed unit.
    void unit_execute_procs(tl::core::LevelScript& machine, int64_t node, int type,
                            tl::core::LevelScript* target_machine, int64_t target_node,
                            bool has_target);
    /// `CSkillManager::executeSkill(skill, player, ...)` on
    /// the holder's manager: the player's (`location` -1) or the worn item's
    /// (`worn_skill_runners`, `+0x1c8`; `doWeaponSkill`, `doWeaponProcs`,
    /// `CInventory::executeProcs`). INVENTED: a bag item runs on the player's.
    bool run_proc_skill(const tl::core::SkillCopy& skill, const tl::core::Vec3& start,
                        const tl::core::Vec3& aim, const ProcTarget& target, const char* why,
                        int location = -1);
    /// `CEquipment::missileApplyingEffects`: the wielder's
    /// `rollAttack` on what its missile met.
    bool weapon_missile_harm(int64_t unit, const Ogre::Vector3& at, float damage_scale,
                             float soak_scale);
    /// The same for a placed character's weapon: `getCharacterCanBeHarmedByMissile`
    /// (`isEnemy(wielder, unit)`), then `rollAttack`.
    bool unit_weapon_missile_harm(tl::core::LevelScript* wielder, int64_t wielder_node,
                                  int64_t unit, const Ogre::Vector3& at, float damage_scale,
                                  float soak_scale);
    void fire_spawner_missile(int64_t node, const std::string& resource, const Ogre::Vector3& at,
                              const Ogre::Vector3& forward);
    void step_missiles(float delta);
    void clear_missiles();
    /// `CMissilePreloader`'s missiles by their upper-cased name, built once.
    std::map<std::string, const tl::formats::CmpNode*> missiles_by_name;
    bool missiles_indexed = false;
    const tl::formats::CmpNode* missile_named(const std::string& name);
    bool level_missile_harm(const tl::core::MissileSpec& spec, int64_t unit,
                            const Ogre::Vector3& at, float damage_scale,
                            const tl::core::Vec3& push);
    /// `CSkillEvent::missileApplyingEffects` behind
    /// `getCharacterCanBeHarmedByMissile`: true when the missile stops on `unit`.
    bool missile_harm(const SkillEventSource& source, int64_t unit, float soak_scale,
                      const Ogre::Vector3& at, float damage_scale, float yaw);
    /// `CSkillEvent::canEffectPosObject(at, unit)`: centre
    /// within 3.0 of `at` in height; null machine is the player.
    bool skill_event_reaches_height(const Ogre::Vector3& at, tl::core::LevelScript* machine,
                                    int64_t node);
    /// `missileApplyingEffects`' `CSkill::triggerEvent(6, ...)`: `EVENT_MISSILEHIT`
    /// on the firing copy, the struck unit its target.
    void fire_missile_hit_event(const SkillEventSource& source, tl::core::LevelScript* machine,
                                int64_t node, const Ogre::Vector3& at, float yaw);
    /// A particle layout placed at `at` (or riding `parent`), and its systems' nodes.
    std::vector<Ogre::SceneNode*> place_particle_layout(const std::string& reference,
                                                        const Ogre::Vector3& at,
                                                        const Ogre::Quaternion& facing,
                                                        Ogre::SceneNode* parent,
                                                        ParticleStart start = ParticleStart::Now);
    void destroy_particle_holders(std::vector<Ogre::SceneNode*>& holders);
    /// `CBaseUnit::sphereCollision` for a drawn unit: the
    /// first `t` in [0, 1] its collision model is touched, above 1 if not; false if not drawn.
    bool unit_collider_sweep(int64_t id, const tl::core::Vec3& from, const tl::core::Vec3& to,
                             float radius, float& t);
    /// A character's collision model (`CCharacter +0x4c8`): `media/models/collider.mesh`,
    /// what `loadModel` gives units with no `COLLISIONFILE`.
    tl::core::CollisionWorld unit_collider;
    bool unit_collider_read = false;

    void follow_subject();
    /// `CCameraControl::updateGameCamera` for the engine's default framing.
    void update_game_camera(const Ogre::Vector3& player_at);
    void step_player(float delta, const tl::core::InputState& in);
    /// The cast or the swing holds the player where they stand: see step_player().
    bool player_motion_held();
    void drive_player(float delta);
    Ogre::Entity* place_mesh(const MeshPlacement& placement);
    /// `CLevel::bake`'s room-piece batching, once the level and its
    /// remembered state are in place.
    void bake_static_scenery();

    /// Carry out what a level script decided, and log it; `nodes` is the map the events
    /// address.
    void apply_script_events(const std::vector<tl::core::ScriptEvent>& events, NodeMap& nodes);

    /// One frame of the level script, from wherever the character is.
    void step_script(float delta);

    // The level remembered and put back.

    /// A machine and its node map: the level's own, then every nested one in build order.
    struct MachineView {
        tl::core::LevelScript* script = nullptr;
        NodeMap* nodes = nullptr;
        uint32_t scene = 0;
        int64_t hierarchy = 0;
    };
    std::vector<MachineView> machines();
    /// A `Cinematic` node's `[CINEMATIC]`, opened by the run loop.
    std::string pending_layout_cinematic;

    /// The level being built's dungeon and depth, and whether it is remembered (`CLevel+600`).
    void set_level_identity(const std::string& dungeon, int32_t depth, bool is_town,
                            bool dont_store, bool allows_portals);

    /// `CGameGlobals +0x2c0 .. +0x2c8` (`reload`).
    tl::core::LevelMemoryRules level_memory_rules();

    /// Increment a journal statistic on the player and log it (except steps).
    void count_journal(tl::core::JournalStatistic stat, int32_t amount = 1) {
        if (amount == 0) return;
        player_character.journal().increment(stat, amount);
        if (stat != tl::core::JournalStatistic::StepsTaken) {
            const int32_t total = player_character.journal().value(stat);
            TL_LOG_INFO("journal: %s +%d = %d", tl::core::journal_statistic_name(stat), amount,
                        total);
        }
    }

    /// `CPlayer::storeLevelSavedState` over this level.
    void store_level_state();

    /// `CGameClient::restoreLevelState`, if remembered.
    void restore_level_state();

    /// Put the level back as it loaded: nothing hidden, no camera held, triggers unspent.
    void reset_script();
    /// `CParticle::Start`'s timer over
    /// `particle_systems[first, last)`: largest `LIFE TIME`, else largest finite emitter
    /// window, none if any never expires; plus 0.1.
    void arm_particle_timer(size_t first, size_t last);
    bool place_particle(const tl::formats::CmpScene& layout, const tl::formats::CmpNode& node,
                        const Ogre::Vector3& position, const Ogre::Quaternion& orientation);
    /// Whether this layout's particles are drawn (`RendererConfig::pu_particle_layouts`).
    bool particle_universe_takes(const std::string& layout_path) const;
    /// One `Particle` node as Torchlight's ParticleUniverse system, started as
    /// `CParticle::Start` starts it.
    bool place_torchlight_particle(const tl::formats::CmpScene& layout,
                                   const tl::formats::CmpNode& node, const Ogre::Vector3& position,
                                   const Ogre::Quaternion& orientation);
    /// `CParticle::updateLevelObject` for a system.
    void step_torchlight_particle(ParticleSystem& system, float delta);
    void step_particles(float delta);

    /// One frame of the idle fidgets: see `Impl::Fidget`.
    void step_fidgets(float delta, float world_delta);

    /// Start a fidget on one entity, or say there is nothing to start.
    bool begin_fidget(Fidget& fidget, Ogre::Entity* entity, const std::string& dir, bool is_subject,
                      const std::string& mesh, bool armed);
    /// `started`: the state enabled, the only way to switch that animation off later.
    /// `played`: the animation class actually started (RUN/WALK may substitute).
    /// `blend`: 0 plays outright (`CGenericModel::playAnimation`), else cross-fades
    /// (`blendAnimation`). `queue`: waits behind what plays (`queueBlendAnimation`).
    bool play_scene_animation(Ogre::Entity* entity, const std::string& mesh_path,
                              const std::string& animation, bool loops, float rate,
                              bool optional = false, Ogre::AnimationState** started = nullptr,
                              bool armed = true, std::string* played = nullptr, float blend = 0.0f,
                              bool queue = false);

    /// How long a placed unit's animation runs (`CGenericModel::getAnimationLengthSeconds`);
    /// negative when missing, 0 for a pose.
    float scene_animation_length(const std::string& unit, const std::string& animation);

    /// Hand a level script everything it asks the renderer for.
    void wire_script(tl::core::LevelScript& machine);

    /// Append what a machine traced to `--logic-trace`.
    void flush_logic_trace(tl::core::LevelScript& machine) {
        if (logic_trace == nullptr) return;
        for (const std::string& line : machine.take_trace()) {
            std::fputs(line.c_str(), logic_trace);
            std::fputc('\n', logic_trace);
        }
        std::fflush(logic_trace);
    }
    void aim_camera(const tl::formats::CmpScene& scene, const Ogre::Vector3& origin);
    bool ground_under(const Ogre::Vector3& from, float& surface_y) const;
    void stand_subject_at(const tl::formats::CmpNode& start, const Ogre::Vector3& origin);
    std::string rules_path_for(const std::string& layout_path);
    /// The rules file each main-menu backdrop was chosen from (`Renderer::menu_layout_for`),
    /// by `backdrop_key` of its layout (file names do not lead back; `CLevel::loadRoomLayout`).
    std::map<std::string, std::string> backdrop_rules;
    /// The layout is a main-menu backdrop (`/MAINMENUS/`), loaded via
    /// `CGameClient::loadMenuLevel`: `CLevel::bake` adds its `Light`s to
    /// `stgeomLight_`, but only `CGameClient::loadLevel`'s pass
    /// builds it, so a menu's layout lights never reach the light map.
    bool loading_menu_level = false;
    static std::string backdrop_key(const std::string& layout_path);
    bool read_rules(const std::string& layout_path, tl::formats::AdmFile& out, bool quiet);
    void read_rim_light(const std::string& rules_reference);
    void apply_environment(const std::string& layout_path);
    void set_model_ambient(Ogre::Entity* entity, const Ogre::ColourValue& colour,
                           std::set<Ogre::Material*>& seen);
    void apply_scenery_fill(const tl::formats::AdmSection& rules);
    /// The level template's `MATERIAL AMBIENT` (`CLevelTemplateData +0x880`),
    /// as apply_scenery_fill() read it.
    Ogre::ColourValue material_ambient = Ogre::ColourValue(0.36078432f, 0.36078432f, 0.36078432f);
    void setup_shadow_map(const tl::formats::AdmSection& rules);
    /// Logs what the scene holds (nodes, entities, particle systems...), with
    /// `when` after "scene" in the line.
    void log_scene_census(const char* when) const;
    /// The end of a timing window: glthread's costs, the shaders made, the GPU
    /// waits (renderer_diagnostics.cpp).
    static void log_driver_costs(bool print, unsigned frames);
    /// A frame over 50 ms, split into drive, advance and render, for the log.
    void log_long_frame(unsigned long long drive_us, unsigned long long advance_us,
                        unsigned long long render_us, unsigned long long total_us);
    void update_player_shadow(const Ogre::Vector3& focus);
    /// Every character's and loose item's light by distance from `focus` (`CCharacter::update`
    /// `CItem::update`, `CGenericModel::setLightOverride`),
    /// once a frame.
    void update_unit_lights(const Ogre::Vector3& focus);
    /// `COLLISION_RADIUS` by unit name, for the fade's sphere centre.
    std::unordered_map<std::string, float> unit_radius_cache;
    void destroy_shadow_map();
    void setup_light_map(const tl::formats::AdmSection& rules);
    void add_light_pass(const Ogre::MaterialPtr& material);
    void step_light_map(float dt);
    void aim_light_map(const Ogre::Vector3& focus);
    void destroy_light_map();
    bool setup_shader_generator();
    /// The program binaries `config.shader_cache` holds, into OGRE and back out before it
    /// goes (RendererConfig::shader_cache).
    void load_shader_cache();
    void save_shader_cache();
    /// Where those go: `config.shader_cache`, or a file of its own while the console runs
    /// GL3Plus (RTSS writes different GLSL per render system).
    std::string shader_cache_file;
    void frame_camera_on(Ogre::Entity* entity);
    void apply_scheme(Ogre::Viewport* viewport) const;
    /// What the view is cleared to: the port's grey for a bare model, else the level's fog
    /// (`CGameClient::refreshLighting`).
    Ogre::ColourValue view_background{0.15f, 0.16f, 0.20f};
    void update_automap_icons();
    /// `CCharacter::updateMotion`'s icon move and `setMeshVisible`'s visibility, per frame.
    void step_automap_icons();
    /// `CQuestManager::getNPCIcon`.
    int npc_quest_icon(const std::string& unit, uint32_t type);
    /// `media/UNITTYPES.HIE`, read once (`CResourceManager`'s hierarchy).
    void load_unit_types();
    tl::core::UnitTypeHierarchy unit_types;
    bool unit_types_loaded = false;
    /// A unit's automap icon (`CCharacter +0x728`), and whether its mesh was
    /// visible the last time it was looked at.
    struct UnitIcon {
        Ogre::SceneNode* node = nullptr;
        Ogre::Entity* entity = nullptr;
        Ogre::Billboard* billboard = nullptr;
        bool shown = true;
        std::string unit;
        Ogre::Vector3 placed = Ogre::Vector3::ZERO;
        bool reported_moving = false;
    };
    std::vector<UnitIcon> unit_icons;
    size_t last_unit_icon_count = static_cast<size_t>(-1);
};
namespace renderer_detail {
/// Declared here because mounting the archive invalidates the caches.
void forget_archive_index();

void forget_parsed_materials();

/// Parses the material script beside a mesh (same basename), not the whole archive's.
std::string dir_of_asset(const std::string& name);

std::vector<std::string> declared_materials(const std::string& text);
std::string qualify_material_names(const std::string& text,
                                   const std::vector<std::pair<std::string, std::string>>& renames);
std::string strip_material_blocks(const std::string& text, const std::vector<std::string>& drop);
std::string deduplicate_material_script(const std::string& text, size_t& dropped);
std::string complete_colour_op_ex(const std::string& text, size_t& fixed);
extern std::set<std::string> g_parsed_materials;
/// How often one script may be parsed before the log warns (a cache being thrashed).
constexpr size_t kNoisyParseCount = 8;

extern std::map<std::string, size_t> g_parse_counts;
extern std::map<std::string, std::string> g_material_home;
extern std::map<std::string, std::string> g_material_alias;
extern std::map<std::string, std::string> g_material_script;
void forget_parsed_materials();
// Defined below, next to the archive index it reads.
size_t material_script_rank(const std::string& script);

extern Ogre::SceneManager* g_material_scene;
// Defined below, next to the other things that know about materials.
size_t rebind_materials(Ogre::Entity* entity);
/// `CGenericModel::reInitialize`'s fully emissive passes drawn unlit (renderer.cpp).
void unlight_full_emissive(Ogre::Entity* entity);

size_t rebind_scene_materials(Ogre::SceneManager* scene);

void apply_qualified_materials(Ogre::Entity* entity, const std::string& dir);

// Defined below, next to the other things that know about the wardrobe's naming.
std::string slot_of_material(const std::string& material_name);

void qualify_texture_paths(const Ogre::MaterialPtr& material, const std::string& dir,
                           const std::string& fallback_dir);

std::string dir_of_asset(const std::string& name);

std::string fill_empty_texture_units(const std::string& text, const std::string& script,
                                     size_t& filled);

void parse_material_script(const std::string& script, std::vector<std::string>* redefined = nullptr,
                           bool force = false);
extern std::unordered_map<std::string, std::vector<Ogre::FileInfo>> g_archive_index;
extern std::unordered_map<std::string, std::vector<std::string>> g_materials_by_dir;
extern bool g_archive_indexed;
bool ends_with_lower(const std::string& text, const char* suffix);
std::string lowered(const std::string& text);
void forget_archive_index();
void ensure_archive_index();
size_t material_script_rank(const std::string& script);
const std::vector<Ogre::FileInfo>* archive_entries(const std::string& basename);
std::vector<std::string> archive_paths_under(const std::string& dir, const char* suffix);
std::vector<std::string> archive_paths_in_listing_order(const std::string& dir, const char* suffix);
/// The file of the spawn class whose `NAME` is `name` (case aside), or empty.
std::string spawn_class_path(const std::string& name);
std::vector<std::string> underscore_words(const std::string& name);
std::string wardrobe_texture_for(const std::string& mesh_name, const std::string& dir,
                                 const std::string& slot, const std::string& set_word);
size_t rebind_materials(Ogre::Entity* entity);
Ogre::TextureUnitState* rim_unit(Ogre::Pass* pass, const std::string& texture);
void apply_rim_light(Ogre::Entity* entity, const std::string& texture);
void apply_render_always(Ogre::Entity* entity, bool render_always);
/// A mesh's bounds as shipped: apply_render_always() replaces a
/// `RENDERALWAYS` mesh's own with the original's never-culled box.
const Ogre::AxisAlignedBox& authored_mesh_bounds(const Ogre::Entity* entity);
void highlight_entity(Ogre::Entity* entity, bool on, bool hostile, const std::string& rim_texture);
void apply_qualified_materials(Ogre::Entity* entity, const std::string& dir);
size_t rebind_scene_materials(Ogre::SceneManager* scene);
size_t report_stale_materials(Ogre::SceneManager* scene, const char* when);
void repoint_materials(const std::vector<Ogre::Entity*>& entities,
                       const std::vector<std::string>& redefined, const std::string& body_dir);
void parse_sibling_material(const std::string& mesh_name,
                            std::vector<std::string>* redefined = nullptr, bool force = false);
std::string archive_dir_of(const std::string& resource_name);
std::string archive_path_in_dir(const std::string& basename, const std::string& dir);
std::string animation_state_of(const std::string& stem, bool& armed, bool& has_armed_suffix,
                               int& variation);
std::string archive_animation_in_dir(const std::string& dir, const std::string& state, bool armed,
                                     int variation);
std::vector<std::string> archive_animation_stems_matching(const std::string& dir,
                                                          const std::string& substring);
std::string class_unit_path(const std::string& class_name, bool* console = nullptr);
std::string archive_path_unique(const std::string& basename);
/// Find the file a material's texture name refers to: by the mesh, then the wearer, then
/// anywhere unambiguous, then with the archive's extensions; never an ambiguous match.
std::string resolve_archive_path(const std::string& reference);
/// The same lookup without the warning, to ask whether a name is a full archive path.
std::string resolve_archive_path_quiet(const std::string& reference);

std::string resolve_texture(const std::string& name, const std::string& dir,
                            const std::string& fallback_dir);
void qualify_animated_frames(Ogre::TextureUnitState* unit, const std::string& dir,
                             const std::string& fallback_dir);
void qualify_texture_paths(const Ogre::MaterialPtr& material, const std::string& dir,
                           const std::string& fallback_dir);
std::string dir_of_asset(const std::string& name);
std::string animation_named(const Ogre::Entity* entity, const std::string& wanted);
std::string slot_of_material(const std::string& material_name);
std::string fill_empty_texture_units(const std::string& text, const std::string& script,
                                     size_t& filled);
void ensure_skeleton(const Ogre::MeshPtr& mesh, const std::string& mesh_name,
                     const std::string& dir);
std::string resolve_archive_path(const std::string& reference);
/// Keep read_resource()'s data files only while the same archives are mounted.
void remember_mounts(const std::string& mounts);
std::vector<uint8_t> read_resource(const std::string& path);
/// The pet a new character starts with unless the creation screen says otherwise
/// (`charactercreate.layout` lists `Dog` first).
constexpr const char* kMenuPetUnit = "Dog";

/// Where a class's idle is parked on the creation screen (as `--animation-time`).
constexpr float kMenuIdleTime = 1.1f;

/// What a main-menu level plays (`CGameClient::loadMenuLevel` hardcodes it), spelled as
/// a rules `MUSIC` key.
constexpr const char* kMenuMusic = "../music/Title.ogg";

std::vector<uint8_t> read_file(const std::string& path);
void log_animation_states(const Ogre::Entity* entity);
bool names_the_bind_pose(const std::string& animation);
bool skeleton_has_tracks(const Ogre::SkeletonPtr& skeleton);
size_t pose_from_skeleton(Ogre::Entity* entity, const Ogre::SkeletonPtr& pose);
size_t merge_shared_animations(const Ogre::SkeletonPtr& master, const Ogre::Skeleton* source,
                               const Ogre::Skeleton::BoneHandleMap& bones,
                               const Ogre::StringVector& wanted);
bool link_path_onto(Ogre::Entity* entity, const std::string& path);

/// Draws the transparent scenery OGRE 14 moves to `RENDER_QUEUE_TRANSPARENTS` (95) as the
/// last thing in the main group, where Torchlight's OGRE 1.6 drew it (particles sit at 51
/// to 90; `CParticleTechWrapper::setAsLight` 0x5a + sort index). Group 95 is drawn just
/// before the first group after the main one, then skipped.
class TransparentsWithScenery : public Ogre::RenderQueueListener {
public:
    explicit TransparentsWithScenery(Ogre::SceneManager* scene) : scene_(scene) {}

    void preRenderQueues() override { drawn_ = false; }

    void renderQueueStarted(Ogre::uint8 id, const Ogre::String&, bool& skip) override {
        if (id == Ogre::RENDER_QUEUE_TRANSPARENTS) {
            skip = drawn_;
            return;
        }
        if (drawn_ || id <= Ogre::RENDER_QUEUE_MAIN || id > Ogre::RENDER_QUEUE_TRANSPARENTS) return;
        drawn_ = true;
        const auto& groups = scene_->getRenderQueue()->_getQueueGroups();
        if (Ogre::RenderQueueGroup* group = groups[Ogre::RENDER_QUEUE_TRANSPARENTS].get()) {
            if (scene_->isRenderQueueToBeProcessed(Ogre::RENDER_QUEUE_TRANSPARENTS)) {
                scene_->_renderQueueGroupObjects(group,
                                                 Ogre::QueuedRenderableCollection::OM_PASS_GROUP);
            }
        }
    }

private:
    Ogre::SceneManager* scene_;
    bool drawn_ = false;
};

/// What the view queues (installed by setup_scene()). The light map's groups 95-99 are
/// never drawn in the view: `CGame::renderableQueued` (installed by
/// `CGame::preViewportUpdate`; removed) refuses
/// them (`CGenericModel::setRenderToLightMap`;
/// `CParticleTechWrapper::setAsLight`; `CGameClient::renderableQueued`,
/// `preRenderTargetUpdate`); here at
/// kLightMapGroupFirst..kLightMapGroupLast (kVisibilityLightMap). And the merged scenery
/// is re-queued at `kLayoutDrawOrderCeiling` (90) so depth ties fall as before the merge.
/// The original's queues: group 0x58 (characters; `CCharacter::setRenderBehind`, 50 with
/// the body re-queued) after the main group, then Runic's 0x5a up. Below 0x58 they are
/// OGRE's numbers; from 0x58 on they follow the main group, capped below the overlay.
constexpr unsigned runic_queue(unsigned queue) {
    return queue < 0x58 ? queue
                        : (queue - 0x58 + Ogre::RENDER_QUEUE_MAIN + 1 < 90
                               ? queue - 0x58 + Ogre::RENDER_QUEUE_MAIN + 1
                               : 90u);
}

constexpr unsigned kCharacterGroup = runic_queue(0x58);

/// Where Runic's group 0x5a lands; the rest follow at the same offsets.
constexpr unsigned kParticleGroupBase = runic_queue(0x5a);

constexpr unsigned kLightMapGroupFirst = kParticleGroupBase + 5;

constexpr unsigned kLightMapGroupLast = kParticleGroupBase + 9;

/// What the light map draws: things with its visibility flag. `preRenderTargetUpdate`
/// masks to 2, carried only by the plate and mask quads,
/// the `stgeomLight_` lights (`CLevel::bake`) and a
/// `setRenderToLightMap(true)` model; everything else
/// carries 1 (`CGame::chooseSceneManager`), particles included. The port's 2 is
/// the automap's, so here it is 4.
constexpr std::uint32_t kVisibilityLightMap = 0x4;

/// A renderable whose material has no compiled technique is queued with OGRE's blank white
/// (`RenderQueue::addRenderable`) and stays on it, as the queue only `prepare()`s it. The
/// shader generator rewriting its shaders (`SGScheme::synchronizeWithLightSettings`,
/// `Material::removeTechnique`) leaves a prepared material so; it is compiled here.
inline void compile_waiting_material(Ogre::Renderable* rend, Ogre::Technique** technique) {
    if (technique == nullptr || *technique == nullptr) return;
    const Ogre::MaterialPtr& held = rend->getMaterial();
    if (!held || (*technique)->getParent() == held.get()) return;
    if (!held->getCompilationRequired() || !held->getSupportedTechniques().empty()) return;
    held->touch();
    if (Ogre::Technique* own = rend->getTechnique()) {
        *technique = own;
        TL_LOG_INFO("material '%s' was left with no technique compiled; compiled again",
                    held->getName().c_str());
    }
}

/// `CCharacter::setRenderBehind(bool)`: a model drawn
/// through what stands in front of it (`CGenericModel::setRenderBehind`);
/// queue 50 on, 0x58 off; equipped items follow
/// (`CEquipment::setRenderBehind`). `hostile`: `CCharacter::setAlignment`'s
/// `outlinered.dds` over `outlineblue.dds`.
struct RenderBehind {
    bool on = false;
    bool hostile = false;
};
void set_render_behind(Ogre::Entity* entity, bool on, bool hostile);
/// The state `set_render_behind` left on an entity; null when it never ran.
const RenderBehind* render_behind_of(const Ogre::Entity* entity);

/// The render-behind materials by colour and skinning (`CGenericModel::reInitialize`'s
/// `rbmat_N`).
class RenderBehindMaterials {
public:
    explicit RenderBehindMaterials(GpuSkinning* gpu) : gpu_(gpu) {}
    Ogre::MaterialPtr material(bool hostile, unsigned bones, unsigned weights);

private:
    GpuSkinning* gpu_;
    std::map<std::tuple<bool, unsigned, unsigned>, Ogre::MaterialPtr> materials_;
};

/// `CGame::renderableQueued`, its render-behind half: a
/// seen sub-entity is queued again on 0x58 (the body) and drawn here additive with depth
/// test `GREATER`. True when it swapped `technique`.
bool render_behind_swap(Ogre::Renderable* rend, Ogre::ushort priority, Ogre::Technique** technique,
                        Ogre::RenderQueue* queue, RenderBehindMaterials& materials,
                        GpuSkinning* gpu);

class ViewRenderables : public Ogre::RenderQueue::RenderableListener {
public:
    bool renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort priority,
                          Ogre::Technique** technique, Ogre::RenderQueue* queue) override {
        compile_waiting_material(rend, technique);
        if (group >= kLightMapGroupFirst && group <= kLightMapGroupLast) return false;
        if (gpu != nullptr) gpu->guard(rend, technique);
        // In play only: `CGameClient +0x38d0` (the game state) 0 refuses it.
        if (behind != nullptr && behind_in_play && group == Ogre::RENDER_QUEUE_MAIN &&
            render_behind_swap(rend, priority, technique, queue, *behind, gpu)) {
            return true;
        }
        if (priority == kLayoutDrawOrderCeiling || opaque == nullptr) return true;
        auto* bucket = dynamic_cast<Ogre::StaticGeometry::GeometryBucket*>(rend);
        if (bucket == nullptr ||
            bucket->getParent()->getParent()->getParent()->getParent() != opaque) {
            return true;
        }
        queue->addRenderable(rend, group, kLayoutDrawOrderCeiling);
        return false;
    }

    /// The level's merge; nothing else is re-queued.
    const Ogre::StaticGeometry* opaque = nullptr;
    /// No skinning shader on what the CPU skins (GpuSkinning::guard).
    GpuSkinning* gpu = nullptr;
    /// The render-behind outline's materials, and whether the game is in play.
    RenderBehindMaterials* behind = nullptr;
    bool behind_in_play = false;
};

extern ViewRenderables g_view_renderables;
bool load_archive_image(const std::string& path, Ogre::Image& out);
}  // namespace renderer_detail
Ogre::Quaternion orientation_from_basis(const Ogre::Vector3& forward, const Ogre::Vector3& right,
                                        const Ogre::Vector3& up);
Ogre::Quaternion node_orientation(const tl::formats::CmpNode& node);
Ogre::Vector3 layout_scale(const tl::formats::CmpNode& node, float unit_scale);
Ogre::MeshPtr load_layout_mesh(const std::string& mesh_path);
Ogre::AnimationState* state_named_any_of(Ogre::AnimationStateSet* states,
                                         const std::vector<std::string>& wanted);
namespace renderer_detail {
/// `g_bCachParticles`: set by the first level a template loads
/// (`CLevel::loadRoomLayout`,
/// `CLayout::setCacheingParticlesForLevel`), never cleared, for the process.
bool caching_particles();
void set_caching_particles();
void restart_particle_system(ParticleSystem& system);
void stop_particle_emission(ParticleSystem& system);
/// The gameplay camera's default distance: `CCameraControl::getDefaultDistanceToDolly()`
/// (.rodata, 28.5) times `CAMERAMULT`; `CCameraControl::processInput()` clamps
/// between 14.0 and that. `CCameraControl::updateGameCamera` offsets
/// `(0.5, 0.75, -0.5)` turned by the camera angle, scaled by the dolly.
inline constexpr float kDefaultCameraDistance = 28.5f;

Ogre::Vector3 default_camera_offset(float camera_mult);
void log_memory(const char* when);
const tl::formats::CmpNode* find_spawn(const tl::formats::CmpScene& layout);
/// The material name of every submesh as the serializer reads it, in order: OGRE 14 keeps
/// no unresolvable name (`SubMesh::getMaterialName`), and collision materials
/// (`tl::core::collision_material`) resolve nowhere (`MeshSerializerImpl::readSubMesh`).
struct SubmeshMaterialNames final : Ogre::MeshSerializerListener {
    std::vector<std::string> names;
    void processMaterialName(Ogre::Mesh*, Ogre::String* name) override { names.push_back(*name); }
    void processSkeletonName(Ogre::Mesh*, Ogre::String*) override {}
    void processMeshCompleted(Ogre::Mesh*) override {}
};

bool mesh_local_bounds(const Ogre::MeshPtr& mesh, tl::core::Vec3& low, tl::core::Vec3& high);
void write_passability_dump(const tl::core::PassabilityMap& grid, const std::string& path);
void log_passability(const tl::core::PassabilityMap& grid, double seconds);
void for_each_triangle(
    const Ogre::MeshPtr& mesh, const Ogre::Matrix4& to_world,
    const std::function<void(const Ogre::Vector3&, const Ogre::Vector3&, const Ogre::Vector3&)>& fn,
    const std::function<bool(const std::string&)>& accept_material = nullptr);
}  // namespace renderer_detail
/// Where the ids given to spawned units start: a block no layout uses (`0x5350` is "SP").
constexpr int64_t kSpawnedNodeBase = 0x5350000000000000LL;

// The player as a missile meets it: no character id is negative.
constexpr int64_t kMissilePlayer = -1;

namespace renderer_detail {
std::vector<tl::core::LevelScript::Reachable> missile_candidates(
    const std::vector<tl::core::LevelScript*>& machines);
float sphere_sweep_t(const tl::core::Vec3& from, const tl::core::Vec3& to, float radius,
                     const tl::core::Vec3& centre, float other);
float box_sweep_t(const tl::core::Vec3& from, const tl::core::Vec3& to, float radius,
                  const tl::core::Vec3& low, const tl::core::Vec3& high);
int64_t missile_closest(const std::vector<tl::core::LevelScript::Reachable>& candidates,
                        const tl::core::Vec3& at, const tl::core::Vec3& forward, float angle,
                        float sight);
bool missile_unit(const std::vector<tl::core::LevelScript::Reachable>& candidates, int64_t id,
                  tl::core::Vec3& c, float& r, bool& alive);
tl::core::SpawnUnitData spawn_data_of(const tl::core::ItemStats& item);
Ogre::Quaternion quaternion_from_axes(const tl::core::SpawnAxes& axes);
/// `ProjectorCam` (`CGameClient::CGameClient`): orthographic,
/// `setFrustumExtents(-24, 24, 24, -24)`, looking straight down, kept 100 above the focus
/// (`CCameraControl::updateGameCamera`).
constexpr float kLightMapHalfExtent = 24.0f;

constexpr float kLightMapNear = 1.0f;

constexpr float kLightMapFar = 300.0f;

constexpr float kLightMapHeight = 100.0f;

/// `ProjectorRTTLight` is `KSETTINGS_SHADOW_RESOLUTION` square;
/// the high preset's 512 (`CSettings::assignDefault`).
constexpr unsigned kLightMapResolution = 512;

/// The light map's clear colour until a level's rules set LIGHTMAP
/// (0x3dc0c0c1, 24/255).
constexpr float kLightMapDefaultClear = 24.0f / 255.0f;

/// What the light map queues: `CGameClient::renderableQueued`
/// with the shadow flag (`+0x2b0`) clear takes groups 0, 95-99 and 101.
class LightMapRenderables : public Ogre::RenderQueue::RenderableListener {
public:
    bool renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort,
                          Ogre::Technique** technique, Ogre::RenderQueue*) override {
        compile_waiting_material(rend, technique);
        return group == Ogre::RENDER_QUEUE_BACKGROUND ||
               (group >= kLightMapGroupFirst && group <= kLightMapGroupLast) ||
               group == kLightMapGroupLast + 2;
    }
};

/// `CGameClient::preRenderTargetUpdate` / `postRenderTargetUpdate`
/// for the light map: its listener in for the update, the quads joined meanwhile; the mask
/// is the viewport's (kVisibilityLightMap). `CLevel::setNonLightStaticGeometryVisible`
/// has nothing to hide.
class LightMapTarget : public Ogre::RenderTargetListener {
public:
    LightMapTarget(Ogre::SceneManager* scene, Ogre::SceneNode* quads,
                   Ogre::RenderQueue::RenderableListener* view)
        : scene_(scene), quads_(quads), view_(view) {}

    // PORT: drawing, the light map's render target refreshed.
    void preRenderTargetUpdate(const Ogre::RenderTargetEvent&) override {
        scene_->getRenderQueue()->setRenderableListener(&renderables_);
        if (!quads_->getParentSceneNode()) scene_->getRootSceneNode()->addChild(quads_);
    }

    void postRenderTargetUpdate(const Ogre::RenderTargetEvent&) override {
        scene_->getRenderQueue()->setRenderableListener(view_);
        if (Ogre::SceneNode* parent = quads_->getParentSceneNode()) parent->removeChild(quads_);
    }

private:
    Ogre::SceneManager* scene_;
    Ogre::SceneNode* quads_;
    Ogre::RenderQueue::RenderableListener* view_;
    LightMapRenderables renderables_;
};

/// `ProjectorCamShadow` (`CGameClient::CGameClient`): FOVy
/// 120, looking down; `CGameClient::refreshLighting`
/// makes it orthographic (`setFrustumExtents(-8, 8, 8, -8)`) for ORTHOGRAPHICSHADOWS
/// (`CLevelTemplateData +0x8b0`). Kept 5 above the focus
/// (`CCameraControl::updateGameCamera`).
constexpr float kShadowMapHalfExtent = 8.0f;

constexpr float kShadowMapFovDegrees = 120.0f;

constexpr float kShadowMapNear = 1.0f;

constexpr float kShadowMapFar = 150.0f;

constexpr float kShadowMapHeight = 5.0f;

/// `ProjectorRTT` is `SHADOW_RESOLUTION` square, cleared to grey 0.5:
/// the receivers double the map, so the clear leaves a surface as it was.
constexpr unsigned kShadowMapResolution = kLightMapResolution;

constexpr float kShadowMapClear = 0.5f;

/// `ProjectorBackgroundQuad` is on group 94 (`setRenderQueueGroup(0x5e)`), after the
/// casters here.
constexpr unsigned kShadowFadeGroup = kParticleGroupBase + 4;

/// `RenderShadow<i>` and its skinned twins (`CGame::createMaterials`):
/// twelve of each.
constexpr int kShadowLevels = 12;

/// What the shadow map queues: `CGameClient::renderableQueued`
/// with the shadow flag (X360 `+0x220`, Linux `+0x2b0`) set: a casting (`+0x1`),
/// visible (`+0x134`) sub-entity drawn with `RenderShadow<floor(opacity * 12) - 1>`, or its
/// skinned twin for its weight count (`+0x12c`), made per bone and weight count.
class ShadowCasters : public Ogre::RenderQueue::RenderableListener {
public:
    explicit ShadowCasters(GpuSkinning* gpu) : gpu_(gpu) {}
    bool renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort,
                          Ogre::Technique** technique, Ogre::RenderQueue*) override;

private:
    Ogre::MaterialPtr material(int level, unsigned bones, unsigned weights);

    GpuSkinning* gpu_;
    std::map<std::tuple<int, unsigned, unsigned>, Ogre::MaterialPtr> materials_;
};

/// `CGameClient::preRenderTargetUpdate` / `postRenderTargetUpdate`
/// for the shadow map (kVisibilityShadowMap).
class ShadowMapTarget : public Ogre::RenderTargetListener {
public:
    ShadowMapTarget(Ogre::SceneManager* scene, Ogre::SceneNode* quad,
                    Ogre::RenderQueue::RenderableListener* view, GpuSkinning* gpu)
        : scene_(scene), quad_(quad), view_(view), casters_(gpu) {}

    // PORT: drawing, the shadow map's render target refreshed.
    void preRenderTargetUpdate(const Ogre::RenderTargetEvent&) override {
        scene_->getRenderQueue()->setRenderableListener(&casters_);
        if (!quad_->getParentSceneNode()) scene_->getRootSceneNode()->addChild(quad_);
    }

    void postRenderTargetUpdate(const Ogre::RenderTargetEvent&) override {
        scene_->getRenderQueue()->setRenderableListener(view_);
        if (Ogre::SceneNode* parent = quad_->getParentSceneNode()) parent->removeChild(quad_);
    }

private:
    Ogre::SceneManager* scene_;
    Ogre::SceneNode* quad_;
    Ogre::RenderQueue::RenderableListener* view_;
    ShadowCasters casters_;
};

std::unique_ptr<Ogre::Rectangle2D> light_map_quad(float half, const Ogre::MaterialPtr& material,
                                                  Ogre::uint8 group);
Ogre::MaterialPtr light_map_material(const std::string& name, Ogre::SceneBlendType blend);
void set_light_map_texture(const Ogre::MaterialPtr& material, const std::string& reference);
}  // namespace renderer_detail

tl::core::DungeonManager& loaded_dungeon_manager();
tl::core::DungeonFiles archive_dungeon_files();
tl::core::SpawnClassLookup archive_spawn_class_lookup();
bool subject_in_shot(const Ogre::Camera* camera, const Ogre::Entity* subject);
namespace renderer_detail {
std::string biome_key_from_ruleset(const std::string& ruleset);
std::string biome_display_name(const std::string& key);
std::array<uint8_t, 64> orientation_bytes(float yaw);
float yaw_of_bytes(const std::array<uint8_t, 64>& bytes);
tl::formats::QuestManagerSave quests_to_save(
    const tl::core::QuestLog& log, const std::function<int64_t(const std::string&)>& unit_guid_of);
std::string save_directory_of(const std::string& path);
extern bool keep_loading_screen;
extern bool keep_game_hud;
extern bool keep_inventory;
Ogre::MeshPtr load_gear_mesh(const std::string& path, const std::string& fallback_dir);
Ogre::Vector3 line_point(const Ogre::Vector3 (&p)[4], float t);
}  // namespace renderer_detail
/// How long a quest's banner stays up, in seconds.
constexpr float kQuestBannerSeconds = 2.0f;

/// How fast it then fades, in alpha per second (.rodata, `CGameUI::updateIngameUI`),
/// after the hold.
constexpr float kBannerFadePerSecond = 2.0f;

/// The fidget cooldown: `CMonster::idleAI` tests `+0x284` against 10.0.
constexpr float kFidgetCooldown = 10.0f;

/// How often a unit that may fidget does, per second: ten in 1001 per AI tick at 60 Hz.
/// DIVERGENCE (frame rate): a rate, not a per-frame roll.
constexpr float kFidgetPerSecond = 60.0f * 10.0f / 1001.0f;

/// The word every fidget file carries, matched as a substring
/// (archive_animation_stems_matching()).
constexpr const char* kFidgetWord = "fidget";

/// The owner the pet's model is filed under in `spawned`, apart from every
/// spawner's: the model itself gets a spawned unit's id (`pet_node_id`).
constexpr int64_t kPetNodeId = 0x5350000000000000LL - 1;

/// How far to the character's side a new pet is put: the two authored radii.
constexpr float kPetSpawnOffset = 1.6f;

}  // namespace tl::render
