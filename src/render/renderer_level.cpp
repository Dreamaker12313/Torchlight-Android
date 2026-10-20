// The renderer: light map, level rules, environment, layouts, dungeon floors.
#include "core/layout_text.h"
#include "core/level_memory.h"
#include "core/logic_names.h"
#include "render/renderer_impl.h"

namespace tl::render {
/// The dynamic light map: made once per scene as CGameClient::CGameClient
/// makes it under LIGHTING_ENABLED (ProjectorRTTLight on ProjectorCam,
/// ProjectorLightQuad at +-0.5 with RttLight, ProjectorLightMask with RttLightMask; the
/// corners reset each frame by CGameClient::updateMenu
/// and updateIngame). Re-textured per level as
/// CGameClient::refreshLighting: cleared to LIGHTMAP
/// (CLevelTemplateData +0x700), TORCHLIGHT (+0x6d0), LIGHTMASK (+0x6e8); constructor
/// defaults.
/// DIVERGENCE: the target is PF_BYTE_RGBA, not PF_R8G8B8 (the console allocates no other
/// manual format), so the pass takes its alpha from the surface.
void Renderer::Impl::setup_light_map(const tl::formats::AdmSection& rules) {
    if (config.no_torchlight) {
        TL_LOG_INFO("  light map left out (--no-torchlight)");
        return;
    }
    LightMap& map = light_map;
    if (!map.texture) {
        map.texture = Ogre::TextureManager::getSingleton().createManual(
            "tl/ProjectorRTTLight", Ogre::RGN_INTERNAL, Ogre::TEX_TYPE_2D, kLightMapResolution,
            kLightMapResolution, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);

        map.camera = scene->createCamera("tl/ProjectorCam");
        // The clip distances before the extents, where sets them
        // after: OGRE 14's setNearClipDistance() rescales extents already set.
        map.camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
        map.camera->setAspectRatio(1.0f);
        map.camera->setNearClipDistance(kLightMapNear);
        map.camera->setFarClipDistance(kLightMapFar);
        map.camera->setFrustumExtents(-kLightMapHalfExtent, kLightMapHalfExtent,
                                      kLightMapHalfExtent, -kLightMapHalfExtent);
        map.camera_node = scene->getRootSceneNode()->createChildSceneNode();
        map.camera_node->setPosition(0.0f, kLightMapHeight, 0.0f);
        map.camera_node->setOrientation(
            Ogre::Quaternion(Ogre::Degree(-90.0f), Ogre::Vector3::UNIT_X));
        map.camera_node->attachObject(map.camera);

        Ogre::RenderTarget* target = map.texture->getBuffer()->getRenderTarget();
        map.viewport = target->addViewport(map.camera);
        map.viewport->setShadowsEnabled(false);
        map.viewport->setClearEveryFrame(true);
        map.viewport->setBackgroundColour(Ogre::ColourValue(
            kLightMapDefaultClear, kLightMapDefaultClear, kLightMapDefaultClear, 1.0f));
        map.viewport->setOverlaysEnabled(false);
        map.viewport->setVisibilityMask(kVisibilityLightMap);
        map.viewport->setMaterialScheme(Ogre::MSN_SHADERGEN);
        target->setAutoUpdated(true);

        map.light_material = light_map_material("tl/RttLight", Ogre::SBT_REPLACE);
        set_light_map_texture(map.light_material, "media/sharedtextures/lantern.dds");
        map.mask_material = light_map_material("tl/RttLightMask", Ogre::SBT_TRANSPARENT_ALPHA);
        set_light_map_texture(map.mask_material, "media/sharedtextures/lightmask.dds");

        map.light_quad = light_map_quad(0.5f, map.light_material, Ogre::RENDER_QUEUE_BACKGROUND);
        map.light_quad->setVisibilityFlags(kVisibilityLightMap);
        map.light_mask =
            light_map_quad(1.0f, map.mask_material, static_cast<Ogre::uint8>(kLightMapGroupLast));
        map.light_mask->setVisibilityFlags(kVisibilityLightMap);
        map.quad_node = scene->createSceneNode();
        map.quad_node->attachObject(map.light_quad.get());
        map.quad_node->attachObject(map.light_mask.get());

        map.listener = std::make_unique<LightMapTarget>(scene, map.quad_node, &g_view_renderables);
        target->addListener(map.listener.get());
    }

    // CLevelTemplateData::load:
    // LIGHTMAP RED/GREEN/BLUE are integers defaulting to 24; the constructor's white
    // is only a never-loaded template's.
    const auto channel = [&rules](const char* key) {
        return static_cast<float>(static_cast<int32_t>(rules.integer_or(key, 24))) / 255.0f;
    };
    const float r = channel("LIGHTMAP RED");
    const float g = channel("LIGHTMAP GREEN");
    const float b = channel("LIGHTMAP BLUE");
    map.viewport->setBackgroundColour(Ogre::ColourValue(r, g, b, 1.0f));
    const std::string torchlight =
        rules.string_or("TORCHLIGHT", "media/sharedtextures/lantern.dds");
    const std::string mask = rules.string_or("LIGHTMASK", "media/sharedtextures/lightmask.dds");
    set_light_map_texture(map.light_material, torchlight);
    set_light_map_texture(map.mask_material, mask);

    // CLevel::setProjectorPass: the room pieces'
    // materials; characters take none (CLevel::addCharacter). Items with LEVELLIGHTING do
    // through CLevel::addItem: MISSING.
    // Aimed over the player, at the level's start (CLevel +0x194).
    if (have_player_start) aim_light_map(player_start);

    std::set<const Ogre::Material*> seen;
    size_t lit = 0;
    for (Ogre::Entity* entity : room_piece_meshes) {
        for (unsigned i = 0; i < entity->getNumSubEntities(); ++i) {
            const Ogre::MaterialPtr& material = entity->getSubEntity(i)->getMaterial();
            if (!material || !seen.insert(material.get()).second) continue;
            add_light_pass(material);
            ++lit;
        }
    }
    TL_LOG_INFO(
        "  light map %ux%u: LIGHTMAP (%.0f, %.0f, %.0f), TORCHLIGHT %s, LIGHTMASK %s; "
        "%zu room piece material(s), %zu light pool(s)",
        kLightMapResolution, kLightMapResolution, static_cast<double>(r * 255.0f),
        static_cast<double>(g * 255.0f), static_cast<double>(b * 255.0f), torchlight.c_str(),
        mask.c_str(), lit, light_pool_meshes.size());
}

/// `LIGHTPASS`: the light map projected onto one material. CLevel::setProjectorPass,
/// CGenericModel::setProjectorPass,
/// lighting-only branch; skipped for `nolight` materials.
/// With fewer than two
/// units the new one is appended, else it replaces the last, which moves to the end. The
/// unit: projected by ProjectorCam, clamped, LBX_MODULATE_X4.
void Renderer::Impl::add_light_pass(const Ogre::MaterialPtr& material) {
    if (!light_map.camera || material->getName().find("nolight") != std::string::npos) return;
    Ogre::Technique* technique = material->getBestTechnique();
    if (!technique || technique->getNumPasses() == 0) return;
    Ogre::Pass* pass = technique->getPass(0);

    // A shadow receiver (a name with `shadow`, CGenericModel::reInitialize)
    // under SHADOWS_ENABLED: CGenericModel::setProjectorPass
    // gives it the shadow map's unit (LBX_MODULATE_X2) and the light
    // map's (LBX_MODULATE_X4), appended or in place of the last, a copy of which goes last.
    if (shadow_map.camera && material->getName().find("shadow") != std::string::npos) {
        const unsigned short units = pass->getNumTextureUnitStates();
        for (unsigned short i = 0; i < units; ++i) {
            if (pass->getTextureUnitState(i)->getName() == "LIGHTPASS") return;
        }
        Ogre::TextureUnitState* shadow_unit = nullptr;
        Ogre::TextureUnitState* light_unit = nullptr;
        if (units < 2) {
            shadow_unit = pass->createTextureUnitState(shadow_map.texture->getName());
            light_unit = pass->createTextureUnitState(light_map.texture->getName());
        } else {
            const unsigned short last = static_cast<unsigned short>(units - 1);
            light_unit = pass->createTextureUnitState();
            Ogre::TextureUnitState* moved = pass->createTextureUnitState();
            *moved = *pass->getTextureUnitState(last);
            moved->setName("");
            shadow_unit = pass->getTextureUnitState(last);
            shadow_unit->setBlank();
            // DIVERGENCE (OGRE 14): as for the light map's unit below, the
            // reused unit drops the transform it had.
            shadow_unit->removeAllEffects();
            shadow_unit->setTextureTransform(Ogre::Matrix4::IDENTITY);
            shadow_unit->setTextureName(shadow_map.texture->getName());
            light_unit->setTextureName(light_map.texture->getName());
        }
        const auto project = [](Ogre::TextureUnitState* unit, Ogre::Frustum* frustum,
                                Ogre::LayerBlendOperationEx op) {
            unit->setProjectiveTexturing(true, frustum);
            unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
            unit->setTextureFiltering(Ogre::FO_POINT, Ogre::FO_LINEAR, Ogre::FO_NONE);
            unit->setColourOperationEx(op, Ogre::LBS_CURRENT, Ogre::LBS_TEXTURE);
            unit->setAlphaOperation(Ogre::LBX_SOURCE2, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
            unit->setName("LIGHTPASS");
        };
        project(shadow_unit, shadow_map.camera, Ogre::LBX_MODULATE_X2);
        project(light_unit, light_map.camera, Ogre::LBX_MODULATE_X4);
        return;
    }

    // The record's `+0x28` is the pass's own `LIGHTPASS` here: a material that
    // has one has had its pass. So `+0x2a`'s exclusion never has anything to
    // take off, and the count is every unit.
    const unsigned short counted = pass->getNumTextureUnitStates();
    for (unsigned short i = 0; i < counted; ++i) {
        if (pass->getTextureUnitState(i)->getName() == "LIGHTPASS") return;
    }

    Ogre::TextureUnitState* unit = nullptr;
    if (counted < 2) {
        unit = pass->createTextureUnitState(light_map.texture->getName());
    } else {
        const unsigned short at = static_cast<unsigned short>(counted - 1);
        Ogre::TextureUnitState* moved = pass->createTextureUnitState();
        *moved = *pass->getTextureUnitState(at);
        moved->setName("");
        unit = pass->getTextureUnitState(at);
        unit->setBlank();
        // DIVERGENCE (OGRE 14): the original's LIGHTPASS inherits the replaced unit's
        // texture transform; the RTSS aborts on a transform of a projected coordinate, so
        // it is dropped.
        unit->removeAllEffects();
        unit->setTextureTransform(Ogre::Matrix4::IDENTITY);
        unit->setTextureName(light_map.texture->getName());
    }
    unit->setProjectiveTexturing(true, light_map.camera);
    unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
    unit->setTextureFiltering(Ogre::FO_POINT, Ogre::FO_LINEAR, Ogre::FO_NONE);
    unit->setColourOperationEx(Ogre::LBX_MODULATE_X4, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    unit->setAlphaOperation(Ogre::LBX_SOURCE2, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    unit->setName("LIGHTPASS");
}

/// The light map's per-frame half of CGameClient::updateMenu
/// and updateIngame: the unread flicker angle
/// (+0x2a8) still draws its volatile numbers every 16 ms, as the original does.
/// DIVERGENCE: at most once per port frame (the frame-rate exception).
void Renderer::Impl::step_light_map(float dt) {
    LightMap& map = light_map;
    if (!map.light_quad) return;
    const float frame = std::min(dt, 0.3f);
    map.flicker_clock += frame;
    if (map.flicker_clock > 0.016f) {
        map.flicker_clock = 0.0f;
        tl::core::RunicRandom& random = tl::core::volatile_random();
        map.flicker_angle += random.between(2.0f, 5.0f) * frame;
        if (random.between(0.0f, 10.0f) > 7.0f) map.flicker_angle += random.between(-0.2f, 0.2f);
        while (map.flicker_angle >= 6.2831855f) map.flicker_angle -= 6.2831855f;
    }
    map.light_quad->setCorners(-0.5f, 0.5f, 0.5f, -0.5f);
}

/// `ProjectorCam` 100 above the camera's focus and `ProjectorCamShadow` 5
/// above it (`CCameraControl::updateGameCamera`, its
/// LIGHTING_ENABLED and SHADOWS_ENABLED branches; the menu's state 3 the same).
/// Every camera path of follow_subject() ends here, once a frame.
void Renderer::Impl::aim_light_map(const Ogre::Vector3& focus) {
    if (light_map.camera_node) {
        light_map.camera_node->setPosition(focus + Ogre::Vector3(0.0f, kLightMapHeight, 0.0f));
    }
    if (shadow_map.camera_node) {
        shadow_map.camera_node->setPosition(focus + Ogre::Vector3(0.0f, kShadowMapHeight, 0.0f));
    }
    // The player measured against the same point (update_player_shadow()).
    update_player_shadow(focus);
    // And every character's light (update_unit_lights()).
    update_unit_lights(focus);
}

/// The shadow map, made once per scene as CGameClient::CGameClient
/// makes it under SHADOWS_ENABLED (ProjectorRTT cleared to 0.5 grey,
/// ProjectorBackgroundQuad with RttBackground); set per level as
/// CGameClient::refreshLighting: SHADOWFADE
/// and ORTHOGRAPHICSHADOWS. Not drawn without the light map.
///
/// DIVERGENCE: the target is `PF_BYTE_RGBA`, for the light map's reason.
void Renderer::Impl::setup_shadow_map(const tl::formats::AdmSection& rules) {
    if (config.no_torchlight || config.no_shadows) {
        TL_LOG_INFO("  shadow map left out (%s)",
                    config.no_shadows ? "--no-shadows" : "--no-torchlight");
        return;
    }
    ShadowMap& map = shadow_map;
    if (!map.texture) {
        map.texture = Ogre::TextureManager::getSingleton().createManual(
            "tl/ProjectorRTT", Ogre::RGN_INTERNAL, Ogre::TEX_TYPE_2D, kShadowMapResolution,
            kShadowMapResolution, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);

        map.camera = scene->createCamera("tl/ProjectorCamShadow");
        map.camera->setAspectRatio(1.0f);
        map.camera->setNearClipDistance(kShadowMapNear);
        map.camera->setFarClipDistance(kShadowMapFar);
        map.camera->setFOVy(Ogre::Degree(kShadowMapFovDegrees));
        map.camera_node = scene->getRootSceneNode()->createChildSceneNode();
        map.camera_node->setPosition(0.0f, kShadowMapHeight, 0.0f);
        map.camera_node->setOrientation(
            Ogre::Quaternion(Ogre::Degree(-90.0f), Ogre::Vector3::UNIT_X));
        map.camera_node->attachObject(map.camera);

        Ogre::RenderTarget* target = map.texture->getBuffer()->getRenderTarget();
        map.viewport = target->addViewport(map.camera);
        map.viewport->setShadowsEnabled(false);
        map.viewport->setClearEveryFrame(true);
        map.viewport->setBackgroundColour(
            Ogre::ColourValue(kShadowMapClear, kShadowMapClear, kShadowMapClear, 1.0f));
        map.viewport->setOverlaysEnabled(false);
        map.viewport->setVisibilityMask(kVisibilityShadowMap);
        map.viewport->setMaterialScheme(Ogre::MSN_SHADERGEN);
        target->setAutoUpdated(true);

        map.background_material =
            light_map_material("tl/RttBackground", Ogre::SBT_TRANSPARENT_ALPHA);
        map.background = light_map_quad(1.0f, map.background_material,
                                        static_cast<Ogre::uint8>(kShadowFadeGroup));
        map.background->setVisibilityFlags(kVisibilityShadowMap);
        map.quad_node = scene->createSceneNode();
        map.quad_node->attachObject(map.background.get());

        map.listener = std::make_unique<ShadowMapTarget>(scene, map.quad_node, &g_view_renderables,
                                                         &gpu_skinning);
        target->addListener(map.listener.get());
    }

    set_light_map_texture(map.background_material,
                          rules.string_or("SHADOWFADE", "media/sharedtextures/shadowfade.dds"));
    // The clip distances before the extents, as setup_light_map() says why.
    const bool orthographic = rules.bool_or("ORTHOGRAPHICSHADOWS", false);
    if (orthographic) {
        map.camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
        map.camera->setFrustumExtents(-kShadowMapHalfExtent, kShadowMapHalfExtent,
                                      kShadowMapHalfExtent, -kShadowMapHalfExtent);
    } else {
        map.camera->resetFrustumExtents();
        map.camera->setProjectionType(Ogre::PT_PERSPECTIVE);
        map.camera->setFOVy(Ogre::Degree(kShadowMapFovDegrees));
    }
    TL_LOG_INFO("  shadow map %ux%u: %s, SHADOWFADE %s", kShadowMapResolution, kShadowMapResolution,
                orthographic ? "orthographic +-8" : "perspective 120",
                rules.string_or("SHADOWFADE", "media/sharedtextures/shadowfade.dds").c_str());
}

/// Before Root, as destroy_light_map().
void Renderer::Impl::destroy_shadow_map() {
    ShadowMap& map = shadow_map;
    if (!map.texture) return;
    Ogre::RenderTarget* target = map.texture->getBuffer()->getRenderTarget();
    if (map.listener) target->removeListener(map.listener.get());
    target->removeAllViewports();
    if (map.quad_node) {
        map.quad_node->detachAllObjects();
        if (Ogre::SceneNode* parent = map.quad_node->getParentSceneNode()) {
            parent->removeChild(map.quad_node);
        }
        scene->destroySceneNode(map.quad_node);
    }
    if (map.camera_node) map.camera_node->detachAllObjects();
    if (map.camera) scene->destroyCamera(map.camera);
    map.background.reset();
    map.listener.reset();
    map.background_material.reset();
    Ogre::TextureManager::getSingleton().remove(map.texture);
    map = ShadowMap{};
}

/// Before Root: the camera, the quads' node and the viewport belong to the
/// scene manager and the render target, and the target's texture to the
/// texture manager.
void Renderer::Impl::destroy_light_map() {
    LightMap& map = light_map;
    if (!map.texture) return;
    Ogre::RenderTarget* target = map.texture->getBuffer()->getRenderTarget();
    if (map.listener) target->removeListener(map.listener.get());
    target->removeAllViewports();
    if (map.quad_node) {
        map.quad_node->detachAllObjects();
        if (Ogre::SceneNode* parent = map.quad_node->getParentSceneNode()) {
            parent->removeChild(map.quad_node);
        }
        scene->destroySceneNode(map.quad_node);
    }
    if (map.camera_node) map.camera_node->detachAllObjects();
    if (map.camera) scene->destroyCamera(map.camera);
    map.light_quad.reset();
    map.light_mask.reset();
    map.listener.reset();
    Ogre::TextureManager::getSingleton().remove(map.texture);
    map = LightMap{};
}

/// Where a layout's rules file lives, or empty when it has none (read before placement
/// for the rim light: read_rim_light()).
// PORT: a layout path as the key the port's backdrop rules are kept under.
std::string Renderer::Impl::backdrop_key(const std::string& layout_path) {
    std::string key = tl::core::upper_ascii(layout_path);
    for (char& c : key) {
        if (c == '\\') c = '/';
    }
    for (const char* suffix : {".CMP", ".ADM"}) {
        const size_t n = std::strlen(suffix);
        if (key.size() > n && key.compare(key.size() - n, n, suffix) == 0)
            key.resize(key.size() - n);
    }
    return key;
}

std::string Renderer::Impl::rules_path_for(const std::string& layout_path) {
    // A generated floor hands over its stratum's RULESET itself, not a layout.
    if (layout_path.size() > 4) {
        std::string tail = layout_path.substr(layout_path.size() - 4);
        for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (tail == ".dat") {
            const std::string direct = resolve_archive_path(layout_path + ".adm");
            if (!direct.empty()) return direct;
        }
    }

    // A main-menu backdrop knows the rules it was laid out from.
    const auto backdrop = backdrop_rules.find(backdrop_key(layout_path));
    if (backdrop != backdrop_rules.end()) {
        const std::string path = resolve_archive_path(backdrop->second + ".adm");
        if (!path.empty()) return path;
    }

    // A main menu's rules: the layout's name plus RULES.DAT, in the parent directory,
    // found by unique basename.
    std::string stem = layout_path;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    for (const char* suffix : {".cmp", ".LAYOUT", ".layout"}) {
        const size_t n = std::strlen(suffix);
        if (stem.size() > n && stem.compare(stem.size() - n, n, suffix) == 0) {
            stem.resize(stem.size() - n);
        }
    }

    std::string rules_path = archive_path_unique(stem + "RULES.DAT.adm");

    // A playable level's rules are `rules.dat` (the dungeon's RULESET), ambiguous by
    // basename, so resolved by path: the layout's directory or its parent.
    if (rules_path.empty()) {
        const size_t last = layout_path.find_last_of('/');
        const std::string own =
            (last == std::string::npos) ? std::string() : layout_path.substr(0, last + 1);

        std::string parent;
        if (own.size() >= 2) {
            const size_t up = own.find_last_of('/', own.size() - 2);
            if (up != std::string::npos) parent = own.substr(0, up + 1);
        }

        // Parent first: that is where the shipped levels keep it, and trying the
        // other order would log an "ambiguous" warning on every level that is
        // laid out normally.
        for (const std::string& dir : {parent, own}) {
            if (dir.empty() || !rules_path.empty()) continue;
            rules_path = resolve_archive_path(dir + "rules.dat.adm");
        }
    }

    return rules_path;
}

/// Read and parse a level's rules file, or report that it has none.
bool Renderer::Impl::read_rules(const std::string& layout_path, tl::formats::AdmFile& out,
                                bool quiet) {
    const std::string rules_path = rules_path_for(layout_path);
    if (rules_path.empty()) {
        if (!quiet) {
            TL_LOG_INFO("no rules file for '%s'; keeping the default lighting",
                        layout_path.c_str());
        }
        return false;
    }

    const std::vector<uint8_t> bytes = read_resource(rules_path);
    if (bytes.empty()) return false;

    out = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!out.ok) {
        if (!quiet) {
            TL_LOG_WARN("could not parse '%s': %s", rules_path.c_str(), out.error.c_str());
        }
        return false;
    }
    if (!quiet) {
        TL_LOG_INFO("rules %s: section %s, %zu records, %zu undecoded byte(s)", rules_path.c_str(),
                    out.section().c_str(), out.records().size(), out.undecoded_bytes);
    }
    return true;
}

// The level's rim light, read before the level is placed: every character needs it as
// it is placed. The default when the
// rules author none.
void Renderer::Impl::read_rim_light(const std::string& rules_reference) {
    tl::formats::AdmFile rules;
    rim_light_texture = read_rules(rules_reference, rules, /*quiet=*/true)
                            ? rules.string_or("RIMLIGHT", kDefaultRimLight)
                            : std::string(kDefaultRimLight);
    TL_LOG_INFO("  rules rim light: '%s'", rim_light_texture.c_str());
}

void Renderer::Impl::apply_environment(const std::string& layout_path) {
    tl::formats::AdmFile rules;
    if (!read_rules(layout_path, rules, /*quiet=*/false)) return;

    // CAMERAMULT: the only camera figure the level data carries, a multiplier on a base
    // distance the data does not hold (see follow_subject()).
    camera_mult = rules.float_or("CAMERAMULT", 1.0f);
    // CGameClient::loadLevel: the camera's multiplier (+0x5c)
    // to CAMERAMULT (+0x98, load) and its dolly to CAMERAMULT * 28.5; no
    // template: 1 and 28.5.
    camera_dolly = kDefaultCameraDistance * camera_mult;
    // CGameClient::unloadCurrentLevel ends every shake
    // and speed modifier.
    //
    camera_shakes.clear();
    game_speed.clear();
    TL_LOG_INFO("  camera multiplier %.2f", static_cast<double>(camera_mult));

    // `ACTIVERANGEMOD` (`CLevelTemplateData::load`, `+0x98`), which
    // monster sight and chase ranges add on this level. Authored only by the
    // mine boss room and the Sunken hub (1000) and the fortress boss room (200).
    const float active_range_mod = rules.float_or("ACTIVERANGEMOD", 0.0f);
    level_active_range_mod = active_range_mod;
    for (tl::core::LevelScript* machine : level_machines) {
        machine->set_active_range_mod(active_range_mod);
    }
    // The active range:
    // INDOOR_UNIT_ACTIVE_RANGE under UNIT_LIGHT_FADE, else OUTDOOR_; CGameGlobals::reload
    // defaults 16 and 24; UNIT_NEAR_RANGE (24).
    {
        float indoor = 16.0f;
        float outdoor = 24.0f;
        float near_range = 24.0f;
        float shadow_range = 10.0f;
        float trigger_near = 40.0f;
        const std::vector<uint8_t> bytes = read_resource("media/GLOBALS.DAT.adm");
        if (!bytes.empty()) {
            const tl::formats::AdmFile globals = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (globals.ok) {
                indoor = globals.float_or("INDOOR_UNIT_ACTIVE_RANGE", indoor);
                outdoor = globals.float_or("OUTDOOR_UNIT_ACTIVE_RANGE", outdoor);
                near_range = globals.float_or("UNIT_NEAR_RANGE", near_range);
                shadow_range = globals.float_or("UNIT_SHADOW_RANGE", shadow_range);
                trigger_near = globals.float_or("TRIGGER_NEAR_RANGE", trigger_near);
            }
        }
        // DIVERGENCE (2026-10-10): the ranges reach past the frame (growing with
        // CAMERAMULT), so units fade in before they come into view; the near range keeps
        // 8 beyond, as the near list is rebuilt once a second.
        {
            const float frame = 32.0f * std::max(1.0f, camera_mult);
            indoor = std::max(indoor, frame);
            outdoor = std::max(outdoor, frame);
            near_range = std::max(near_range, frame + 8.0f);
            trigger_near = std::max(trigger_near, near_range);
        }
        const bool fade = rules.bool_or("UNIT_LIGHT_FADE", true);
        level_unit_active_range = fade ? indoor : outdoor;
        level_unit_near_range = near_range;
        level_unit_shadow_range = shadow_range;
        level_light_fade = fade;
        // The light fade beside it (`CCharacter::update`):
        // UNIT_FADE_DISTANCE (`+0x8b4`, Linux `+0x764`, default 14.5) and
        // UNIT_FADE_START (`+0x8b8`, Linux `+0x768`, default 7), from the
        // template's defaults (`CLevelTemplateData::CLevelTemplateData`).
        level_unit_fade_distance = rules.float_or("UNIT_FADE_DISTANCE", 14.5f);
        level_unit_fade_start = rules.float_or("UNIT_FADE_START", 7.0f);
        level_indoor_range = indoor;
        level_outdoor_range = outdoor;
        level_trigger_near_range = trigger_near;
        for (tl::core::LevelScript* machine : level_machines) configure_unit_ranges(*machine);
        TL_LOG_INFO("  unit active range %.0f (%s)", static_cast<double>(fade ? indoor : outdoor),
                    fade ? "indoor" : "outdoor");
    }
    if (active_range_mod != 0.0f) {
        TL_LOG_INFO("  monster active range +%.0f", static_cast<double>(active_range_mod));
    }

    // Fill light: AMBIENT, which the game's shaders bind as ambient_light_colour.
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (rules.colour("AMBIENT", r, g, b)) {
        scene->setAmbientLight(Ogre::ColourValue(r, g, b));
        TL_LOG_INFO("  ambient light (%.2f, %.2f, %.2f)", r, g, b);
    }

    if (key_light && rules.colour("DIRECTIONAL", r, g, b)) {
        // CGameClient::refreshLighting: the key light is the template's
        // (+0x890) DIRECTIONAL times DIRECTIONAL INTENSITY, unclamped (lighting adds it
        // unclamped: patches/ogre/0015).
        const float intensity = rules.float_or("DIRECTIONAL INTENSITY", 1.0f);
        key_light->setDiffuseColour(Ogre::ColourValue(r * intensity, g * intensity, b * intensity));

        const Ogre::Vector3 from(rules.float_or("DIRECTIONAL X", 0.0f),
                                 rules.float_or("DIRECTIONAL Y", 1.0f),
                                 rules.float_or("DIRECTIONAL Z", 0.0f));
        if (!from.isZeroLength() && key_light_node) {
            // The data gives where the light is, so the direction it shines is
            // the way back towards the scene.
            key_light_node->setDirection(-from.normalisedCopy(), Ogre::Node::TS_WORLD);
        }
        TL_LOG_INFO("  directional (%.2f, %.2f, %.2f) x%.2f from (%.0f, %.0f, %.0f)", r, g, b,
                    static_cast<double>(intensity), from.x, from.y, from.z);
    }

    apply_scenery_fill(rules.root);
    setup_shadow_map(rules.root);
    setup_light_map(rules.root);

    // CGameClient::refreshLighting: cleared to the fog
    // colour, fog linear from FOG START to FOG END times 1.3 (1 in NETBOOK_MODE). Defaults:
    // 30, 42, 58; 24 and 50.
    {
        const float fog_r =
            static_cast<float>(static_cast<int32_t>(rules.integer_or("FOG RED", 30))) / 255.0f;
        const float fog_g =
            static_cast<float>(static_cast<int32_t>(rules.integer_or("FOG GREEN", 42))) / 255.0f;
        const float fog_b =
            static_cast<float>(static_cast<int32_t>(rules.integer_or("FOG BLUE", 58))) / 255.0f;
        const float start =
            static_cast<float>(static_cast<int32_t>(rules.integer_or("FOG START", 24)));
        const float end = static_cast<float>(static_cast<int32_t>(rules.integer_or("FOG END", 50)));
        const float scale = 1.3f;
        const Ogre::ColourValue fog(fog_r, fog_g, fog_b, 1.0f);
        scene->setFog(Ogre::FOG_LINEAR, fog, 0.0f, scale * start, end * scale);
        view_background = fog;
        if (window != nullptr) {
            for (unsigned short i = 0; i < window->getNumViewports(); ++i) {
                window->getViewport(i)->setBackgroundColour(fog);
            }
        }
        TL_LOG_INFO("  fog and background (%.2f, %.2f, %.2f) from %.1f to %.1f",
                    static_cast<double>(fog_r), static_cast<double>(fog_g),
                    static_cast<double>(fog_b), static_cast<double>(scale * start),
                    static_cast<double>(end * scale));
    }

    const std::string sky = rules.string_or("SKYBOX", "");
    if (!sky.empty() && !sky_node) {
        const std::string sky_path = resolve_archive_path(sky);
        if (sky_path.empty()) {
            TL_LOG_WARN("  skybox '%s' not in the archive", sky.c_str());
        } else {
            parse_sibling_material(sky_path);
            try {
                Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().load(sky_path, kGameGroup);
                const std::string dir = dir_of_asset(sky_path);
                auto& materials = Ogre::MaterialManager::getSingleton();
                for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
                    qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup),
                                          dir, dir);
                }
                Ogre::Entity* entity = scene->createEntity(mesh);
                // A cached mesh hands its submeshes' *old* MaterialPtrs to a new entity.
                rebind_materials(entity);
                // Drawn before everything else and never written to the depth
                // buffer, so it stays behind the scene however large it is.
                entity->setRenderQueueGroup(Ogre::RENDER_QUEUE_SKIES_EARLY);
                entity->setCastShadows(false);
                sky_node = scene->getRootSceneNode()->createChildSceneNode();
                sky_node->attachObject(entity);
                TL_LOG_INFO("  skybox %s (radius %.1f)", sky_path.c_str(),
                            static_cast<double>(entity->getBoundingRadius()));
            } catch (const Ogre::Exception& e) {
                TL_LOG_WARN("  could not load skybox '%s': %s", sky_path.c_str(),
                            e.getDescription().c_str());
            }
        }
    }

    // CGameClient::loadLevel: WATER
    // (+0x6b0), POSTWATER (+0x6b8), MIDWATER (+0x6c0), generic models in queues 4, 0x59 and
    // 0x58, shown, at the origin.
    static const struct {
        const char* key;
        Ogre::uint8 queue;
    } kLevelModels[] = {{"WATER", 4}, {"POSTWATER", 0x59}, {"MIDWATER", 0x58}};
    for (const auto& model : kLevelModels) {
        const std::string wanted = rules.string_or(model.key, "");
        if (wanted.empty()) continue;
        const std::string path = resolve_archive_path(wanted);
        if (path.empty()) {
            TL_LOG_WARN("  %s '%s' not in the archive", model.key, wanted.c_str());
            continue;
        }
        parse_sibling_material(path);
        try {
            Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().load(path, kGameGroup);
            const std::string dir = dir_of_asset(path);
            auto& materials = Ogre::MaterialManager::getSingleton();
            for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
                qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup), dir,
                                      dir);
            }
            Ogre::Entity* entity = scene->createEntity(mesh);
            rebind_materials(entity);
            entity->setRenderQueueGroup(static_cast<Ogre::uint8>(runic_queue(model.queue)));
            entity->setCastShadows(false);
            Ogre::SceneNode* node = scene->getRootSceneNode()->createChildSceneNode();
            node->setPosition(Ogre::Vector3::ZERO);
            node->attachObject(entity);
            // CLevel::setProjectorPass:
            // the light map on MIDWATER always, on WATER only with `LIGHT` in its file name;
            // CGenericModel::setAmbient of MATERIAL AMBIENT either way.
            const bool lit = model.queue == 0x58 ||
                             (model.queue == 4 &&
                              tl::core::upper_ascii(path).find("LIGHT") != std::string::npos);
            if (lit) {
                for (unsigned i = 0; i < entity->getNumSubEntities(); ++i) {
                    const Ogre::MaterialPtr& material = entity->getSubEntity(i)->getMaterial();
                    if (material) add_light_pass(material);
                }
                std::set<Ogre::Material*> seen;
                set_model_ambient(entity, material_ambient, seen);
            }
            TL_LOG_INFO("  %s %s (queue %u%s)", model.key, path.c_str(),
                        static_cast<unsigned>(model.queue), lit ? ", light map" : "");
        } catch (const Ogre::Exception& e) {
            TL_LOG_WARN("  could not load %s '%s': %s", model.key, path.c_str(),
                        e.getDescription().c_str());
        }
    }
}

void Renderer::Impl::place_layout_item(const tl::formats::CmpNode& node, const Ogre::Vector3& at) {
    // CEquipment::createNewEquipment via CEquipment::convertEquipment:
    // the ITEMS record of that name, unitInit(data, false) on a fresh
    // CItem. No such item, nothing.
    const tl::formats::CmpField* name = node.field(tl::formats::cmp_field::item::kItem);
    if (name == nullptr || name->as_text().empty()) return;
    ensure_item_index();
    const tl::core::ItemStats* data = items.find(name->as_text());
    if (data == nullptr) {
        TL_LOG_WARN("layout item '%s': no item named '%s'", node.name.c_str(),
                    name->as_text().c_str());
        return;
    }
    const tl::core::ItemStats made = create_item(*data, false, false);
    // `descriptorSceneActivated`: outside the editor, each item is marked as
    // the layout's (`+0x191`) and `CLevel::addItem(item, its position, true)`
    // puts it in the level - for a `CEquipment`, on the ground and tossed from
    // there (`drop_item`).
    TL_LOG_INFO("layout item '%s': %s", node.name.c_str(), name->as_text().c_str());
    drop_item(made, 1, at);
}

const tl::formats::CmpScene* Renderer::Impl::layout_file(const std::string& path) {
    const auto found = layout_files.find(path);
    if (found != layout_files.end()) return found->second.get();
    // `CEditorScene::loadScene` takes the
    // compiled file when there is one (`CFileSystem::getFileInfo`'s compressed
    // check), and reads the layout as a data group - a mod's text `.layout` -
    // only when there is not (`core/layout_text`).
    std::string source = path;
    if (!ends_with_lower(path, ".cmp")) {
        const std::string compiled = resolve_archive_path(path + ".cmp");
        if (!compiled.empty()) source = compiled;
    }
    const std::vector<uint8_t> bytes = read_resource(source);
    if (bytes.empty()) return nullptr;
    auto parsed = std::make_unique<const tl::formats::CmpScene>(
        tl::core::parse_layout_file(bytes.data(), bytes.size()));
    return layout_files.emplace(path, std::move(parsed)).first->second.get();
}

bool Renderer::Impl::place_layout(const std::string& reference, const Ogre::Vector3& origin,
                                  const Ogre::Quaternion& basis, int depth,
                                  uint32_t load_seed_arg) {
    // A nested layout may reference an ancestor: the set catches a cycle, the depth limit
    // a long chain. Also whether a layout link placed this file.
    struct LinkGuard {
        bool* slot;
        bool previous;
        ~LinkGuard() { *slot = previous; }
    } link_guard{&placing_under_link, placing_under_link};
    placing_under_link = std::exchange(link_places_next_layout, false);
    // A `CParticle`'s file: `CParticlePreloader::LoadParticle`.
    //
    const bool as_cparticle = std::exchange(cparticle_places_next_layout, false);

    constexpr int kMaxDepth = 8;
    if (depth > kMaxDepth) {
        TL_LOG_WARN("layout nesting deeper than %d, stopping at '%s'", kMaxDepth,
                    reference.c_str());
        return false;
    }

    // A layout the port made itself (`place_populated_trigger`) is in
    // `layout_files` under its own name, not in the archive.
    const bool made_here = reference.rfind("populate:", 0) == 0;
    std::string path = made_here ? reference : resolve_archive_path(reference);
    if (path.empty()) path = resolve_archive_path(reference + ".cmp");
    if (path.empty()) {
        TL_LOG_WARN("no layout '%s' in the archive", reference.c_str());
        return false;
    }

    if (!layouts_in_progress.insert(path).second) {
        TL_LOG_WARN("layout '%s' references itself; not recursing", path.c_str());
        return false;
    }

    const tl::formats::CmpScene* const parsed = layout_file(path);
    if (parsed == nullptr) {
        layouts_in_progress.erase(path);
        return false;
    }

    const tl::formats::CmpScene& layout = *parsed;
    if (!layout.ok) {
        TL_LOG_ERROR("could not parse '%s': %s", path.c_str(), layout.error.c_str());
        layouts_in_progress.erase(path);
        return false;
    }

    // resyncs is how much of the file this parser did not understand. Logged
    // rather than swallowed: a silently skipped subtree is missing geometry
    // that looks exactly like geometry the level never had.
    TL_LOG_INFO("layout %s: %zu nodes, %zu resync(es)", path.c_str(), layout.nodes.size(),
                layout.resyncs);

    // `CLayout::loadLayoutFile`: the load
    // stack, and the reseed a dungeon room piece gets. The child counter
    // restarts when an outermost load begins; while `layout_seeding` is set
    // each load bumps it and seeds `g_Rand` with its argument + stack depth +
    // counter + the level seed. A room piece passes `1 << index`, a nested
    // `Layout Link` 0 (`CLayoutDescriptor::Set_loadLayoutFile`).
    if (layout_stack_depth == 0) layout_child_count = 0;
    ++layout_stack_depth;
    struct LoadStackPop {
        uint32_t& count;
        ~LoadStackPop() { --count; }
    } load_stack_pop{layout_stack_depth};
    if (rng_trace_file != nullptr) {
        // `--rng-trace`: which file this load is, beside the original's `L` lines
        // from the gdb trace of the original (depth there is the count before the push).
        std::fprintf(rng_trace_file, "L %s depth=%u child=%u seeded=%d\n", reference.c_str(),
                     layout_stack_depth - 1, layout_child_count + (layout_seeding ? 1u : 0u),
                     layout_seeding ? 1 : 0);
        std::fflush(rng_trace_file);
    }
    if (layout_seeding) {
        ++layout_child_count;
        tl::core::game_random().set_seed(
            static_cast<int32_t>(load_seed_arg + layout_stack_depth + layout_child_count +
                                 static_cast<uint32_t>(layout_level_seed)));
    }

    if (depth == 0) {
        // Only the rim light here: the lighting acts on the scenery placed below, so it
        // stays at the end.
        read_rim_light(path);
    }

    // Quest-gated subtrees, skipped: a Group naming a quest in one of five fields holds
    // its subtree back, evaluated against the quest state (src/core/quest.h). The random
    // choice among child groups: CEditorScene::loadCompressedLayout
    // hands every Group to rollRandomGroups, which drops failed gates
    // (CRandomGroup::getGroupCanBeCreated; core/random_groups.h).
    std::set<int64_t> gated;
    {
        std::map<int64_t, const tl::formats::CmpNode*> by_id;
        for (const tl::formats::CmpNode& n : layout.nodes) by_id.emplace(n.id, &n);

        namespace group_field = tl::formats::cmp_field::group;
        std::string class_upper = player_class;
        for (char& c : class_upper)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::vector<tl::core::LayoutGroup> groups;
        for (const tl::formats::CmpNode& n : layout.nodes) {
            if (n.type != tl::formats::cmp_type::kGroup) continue;
            tl::core::LayoutGroup g;
            g.id = n.id;
            g.parent_id = n.parent_id;
            // CRandomGroup::canGroupBeCreatedBasedOffOfPlayerClass:
            // no CLASS, or CGameClient::getPlayerClassName() equal as
            // authored - the upper-cased NAME,
            // so only an upper-case CLASS matches.
            const tl::formats::CmpField* class_field = n.field(group_field::kClass);
            const std::string group_class =
                class_field != nullptr ? class_field->as_text() : std::string();
            const bool class_ok = group_class.empty() || group_class == class_upper;
            // `CRandomGroup::canGroupBeCreatedBasedOffOfDungeon`: a
            // `DUNGEON` (`+0x188`), on a level that has a dungeon name (`CLevel
            // +0x280`), must be that name exactly.
            const tl::formats::CmpField* dungeon_field = n.field(group_field::kDungeon);
            const std::string group_dungeon =
                dungeon_field != nullptr ? dungeon_field->as_text() : std::string();
            const bool dungeon_ok =
                group_dungeon.empty() || level_dungeon.empty() || group_dungeon == level_dungeon;
            // CRandomGroup::canGroupBeCreatedBasedOffOfDifficulty: a DIFFICULTY
            // MODE (+0x190) below 4 must be the game's (+0x38ec);
            // CRandomGroupDescriptor::getDifficultyIDByString:
            // EASY..VERY HARD are 0..3, anything else (the empty name) 0xe.
            const tl::formats::CmpField* mode_field = n.field(group_field::kDifficultyMode);
            const std::string mode_name =
                mode_field != nullptr ? mode_field->as_text() : std::string();
            const uint32_t group_mode = tl::core::difficulty_id_by_string(mode_name);
            const bool difficulty_ok =
                !(group_mode < 4u) || group_mode == static_cast<uint32_t>(config.difficulty);
            g.can_be_created = class_ok && dungeon_ok && difficulty_ok &&
                               tl::core::group_is_open(n, quest_log.state());
            if (const tl::formats::CmpField* f = n.field(group_field::kChoice)) {
                g.random_type = tl::core::random_group_type_from_string(f->as_text());
            }
            if (const tl::formats::CmpField* f = n.field(group_field::kNumber)) {
                g.number_of_picks = f->as_uint();
            }
            if (const tl::formats::CmpField* f = n.field(group_field::kRandomisation)) {
                g.random_weight = f->as_uint();
            }
            groups.push_back(g);
        }
        gated = tl::core::roll_random_groups(groups, tl::core::game_random());

        if (!gated.empty()) {
            size_t hidden_by_quest = 0;
            for (const tl::formats::CmpNode& n : layout.nodes) {
                int64_t at = n.parent_id;
                for (int depth = 0; depth < 16 && at != -1; ++depth) {
                    if (gated.count(at)) {
                        gated.insert(n.id);
                        ++hidden_by_quest;
                        break;
                    }
                    const auto it = by_id.find(at);
                    if (it == by_id.end()) break;
                    at = it->second->parent_id;
                }
            }
            TL_LOG_INFO(
                "  %zu node(s) held back by %zu group(s) not created (gates or random choice)",
                hidden_by_quest, gated.size() - hidden_by_quest);
        }
    }

    // LoadParticle keeps the objects of three
    // descriptors - Particle (and what hangs from it), Sound, Camera Shake - and deletes
    // the rest (DeleteAllObjectsFromScene).
    if (as_cparticle) {
        std::map<int64_t, const tl::formats::CmpNode*> by_id;
        for (const tl::formats::CmpNode& n : layout.nodes) by_id.emplace(n.id, &n);
        size_t dropped = 0;
        for (const tl::formats::CmpNode& n : layout.nodes) {
            namespace type = tl::formats::cmp_type;
            if (n.type == type::kParticle || n.type == type::kSound ||
                n.type == type::kCameraShake || n.type == type::kGroup) {
                continue;
            }
            bool under_particle = false;
            int64_t at = n.parent_id;
            for (int up = 0; up < 16 && at != -1 && !under_particle; ++up) {
                const auto it = by_id.find(at);
                if (it == by_id.end()) break;
                under_particle = it->second->type == type::kParticle;
                at = it->second->parent_id;
            }
            if (under_particle) continue;
            if (gated.insert(n.id).second) ++dropped;
        }
        if (dropped > 0) {
            TL_LOG_INFO(
                "  a particle's file: %zu node(s) not kept (only Particle, Sound and Camera "
                "Shake are)",
                dropped);
        }
    }

    // The layout's Animation Controller nodes, by the unit (`MONSTER`) they drive; they
    // have no position, so the walk below would not see them. START ON LOAD starts at once.
    std::map<std::string, std::string> unit_animations;
    for (const tl::formats::CmpNode& n : layout.nodes) {
        if (n.type != tl::formats::cmp_type::kAnimationController) continue;
        if (gated.count(n.id)) continue;
        namespace controller = tl::formats::cmp_field::animation_controller;

        const tl::formats::CmpField* start = n.field(controller::kStartOnLoad);
        if (!start || start->count() != 1 || !start->as_bool()) continue;

        const tl::formats::CmpField* unit = n.field(controller::kUnit);
        const tl::formats::CmpField* animation = n.field(controller::kAnimation);
        if (!unit || !animation) continue;

        const std::string unit_name = unit->as_text();
        const std::string animation_name = animation->as_text();
        if (unit_name.empty() || animation_name.empty()) continue;

        unit_animations[unit_name] = animation_name;
        TL_LOG_INFO("  animation controller '%s': %s plays '%s' on load", n.name.c_str(),
                    unit_name.c_str(), animation_name.c_str());
    }

    // The units this layout places, read before anything is placed: a `Monster`
    // that authors no `SCALE` of its own takes the one in its unit `.DAT`, and
    // that decision is made while the mesh is being placed.
    if (depth == 0) {
        load_unit_stats(layout, gated);
    } else {
        // A room's own Monsters are added to the level's book, not replacing it (a
        // generated floor has no top-level file).
        for (const tl::formats::CmpNode& node : layout.nodes) {
            if (node.type != tl::formats::cmp_type::kMonster) continue;
            if (gated.count(node.id) != 0) continue;
            const tl::formats::CmpField* field =
                node.field(tl::formats::cmp_field::monster::kMonster);
            if (field == nullptr || field->as_text().empty()) continue;
            if (!index_unit(field->as_text())) {
                TL_LOG_WARN("  unit '%s' in %s has no .DAT this engine could read",
                            field->as_text().c_str(), path.c_str());
            }
        }
    }

    // A linked layout with a script is a machine in a file, owning everything it places;
    // each instance gets its own node map (NestedScript). Saved and restored, because a
    // linked layout may link another.
    NodeMap* outer_sink = node_sink;
    size_t nested_script_index = nested_scripts.size();
    // A room needs a machine even with nothing wired when it places a Unit Trigger
    // (CTriggerUnit::interact() needs one), a Monster (one of the level's characters,
    // CLevel +0x98), a Unit Spawner (SPAWN ON CREATE:
    // CUnitSpawnerDescriptor::descriptorSceneActivated,
    // here level_activated) or a Sound (CSoundObjectDescriptor, core/sound_object.h).
    bool owns_a_trigger = false;
    bool owns_a_monster = false;
    bool owns_a_spawner = false;
    bool owns_a_sound = false;
    if (depth > 0) {
        for (const tl::formats::CmpNode& n : layout.nodes) {
            if (n.type == tl::formats::cmp_type::kUnitTrigger) owns_a_trigger = true;
            if (n.type == tl::formats::cmp_type::kMonster) owns_a_monster = true;
            if (n.type == tl::formats::cmp_type::kUnitSpawner) owns_a_spawner = true;
            if (n.type == tl::formats::cmp_type::kSound) owns_a_sound = true;
        }
    }
    const bool owns_a_script = depth > 0 && !as_cparticle &&
                               (!layout.logic_groups.empty() || !layout.timelines.empty() ||
                                owns_a_trigger || owns_a_monster || owns_a_spawner || owns_a_sound);
    if (owns_a_script) {
        NestedScript machine;
        machine.reference = path;
        machine.owner = nested_link;
        machine.owner_name = nested_link_name;
        machine.owner_nodes = outer_sink != nullptr ? outer_sink : &scripted_nodes;
        // The file's placement: `origin + basis * local`, the same transform
        // place_mesh() gives every scene node of it.
        {
            tl::core::LayoutFrame frame;
            const Ogre::Vector3 x = basis * Ogre::Vector3::UNIT_X;
            const Ogre::Vector3 y = basis * Ogre::Vector3::UNIT_Y;
            const Ogre::Vector3 z = basis * Ogre::Vector3::UNIT_Z;
            frame.origin = {origin.x, origin.y, origin.z};
            frame.x = {x.x, x.y, x.z};
            frame.y = {y.x, y.y, y.z};
            frame.z = {z.x, z.y, z.z};
            machine.script.set_frame(frame);
        }
        machine.scene = placing_scene;
        machine.hierarchy = placing_hierarchy;
        nested_scripts.push_back(std::move(machine));
        node_sink = &nested_scripts.back().nodes;
        level_machines.push_back(&nested_scripts.back().script);
    }
    struct SinkGuard {
        NodeMap** slot;
        NodeMap* previous;
        ~SinkGuard() { *slot = previous; }
    } sink_guard{&node_sink, outer_sink};

    std::vector<MeshPlacement> mesh_placements;
    std::vector<MeshPlacement> collision_placements;
    struct NestedLayout {
        std::string reference;
        Ogre::Vector3 origin;
        Ogre::Quaternion basis;
        int64_t owner = 0;

        /// The link node itself, and what it is called. A linked layout with a
        /// script of its own belongs to this node: the parent's `Play` on it is
        /// what runs the machine inside. See NestedScript.
        int64_t link = 0;
        std::string link_name;

        /// A `Layout Link Particle`: the original loads its file through
        /// `CParticlePreloader::LoadParticle`,
        /// on the preloader's own `CLayout`,
        /// not as a nested load of the level's layout.
        bool particle = false;

        /// A particle link authored `START ON LOAD: false`:
        /// `CLayoutDescriptor::descriptorSceneActivated` calls
        /// `CLayout::stop(false)` on it, so its systems wait for `Start Particle`.
        bool stopped = false;

        /// A `Layout Link Timeline` left at `START ON LOAD` (its default):
        /// the same `descriptorSceneActivated` calls `CLayout::start` on it,
        /// `callFunctionOnObjects(0)`, which
        /// plays each of the linked layout's `LAYOUT CONTROL` timelines from
        /// the beginning.
        bool starts = false;

        /// A `Layout Link Timeline` authored `START ON LOAD: false`: the same
        /// call is `CLayout::stop(false)` on it. See the `held` branch below.
        bool held = false;

        /// A `Layout Link Timeline`'s `DURATION MODIFY` (`CLayout +0x1b4`, 0
        /// from the constructor).
        float duration_modify = 0.0f;
    };
    std::vector<NestedLayout> nested_layouts;
    size_t nested = 0;
    size_t particles = 0;
    size_t skipped_room_pieces = 0;
    std::vector<int64_t> skipped_room_guids;

    size_t hidden = 0;
    // `Unit Trigger`s whose unit names a `LAYOUTFILE`, placed once the nodes
    // are: see below.
    struct TriggerOwnLayout {
        int64_t node = 0;
        std::string path;
        Ogre::Vector3 position;
        Ogre::Quaternion orientation;
    };
    std::vector<TriggerOwnLayout> trigger_own_layouts;
    for (const tl::formats::CmpNode& node : layout.nodes) {
        if (!node.has_position()) continue;
        if (gated.count(node.id)) continue;

        // Field 0 is VISIBLE on every descriptor; a hidden node is placed hidden
        // (MeshPlacement::start_hidden) so a later Show finds it. A Monster's VISIBLE is
        // field 12 (LevelScript::build reads it there too).
        bool node_start_hidden = false;
        const size_t visible_field = node.type == tl::formats::cmp_type::kMonster
                                         ? tl::formats::cmp_field::monster::kVisible
                                         : tl::formats::cmp_field::linked::kVisible;
        if (const tl::formats::CmpField* visible = node.field(visible_field)) {
            if (visible->count() == 1 && !visible->as_bool()) {
                ++hidden;
                node_start_hidden = true;
            }
        }

        // A node's position and basis are in its layout's frame: the offset is rotated
        // before it is added, and the rotations compose.
        const Ogre::Vector3 local(node.position().x, node.position().y, node.position().z);
        const Ogre::Vector3 position = origin + basis * local;
        const Ogre::Quaternion orientation = basis * node_orientation(node);

        // `CLevel::loadRoomLayout`: a
        // `Property Node` typed `Quest Item` (12) joins `+0x108`, `Quest Boss`
        // (13) joins `+0xf0`.
        if (node.type == tl::formats::cmp_type::kMarker) {
            if (const tl::formats::CmpField* kind =
                    node.field(tl::formats::cmp_field::property_node::kType)) {
                const std::string text = kind->as_text();
                // CLevel::loadRoomLayout, cases 4 and 5:
                // automap tile 0x14 or 0x15, facing (1, 0, -1), shown once explored.
                if (automap_initialized && (text == "Entrance" || text == "Exit")) {
                    automap.add_tile(text == "Entrance" ? 0x14 : 0x15, position,
                                     Ogre::Vector3(1.0f, 0.0f, -1.0f), false, false);
                    TL_LOG_INFO("automap: %s marker at (%.2f, %.2f)", text.c_str(),
                                static_cast<double>(position.x), static_cast<double>(position.z));
                }
                if (text == "Quest Item") quest_item_markers.push_back({position, orientation});
                if (text == "Quest Boss") quest_boss_markers.push_back({position, orientation});
                // `Town Portal` (7) is `+0x14c` and `Waypoint Start` (15) `+0x158`,
                // whichever was met last.
                if (text == "Town Portal") {
                    town_portal_spot = position;
                    has_town_portal_spot = true;
                }
                if (text == "Waypoint Start") waypoint_spot = position;
                // Entry type 6's start: case 14 (`Waypoint`, the waygate's own
                // node) and case 15 alike set `+0x140`, the last one met.
                if (text == "Waypoint" || text == "Waypoint Start") {
                    waypoint_entry_spot = position;
                    has_waypoint_entry_spot = true;
                }
                // `No Spawn Region` (3) joins `+0xd8` after `positionUpdated`
                // has put its box through `updateBoundingBox`, in the
                // level's frame: the node's world position and the columns of its
                // world orientation's matrix. Scale as `apply_path_blocker` reads it.
                if (text == "No Spawn Region") {
                    namespace f = tl::formats::cmp_field::property_node;
                    const auto number = [&node](size_t index) {
                        const tl::formats::CmpField* field = node.field(index);
                        return field != nullptr && field->count() == 1 ? field->as_float() : 0.0f;
                    };
                    const float width = number(f::kBoxWidth);
                    const float length = number(f::kBoxLength);
                    const Ogre::Vector3 x = orientation.xAxis();
                    const Ogre::Vector3 y = orientation.yAxis();
                    const Ogre::Vector3 z = orientation.zAxis();
                    no_spawn_regions.push_back(tl::core::property_node_bounds(
                        {position.x, position.y, position.z}, {x.x, x.y, x.z}, {y.x, y.y, y.z},
                        {z.x, z.y, z.z}, width != 0.0f ? width : number(f::kRadius),
                        length != 0.0f ? length : 1.0f));
                }
            }
        }
        // ... and every `Warper` with a `DUNGEON` (`+0x110`, non-empty): tile
        // 0x15, or 0x22 for one marked `WAYPOINT` (`+0x109`), at the warper
        // (the loop over `GetObjectsCreatedByADescriptor("Warper")`).
        if (automap_initialized && node.type == tl::formats::cmp_type::kWarper) {
            namespace w = tl::formats::cmp_field::warper;
            const tl::formats::CmpField* dungeon = node.field(w::kDungeonName);
            if (dungeon != nullptr && !dungeon->as_text().empty()) {
                const tl::formats::CmpField* waypoint = node.field(w::kWaypoint);
                const bool is_waypoint =
                    waypoint != nullptr && waypoint->count() == 1 && waypoint->as_bool();
                automap.add_tile(is_waypoint ? 0x22 : 0x15, position,
                                 Ogre::Vector3(1.0f, 0.0f, -1.0f), false, false);
                TL_LOG_INFO("automap: warper to '%s' marked at (%.2f, %.2f)",
                            dungeon->as_text().c_str(), static_cast<double>(position.x),
                            static_cast<double>(position.z));
            }
        }

        if (node.type == tl::formats::cmp_type::kItem) {
            place_layout_item(node, position);
            continue;
        }

        // A prop's authored scale: Generic Model fields 6-8, Room Piece 11-13, default 1.
        // A Monster's unit may carry a scale the layout does not repeat.
        float unit_scale = 1.0f;
        if (node.type == tl::formats::cmp_type::kMonster) {
            namespace mf = tl::formats::cmp_field::monster;
            if (const tl::formats::CmpField* who = node.field(mf::kMonster)) {
                unit_scale = stats_for_unit(who->as_text()).scale;
            }
        }
        const Ogre::Vector3 node_scale = layout_scale(node, unit_scale);

        // A `Particle`'s MESH is its entity renderer's (`CParticleTechWrapper::
        // setModelPath`), not a model of the
        // layout's.
        std::string mesh_ref =
            node.type == tl::formats::cmp_type::kParticle ? std::string() : node.reference(".MESH");

        // A Unit Trigger names a unit, not a mesh: its geometry is that unit's
        // (trigger_prop()).
        TriggerProp trigger;
        std::string trigger_unit;
        Ogre::Vector3 trigger_scale = node_scale;
        if (mesh_ref.empty() && node.type == tl::formats::cmp_type::kUnitTrigger) {
            if (const tl::formats::CmpField* model =
                    node.field(tl::formats::cmp_field::unit_trigger::kModel)) {
                trigger_unit = model->as_text();
            }
            const bool has_model = !trigger_unit.empty() && trigger_prop(trigger_unit, trigger);
            if (!trigger_unit.empty()) {
                const std::string own = prop_layout_path(trigger_unit);
                if (!own.empty()) {
                    trigger_own_layouts.push_back({node.id, own, position, orientation});
                }
            }
            if (!trigger_unit.empty()) {
                // `CBaseUnit::unitInit`: the
                // unit's own scale, `SCALE + randomBetweenVolatile(0,
                // SCALE_VARIATION)`, drawn as it is made - here, or by
                // `populate` for a prop it made (`made_trigger_scale`).
                const auto drawn = made_trigger_scale.find(node.id);
                trigger_scale *= drawn != made_trigger_scale.end()
                                     ? drawn->second
                                     : trigger.scale + tl::core::volatile_random().between(
                                                           0.0f, trigger.scale_variation);
            }
            if (has_model) {
                mesh_ref = trigger.mesh;
            } else {
                // Nothing to draw, but a unit with a collision model still
                // blocks (`CTriggerUnit::getUnitCollisionModel`) until
                // a `Not Collidable` takes it out.
                if (!trigger_unit.empty() && !trigger.collision.empty() && trigger.collideable) {
                    const std::string collision_path = resolve_archive_path(trigger.collision);
                    if (collision_path.empty()) {
                        TL_LOG_WARN("  '%s' names collision '%s', not in the archive",
                                    trigger_unit.c_str(), trigger.collision.c_str());
                    } else {
                        MeshPlacement collision_placement;
                        collision_placement.mesh = collision_path;
                        collision_placement.node_id = node.id;
                        collision_placement.position = position;
                        collision_placement.orientation = orientation;
                        collision_placement.scale = trigger_scale;
                        collision_placement.collision_group = instance_key(
                            node.id, node_sink != nullptr ? *node_sink : scripted_nodes);
                        collision_placement.collision_blocks = trigger.block;
                        if (trigger.occupies_nodes) {
                            door_trigger_nodes.push_back(collision_placement.collision_group);
                        }
                        collision_placements.push_back(std::move(collision_placement));
                        TL_LOG_INFO("  unit trigger '%s' blocks with %s at (%.2f, %.2f, %.2f)",
                                    trigger_unit.c_str(), collision_path.c_str(),
                                    static_cast<double>(position.x),
                                    static_cast<double>(position.y),
                                    static_cast<double>(position.z));
                    }
                }
                trigger_unit.clear();
            }
        }

        if (!mesh_ref.empty()) {
            const std::string mesh_path = resolve_archive_path(mesh_ref);
            if (mesh_path.empty()) {
                TL_LOG_WARN("  no '%s' in the archive", mesh_ref.c_str());
                continue;
            }

            MeshPlacement placement;
            placement.mesh = mesh_path;
            placement.node_id = node.id;
            placement.position = position;
            placement.orientation = orientation;
            placement.scale = trigger_unit.empty() ? node_scale : trigger_scale;
            placement.start_hidden = node_start_hidden;
            // A Room Piece is made by CRoomPieceDescriptor::CreateObject,
            // a Generic Model by CGenericModelDescriptor::CreateObject,
            // constructor defaults before the properties.
            placement.room_piece = node.type == tl::formats::cmp_type::kRoomPiece;

            // The trigger prop's resting state and collision, authored on the unit; `unit`
            // registers it in scene_units so its Triggered reaches it.
            if (!trigger_unit.empty()) {
                placement.unit = trigger_unit;
                // The looping key when the unit authors one, otherwise the
                // state animation *once* - see TriggerProp. Otherwise a door
                // is left looping its `closing` animation.
                placement.animation =
                    trigger.resting_loop.empty() ? trigger.resting : trigger.resting_loop;
                placement.loops = !trigger.resting_loop.empty();
                placement.animation_rate = 1.0f;
                placement.trigger_resting_layout = trigger.state_layout[0];
                if (!trigger.collision.empty() && trigger.collideable) {
                    const std::string collision_path = resolve_archive_path(trigger.collision);
                    if (collision_path.empty()) {
                        TL_LOG_WARN("  '%s' names collision '%s', not in the archive",
                                    trigger_unit.c_str(), trigger.collision.c_str());
                    } else {
                        MeshPlacement collision_placement = placement;
                        collision_placement.mesh = collision_path;
                        collision_placement.animation.clear();
                        collision_placement.trigger_resting_layout.clear();
                        // Tagged with the node so Triggered can take it out again; one
                        // group per placed copy (instance_key()).
                        collision_placement.collision_group = instance_key(
                            node.id, node_sink != nullptr ? *node_sink : scripted_nodes);
                        collision_placement.collision_blocks = trigger.block;
                        // A map grid candidate once passability.bake() has run
                        // (door_trigger_nodes); not one authoring OCCUPIESNODES:False
                        // (CBaseUnit::addToAvoidanceMap, +0x18f).
                        if (trigger.occupies_nodes) {
                            door_trigger_nodes.push_back(collision_placement.collision_group);
                        }
                        collision_placements.push_back(std::move(collision_placement));
                    }
                }
                TL_LOG_INFO("  unit trigger '%s' draws %s at (%.2f, %.2f, %.2f)%s",
                            trigger_unit.c_str(), mesh_path.c_str(),
                            static_cast<double>(position.x), static_cast<double>(position.y),
                            static_cast<double>(position.z),
                            trigger.resting.empty() ? "" : ", resting state authored");
            }

            // What the layout asks this thing to play: Monster and Generic Model carry
            // ANIMATION, LOOPS and SPEED (play_scene_animation()).
            {
                size_t animation_index = 0, loops_index = 0, speed_index = 0, unit_index = 0;
                if (node.type == tl::formats::cmp_type::kMonster) {
                    namespace monster = tl::formats::cmp_field::monster;
                    // Somebody, not something: see `MeshPlacement::is_unit`.
                    placement.is_unit = true;
                    animation_index = monster::kAnimation;
                    loops_index = monster::kLoops;
                    speed_index = monster::kSpeed;
                    unit_index = monster::kMonster;
                } else if (node.type == tl::formats::cmp_type::kModel) {
                    namespace model = tl::formats::cmp_field::model;
                    animation_index = model::kAnimation;
                    loops_index = model::kLoops;
                    speed_index = model::kSpeed;
                }
                if (animation_index != 0) {
                    if (const tl::formats::CmpField* f = node.field(animation_index)) {
                        placement.animation = f->as_text();
                    }
                    if (const tl::formats::CmpField* f = node.field(loops_index)) {
                        if (f->count() == 1) placement.loops = f->as_bool();
                    }
                    if (const tl::formats::CmpField* f = node.field(speed_index)) {
                        if (f->count() == 1) {
                            const float value = f->as_float();
                            // A Monster's: CCharacter::setAnimationSpeed
                            // (X360 inline in the SPEED setter) keeps it as
                            // authored; ANIMATION, SPEED, LOOPS give playAnimation(index,
                            // LOOPS, SPEED) (ModelAnimator::set_animation_*), here by name.
                            if (node.type == tl::formats::cmp_type::kMonster ||
                                (value > 0.0f && value <= 100.0f)) {
                                placement.animation_rate = value;
                            }
                        }
                    }
                    if (unit_index != 0) {
                        if (const tl::formats::CmpField* f = node.field(unit_index)) {
                            placement.unit = f->as_text();
                        }
                    }
                    // A controller elsewhere in the layout wins: it is the later,
                    // more specific instruction, and it is how a monster placed
                    // in its bind pose is told to lie down dead.
                    const auto controlled = unit_animations.find(placement.unit);
                    if (!placement.unit.empty() && controlled != unit_animations.end()) {
                        placement.animation = controlled->second;
                    }
                }

                // INVENTED: a Monster with nothing else to play (or `bind`) plays its
                // IDLE: no unit data names an animation, the meshes ship an idle, and the
                // engine holds the name.
                if (node.type == tl::formats::cmp_type::kMonster &&
                    names_the_bind_pose(placement.animation)) {
                    placement.animation = kUnitIdleAnimation;
                    placement.idle_fallback = true;
                    placement.loops = true;
                    placement.animation_rate = 1.0f;
                }
            }

            // A Light is a pool drawn into the light map only. CLight:
            // scale (1, 0.25, 1), Y held at 0.25,
            // CLight::setRotation by ANGLE, setBitmapFile
            // the FILE mesh. CLevel::bake adds it
            // to static geometry on queue 0x5f once per LIGHT DENSITY (+0x100).
            // (CLightDescriptor::descriptorSceneActivated;
            // updateLightDensity is editor-only.)
            // CLightDescriptor::CreateObject;
            // CLight::CLight gives the fallbacks below.
            if (node.type == tl::formats::cmp_type::kLight) {
                using namespace tl::formats::cmp_field::light;
                float scale_x = 1.0f;
                float scale_z = 1.0f;
                float angle = 0.0f;
                if (const tl::formats::CmpField* f = node.field(kScaleX)) {
                    if (f->count() == 1) scale_x = f->as_float();
                }
                if (const tl::formats::CmpField* f = node.field(kScaleZ)) {
                    if (f->count() == 1) scale_z = f->as_float();
                }
                if (const tl::formats::CmpField* f = node.field(kAngle)) {
                    if (f->count() == 1) angle = f->as_float();
                }
                placement.light_density = 1;
                if (const tl::formats::CmpField* f = node.field(kBrightness)) {
                    if (f->count() == 1) placement.light_density = f->as_uint();
                }
                placement.scale = Ogre::Vector3(scale_x, 0.25f, scale_z);
                placement.orientation =
                    basis * Ogre::Quaternion(Ogre::Degree(angle), Ogre::Vector3::UNIT_Y);
                TL_LOG_INFO(
                    "  light %s at (%.2f, %.2f) scale (%.2f, %.2f) angle %.0f "
                    "density %u",
                    mesh_path.c_str(), position.x, position.z, scale_x, scale_z,
                    static_cast<double>(angle), placement.light_density);
            }

            mesh_placements.push_back(std::move(placement));
            continue;
        }

        // The four `Layout Link` kinds: `CLayoutDescriptor::CreateObject`
        // makes a `CLayout` of the
        // descriptor's own variant (`+0x170`).
        const std::string layout_ref = node.reference(".LAYOUT");
        if (!layout_ref.empty()) {
            const bool particle_link = node.type == tl::formats::cmp_type::kParticleLink;
            bool stopped = false;
            if (particle_link) {
                const tl::formats::CmpField* start =
                    node.field(tl::formats::cmp_field::particle_link::kStartOnLoad);
                stopped = start != nullptr && start->count() == 1 && !start->as_bool();
            }
            nested_layouts.push_back(NestedLayout{layout_ref, position, orientation,
                                                  particle_link ? node.id : particle_owner, node.id,
                                                  node.name, particle_link, stopped});
            if (node.type == tl::formats::cmp_type::kTimelineLink) {
                // `CLayout::setStartOnLoad`:
                // `+0x1a0`, 1 from the constructor; the rest of it runs in the
                // editor alone.
                const tl::formats::CmpField* start =
                    node.field(tl::formats::cmp_field::timeline_link::kStartOnLoad);
                nested_layouts.back().starts =
                    start == nullptr || start->count() != 1 || start->as_bool();
                nested_layouts.back().held = !nested_layouts.back().starts;
                const tl::formats::CmpField* modify =
                    node.field(tl::formats::cmp_field::timeline_link::kDurationModify);
                if (modify != nullptr && modify->count() == 1) {
                    nested_layouts.back().duration_modify = modify->as_float();
                }
            }
            continue;
        }

        if (node.type == tl::formats::cmp_type::kParticle) {
            if (config.no_particles) continue;
            placing_layout_path = path;
            if (place_particle(layout, node, position, orientation)) ++particles;
            continue;
        }

        if (node.type == tl::formats::cmp_type::kRoomPiece) {
            // A room piece names a 64-bit GUID as a decimal string (field 8), and
            // in field 9 (`PIECE`) either the word RANDOM or the variant index the
            // level designer picked. The GUID is looked up in the tileset index.
            load_tileset();

            int64_t guid = 0;
            bool have_guid = false;
            for (const std::string& text : node.strings) {
                if (text.empty()) continue;
                const bool numeric = (text[0] == '-' || (text[0] >= '0' && text[0] <= '9')) &&
                                     text.find_first_not_of("-0123456789", 1) == std::string::npos;
                if (!numeric) continue;
                errno = 0;
                char* end = nullptr;
                const long long value = std::strtoll(text.c_str(), &end, 10);
                if (end && *end == '\0' && errno == 0) {
                    guid = static_cast<int64_t>(value);
                    have_guid = true;
                    break;
                }
            }

            // GUID -1 names no piece (two of Necropolis 1's): nothing to place.
            // `CRoomPiece::setRoomPieceGuid`
            // asks for no record and makes nothing of it.
            if (have_guid && guid == -1) continue;
            // Any other GUID the index lacks is drawn as the ERROR piece:
            // CRoomPieceDataInformation::_GetRoomPieceCoreDataByGuid
            // answers the default.
            if (have_guid && tileset_known.count(guid) == 0 && tileset_default_guid != 0) {
                TL_LOG_WARN("  room piece %lld is not in the index; drawn as ERROR",
                            static_cast<long long>(guid));
                guid = tileset_default_guid;
            }
            auto piece = have_guid ? tileset.find(guid) : tileset.end();
            if (piece == tileset.end()) {
                // A piece with COLLISIONFILE and no FILE (ParseRoomPieceData
                // fills the two lists apart): no variant to roll; its collision
                // is the first listed.
                const auto only_collision =
                    have_guid ? tileset_collision.find(guid) : tileset_collision.end();
                if (only_collision == tileset_collision.end()) {
                    ++skipped_room_pieces;
                    if (skipped_room_guids.size() < 4) skipped_room_guids.push_back(guid);
                    continue;
                }
                bool collides = true;
                if (const tl::formats::CmpField* enabled =
                        node.field(tl::formats::cmp_field::room_piece::kCollisionEnabled)) {
                    if (enabled->count() == 1) collides = enabled->as_bool();
                }
                if (!collides) continue;
                const std::string& collision_file = only_collision->second.front();
                const std::string collision_path = resolve_archive_path(collision_file);
                if (collision_path.empty()) {
                    TL_LOG_WARN("  room piece %lld names collision '%s', not in the archive",
                                static_cast<long long>(guid), collision_file.c_str());
                    ++collision_skipped;
                    continue;
                }
                MeshPlacement collision_placement;
                collision_placement.mesh = collision_path;
                collision_placement.node_id = node.id;
                collision_placement.position = position;
                collision_placement.orientation = orientation;
                collision_placement.scale = node_scale;
                collision_placement.start_hidden = node_start_hidden;
                collision_placement.room_piece = true;
                collision_placements.push_back(std::move(collision_placement));
                continue;
            }

            // CRoomPiece::initRoomPiece: the variant
            // (+0x138) is PIECE, or 0xffffffff for RANDOM; one not below the count becomes 0,
            // and with several meshes it is rolled, randomIntegerBetween(0, count - 1) on
            // g_Rand. DIVERGENCE: rolled in file order, not the original's post-load walk
            // ordered by per-launch GUIDs, so a floor stays reproducible.
            const std::vector<std::string>& variants = piece->second;
            // `CRoomPiece::CRoomPiece`: no
            // variant (`+0x138` -1, `RANDOM`), baked (`+0x150`), colliding
            // (`+0x153`), on the map (`+0x155`), visible - each fallback here
            // and below.
            uint32_t variant = 0xffffffffu;
            if (const tl::formats::CmpField* piece_field =
                    node.field(tl::formats::cmp_field::room_piece::kVisual)) {
                const std::string authored = piece_field->as_text();
                if (!authored.empty() &&
                    authored.find_first_not_of("0123456789") == std::string::npos) {
                    variant = static_cast<uint32_t>(std::strtoul(authored.c_str(), nullptr, 10));
                }
            }
            const uint32_t variant_count = static_cast<uint32_t>(variants.size());
            if (variant_count <= variant) {
                variant = 0;
                if (variant_count != 1) {
                    variant = static_cast<uint32_t>(tl::core::game_random().integer_between(
                        0, static_cast<int>(variant_count) - 1));
                }
            }
            const std::string& variant_file = variants[variant];
            const std::string mesh_path = resolve_archive_path(variant_file);
            if (mesh_path.empty()) {
                TL_LOG_WARN("  room piece %lld names '%s', which is not in the archive",
                            static_cast<long long>(guid), variant_file.c_str());
                ++skipped_room_pieces;
                continue;
            }
            // Room-piece geometry is authored in world coordinates; the node position is
            // still applied (a small designer offset).
            MeshPlacement piece_placement;
            piece_placement.mesh = mesh_path;
            piece_placement.node_id = node.id;
            piece_placement.position = position;
            piece_placement.orientation = orientation;
            piece_placement.scale = node_scale;
            piece_placement.start_hidden = node_start_hidden;

            // The piece's collision at the same transform; COLLISION ENABLED is field 5,
            // default true.
            bool collides = true;
            if (const tl::formats::CmpField* enabled =
                    node.field(tl::formats::cmp_field::room_piece::kCollisionEnabled)) {
                if (enabled->count() == 1) collides = enabled->as_bool();
            }

            // The collision variant, as `initRoomPiece` picks it: the rolled index
            // when the record lists as many collision meshes as meshes, the first
            // one otherwise.
            const auto collision_entry = tileset_collision.find(guid);
            if (collides && collision_entry != tileset_collision.end()) {
                const std::vector<std::string>& collisions = collision_entry->second;
                const std::string& collision_file =
                    collisions.size() == variants.size() ? collisions[variant] : collisions.front();
                const std::string collision_path = resolve_archive_path(collision_file);
                if (collision_path.empty()) {
                    TL_LOG_WARN("  room piece %lld names collision '%s', not in the archive",
                                static_cast<long long>(guid), collision_file.c_str());
                    ++collision_skipped;
                } else {
                    MeshPlacement collision_placement = piece_placement;
                    collision_placement.mesh = collision_path;
                    // A piece with collision is one of `populate`'s sections.
                    collision_placement.room_piece = true;
                    collision_placements.push_back(std::move(collision_placement));
                }
            }

            piece_placement.room_piece = true;
            if (const tl::formats::CmpField* bake =
                    node.field(tl::formats::cmp_field::room_piece::kBake);
                bake != nullptr && bake->count() == 1) {
                piece_placement.bake = bake->as_bool();
            }
            if (const auto phase = tileset_render_phase.find(guid);
                phase != tileset_render_phase.end()) {
                piece_placement.render_phase = phase->second;
            }
            mesh_placements.push_back(std::move(piece_placement));
        }
    }

    // Every material script first, then every entity: a material may be defined by a
    // script of a later node.
    for (const auto& placement : mesh_placements) {
        parse_sibling_material(placement.mesh);
    }

    // Collision meshes' own material scripts are not parsed: a collision mesh is
    // never drawn, and the submesh material names that decide its faces come
    // from the serializer itself (see add_collision_mesh()).
    size_t animated_props = 0;
    size_t idled = 0;
    for (const auto& placement_from_layout : mesh_placements) {
        MeshPlacement placement = placement_from_layout;
        Ogre::Entity* entity = place_mesh(placement);

        // INVENTED: a skinned prop with no ANIMATION plays its idle (the mesh having a
        // skeleton is the instruction). Gated on hasSkeleton(), so the scan is rare.
        if (entity != nullptr && placement.animation.empty() && entity->hasSkeleton()) {
            placement.animation = kUnitIdleAnimation;
            placement.idle_fallback = true;
            placement.loops = true;
            placement.animation_rate = 1.0f;
        }

        const bool armed = placement.unit.empty() ? true : stats_for_unit(placement.unit).armed;
        Ogre::AnimationState* started = nullptr;
        if (entity && !placement.animation.empty() &&
            play_scene_animation(entity, placement.mesh, placement.animation, placement.loops,
                                 placement.animation_rate, placement.idle_fallback, &started,
                                 armed)) {
            remember_state(entity, placement.animation, started);
            ++animated_props;
            if (placement.idle_fallback) ++idled;
            if (placement.room_piece) {
                for (SceneAnimation& animation : scene_animations) {
                    if (animation.entity == entity) animation.room_piece = true;
                }
            }
        }
        if (entity && !placement.unit.empty()) {
            scene_units.emplace(placement.unit,
                                SceneUnit{entity, placement.mesh, placement.animation_rate,
                                          placement.node_id, placement.animation, placement.loops,
                                          placement.animation, started, 0.0f, armed});
            {
                const tl::core::UnitStats held = stats_for_unit(placement.unit);
                equip_unit_items(entity, placement.unit, held.right_hand, held.left_hand);
                apply_render_always(entity, held.render_always);
                // `CCharacter::loadModel`: `setRenderBehind`
                // with its `RENDERBEHIND`.
                set_render_behind(entity, held.render_behind, outline_hostile(held));
            }
            apply_rim_light(entity, rim_light_texture);
            if (!placement.trigger_resting_layout.empty()) {
                trigger_layout_props.emplace_back(entity, placement.unit);
                pending_trigger_layouts.emplace_back(entity, placement.unit);
            }
        }
    }
    const size_t meshes = mesh_placements.size();

    // Collision meshes are loaded for their triangles only, never placed in the scene.
    for (const auto& placement : collision_placements) {
        Ogre::Matrix4 to_world;
        to_world.makeTransform(placement.position, placement.scale, placement.orientation);
        add_collision_mesh(placement.mesh, to_world, placement.collision_group,
                           placement.room_piece ? &placement.position : nullptr);
        if (placement.collision_group != 0) {
            collision.set_group_blocks(placement.collision_group, placement.collision_blocks);
        }
    }

    // CTriggerUnit::unitInit: a LAYOUTFILE
    // gives a kind-0 CLayout (+0x240, X360 +0x1e8) under the trigger, shown, started; its
    // systems are the trigger's (CTriggerUnit::setVisible decides a Hide).
    if (!trigger_own_layouts.empty()) {
        NodeMap& sink = (node_sink != nullptr ? *node_sink : scripted_nodes);
        for (const TriggerOwnLayout& own : trigger_own_layouts) {
            const size_t before = particle_systems.size();
            place_particle_layout(own.path, own.position, own.orientation, nullptr,
                                  ParticleStart::Live);
            for (size_t i = before; i < particle_systems.size(); ++i) {
                particle_systems[i].owner = own.node;
                particle_systems[i].owner_nodes = &sink;
                particle_systems[i].trigger_own_layout = true;
            }
            TL_LOG_INFO("  unit trigger %lld carries its own layout %s (%zu system(s))",
                        static_cast<long long>(own.node), own.path.c_str(),
                        particle_systems.size() - before);
        }
    }

    // A Group places no mesh, so it is indexed onto its placed descendants' scene nodes:
    // a wire naming the group reaches them. Scoped to this layout's map; a nested link's
    // nodes are in its own NestedScript::nodes, as a wire cannot cross a file.
    {
        NodeMap& sink = (node_sink != nullptr ? *node_sink : scripted_nodes);
        std::map<int64_t, int64_t> parent_of;
        std::set<int64_t> group_ids;
        for (const tl::formats::CmpNode& n : layout.nodes) {
            parent_of.emplace(n.id, n.parent_id);
            if (n.type == tl::formats::cmp_type::kGroup) group_ids.insert(n.id);
        }

        const std::vector<std::pair<int64_t, Ogre::SceneNode*>> leaves(sink.begin(), sink.end());
        size_t group_entries = 0;
        for (const auto& leaf : leaves) {
            int64_t at = leaf.first;
            for (int depth = 0; depth < 16; ++depth) {
                const auto it = parent_of.find(at);
                if (it == parent_of.end() || it->second == -1) break;
                at = it->second;
                if (group_ids.count(at)) {
                    sink.emplace(at, leaf.second);
                    ++group_entries;
                }
            }
        }
        if (group_entries > 0) {
            TL_LOG_INFO(
                "  %zu group-ancestor entr%s added so Hide/Show can address a "
                "Group by id",
                group_entries, group_entries == 1 ? "y" : "ies");
        }
    }

    // What this file's wires and timelines can reach (Group entries included, gated
    // groups too), so bake_static_scenery() leaves it drawn on its own.
    {
        const NodeMap& sink = (node_sink != nullptr ? *node_sink : scripted_nodes);
        std::set<int64_t> addressed;
        for (const tl::formats::CmpLogicGroup& group : layout.logic_groups) {
            for (const tl::formats::CmpLogicObject& member : group.objects) {
                addressed.insert(member.object_id);
            }
        }
        for (const tl::formats::CmpTimeline& timeline : layout.timelines) {
            for (const tl::formats::CmpTimelineTarget& target : timeline.targets) {
                addressed.insert(target.object_id);
            }
        }
        for (const int64_t id : addressed) {
            const auto range = sink.equal_range(id);
            for (auto it = range.first; it != range.second; ++it) {
                scripted_scene_nodes.insert(it->second);
            }
        }
    }

    if (owns_a_script) {
        NestedScript& machine = nested_scripts[nested_script_index];
        machine.script.set_units(&unit_book);
        wire_script(machine.script);
        machine.script.build(layout, gated);
        init_unit_triggers(machine.script);
        log_puzzles(machine.script);
        TL_LOG_INFO(
            "  linked machine '%s' from %s: %zu object(s), %zu wire(s), "
            "%zu timeline(s)",
            machine.owner_name.c_str(), path.c_str(), machine.script.object_count(),
            machine.script.wire_count(), machine.script.timeline_count());

        // A machine with no owning Layout Link (nested_link == 0) can never be started
        // from outside (ScriptAction::NestedTimeline), so it activates itself as placed,
        // as a depth-0 layout does.
        if (nested_link == 0) {
            machine.running = true;
            // DIVERGENCE: headless with no pad, a Unit Trigger is answered by walking up
            // to it (enable_proximity_unit_triggers()).
            if (config.headless && !config.pad) {
                machine.script.enable_proximity_unit_triggers(
                    [this](const std::string& unit) { return is_enchant_shrine(unit); });
            }
        } else {
            // Activated with its level: CLayoutDescriptor::descriptorSceneActivated
            // calls CEditorScene::setActivated on
            // every linked layout, and CLayoutDescriptor::update steps each with
            // CLayout::update every frame.
            machine.running = true;
        }
        // A generated floor's rooms are activated by `CLevel::populate`
        // once the level is populated
        // (`defer_room_activation`).
        if (defer_room_activation) {
            deferred_room_machines.emplace_back(nested_script_index, path);
        } else {
            activate_room_machine(nested_script_index, path);
        }
    }

    for (const auto& nested_layout : nested_layouts) {
        const int64_t outer = particle_owner;
        const int64_t outer_link = nested_link;
        const std::string outer_link_name = nested_link_name;
        particle_owner = nested_layout.owner;
        nested_link = nested_layout.link;
        nested_link_name = nested_layout.link_name;
        // A particle layout does not reseed: entered through
        // CLayoutDescriptor::Set_loadLayoutFile, CLayout::loadLayoutFile neither reseeds
        // nor bumps the child count, and CParticlePreloader::LoadParticle loads the file.
        const bool outer_seeding = layout_seeding;
        if (nested_layout.particle) layout_seeding = false;
        const bool outer_particle_file = placing_particle_file;
        if (nested_layout.particle) placing_particle_file = true;
        link_places_next_layout = true;
        // A particle link's `CLayout` is of kind 1 (`Set_CLayoutDescriptor
        // KLAYOUT_TYPE_PARTICLEParam2`), so its file
        // is a `CParticle` once particles are cached; before that (the title's
        // menu) it is a live layout of its own.
        const bool link_cparticle = nested_layout.particle && renderer_detail::caching_particles();
        cparticle_places_next_layout = link_cparticle;
        const int64_t outer_hierarchy = placing_hierarchy;
        placing_hierarchy |= nested_layout.link;
        const size_t systems_before = particle_systems.size();
        const size_t machines_before = nested_scripts.size();
        if (place_layout(nested_layout.reference, nested_layout.origin, nested_layout.basis,
                         depth + 1)) {
            ++nested;
        }
        cparticle_places_next_layout = false;
        // `CLayout::setDurationModification`: the link's `DURATION
        // MODIFY`, set once its file is loaded and before anything starts it.
        if (0.0f < nested_layout.duration_modify) {
            for (size_t i = machines_before; i < nested_scripts.size(); ++i) {
                NestedScript& machine = nested_scripts[i];
                if (machine.owner != nested_layout.link) continue;
                machine.script.set_duration_modification(nested_layout.duration_modify);
            }
        }
        if (nested_layout.starts) {
            // CLayoutDescriptor::descriptorSceneActivated: then CLayout::start
            // on the machine this link just made, not every machine sharing its id.
            for (size_t i = machines_before; i < nested_scripts.size(); ++i) {
                NestedScript& machine = nested_scripts[i];
                if (machine.owner != nested_layout.link) continue;
                std::vector<tl::core::ScriptEvent> inner;
                machine.running = true;
                machine.script.play_layout_timelines(quest_log.state(), inner, /*backwards=*/false);
                if (!inner.empty()) apply_script_events(inner, machine.nodes);
                TL_LOG_INFO("  linked machine '%s' starts on load: %zu event(s)",
                            machine.owner_name.c_str(), inner.size());
            }
        } else if (nested_layout.held) {
            // CLayout::initOnLoad(): a link that does not
            // start on load takes START ON LOAD off every timeline of its layout.
            for (size_t i = machines_before; i < nested_scripts.size(); ++i) {
                NestedScript& machine = nested_scripts[i];
                if (machine.owner != nested_layout.link) continue;
                machine.script.init_on_load();
            }
        }
        // One `Layout Link Particle` is one `CParticle` over the whole file.
        if (nested_layout.particle) arm_particle_timer(systems_before, particle_systems.size());
        if (nested_layout.particle) {
            // Its own systems (an inner link's have their own owner) learn
            // which file's events name the link, and where they stand from it.
            const Ogre::Quaternion inverse = nested_layout.basis.Inverse();
            for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                ParticleSystem& system = particle_systems[i];
                if (system.owner != nested_layout.owner || system.claimed) continue;
                Ogre::SceneNode* holder = system.holder();
                if (holder == nullptr) continue;
                system.owner_nodes = node_sink != nullptr ? node_sink : &scripted_nodes;
                system.link_position = nested_layout.origin;
                system.link_orientation = nested_layout.basis;
                system.offset_from_link = inverse * (holder->getPosition() - nested_layout.origin);
                system.turn_from_link = inverse * holder->getOrientation();
            }
        }
        if (link_cparticle) {
            const int64_t link_owner = nested_layout.owner;
            const uint32_t id =
                register_cparticle(nested_layout.reference, systems_before, nested_layout.origin,
                                   [link_owner](const ParticleSystem& system) {
                                       return system.owner == link_owner && !system.claimed;
                                   });
            // `CLayoutDescriptor::descriptorSceneActivated`: `start`
            // when `START ON LOAD`, which is the `CParticle`'s `Start`.
            if (id != 0 && !nested_layout.stopped) cparticle_started(id);
            // `CLevel::findParticles`: a
            // link that starts on load joins the level's list unless one of
            // its `Particle` nodes has a `NO CULLING` technique.
            if (id != 0 && !nested_layout.stopped) {
                bool no_cull = false;
                for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                    const ParticleSystem& system = particle_systems[i];
                    if (system.cparticle != id) continue;
                    if (torchlight_particle_no_cull(system.pu)) no_cull = true;
                }
                PlacedParticle& placed = placed_particles[id];
                if (!no_cull && !placed.listed) {
                    placed.listed = true;
                    ++listed_particles;
                }
            }
        }
        if (nested_layout.stopped) {
            for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                ParticleSystem& system = particle_systems[i];
                // `CLayoutDescriptor::descriptorSceneActivated` stops it.
                if (system.pu.system != nullptr && !system.claimed) {
                    stop_torchlight_particle(system.pu);
                    system.finished = true;
                }
            }
        }
        placing_hierarchy = outer_hierarchy;
        layout_seeding = outer_seeding;
        placing_particle_file = outer_particle_file;
        particle_owner = outer;
        nested_link = outer_link;
        nested_link_name = outer_link_name;
    }

    if (skipped_room_pieces > 0) {
        std::string guids;
        for (const int64_t guid : skipped_room_guids) {
            guids += (guids.empty() ? "" : ", ") + std::to_string(guid);
        }
        TL_LOG_WARN("  %zu room piece(s) unresolved against the tileset index (%s)",
                    skipped_room_pieces, guids.c_str());
    }
    if (hidden > 0) TL_LOG_INFO("  %zu node(s) hidden by VISIBLE", hidden);
    TL_LOG_INFO(
        "  placed %zu mesh(es), %zu particle system(s), %zu nested layout(s), "
        "%zu animated (%zu of them on the default idle)",
        meshes, particles, nested, animated_props, idled);

    // The level's script (past the last node; CmpScene::logic_groups), reported even
    // when empty.
    if (layout.script_records > 0 || !layout.logic_groups.empty()) {
        TL_LOG_INFO("  script: %zu record(s), %zu logic group(s), %zu timeline(s)%s",
                    layout.script_records, layout.logic_groups.size(), layout.timelines.size(),
                    layout.script_undecoded > 0 ? " (some not decoded)" : "");
        if (layout.script_undecoded > 0) {
            TL_LOG_WARN("  %zu trailing record(s) of a kind this reader does not decode",
                        layout.script_undecoded);
        }
    }

    if (depth == 0) {
        // Where the player stands, recorded before the lighting is applied:
        // the light map starts there (aim_light_map()).
        if (const tl::formats::CmpNode* start = find_spawn(layout)) {
            if (start->has_position()) {
                player_start = origin + Ogre::Vector3(start->position().x, start->position().y,
                                                      start->position().z);
                have_player_start = true;
            }
        }
        apply_environment(path);
        aim_camera(layout, origin);
        // The sky has no position of its own; it follows the viewer.
        if (sky_node && camera_node) sky_node->setPosition(camera_node->getPosition());

        // Player Start is where the game puts the character, so it is where the
        // subject belongs - rather than the origin, which in this scene is tens
        // of units away from anything.
        if (const tl::formats::CmpNode* start = find_spawn(layout)) {
            stand_subject_at(*start, origin);
            if (subject_node != nullptr) {
                const Ogre::Vector3 f = subject_node->getOrientation() * Ogre::Vector3::UNIT_Z;
                player_start_yaw = std::atan2(f.x, f.z);
            }
        }

        // `CGameClient::loadLevel` ends in
        // `CLevel::updateNPCIcons`; the player is not in control yet, so it
        // runs on the first frame that it is.
        npc_icons_dirty = true;

        if (automap_initialized) {
            update_automap_icons();
            if (have_player_start) {
                automap.set_player_position(player_start, Ogre::Quaternion::IDENTITY);
                automap.update(player_start, 0.0f);
            }
        }
        log_scene_census(" as placed");

        // The level script, built once the whole tree is placed so a wire can find what
        // it drives. Top-level only; nested graphs run as their own machines.
        script_layout = layout;
        script_gated = gated;
        initial_quests = quest_log.state();
        script_ready = true;

        // Index the collision soup once the whole tree, nested layouts and all,
        // has contributed to it.
        collision.build();
        if (collision.empty()) {
            TL_LOG_WARN(
                "  no collision geometry: none of this layout's pieces names a "
                "COLLISIONFILE, so nothing will block a character");
        } else {
            TL_LOG_INFO("  collision: %zu face(s) from %zu mesh(es), %zu of them nocollide",
                        collision.triangle_count(), collision_meshes, collision.nocollide_count());
        }

        // Where a character may be routed, after the whole tree has contributed. A level
        // with no generated chunks is cast whole (CLevel::bakeCollision).
        nav.clear();
        passability.clear();
        if (!collision.empty()) {
            const unsigned long nav_started =
                root && root->getTimer() ? root->getTimer()->getMilliseconds() : 0;
            passability.bake(collision, nullptr);
            const unsigned long nav_took =
                (root && root->getTimer() ? root->getTimer()->getMilliseconds() : 0) - nav_started;
            log_passability(passability, static_cast<double>(nav_took) / 1000.0);
            if (!passability_dump_path.empty()) {
                write_passability_dump(passability, passability_dump_path);
            }
            // Before `nav.build()`, which takes a one-time snapshot of the map
            // grid's open cells - a door registered afterwards would be solid
            // in `passability` but walkable in `nav` forever.
            register_door_avoidance();
            nav.build(passability);
            TL_LOG_INFO("  navigation: %zu of %zu cell(s) walkable at %.2f units",
                        nav.walkable_count(), nav.cell_count(),
                        static_cast<double>(nav.cell_size()));
            apply_path_blockers(layout, gated);
        } else {
            door_trigger_nodes.clear();
        }
        // `CLevel::loadRoomLayout`: the map, drawn from the
        // grid just baked (none without one); a town's whole (its town flag).
        automap.create(passability, level_is_town);
        // A hunting monster routes through the same grid the player does,
        // and stands on the same floor - see LevelScript::set_collision().
        script.set_navigation(&nav);
        script.set_collision(&collision);

        // The script runs after the navigation grid: its Level Activated wires may switch
        // pathing blockers. Before the build, which makes the layout's characters.
        script.set_equipment_roller(
            [this](const std::string& unit, tl::core::LevelScript::UnitCreation& made) {
                roll_unit_equipment(unit, made);
            });
        script.set_unit_equipment(unit_equipment_hooks());
        // The level's observer map (`CLevel +0x200`) is the level's own.
        unit_observers.clear();
        skill_observers.clear();
        script.build(layout, gated);
        init_unit_triggers(script);
        log_puzzles(script);
        // `CGameClient::loadLevel`: once the level's
        // layout is built, `setSeed(0)` and `setSeedVolatile(0)` - the clock.
        tl::core::reseed_level_generators();
        // A Unit Trigger is acted on with A through the console's second target pass.
        // DIVERGENCE: with nobody holding a
        // pad (headless), walking up to it fires it.
        if (config.headless && !config.pad) {
            script.enable_proximity_unit_triggers(
                [this](const std::string& unit) { return is_enchant_shrine(unit); });
        }

        // And the level's hostile units go looking for the player. Nothing in
        // a layout wires this - a dungeon's monsters carry no wiring at all -
        // because in the original it is what being hostile *means*.
        if (const size_t armed = script.arm_hostiles()) {
            TL_LOG_INFO("  %zu hostile unit(s) will come after the player", armed);
        }
        if (script.wire_count() > 0 || script.trigger_count() > 0 || script.has_sound_nodes()) {
            TL_LOG_INFO(
                "  level script: %zu object(s), %zu wire(s), %zu trigger(s), "
                "%zu timeline(s)",
                script.object_count(), script.wire_count(), script.trigger_count(),
                script.timeline_count());

            std::vector<tl::core::ScriptEvent> events;
            if (logic_trace != nullptr) std::fputs("# level load\n", logic_trace);
            script.set_has_saved_state(level_has_saved_state);
            script.set_stored_unit_exists([this](int64_t id) { return level_stores_unit(id); });
            script.level_activated(quest_log.state(), events);
            flush_logic_trace(script);
            apply_script_events(events, scripted_nodes);
            if (!events.empty()) {
                TL_LOG_INFO("  level activated: %zu event(s) fired", events.size());
            }
        }

        if (collision_skipped > 0) {
            TL_LOG_WARN("  %zu collision mesh(es) named but not loaded", collision_skipped);
        }

        log_memory("after the layout");
    }

    layouts_in_progress.erase(path);
    return true;
}

/// Find the authored marker inside a placed exit chunk, if it has one: the chunk links
/// a shared prop (EXIT_NODE.LAYOUT or a `_PORTAL_NODE` one) holding a Property Node, a
/// Warper, a Unit Trigger and a Logic Group wiring Triggered to Activate Warper. Only the
/// marker's position and radius are read; one Layout Link deep.
bool Renderer::Impl::find_chunk_marker(const std::string& chunk_cmp_path,
                                       const Ogre::Vector3& chunk_origin,
                                       const std::string& wanted_type,
                                       Ogre::Vector3& world_position, float& radius,
                                       ChunkMarkerExtra* extra) {
    const tl::formats::CmpScene* const parsed = layout_file(chunk_cmp_path);
    if (parsed == nullptr || !parsed->ok) return false;
    const tl::formats::CmpScene& scene = *parsed;

    namespace node_field = tl::formats::cmp_field::property_node;
    for (const tl::formats::CmpNode& link : scene.nodes) {
        if (link.type != tl::formats::cmp_type::kLayoutLink) continue;
        const std::string reference = link.reference(".layout");
        if (reference.empty()) continue;
        // The reference names the layout, not the compiled file: resolved in two steps as
        // place_layout() does for a nested Layout Link.
        std::string sub_path = resolve_archive_path(reference);
        if (sub_path.empty()) sub_path = resolve_archive_path(reference + ".cmp");
        if (sub_path.empty()) continue;
        const tl::formats::CmpScene* const parsed_sub = layout_file(sub_path);
        if (parsed_sub == nullptr || !parsed_sub->ok) continue;
        const tl::formats::CmpScene& sub = *parsed_sub;

        const Ogre::Vector3 link_local =
            link.has_position()
                ? Ogre::Vector3(link.position().x, link.position().y, link.position().z)
                : Ogre::Vector3::ZERO;
        const Ogre::Quaternion link_turn = node_orientation(link);
        // A node's forward as the level reads it: the z axis of its world
        // orientation, flattened and normalised (`CLevel::loadRoomLayout`
        // ahead of its `Property Node` switch).
        const auto flat_forward = [&link_turn](const Ogre::Quaternion& own) {
            Ogre::Vector3 forward = (link_turn * own) * Ogre::Vector3::UNIT_Z;
            forward.y = 0.0f;
            const float length = forward.length();
            if (1e-08f < length) forward /= length;
            return forward;
        };

        for (const tl::formats::CmpNode& inner : sub.nodes) {
            if (inner.type != tl::formats::cmp_type::kMarker) continue;
            const tl::formats::CmpField* type_field = inner.field(node_field::kType);
            if (type_field == nullptr || lowered(type_field->as_text()) != wanted_type) continue;

            const Ogre::Vector3 inner_local =
                inner.has_position()
                    ? Ogre::Vector3(inner.position().x, inner.position().y, inner.position().z)
                    : Ogre::Vector3::ZERO;
            world_position = chunk_origin + link_local + link_turn * inner_local;
            const tl::formats::CmpField* radius_field = inner.field(node_field::kRadius);
            radius = (radius_field != nullptr && radius_field->count() > 0)
                         ? radius_field->as_float()
                         : 3.0f;
            if (extra != nullptr) {
                extra->forward = flat_forward(node_orientation(inner));
                // The Warper the same layout holds: where placePlayerAtWarpToDungeonFloor
                // stands a player who came through its
                // twin, turned its getForwardAbsolute().
                for (const tl::formats::CmpNode& warper : sub.nodes) {
                    if (warper.type != tl::formats::cmp_type::kWarper) continue;
                    const Ogre::Vector3 warper_local =
                        warper.has_position()
                            ? Ogre::Vector3(warper.position().x, warper.position().y,
                                            warper.position().z)
                            : Ogre::Vector3::ZERO;
                    extra->has_warper = true;
                    extra->warper_position = chunk_origin + link_local + link_turn * warper_local;
                    Ogre::Vector3 own_forward = Ogre::Vector3::UNIT_Z;
                    if (const tl::formats::CmpField* f =
                            warper.field(tl::formats::cmp_field::warper::kForward)) {
                        if (f->count() == 3) {
                            own_forward = {f->as_float(0), f->as_float(1), f->as_float(2)};
                        }
                    }
                    extra->warper_forward = link_turn * own_forward;
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

// CGameClient::loadLevel, after the level is created:
// the depth's MUST_BE_COMPLETED quests not complete are completed
// (CQuestManager::completeQuest), and MUST_BE_COMPLETED_OR_ACTIVE ones neither complete
// nor active are given (giveQuest(quest, nullptr, false), no banner), accepted and marked
// interacted.
void Renderer::Impl::apply_floor_quests(const tl::core::DungeonDef& def, uint32_t depth) {
    if (depth >= def.depth_strata.size()) return;
    const uint32_t index = def.depth_strata[depth];
    if (index >= def.strata.size()) return;
    const tl::core::DungeonStrata& stratum = def.strata[index];
    if (stratum.must_be_completed.empty() && stratum.must_be_completed_or_active.empty()) return;
    load_quest_book();
    load_stat_graphs();
    for (const std::string& name : stratum.must_be_completed) {
        const tl::core::QuestDefinition* quest = quest_book.find(name);
        if (quest == nullptr) continue;
        if (quest_log.complete_quest(*quest)) {
            TL_LOG_INFO("floor quests: '%s' completed", quest->name.c_str());
        }
    }
    for (const std::string& name : stratum.must_be_completed_or_active) {
        const tl::core::QuestDefinition* quest = quest_book.find(name);
        if (quest == nullptr) continue;
        const tl::core::QuestStatus status = quest_log.state().status(quest->name);
        if (status == tl::core::QuestStatus::Complete || status == tl::core::QuestStatus::Active) {
            continue;
        }
        // Given with no NPC, so `CQuest +0x1c` keeps the constructor's -1:
        // `initializeQuestWithNPC` rolls a seed only for a non-null NPC.
        // `getDialog` then draws without reseeding `g_Rand`.
        const uint32_t seed = static_cast<uint32_t>(-1);
        if (!quest_log.accept(*quest, quest_level_of(*quest), seed, stat_graphs)) continue;
        if (tl::core::QuestEntry* held = quest_log.find(quest->name))
            held->dialog_interacted = true;
        TL_LOG_INFO("floor quests: '%s' given", quest->name.c_str());
    }
}

/// The process's `CDungeonManager`, built the first time anything asks, with
/// what the original does once at start on either side of it:
/// `CMasterResourceManager`'s constructor makes it over `media/dungeons/`,
/// and its constructor reloads it at once.
tl::core::DungeonManager& loaded_dungeon_manager() {
    tl::core::DungeonManager& manager = tl::core::dungeon_manager();
    if (manager.loaded) return manager;
    // `CGame::CGame` comes first: `srand(time(0))`.
    tl::core::seed_crt_from_time();
    // `getFileList(L"MEDIA/DUNGEONS/", list, L"*.dat", ...)`: in pak mode
    // `getFilesInDirectory` lists the archive's `*.dat.*` entries in the zip's
    // own order and cuts the compiled extension; `CDungeon::loadDungeon` reads
    // each. A file that is no dungeon still makes a `CDungeon` and takes a salt.
    std::vector<tl::core::DungeonDef> definitions;
    for (const std::string& path : archive_paths_in_listing_order("media/dungeons/", ".dat.adm")) {
        const std::vector<uint8_t> bytes = read_resource(path);
        tl::core::DungeonDef definition;
        if (!bytes.empty()) {
            definition =
                tl::core::parse_dungeon_def(tl::formats::parse_adm(bytes.data(), bytes.size()));
        }
        definitions.push_back(std::move(definition));
    }
    manager.reload(std::move(definitions), tl::core::game_random(), tl::core::volatile_random());
    // And `CGameClient::CGameClient` after it: the game seed.
    tl::core::game_seed().seed =
        tl::core::roll_game_seed(tl::core::kLevelSeedSetting, tl::core::volatile_random());
    TL_LOG_INFO("dungeons: %zu loaded, each with its own salt", manager.dungeons.size());
    return manager;
}

/// The file-system questions a `Dungeon` and its templates ask, answered from
/// the mounted archive, and `CDungeonManager::getDungeonByName` from the
/// process's manager.
tl::core::DungeonFiles archive_dungeon_files() {
    // The level template, as CLevelTemplateData::load builds it, its file listings as
    // CFileSystem::getFilesInDirectory names files:
    // upper-cased, the compiled extension cut.
    tl::core::DungeonFiles dungeon_files;
    tl::core::LevelTemplateFiles& template_files = dungeon_files.layouts;
    template_files.exists = [](const std::string& layout) {
        return !resolve_archive_path(layout + ".cmp").empty();
    };
    template_files.list_layouts = [](const std::string& folder) {
        std::vector<std::string> listed;
        for (const std::string& path : archive_paths_under(folder, ".layout.cmp")) {
            std::string name = path;
            for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            name.resize(name.size() - 4);
            listed.push_back(std::move(name));
        }
        return listed;
    };
    dungeon_files.read_ruleset = [](const std::string& ruleset) {
        const std::string path = resolve_archive_path(ruleset + ".adm");
        if (path.empty()) return tl::formats::AdmFile{};
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) return tl::formats::AdmFile{};
        return tl::formats::parse_adm(bytes.data(), bytes.size());
    };
    dungeon_files.dungeon_by_name = [](const std::string& upper_name) {
        return loaded_dungeon_manager().dungeon_by_name(upper_name);
    };
    return dungeon_files;
}

std::string Renderer::menu_layout_for(const std::string& dungeon_name, int32_t depth) {
    const tl::core::DungeonFiles files = archive_dungeon_files();

    // `CGameClient::loadMenuLevel`:
    // `setCurrentDungeon(name)`, `resetGameSeed(depth)`, then
    // `getLevelTemplateDataForDepth(depth)`. With no template,
    // `mainmenu_townrules.dat`.
    std::string rules = "media/layouts/mainmenus/mainmenu_townrules.dat";
    tl::core::RunicRandom& rng = tl::core::game_random();
    tl::core::GameSeed& seed = tl::core::game_seed();
    seed.current_dungeon = loaded_dungeon_manager().dungeon_by_name(dungeon_name);
    tl::core::reset_game_seed(seed, depth, rng);
    if (tl::core::Dungeon* dungeon = seed.current_dungeon) {
        const tl::core::LevelTemplate* found = dungeon->get_level_template_for_depth(
            static_cast<uint32_t>(std::max(0, depth)), files, rng);
        // INVENTED: a template that names no `MAINMENURULES` - none ships
        // without one that a save can stand on - keeps the town's.
        if (found != nullptr && !found->main_menu_rules.empty()) rules = found->main_menu_rules;
    }

    // `CLevel::loadRoomLayout(rules, 0, 3)`: the rules' own template, and the
    // one room piece its single chunk holds.
    tl::core::LevelTemplate backdrop;
    backdrop.load(rules, files.read_ruleset(rules), files.layouts, rng);
    const tl::core::LevelLayout* layout = tl::core::choose_room_layout(
        backdrop, rng, tl::core::level_seed(tl::core::kLevelSeedSetting, seed, depth));
    std::string chosen;
    if (layout != nullptr && !layout->pieces.empty() &&
        layout->pieces[0] < backdrop.chunks.size()) {
        chosen = backdrop.chunks[layout->pieces[0]].path;
    }
    if (impl_ && !chosen.empty()) impl_->backdrop_rules[Impl::backdrop_key(chosen)] = rules;
    TL_LOG_INFO("main menu: '%s' depth %d -> %s -> %s", dungeon_name.c_str(), depth, rules.c_str(),
                chosen.empty() ? "(nothing)" : chosen.c_str());
    return chosen;
}

// `CResourceManager::createUnit` and
// `CLevel::addItem` for a `CREATEAS: TRIGGER`
// prop: the port lays down a layout of one `Unit Trigger` node (the header).
bool Renderer::Impl::place_populated_trigger(const std::string& unit, const Ogre::Vector3& at,
                                             const Ogre::Quaternion& facing, int64_t id,
                                             float scale) {
    TriggerProp prop;
    if (!trigger_prop(unit, prop)) return false;

    using tl::formats::CmpField;
    const auto vec = [](float x, float y, float z) {
        CmpField f;
        for (float v : {x, y, z}) {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof bits);
            f.words.push_back(bits);
        }
        return f;
    };
    const auto number = [](uint32_t value) {
        CmpField f;
        f.words.push_back(value);
        return f;
    };
    // A string field: UTF-16LE with a NUL, padded to whole dwords.
    const auto text = [](const std::string& value) {
        const size_t dwords = ((value.size() + 1) * 2 + 3) / 4;
        CmpField f;
        f.words.assign(dwords, 0u);
        for (size_t i = 0; i < value.size(); ++i) {
            f.words[i / 2] |= static_cast<uint32_t>(static_cast<unsigned char>(value[i]))
                              << (16 * (i % 2));
        }
        return f;
    };

    tl::formats::CmpNode node;
    node.type = tl::formats::cmp_type::kUnitTrigger;
    node.name = unit;
    node.id = id;
    node.parent_id = -1;
    node.root = true;
    // `unit_trigger`'s fields in their order: the position and the three axes
    // (the layout is put down already turned), enabled, the unit, and the
    // rest at what an unauthored node holds.
    node.fields = {vec(0.0f, 0.0f, 0.0f),
                   vec(0.0f, 0.0f, 1.0f),
                   vec(1.0f, 0.0f, 0.0f),
                   vec(0.0f, 1.0f, 0.0f),
                   number(1),
                   text(unit),
                   number(0),
                   text(""),
                   text(""),
                   number(0),
                   number(0)};
    node.vec3s = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    node.strings = {unit};

    auto scene = std::make_unique<tl::formats::CmpScene>();
    scene->ok = true;
    scene->nodes.push_back(std::move(node));
    const std::string key = "populate:" + unit + "#" + std::to_string(id);
    layout_files[key] = std::move(scene);
    made_trigger_scale[id] = scale;
    const bool placed = place_layout(key, at, facing, /*depth=*/1);
    made_trigger_scale.erase(id);
    if (!placed) return false;
    populated_triggers[id] = PopulatedTrigger{unit, facing};
    return true;
}

// `CLevel::randomOpenPosition(p, radius, true)`, as the population
// phases ask it.
tl::core::Vec3 Renderer::Impl::populate_open_position(const tl::core::Vec3& at, float radius) {
    return tl::core::random_open_position(
        at, 0.0f, radius,
        [this](const tl::core::Vec3& p) {
            return !passability.ready() ||
                   (passability.map_passable(p) && passability.position_passable(p) &&
                    !tl::core::is_in_no_spawn_region(no_spawn_regions, p, 0.0f));
        },
        tl::core::volatile_random());
}

// CCharacter::createPathfinder(CLevel&), called by
// CLevel::addCharacter: MINION_SPAWNCLASS (+0x6b8)
// rolled once - rollSpawnClass(out, null, this, this, -1, 0xffffffff, 0, -1, 0, 0) on
// g_Rand - and cleared.
std::vector<std::string> Renderer::Impl::roll_unit_minions(const std::string& unit, int level) {
    std::vector<std::string> rolled;
    const std::string spawn_class = stats_for_unit(unit).minion_spawnclass;
    if (spawn_class.empty()) return rolled;
    SpawnTables tables;
    build_spawn_context(tables, /*items_by_type=*/false);
    const std::optional<tl::core::SpawnClass> loaded =
        load_spawn_class(spawn_class, tables.context);
    // `getSpawnClassByName` finding none leaves `+0x6b8` null.
    if (!loaded) return rolled;
    tl::core::SpawnCharacterContext self;
    self.level = level;
    loaded->roll_spawn_class(rolled, nullptr, &self, &self, -1, 0xffffffffu, /*level_bonus=*/0,
                             /*rarity_override=*/-1, /*magic_find=*/0, /*flags=*/0,
                             tl::core::game_random(), tables.context);
    return rolled;
}

// The same function's loop: each unit made at the level's monster
// level, turned randomBetweenVolatile(0, 360), a character placed at
// randomOpenPositionRange(master, 1, 4, true) and made the master's pet.
//
void Renderer::Impl::place_unit_minions(int64_t master, int64_t owner,
                                        const std::vector<std::string>& minions) {
    for (const std::string& name : minions) {
        // Not a character: made and left out of the level.
        ensure_monster_index();
        if (monster_files.count(lowered(name)) == 0) continue;
        index_unit(name);
        const tl::core::LevelScript::UnitCreation creation = script.roll_unit_creation(name);
        const float yaw = tl::core::volatile_random().between(0.0f, 360.0f) * 0.017453292f;
        tl::core::Vec3 from;
        if (!script.unit_position(master, from)) return;
        const tl::core::Vec3 at = tl::core::random_open_position(
            from, 1.0f, 4.0f,
            [this](const tl::core::Vec3& p) {
                return !passability.ready() ||
                       (passability.map_passable(p) && passability.position_passable(p) &&
                        !tl::core::is_in_no_spawn_region(no_spawn_regions, p, 0.0f));
            },
            tl::core::volatile_random());
        // `CLevel::addCharacter(minion, at, false)`: on the ground.
        const Ogre::Quaternion turn(Ogre::Radian(yaw), Ogre::Vector3::UNIT_Y);
        const Ogre::Vector3 place = dropped_to_ground(Ogre::Vector3(at.x, at.y, at.z));
        if (spawn_unit(name, place, owner, level_monster_level, /*champion=*/false,
                       /*spawn_anim=*/false, turn, creation.scale) == nullptr) {
            continue;
        }
        const int64_t id = spawned[owner].back().id;
        const Ogre::Vector3 forward = turn * Ogre::Vector3::UNIT_Z;
        script.add_spawned_unit(id, name, tl::core::Vec3{place.x, place.y, place.z},
                                /*hunts=*/false, std::atan2(forward.x, forward.z), &creation);
        int made_level = 0;
        if (0 < level_monster_level &&
            !(script.unit_level(id, made_level) && made_level == level_monster_level)) {
            apply_spawn_level(id);
        }
        // ... which rolls a minion's own minions, before its master takes it.
        spawn_unit_minions(id, owner);
        add_monster_pet(script, id, script, master);
        TL_LOG_INFO("  minion: %s beside %s at (%.2f, %.2f, %.2f)", name.c_str(),
                    script.unit_of(master).c_str(), static_cast<double>(place.x),
                    static_cast<double>(place.y), static_cast<double>(place.z));
    }
}

// `CCharacter::createPathfinder(CLevel&)`'s
// minions for a unit: rolled at its level and placed (`place_unit_minions`).
void Renderer::Impl::spawn_unit_minions(int64_t master, int64_t owner) {
    const std::string unit = script.unit_of(master);
    if (unit.empty()) return;
    int level = 0;
    if (!script.unit_level(master, level)) level = level_monster_level;
    place_unit_minions(master, owner, roll_unit_minions(unit, level));
}

// `[FORMATION]`s of a template's `FORMATIONS` file (`CLevelTemplateData +0x5b0`),
// as `CLevel::populateFormations` reads them:
// every child of the file in order, its `FILE` and its `WEIGHT` (1 when
// unauthored).
std::vector<tl::core::FormationChoice> Renderer::Impl::formation_choices(const std::string& table) {
    std::vector<tl::core::FormationChoice> choices;
    const std::string path = resolve_archive_path(table + ".adm");
    if (path.empty()) return choices;
    const std::vector<uint8_t> bytes = read_resource(path);
    if (bytes.empty()) return choices;
    const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!file.ok) return choices;
    for (const tl::formats::AdmSection& child : file.root.children) {
        tl::core::FormationChoice choice;
        choice.file = child.string_or("FILE", "");
        choice.weight = static_cast<int32_t>(child.integer_or("WEIGHT", 1u));
        choices.push_back(std::move(choice));
    }
    return choices;
}

int Renderer::Impl::populate_units(const std::string& spawn_class, uint32_t wanted, int level,
                                   tl::core::Randomizer& sections, const PopulatePhase& phase) {
    if (wanted == 0) return 0;
    if (population_sections.empty()) {
        TL_LOG_WARN("  %s: no room piece made a section; none placed", phase.label);
        return 0;
    }

    SpawnTables tables;
    build_spawn_context(tables, /*items_by_type=*/false);
    std::optional<tl::core::SpawnClass> loaded;
    if (!spawn_class.empty()) loaded = load_spawn_class(spawn_class, tables.context);
    // No such class (`NONE` is what a template names for a phase it leaves
    // out): the phase still runs, as the original's does, and places nothing.
    if (!loaded) TL_LOG_INFO("  %s: no spawn class '%s'", phase.label, spawn_class.c_str());

    tl::core::RunicRandom& g_rand = tl::core::game_random();
    tl::core::RunicRandom& volatile_rolls = tl::core::volatile_random();

    // `CLevel +0xf0`, the `Quest Boss` property nodes, and how often each has
    // been stood on (`CPropertyNode +0x114`).
    std::vector<tl::core::SpawnMarker> markers;
    if (phase.markers) {
        for (const QuestMarker& marker : quest_boss_markers) {
            tl::core::SpawnMarker m;
            m.position = {marker.position.x, marker.position.y, marker.position.z};
            m.orientation = {marker.orientation.w, marker.orientation.x, marker.orientation.y,
                             marker.orientation.z};
            m.taken = marker.taken;
            markers.push_back(m);
        }
    }

    const auto turn_of = [](const tl::core::PopulatedFacing& facing) {
        if (facing.from_marker) {
            return Ogre::Quaternion(facing.orientation[0], facing.orientation[1],
                                    facing.orientation[2], facing.orientation[3]);
        }
        return Ogre::Quaternion(Ogre::Radian(facing.yaw), Ogre::Vector3::UNIT_Y);
    };

    int breakables = 0;
    int triggers = 0;
    std::map<std::string, int> counts;
    // What `createUnit` drew for a unit, by handle, until it is added.
    std::map<int64_t, tl::core::LevelScript::UnitCreation> made_characters;
    std::map<int64_t, float> made_scales;
    int64_t handles = 0;

    tl::core::PopulateWorld world;
    world.volatile_rolls = &volatile_rolls;
    world.markers = phase.markers ? &markers : nullptr;
    world.roll = [&](const std::string&) {
        // `rollSpawnClass(out, null, null, null, CLevel +0x1a8, 0xffffffff, 0, -1, 0, 0)`:
        // on `g_Rand` as it stands.
        std::vector<std::string> rolled;
        if (!loaded) return rolled;
        loaded->roll_spawn_class(rolled, nullptr, nullptr, nullptr, level, 0xffffffffu,
                                 /*level_bonus=*/0, /*rarity_override=*/-1, /*magic_find=*/0,
                                 /*flags=*/0, g_rand, tables.context);
        return rolled;
    };
    world.random_open_position = [this](const tl::core::Vec3& at, float radius) {
        return populate_open_position(at, radius);
    };
    world.in_no_spawn_region = [this](const tl::core::Vec3& at, float radius) {
        return tl::core::is_in_no_spawn_region(no_spawn_regions, at, radius);
    };
    world.passable_between = [this](const tl::core::Vec3& from, const tl::core::Vec3& to) {
        return !passability.ready() || passability.passable_between(from, to, /*map_only=*/false);
    };
    world.create = [&](const std::string& unit) {
        tl::core::PopulatedUnit made;
        made.handle = ++handles;
        ensure_prop_index();
        const auto prop_file = prop_files.find(lowered(unit));
        if (prop_file == prop_files.end()) {
            // Not a prop: a character (the props class's `Mimic` among them).
            ensure_monster_index();
            if (monster_files.count(lowered(unit)) == 0) return tl::core::PopulatedUnit{};
            // Its numbers, which making it reads.
            index_unit(unit);
            made.kind = tl::core::PopulatedKind::Character;
            made_characters[made.handle] = script.roll_unit_creation(unit);
            return made;
        }
        const tl::formats::AdmFile file = read_unit_file(prop_file->second);
        if (!file.ok) return tl::core::PopulatedUnit{};
        made.kind = tl::core::PopulatedKind::Item;
        if (is_breakable_unit(unit)) {
            // `CBaseUnit::unitInit`, through
            // `CItem::unitInit`: the scale. A breakable (`BREAKABLE` ->
            // `CONTAINER` -> `ITEM`) never blocks.
            made_scales[made.handle] =
                tl::core::breakable_scale(tl::core::read_breakable(file), volatile_rolls);
            return made;
        }
        // A `CREATEAS: TRIGGER` prop (the chests).
        TriggerProp prop;
        if (!trigger_prop(unit, prop)) {
            TL_LOG_WARN("  %s: '%s' is neither a breakable nor a trigger prop", phase.label,
                        unit.c_str());
            return tl::core::PopulatedUnit{};
        }
        // `CBaseUnit::unitInit` draws its
        // scale as it is made.
        made_scales[made.handle] = prop.scale + volatile_rolls.between(0.0f, prop.scale_variation);
        // `+0x18d`, `+0x19c` and `ISA 0x20`: a collideable chest.
        made.blocks = prop.collideable && !prop.collision.empty() &&
                      unit_types.isa(prop.unittype, tl::core::unit_type::kInteractable);
        return made;
    };
    world.add_character = [&](const tl::core::PopulatedUnit& made, const std::string& unit,
                              const tl::core::Vec3& at, const tl::core::PopulatedFacing& facing,
                              bool champion) {
        const auto creation = made_characters.find(made.handle);
        if (creation == made_characters.end()) return false;
        // `CLevel::addCharacter(unit, at, false)`:
        // on the ground. `makeChampion(name,
        // level + 1)` is `spawn_unit`'s.
        const Ogre::Quaternion turn = turn_of(facing);
        const Ogre::Vector3 forward = turn * Ogre::Vector3::UNIT_Z;
        const Ogre::Vector3 place = dropped_to_ground(Ogre::Vector3(at.x, at.y, at.z));
        if (spawn_unit(unit, place, phase.owner, level, champion, /*spawn_anim=*/false, turn,
                       creation->second.scale) == nullptr) {
            return false;
        }
        const SpawnedUnit& placed = spawned[phase.owner].back();
        script.add_spawned_unit(placed.id, unit, tl::core::Vec3{place.x, place.y, place.z},
                                /*hunts=*/false, std::atan2(forward.x, forward.z),
                                &creation->second);
        // Made at the level's monster level already; only a champion's
        // `level + 1` sets it again.
        const auto wanted_level = unit_spawn_level.find(placed.id);
        int made_level = 0;
        if (wanted_level != unit_spawn_level.end() && wanted_level->second.level > 0 &&
            !(script.unit_level(placed.id, made_level) &&
              made_level == wanted_level->second.level)) {
            apply_spawn_level(placed.id);
        }
        // `populateSectionOfLevel`: a champion's name is
        // `CRandomNames::generateName(true)`, drawn on the game generator once
        // the character is in the level, and `makeChampion` takes it.
        if (champion) {
            load_quest_book();
            if (!random_names.empty()) {
                unit_names[placed.id] = random_names.generate_name(true, g_rand);
                TL_LOG_INFO("  champion: %s is '%s'", unit.c_str(), unit_names[placed.id].c_str());
            }
            // `makeChampion` on it: the
            // champion's hit points, armour, experience and fame, and its size.
            script.make_unit_champion(placed.id);
        }
        ++counts[unit];
        // `addCharacter` -> `createPathfinder`: its minions, rolled after the
        // name `makeChampion` took.
        spawn_unit_minions(placed.id, phase.owner);
        return true;
    };
    world.add_item = [&](const tl::core::PopulatedUnit& made, const std::string& unit,
                         const tl::core::Vec3& at, const tl::core::PopulatedFacing& facing) {
        const Ogre::Quaternion turn = turn_of(facing);
        if (!is_breakable_unit(unit)) {
            // `addItem`, on the ground (`CItem::snapToGround`).
            //
            const Ogre::Vector3 ground = snap_item_to_ground(Ogre::Vector3(at.x, at.y, at.z));
            const auto scale = made_scales.find(made.handle);
            if (!place_populated_trigger(unit, ground, turn, next_spawned_id(),
                                         scale != made_scales.end() ? scale->second : 1.0f)) {
                return false;
            }
            ++triggers;
            ++counts[unit];
            return true;
        }
        // `CLevel::addItem(item, at, true)`:
        // only a `CEquipment` is tossed, so a `CBreakable` stands where it was
        // put, on the ground (`setActiveInLevel`).
        const auto scale = made_scales.find(made.handle);
        if (!place_breakable(unit, Ogre::Vector3(at.x, at.y, at.z), turn, next_spawned_id(),
                             nullptr, scale != made_scales.end() ? &scale->second : nullptr)) {
            return false;
        }
        ++breakables;
        ++counts[unit];
        return true;
    };
    world.discard = [&](const tl::core::PopulatedUnit& made) {
        made_characters.erase(made.handle);
        made_scales.erase(made.handle);
    };

    const int placed =
        tl::core::populate_phase(population_sections, sections, g_rand, spawn_class, wanted,
                                 phase.champion, phase.markers, phase.empty_counts, world);
    if (phase.markers) {
        for (size_t i = 0; i < markers.size() && i < quest_boss_markers.size(); ++i) {
            quest_boss_markers[i].taken = markers[i].taken;
        }
    }

    std::string summary;
    for (const auto& entry : counts) {
        summary += (summary.empty() ? "" : ", ") + std::to_string(entry.second) + " " + entry.first;
    }
    if (triggers > 0) {
        // The chests' own collision, into the soup and onto the map grid
        // (`addToAvoidanceMap`), and the routes around them.
        collision.build();
        register_door_avoidance();
        if (passability.ready()) nav.build(passability);
    }
    if (phase.props) {
        TL_LOG_INFO(
            "  props: %d of %u placed from '%s' over %zu section(s) (%d breakable(s), "
            "%d trigger prop(s)%s%s)",
            placed, wanted, spawn_class.c_str(), population_sections.size(), breakables, triggers,
            summary.empty() ? "" : ": ", summary.c_str());
    } else if (placed > 0) {
        TL_LOG_INFO("  %s: %d of %u placed from '%s'%s (%s)", phase.label, placed, wanted,
                    spawn_class.c_str(), phase.champion ? ", promoted to champion" : "",
                    summary.c_str());
    } else if (loaded) {
        TL_LOG_WARN("  %s: none placed from '%s'", phase.label, spawn_class.c_str());
    }
    return placed;
}

// A CFormationNode: setLayoutFile loads the layout
// hidden, its Monsters out of the level, until the player comes near (wake_formations()).
bool Renderer::Impl::add_pending_formation(const std::string& reference, const Ogre::Vector3& at,
                                           int monster_level) {
    std::string formation_path = resolve_archive_path(reference);
    if (formation_path.empty()) formation_path = resolve_archive_path(reference + ".cmp");
    if (formation_path.empty()) {
        TL_LOG_WARN("  formations: '%s' named but not found in the archive", reference.c_str());
        return false;
    }
    const tl::formats::CmpScene* const scene = layout_file(formation_path);
    if (scene == nullptr || !scene->ok) return false;
    bool spawners = false;
    for (const tl::formats::CmpNode& node : scene->nodes) {
        if (node.type == tl::formats::cmp_type::kUnitSpawner) spawners = true;
    }
    pending_formations.push_back({formation_path, at, monster_level, false, spawners, reference});
    TL_LOG_INFO("  formation %s%s waits at (%.2f, %.2f, %.2f)", formation_path.c_str(),
                spawners ? " (unit spawners)" : "", static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z));
    return true;
}

// `CLevel::populate(true)`, the random
// population: formations, the six counts, then monsters, champions, NPCs,
// creeps, props and gold.
void Renderer::Impl::populate_level(
    const tl::core::LevelTemplate& level_template, int level,
    std::vector<std::pair<std::string, Ogre::Vector3>>& placed_pieces) {
    tl::core::RunicRandom& g_rand = tl::core::game_random();
    tl::core::RunicRandom& volatile_rolls = tl::core::volatile_random();
    const tl::core::SpawnSlots& spawn_classes = level_template.chosen_spawn_classes;

    // The one section randomizer every phase picks from: `CRandomizer(0)`, a
    // choice a section, weight 100.
    tl::core::Randomizer sections(tl::core::RandomizerType::kNormal);
    for (size_t i = 0; i < population_sections.size(); ++i) {
        sections.add_choice(static_cast<int32_t>(i), 100);
    }

    // The pathable area is `CLevel +0x58`, the cells `calculatePassability`
    // left open before erosion (core/passability.h).
    const uint32_t pathable_cells = passability.passable_cells();

    // `populateFormations`, run only for a
    // template that names a formations file (`+0x5b0`); its count is drawn
    // inside, after the file randomizer is filled.
    if (!spawn_classes[7].empty()) {
        std::vector<tl::core::Vec3> existing;
        for (const PendingFormation& formation : pending_formations) {
            existing.push_back({formation.at.x, formation.at.y, formation.at.z});
        }
        int formations_placed = 0;
        tl::core::FormationWorld world;
        world.volatile_rolls = &volatile_rolls;
        world.random_open_position = [this](const tl::core::Vec3& at, float radius) {
            return populate_open_position(at, radius);
        };
        world.in_no_spawn_region = [this](const tl::core::Vec3& at, float radius) {
            return tl::core::is_in_no_spawn_region(no_spawn_regions, at, radius);
        };
        world.add_formation = [&](const std::string& reference, const tl::core::Vec3& open) {
            // `CFormationNode::setPosition(at)`; its monsters go to the ground
            // as they join the level.
            const Ogre::Vector3 at = dropped_to_ground(Ogre::Vector3(open.x, open.y, open.z));
            if (!add_pending_formation(reference, at, level)) return;
            placed_pieces.emplace_back(pending_formations.back().path, at);
            ++formations_placed;
        };
        const uint32_t formations_wanted = tl::core::populate_formations(
            population_sections, sections, formation_choices(spawn_classes[7]),
            [&]() {
                return static_cast<uint32_t>(
                    tl::core::units_to_create(level_template, 7, pathable_cells, g_rand));
            },
            existing, g_rand, world);
        TL_LOG_INFO("  Formations Created: %u (%d placed from '%s')", formations_wanted,
                    formations_placed, spawn_classes[7].c_str());
    }

    // The counts, in `populate`'s order: slots 0
    // (monsters), 2 (props), 1 (champions), 3 (NPCs), 4 (creeps) and 5 (gold).
    // `KSETTINGS_NETBOOK_MODE` 1 would zero the creeps; it is 0 here.
    const auto count = [&](size_t slot) {
        return static_cast<uint32_t>(std::max<int64_t>(
            0, tl::core::units_to_create(level_template, slot, pathable_cells, g_rand)));
    };
    const uint32_t monsters_wanted = count(0);
    const uint32_t props_wanted = count(2);
    const uint32_t champions_wanted = count(1);
    const uint32_t npcs_wanted = count(3);
    const uint32_t creeps_wanted = count(4);
    const uint32_t gold_wanted = count(5);
    TL_LOG_INFO(
        "  pathable area %.1f m^2: wants %u monster(s), %u prop(s), %u champion(s), "
        "%u NPC(s), %u creep(s), %u gold",
        static_cast<double>(static_cast<float>(pathable_cells) / 6.5f), monsters_wanted,
        props_wanted, champions_wanted, npcs_wanted, creeps_wanted, gold_wanted);

    // The phases (monsters, champions, NPCs, creeps
    // props). Only the monsters' loop lets ten empty rolls
    // pass before counting one; only the NPCs stand on the level's markers.
    PopulatePhase monsters{-2001, "monsters"};
    PopulatePhase champions{-2003, "champions"};
    champions.champion = true;
    champions.empty_counts = true;
    PopulatePhase npcs{-2004, "NPCs"};
    npcs.markers = true;
    npcs.empty_counts = true;
    PopulatePhase creeps{-2002, "creeps"};
    creeps.empty_counts = true;
    PopulatePhase props{-2005, "props"};
    props.empty_counts = true;
    props.props = true;
    populate_units(spawn_classes[0], monsters_wanted, level, sections, monsters);
    populate_units(spawn_classes[1], champions_wanted, level, sections, champions);
    populate_units(spawn_classes[3], npcs_wanted, level, sections, npcs);
    populate_units(spawn_classes[4], creeps_wanted, level, sections, creeps);
    // `setRandomizerToNormal(1.0)` ahead of the props.
    sections.set_randomizer_to_normal(1.0f);
    populate_units(spawn_classes[2], props_wanted, level, sections, props);

    // Gold.
    if (gold_wanted > 0 && !population_sections.empty()) {
        tl::core::GoldWorld world;
        world.volatile_rolls = &volatile_rolls;
        world.random_open_item_position = [this](const tl::core::Vec3& at, float radius) {
            return tl::core::random_open_item_position(
                at, 0.0f, radius,
                [this](const tl::core::Vec3& p) {
                    return !passability.ready() || passability.map_passable(p);
                },
                tl::core::volatile_random());
        };
        world.in_no_spawn_region = [this](const tl::core::Vec3& at, float radius) {
            return tl::core::is_in_no_spawn_region(no_spawn_regions, at, radius);
        };
        world.add_gold = [this](int32_t amount, const tl::core::Vec3& at) {
            // `new CItemGold(rm, amount)`,
            // `addItem(gold, at, false)`.
            place_gold(tl::core::make_gold_pile(amount, tl::core::volatile_random()),
                       Ogre::Vector3(at.x, at.y, at.z), nullptr, /*drop_sound=*/false);
        };
        const int piles =
            tl::core::populate_gold(population_sections, sections, gold_wanted, g_rand, world);
        TL_LOG_INFO("  gold: %d of %u pile(s) placed", piles, gold_wanted);
    }

    if (!spawn_classes[6].empty()) {
        TL_LOG_INFO("  fish: '%s' named, not placed - this engine has no fishing spots",
                    spawn_classes[6].c_str());
    }
}

/// `CResourceManager::getSpawnClassByName`, as `chooseUnitSpawners` asks it.
tl::core::SpawnClassLookup archive_spawn_class_lookup() {
    return [](const std::string& name, std::vector<tl::core::SpawnClassRef>& entries) {
        // `CSpawnClassParser::getSpawnClass`: by its upper-cased `NAME`.
        const std::string path = spawn_class_path(name);
        if (path.empty()) return false;
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) return false;
        const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (!file.ok) return false;
        // `CSpawnClassParser::createSpawnClassData`:
        // `UNITTYPE`, then `UNIT`, then
        // `SPAWNCLASS`; an object with none adds no entry, and neither does a
        // `SPAWNCLASS` naming no class (`createSpawnClassDataRefSpawnClass`).
        //
        for (const tl::formats::AdmSection& object : file.root.children) {
            if (object.name != "OBJECT") continue;
            tl::core::SpawnClassRef entry;
            if (object.string_or("UNITTYPE", "").empty() && object.string_or("UNIT", "").empty()) {
                entry.spawn_class = object.string_or("SPAWNCLASS", "");
                if (entry.spawn_class.empty() || spawn_class_path(entry.spawn_class).empty()) {
                    continue;
                }
            }
            const int32_t weight = static_cast<int32_t>(object.integer_or("WEIGHT", 0xffffffffu));
            entry.weight = weight == 0 ? -1 : weight;
            entries.push_back(std::move(entry));
        }
        return true;
    };
}

// `CGameClient::storedUnitExists(long long)`: the travel
// record's state of this level (`CPlayer::getLevelSavedState`) holds a
// character of that object id.
bool Renderer::Impl::level_stores_unit(int64_t id) {
    tl::core::TravelRecord* travel = config.travel;
    if (travel == nullptr || level_dungeon.empty()) return false;
    const tl::formats::LevelSaveState* state =
        tl::core::get_level_saved_state(*travel, level_dungeon, level_depth);
    if (state == nullptr) return false;
    for (const tl::formats::CharacterSaveState& saved : state->characters) {
        if (saved.object_id == id) return true;
    }
    return false;
}

void Renderer::Impl::activate_room_machine(size_t index, const std::string& path) {
    NestedScript& machine = nested_scripts[index];
    std::vector<tl::core::ScriptEvent> events;
    machine.script.set_has_saved_state(level_has_saved_state);
    machine.script.set_stored_unit_exists([this](int64_t id) { return level_stores_unit(id); });
    machine.script.level_activated(quest_log.state(), events);
    if (machine.owner == 0) {
        TL_LOG_INFO("  room script '%s' armed (no owning link): %zu event(s) on activation",
                    path.c_str(), events.size());
    } else if (!events.empty()) {
        TL_LOG_INFO("  linked machine '%s' activated with the level: %zu event(s)",
                    machine.owner_name.c_str(), events.size());
    }
    if (!events.empty()) apply_script_events(events, machine.nodes);
}

bool Renderer::Impl::place_dungeon_floor(const std::string& dungeon_name, int floor_number) {
    // `CDungeonManager::getDungeonByName`:
    // the process's dungeon of that `NAME`, with the salt and the floor
    // templates it has kept.
    tl::core::DungeonManager& manager = loaded_dungeon_manager();
    tl::core::Dungeon* named = manager.dungeon_by_name(dungeon_name);
    if (named == nullptr) {
        // DIVERGENCE (tooling): `--dungeon` has always taken a file of
        // `media/dungeons/` as well; such a name is read for its `NAME`.
        const std::string def_path = archive_path_unique(dungeon_name + ".DAT.adm");
        const std::vector<uint8_t> def_bytes =
            def_path.empty() ? std::vector<uint8_t>() : read_resource(def_path);
        if (!def_bytes.empty()) {
            const tl::core::DungeonDef by_file = tl::core::parse_dungeon_def(
                tl::formats::parse_adm(def_bytes.data(), def_bytes.size()));
            if (by_file.ok) named = manager.dungeon_by_name(by_file.name);
        }
    }
    if (named == nullptr || !named->def.ok) {
        TL_LOG_ERROR("no dungeon called '%s' in the archive", dungeon_name.c_str());
        return false;
    }
    tl::core::Dungeon& dungeon = *named;
    const tl::core::DungeonDef& def = dungeon.def;

    // `CGameClient::setCurrentDungeon`. A
    // warp has set it already (`Renderer::leave_level`); a floor loaded with
    // none before it (`--dungeon`, the Choose Level list) sets it here.
    tl::core::GameSeed& seed = tl::core::game_seed();
    seed.current_dungeon = &dungeon;

    // The first floor is depth 0 (STRATA0).
    const uint32_t depth = static_cast<uint32_t>(std::max(1, floor_number) - 1);
    // Before the level is placed, so its quest gates see what this gives.
    apply_floor_quests(def, depth);

    const tl::core::DungeonFiles dungeon_files = archive_dungeon_files();

    // CGameClient::loadLevel: resetGameSeed(depth) (game
    // seed, the dungeon's salt, depth), then the depth's template, kept by the dungeon;
    // building one draws room pieces from that stream.
    tl::core::RunicRandom& g_rand = tl::core::game_random();
    tl::core::reset_game_seed(seed, static_cast<int32_t>(depth), g_rand);
    const tl::core::DungeonStrata* source = nullptr;
    tl::core::LevelTemplate* found_template =
        dungeon.get_level_template_for_depth(depth, dungeon_files, g_rand, &source);
    if (found_template == nullptr || source == nullptr || found_template->chunks.empty()) {
        TL_LOG_ERROR("dungeon '%s' floor %d: no level template with room pieces", def.name.c_str(),
                     floor_number);
        return false;
    }
    tl::core::LevelTemplate& level_template = *found_template;
    const tl::core::DungeonStrata& strata = *source;
    // The level seed `CLevel::loadRoomLayout` seeds and prints.
    const int32_t level_seed =
        tl::core::level_seed(tl::core::kLevelSeedSetting, seed, static_cast<int32_t>(depth));
    // `CLevel+0x280` is the dungeon's upper-cased `NAME` and `+0x1a4` the depth;
    // the stratum lays `IS_TOWN` and a set `DONTSTORE` onto the template
    // (`CDungeon::getStrataTemplate`).
    set_level_identity(def.name, static_cast<int32_t>(depth), strata.is_town,
                       level_template.dont_store || strata.dont_store, strata.allow_portals);
    level_allows_pet_return = strata.allow_pet_return;
    level_cinematic = level_template.cinematic;
    const std::string& ruleset_path = strata.ruleset;
    // Its rooms are placed at depth 1, below place_layout()'s own reading.
    read_rim_light(ruleset_path);
    const tl::core::LevelTemplate* own_template =
        dungeon.templates[std::min<size_t>(depth, dungeon.templates.size() - 1)].get();
    const std::string redirected =
        own_template != nullptr ? own_template->redirect_dungeon : std::string();
    TL_LOG_INFO("dungeon '%s': floor %d is depth %u of %zu, ruleset '%s'%s%s", def.name.c_str(),
                floor_number, depth, def.depth_strata.size(), ruleset_path.c_str(),
                redirected.empty() ? "" : ", a random floor of ", redirected.c_str());
    TL_LOG_INFO("Seeding: %d", level_seed);

    // `CGameClient::loadLevel` empties a `RANDOMIZED` template's pool before
    // every load, so each visit to a floor generates afresh.
    if (level_template.randomized) level_template.delete_layouts();
    const tl::core::LevelLayout* layout =
        tl::core::choose_room_layout(level_template, g_rand, level_seed);
    if (layout == nullptr || layout->instances.empty()) {
        TL_LOG_ERROR("could not generate a floor for '%s' from '%s'", def.name.c_str(),
                     ruleset_path.c_str());
        return false;
    }
    const int entrance_index = 0;
    int exit_index = -1;
    TL_LOG_INFO("dungeon '%s' floor %d: %zu chunk(s), one of %zu layout(s)", def.name.c_str(),
                floor_number, layout->instances.size(), level_template.layouts.size());
    for (size_t i = 0; i < layout->instances.size(); ++i) {
        const tl::core::ChunkInstance& chunk = layout->instances[i];
        const tl::core::ChunkType& type = level_template.type_at(chunk.type_index);
        if (exit_index < 0 && type.exit) exit_index = static_cast<int>(i);
        TL_LOG_INFO("  chunk %zu: '%s' at (%.2f, %.2f, %.2f), piece '%s'%s%s", i, type.name.c_str(),
                    chunk.x, chunk.y, chunk.z,
                    level_template.chunks[layout->pieces[i]].path.c_str(),
                    static_cast<int>(i) == entrance_index ? " [entrance]" : "",
                    static_cast<int>(i) == exit_index ? " [exit]" : "");
    }

    // CLevel::loadRoomLayout: the start (+0x140) is the
    // Entrance node, facing (+0x164; CCharacter::setToward) its
    // forward, flattened; +Z without one.
    const Ogre::Vector3 entrance_facing = Ogre::Vector3::UNIT_Z;
    dungeon_entrance_warp_at = dungeon_exit_warp_at = Ogre::Vector3::ZERO;
    dungeon_entrance_warp_forward = dungeon_exit_warp_forward = Ogre::Vector3::UNIT_Z;

    level_reference = "dungeon:" + def.name;

    // CGameClient::loadLevel: monster level
    // is computed on every load, before loadRoomLayout and before
    // setSeedVolatile(0) - a room's placed monsters are made at it
    // (`CMonsterDescriptor::CreateObject`).
    tl::core::TravelRecord* travel = config.travel;
    const int32_t player_lvl = 0 < arriving_player_level
                                   ? arriving_player_level
                                   : static_cast<int32_t>(player_character.level());
    const bool has_player = travel != nullptr;
    const int32_t floor_rank = tl::core::rank_of_monsters_on_floor(static_cast<int32_t>(depth),
                                                                   &def, travel, player_lvl, true);
    const int32_t floor_monster_level = tl::core::level_monster_level(
        &level_template, floor_rank, has_player, player_lvl, tl::core::volatile_random());
    const bool has_history = travel != nullptr && tl::core::has_dungeon_history(*travel, def.name);
    TL_LOG_INFO("  monster level: %d (rank %d, player level %d%s)", floor_monster_level, floor_rank,
                player_lvl, has_history ? ", history" : "");
    level_monster_level = floor_monster_level;

    size_t placed_chunks = 0;
    std::string exit_chunk_path;
    Ogre::Vector3 exit_chunk_origin = Ogre::Vector3::ZERO;
    std::string entrance_chunk_path;
    Ogre::Vector3 entrance_chunk_origin = Ogre::Vector3::ZERO;
    // Every chunk's chosen piece and where it landed, for reading its Sound nodes later.
    std::vector<std::pair<std::string, Ogre::Vector3>> placed_pieces;
    // CLevel::loadRoomLayout sets CResourceManager+0x42
    // and passes 1 << index: a piece's groups roll from level seed + (1 << index) + 2.
    // INVENTED: nested layouts load in file order (only their seeds move).
    // The rooms' machines wait for the population (`activate_room_machine`).
    defer_room_activation = true;
    deferred_room_machines.clear();
    layout_seeding = true;
    layout_level_seed = level_seed;
    for (size_t chunk_i = 0; chunk_i < layout->instances.size(); ++chunk_i) {
        const tl::core::ChunkInstance& chunk = layout->instances[chunk_i];
        const std::string& piece = level_template.chunks[layout->pieces[chunk_i]].path;
        const std::string piece_path = resolve_archive_path(piece + ".cmp");
        if (piece_path.empty()) {
            TL_LOG_WARN("  room piece '%s' is not in the archive; leaving a gap", piece.c_str());
            continue;
        }
        const Ogre::Vector3 origin(chunk.x, chunk.y, chunk.z);
        // Each placed room is one of `CLevel`'s room layouts, and passability is
        // cast per room over the box its pieces' collision makes.
        collision_sections.emplace_back();
        level_rooms.emplace_back();
        collision_section = static_cast<int>(collision_sections.size()) - 1;
        placing_scene = static_cast<uint32_t>(chunk_i);
        placing_hierarchy = 0;
        if (place_layout(piece_path, origin, Ogre::Quaternion::IDENTITY, /*depth=*/1,
                         1u << (chunk_i & 31u))) {
            ++placed_chunks;
            placed_pieces.emplace_back(piece_path, origin);
            if (static_cast<int>(chunk_i) == exit_index) {
                exit_chunk_path = piece_path;
                exit_chunk_origin = origin;
            }
            if (static_cast<int>(chunk_i) == entrance_index) {
                entrance_chunk_path = piece_path;
                entrance_chunk_origin = origin;
            }
        }
        collision_section = -1;
    }

    layout_seeding = false;
    if (placed_chunks == 0) {
        defer_room_activation = false;
        deferred_room_machines.clear();
        TL_LOG_ERROR("dungeon '%s' floor %d: not one room piece could be placed", def.name.c_str(),
                     floor_number);
        return false;
    }
    TL_LOG_INFO("  %zu of %zu room(s) placed", placed_chunks, layout->instances.size());

    // The level-wide atmosphere: the ruleset's LEVELPARTICLE layout (a Sound and a Layout
    // Link Particle), placed once.
    const std::string level_particle_ref = level_template.level_particle;
    if (!level_particle_ref.empty()) {
        std::string level_particle_path = resolve_archive_path(level_particle_ref);
        if (level_particle_path.empty()) {
            level_particle_path = resolve_archive_path(level_particle_ref + ".cmp");
        }
        const size_t systems_before = particle_systems.size();
        if (!level_particle_path.empty() &&
            place_layout(level_particle_path, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY,
                         /*depth=*/1)) {
            placed_pieces.emplace_back(level_particle_path, Ogre::Vector3::ZERO);
            // CGameClient::loadLevel keeps the layout
            // (+0x1080) and updateIngame puts it at the
            // player every frame.
            level_particle_node = scene->getRootSceneNode()->createChildSceneNode();
            size_t riding = 0;
            for (size_t i = systems_before; i < particle_systems.size(); ++i) {
                Ogre::SceneNode* holder = particle_systems[i].holder();
                if (holder == nullptr || holder->getParent() == level_particle_node) continue;
                if (Ogre::Node* up = holder->getParent()) up->removeChild(holder);
                level_particle_node->addChild(holder);
                ++riding;
            }
            TL_LOG_INFO("  LEVELPARTICLE '%s': %zu system node(s) follow the player",
                        level_particle_ref.c_str(), riding);
        } else {
            TL_LOG_WARN("  LEVELPARTICLE '%s' named but could not be placed",
                        level_particle_ref.c_str());
        }
    }

    // Lighting, CAMERAMULT and fog over every room: apply_environment() takes the STRATA's
    // RULESET (rules_path_for()).
    apply_environment(strata.ruleset);

    // Collision and the navigation grid over the whole floor (place_layout()'s depth 0
    // tail), before the player is stood: the grid finds where.
    collision.build();
    TL_LOG_INFO("  collision: %zu face(s) from %zu mesh(es), %zu of them nocollide",
                collision.triangle_count(), collision_meshes, collision.nocollide_count());
    nav.clear();
    passability.clear();
    if (!collision.empty()) {
        const unsigned long bake_started =
            root && root->getTimer() ? root->getTimer()->getMilliseconds() : 0;
        passability.bake(collision, &collision_sections);
        const unsigned long bake_took =
            (root && root->getTimer() ? root->getTimer()->getMilliseconds() : 0) - bake_started;
        log_passability(passability, static_cast<double>(bake_took) / 1000.0);
        if (!passability_dump_path.empty()) {
            write_passability_dump(passability, passability_dump_path);
        }
        // Before `nav.build()` - see the matching note in place_layout()'s depth == 0 tail.
        register_door_avoidance();
        nav.build(passability);
        TL_LOG_INFO("  navigation: %zu of %zu cell(s) walkable at %.2f units", nav.walkable_count(),
                    nav.cell_count(), static_cast<double>(nav.cell_size()));
    } else {
        door_trigger_nodes.clear();
    }
    automap.create(passability, level_is_town);
    script.set_navigation(&nav);
    script.set_collision(&collision);

    const float chunk_w = level_template.chunk_width_basis * level_template.tile_basis;
    const float chunk_h = level_template.chunk_height_basis * level_template.tile_basis;

    // The floor's extent: x in [origin - w/2, origin + (WIDTH - 1) * w + w/2] as
    // CLevelTemplateData::intersectsChunks bounds it, z in [origin, origin + HEIGHT * h].
    float min_x = 1e9f, max_x = -1e9f, min_z = 1e9f, max_z = -1e9f;
    for (const tl::core::ChunkInstance& chunk : layout->instances) {
        const tl::core::ChunkType& type = level_template.type_at(chunk.type_index);
        min_x = std::min(min_x, chunk.x - chunk_w * 0.5f);
        max_x = std::max(max_x, chunk.x + (static_cast<float>(type.width) - 0.5f) * chunk_w);
        min_z = std::min(min_z, chunk.z);
        max_z = std::max(max_z, chunk.z + static_cast<float>(type.height) * chunk_h);
    }

    // INVENTED (no marker for either): the player stands on walkable ground in the
    // entrance room, facing the first corridor, and able to reach the exit (ground_near()
    // spirals out; nav.find_path() checks reachability).
    auto ground_near = [&](const Ogre::Vector3& centre, Ogre::Vector3& out) -> bool {
        if (!nav.ready()) return false;
        constexpr int kSteps = 16;
        const float span = std::max(max_x - min_x, max_z - min_z);
        const int rings = 1 + static_cast<int>(span / (nav.cell_size() * 1.5f));
        for (int ring = 0; ring < rings; ++ring) {
            const float radius = static_cast<float>(ring) * (nav.cell_size() * 1.5f);
            for (int step = 0; step < kSteps; ++step) {
                const float a = 6.2831853f * static_cast<float>(step) / static_cast<float>(kSteps);
                const float x = centre.x + (ring == 0 ? 0.0f : std::cos(a) * radius);
                const float z = centre.z + (ring == 0 ? 0.0f : std::sin(a) * radius);
                if (!nav.walkable(x, z)) continue;
                const tl::core::Vec3 sample = nav.sample_at(x, z);
                out = Ogre::Vector3(sample.x, sample.y, sample.z);
                return true;
            }
        }
        return false;
    };
    auto to_vec3 = [](const Ogre::Vector3& v) { return tl::core::Vec3{v.x, v.y, v.z}; };

    // Where run()'s exit check triggers: the authored marker (find_exit_marker()) when the
    // chunk's piece has one, else the chunk's grounded origin.
    bool exit_marker_found = false;
    Ogre::Vector3 exit_forward = Ogre::Vector3::UNIT_Z;
    float exit_marker_radius = 4.0f;
    if (exit_index >= 0) {
        const tl::core::ChunkInstance& exit_chunk =
            layout->instances[static_cast<size_t>(exit_index)];
        dungeon_exit_point = Ogre::Vector3(exit_chunk.x, exit_chunk.y, exit_chunk.z);
        if (!exit_chunk_path.empty()) {
            Ogre::Vector3 marker_pos;
            float marker_radius = 3.0f;
            ChunkMarkerExtra exit_extra;
            if (find_chunk_marker(exit_chunk_path, exit_chunk_origin, "exit", marker_pos,
                                  marker_radius, &exit_extra)) {
                dungeon_exit_warp_at =
                    exit_extra.has_warper ? exit_extra.warper_position : marker_pos;
                dungeon_exit_warp_forward =
                    exit_extra.has_warper ? exit_extra.warper_forward : exit_extra.forward;
                dungeon_exit_point = marker_pos;
                exit_marker_radius = marker_radius;
                exit_marker_found = true;
                exit_forward = exit_extra.forward;
                TL_LOG_INFO("  exit marker at (%.2f, %.2f, %.2f), radius %.1f", marker_pos.x,
                            marker_pos.y, marker_pos.z, static_cast<double>(marker_radius));
            } else {
                TL_LOG_WARN(
                    "  no authored exit marker in '%s'; falling back to the nearest "
                    "walkable ground to its chunk's centre",
                    exit_chunk_path.c_str());
            }
        }
    }

    {
        const tl::core::ChunkInstance& entrance = layout->instances[0];
        Ogre::Vector3 facing = entrance_facing;
        const Ogre::Vector3 entrance_origin(entrance.x, entrance.y, entrance.z);

        // On the stairs: the entrance chunk's ENTRANCE_NODE `Entrance` Property Node; the
        // nearest ground to the chunk's centre as fallback.
        Ogre::Vector3 spawn_at = entrance_origin;
        bool grounded = false;
        if (!entrance_chunk_path.empty()) {
            Ogre::Vector3 marker_pos;
            float marker_radius = 3.0f;
            ChunkMarkerExtra entrance_extra;
            if (find_chunk_marker(entrance_chunk_path, entrance_chunk_origin, "entrance",
                                  marker_pos, marker_radius, &entrance_extra)) {
                facing = entrance_extra.forward;
                dungeon_entrance_warp_at =
                    entrance_extra.has_warper ? entrance_extra.warper_position : marker_pos;
                dungeon_entrance_warp_forward = entrance_extra.has_warper
                                                    ? entrance_extra.warper_forward
                                                    : entrance_extra.forward;
                spawn_at = marker_pos;
                grounded = true;
                dungeon_entrance_point = marker_pos;
                dungeon_entrance_radius = marker_radius;
                float surface = 0.0f;
                if (collision.ground_under(spawn_at.x, spawn_at.z, spawn_at.y + 8.0f, surface)) {
                    spawn_at.y = surface;
                }
                TL_LOG_INFO("  entrance marker at (%.2f, %.2f, %.2f)", spawn_at.x, spawn_at.y,
                            spawn_at.z);
            } else {
                TL_LOG_WARN(
                    "  no authored entrance node in '%s'; standing at the nearest walkable "
                    "ground to its chunk's centre",
                    entrance_chunk_path.c_str());
            }
        }
        if (!grounded) grounded = ground_near(entrance_origin, spawn_at);

        // Reachability over the passability grid is logged, not enforced: a
        // floor whose exit no route reaches is a defect to look at, not
        // something to paper over by moving the player.
        if (grounded && exit_index > entrance_index) {
            const tl::core::ChunkInstance& exit_chunk =
                layout->instances[static_cast<size_t>(exit_index)];
            Ogre::Vector3 exit_ground;
            if (ground_near(Ogre::Vector3(exit_chunk.x, exit_chunk.y, exit_chunk.z), exit_ground)) {
                // The trigger point substitutes for the marker only when there is none; the
                // connectivity check still wants a walkable point near the exit.
                if (!exit_marker_found) dungeon_exit_point = exit_ground;
                const Ogre::Vector3 route_to = exit_marker_found ? dungeon_exit_point : exit_ground;
                if (nav.find_path(to_vec3(spawn_at), to_vec3(route_to)).empty()) {
                    // A closed door is a wall here too (register_door_avoidance()), so a
                    // floor behind one reads unreachable; named rather than silenced.
                    if (!door_avoidance_added.empty()) {
                        TL_LOG_WARN(
                            "  no pathfinder route from the entrance to this floor's exit "
                            "(%zu closed door/chest/trigger collision model(s) present - may "
                            "open the way on approach)",
                            door_avoidance_added.size());
                    } else {
                        TL_LOG_WARN("  no pathfinder route from the entrance to this floor's exit");
                    }
                } else {
                    TL_LOG_INFO("  a route leads from the entrance to the exit");
                }
            }
        }

        if (!grounded) {
            // No nav grid at all, or the whole floor is ungrounded: a plain
            // ray straight down, the way stand_subject_at() does for an
            // authored marker.
            Ogre::Vector3 probe(spawn_at.x + facing.x * 6.0f, 20.0f, spawn_at.z + facing.z * 6.0f);
            float surface = 0.0f;
            if (ground_under(probe, surface)) {
                probe.y = surface;
                spawn_at = probe;
                grounded = true;
            }
        }
        if (!grounded) {
            TL_LOG_WARN("  no ground found anywhere on the generated floor; standing at y=0");
        }

        // CLevel::loadRoomLayout: entered with type 0
        // (from below) the level starts at its Exit node, turned as it is (performWarp
        // then stands the player at the matching Warper).
        if (level_entry_type == 0 && exit_marker_found) {
            spawn_at = dungeon_exit_point;
            facing = exit_forward;
            float surface = 0.0f;
            if (collision.ground_under(spawn_at.x, spawn_at.z, spawn_at.y + 8.0f, surface)) {
                spawn_at.y = surface;
            }
            grounded = true;
            TL_LOG_INFO("  entered from below: the start is the exit node");
        }

        if (subject_node != nullptr) {
            subject_node->setPosition(spawn_at);
            if (!facing.isZeroLength()) {
                subject_node->setOrientation(Ogre::Vector3::UNIT_Z.getRotationTo(facing));
            }
        }
        player_start = spawn_at;
        if (!facing.isZeroLength()) player_start_yaw = std::atan2(facing.x, facing.z);
        have_player_start = true;
        TL_LOG_INFO(
            "  subject at the dungeon entrance (%.2f, %.2f, %.2f), facing (%.2f, 0, %.2f)%s",
            spawn_at.x, spawn_at.y, spawn_at.z, facing.x, facing.z, grounded ? "" : ", ungrounded");
    }

    // A clean level-script slate: build() on an empty scene resets the machine's tables.
    script.set_equipment_roller(
        [this](const std::string& unit, tl::core::LevelScript::UnitCreation& made) {
            roll_unit_equipment(unit, made);
        });
    script.set_unit_equipment(unit_equipment_hooks());
    unit_observers.clear();
    skill_observers.clear();
    script.build(tl::formats::CmpScene{}, {});

    // build() leaves units_ alone and load_unit_stats() never runs for a generated floor,
    // so the unit book is set here (stats_for(), COLLIDEABLE and the rest).
    script.set_units(&unit_book);
    wire_script(script);
    script_ready = true;
    for (MachineView& view : machines()) view.script->set_level_monster_level(level_monster_level);
    for (MachineView& view : machines()) view.script->set_random_names(&random_names);

    // CGameClient::loadLevel, the room layout loaded:
    // resetGameSeed(depth), CLevelTemplateData::chooseUnitSpawners, then setSeed(0) and
    // setSeedVolatile(0) - the clock - before CLevel::populate.
    tl::core::reset_game_seed(seed, static_cast<int32_t>(depth), g_rand);
    tl::core::choose_unit_spawners(level_template, archive_spawn_class_lookup(), g_rand);
    const tl::core::SpawnSlots& spawn_classes = level_template.chosen_spawn_classes;
    // What a hooked fish rolls (`CPlayer::fishingAICatch` reads the level's
    // template, `+0x1d8 -> +0x5a8`).
    level_fish_spawn_class = spawn_classes[6];
    if (level_template.spawn_classes_randomized[0]) {
        TL_LOG_INFO("  monster spawn class '%s' chosen from '%s'", spawn_classes[0].c_str(),
                    level_template.spawn_classes[0].c_str());
    }
    tl::core::reseed_level_generators();

    // CGameClient::loadLevel calls
    // CLevel::populate(false) on a remembered level: no random population;
    // its units come from the state.
    if (!level_has_saved_state) populate_level(level_template, floor_monster_level, placed_pieces);

    // `CLevel::populate` ends with
    // `CQuestManager::populate`, outside the random-population branch: a
    // remembered level runs it too, and `+0xb9` keeps a placed objective from
    // being placed twice.
    populate_quests();

    // ... and its last act: every room scene activated (setActivated, through
    // CLayoutDescriptor::descriptorSceneActivated) in room order;
    // CGameClient::loadLevel the same for LEVELPARTICLE after populate.
    defer_room_activation = false;
    for (const auto& [index, path] : deferred_room_machines) activate_room_machine(index, path);
    deferred_room_machines.clear();

    if (const size_t armed = script.arm_hostiles()) {
        TL_LOG_INFO("  %zu hostile unit(s) will come after the player", armed);
    }

    // The music and the HUD, as load_layout() does once its layout is placed.
    start_level_audio(strata.ruleset);
    // The template's own name and flag, which a stratum's `NAME_OVERRIDE` has
    // already been laid over (`CDungeon::getStrataTemplate`)
    // - the ruleset file read above does not know
    // about the override.
    level_has_rules = true;
    level_name_authored = tl::core::strip_markup(level_template.level_name);
    level_shows_floor = level_template.show_floor_number;
    level_display_name =
        tl::core::level_display_name(level_name_authored, level_shows_floor, level_depth);
    raise_arrival_banner();
    refresh_game_hud();

    // Arms run()'s exit check: exit_marker_radius is the marker's RADIUS when found;
    // INVENTED: 4 without one.
    dungeon_active = true;
    dungeon_active_name = def.name;
    dungeon_active_floor = floor_number;
    dungeon_active_floors_total = static_cast<int>(def.depth_strata.size());
    dungeon_exit_radius = exit_marker_radius;
    dungeon_transition_pending = false;
    TL_LOG_INFO("  exit at (%.2f, %.2f, %.2f), radius %.1f - floor %d of %d", dungeon_exit_point.x,
                dungeon_exit_point.y, dungeon_exit_point.z,
                static_cast<double>(dungeon_exit_radius), floor_number,
                dungeon_active_floors_total);

    restore_level_state();
    bake_static_scenery();

    log_memory("after the dungeon floor");
    return true;
}

/// Whether the character is in shot, logged when not: checked at hand-over and at each
/// dumped frame.
bool subject_in_shot(const Ogre::Camera* camera, const Ogre::Entity* subject) {
    if (!camera || !subject || !subject->getParentSceneNode()) return true;
    const Ogre::AxisAlignedBox box = subject->getWorldBoundingBox(true);
    if (box.isNull()) return true;
    return camera->isVisible(box);
}

bool Renderer::enable_player_control() {
    Impl& m = *impl_;
    if (!m.subject || !m.subject_node) {
        TL_LOG_ERROR("nothing to control; load a character first");
        return false;
    }
    if (!m.camera_node) {
        TL_LOG_ERROR("no camera to follow with");
        return false;
    }

    m.player_control = true;
    m.load_player_stats();

    tl::core::Body body;
    // `media/units/players/base.dat` authors no COLLISION_RADIUS, so the player
    // is `CCharacter::unitInit`'s default sphere.
    body.radius = tl::core::kDefaultCollisionRadius;

    // The character's own height, from the mesh's bounds.
    const Ogre::AxisAlignedBox& bounds = m.subject->getMesh()->getBounds();
    if (!bounds.isNull())
        body.height = std::max(0.5f, bounds.getMaximum().y - bounds.getMinimum().y);
    m.player.set_body(body);

    // Stood on the collision: CCharacter::dropToGround,
    // the ray +100 to -100 about the unit.
    Ogre::Vector3 at = m.subject_node->getPosition();
    float surface = at.y;
    if (!m.collision.empty() && m.collision.ground_under(at.x, at.z, at.y + 100.0f, surface)) {
        at.y = surface;
        m.subject_node->setPosition(at);
    }
    m.player.place({at.x, at.y, at.z}, m.subject_node->getOrientation().getYaw().valueRadians());

    // The camera keeps the layout's framing, scaled by CAMERAMULT (follow_subject()). The
    // yaw axis is pinned, or lookAt accumulates roll.
    m.camera_node->setFixedYawAxis(true, Ogre::Vector3::UNIT_Y);

    if (m.layout_has_camera) {
        // The scene authored its own framing. Keep both halves of it: the offset
        // the camera sits at, and the point it looks at, each relative to the
        // character, so the whole framing travels rather than just the position.
        const Ogre::Vector3 eye = m.camera_node->getPosition();
        const Ogre::Vector3 forward = m.layout_camera_target - eye;
        if (forward.squaredLength() < 1e-6f) {
            TL_LOG_ERROR("the camera is standing on its own target; cannot derive a heading");
            return false;
        }
        m.camera_yaw = std::atan2(forward.x, forward.z);
        m.camera_offset = (eye - at) * m.camera_mult;
        m.camera_look_offset = m.layout_camera_target - at;
        TL_LOG_INFO("  camera framing from the layout's own Camera/Camera Target");
    } else {
        m.camera_offset = default_camera_offset(m.camera_mult);
        m.camera_yaw = std::atan2(-m.camera_offset.x, -m.camera_offset.z);

        // `CCameraControl::updateGameCamera` aims at the
        // focus itself; a captured town frame's view matrix agrees.
        m.camera_look_offset = Ogre::Vector3::ZERO;

        // Clip planes sized for the framing, as aim_camera() does.
        const float distance = m.camera_offset.length();
        m.camera->setNearClipDistance(std::max(0.05f, distance * 0.02f));
        m.camera->setFarClipDistance(std::max(200.0f, distance * 20.0f));

        TL_LOG_INFO(
            "  this layout authors no camera; using the engine default "
            "(heading %.1f deg, distance %.1f x CAMERAMULT %.2f) "
            "from CCameraControl::updateGameCamera.",
            static_cast<double>(Ogre::Radian(m.camera_yaw).valueDegrees()),
            static_cast<double>(kDefaultCameraDistance), static_cast<double>(m.camera_mult));
    }
    m.camera_follows = true;
    m.follow_subject();

    // What the character's own unit file says it is holding, and what it
    // permanently wears. Before the log line below, so a run's opening lines
    // say what it is armed with.
    m.equip_starting_weapon();
    m.equip_carried_particles();

    // And the animal that came with it. Here rather than at load time because a
    // pet needs three things that only exist by now: somebody to stand beside,
    // a level to stand on, and a navigation grid to follow them across.
    if (!m.config.pet.empty()) m.spawn_pet(m.config.pet);

    // The player is a character the level took in like any other, so it
    // carries the same sheen (`CLevel::addCharacter`).
    //
    apply_rim_light(m.subject, m.rim_light_texture);
    // The player's model draws behind (`CCharacter::loadModel`:
    // `RENDERBEHIND`, which no player file authors), its weapon and
    // worn pieces with it; and the outline is drawn now the game is in play
    // (`CGame::renderableQueued` refuses it in game state 0).
    m.set_subject_render_behind(true, false);
    g_view_renderables.behind_in_play = true;
    if (m.config.invulnerable) m.god_mode = true;
    TL_LOG_INFO("player control on: body radius %.2f, height %.2f", body.radius, body.height);
    TL_LOG_INFO("  camera offset (%.2f, %.2f, %.2f), heading %.1f deg, CAMERAMULT %.2f",
                m.camera_offset.x, m.camera_offset.y, m.camera_offset.z,
                m.camera_yaw * 180.0f / 3.14159265358979323846f, m.camera_mult);
    if (m.collision.empty()) {
        TL_LOG_WARN("  walking with no collision geometry loaded");
    }
    if (!subject_in_shot(m.camera, m.subject)) {
        TL_LOG_ERROR(
            "  the character is NOT in the camera's frustum - the framing is wrong, "
            "and the frame will not show what you expect");
    }
    if (m.automap_initialized && m.config.minimap) {
        m.automap.show();
    }
    return true;
}

// `CTriggerUnit::setUnitModelName` ->
// `unitInit` for each `Unit Trigger` whose
// `MODEL` a unit file under `media/units/props/` names: its `MAXSTATES` and
// `LOOPTYPE`, and the `resetTrigger` it ends with.
void Renderer::Impl::init_unit_triggers(tl::core::LevelScript& machine) {
    for (const tl::core::LevelScript::UnitTriggerView& view : machine.every_unit_trigger()) {
        if (view.unit.empty()) continue;
        TriggerProp prop;
        trigger_prop(view.unit, prop);
        if (!prop.found) continue;
        machine.init_unit_trigger(view.id, prop.max_states, prop.loop_type);
    }
}

void Renderer::Impl::log_puzzles(const tl::core::LevelScript& script) {
    for (const tl::core::LevelScript::NodeRecord& n : script.nodes_of_type(46)) {
        const uint32_t count = script.puzzle_active_inputs(n.node);
        std::string order;
        for (uint32_t i = 0; i < count; ++i) {
            order += (i == 0 ? "" : ", ") + std::to_string(script.puzzle_order_at(n.node, i) + 1);
        }
        TL_LOG_INFO("  puzzle '%s': %u input(s), to be pulled in the order %s", n.name.c_str(),
                    count, order.c_str());
    }
}

}  // namespace tl::render
