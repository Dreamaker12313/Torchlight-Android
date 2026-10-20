// A character's fade drawn: CGenericModel::setOpacity in
// OGRE 14 terms. Each entity gets, on its first fade, a clone of each material (its RTSS
// technique included) drawn blended at diffuse alpha times the opacity; at 1 it gets its
// own materials back.
#pragma once

#include <OgrePrerequisites.h>

#include <cstddef>
#include <cstdint>

namespace tl::render {

/// The shadow map's visibility flag. CGenericModel::setCastsShadows gives a
/// caster 4, and CGameClient::preRenderTargetUpdate masks 4 for the shadow map;
/// the port's 4 is the light map's (kVisibilityLightMap), so a caster carries 8.
constexpr std::uint32_t kVisibilityShadowMap = 0x8;

/// Every entity attached under `node`, and every object attached to those
/// entities' bones (a worn weapon), at `opacity` (0..1).
void set_node_opacity(Ogre::SceneNode* node, float opacity);

/// The same entities cast a shadow or not (`CGenericModel::setCastsShadows`):
/// each record's `+0x1` and the visibility flag (kVisibilityShadowMap).
void set_node_casts_shadows(Ogre::SceneNode* node, bool on);

/// The opacity set_node_opacity() last gave `entity`, 1 if never: the
/// `CRenderableStates +0x34` (X360 `+0x134`) `CGenericModel::setOpacity`
/// writes and the shadow map's caster pass reads.
float entity_opacity(const Ogre::Entity* entity);

/// The renderable custom parameter that carries Runic's per-renderable light
/// (`Renderable +0x1c` on the console), read by the patched
/// `AutoParamDataSource` (patches/ogre/0017): the surface ambient and emissive
/// colours are multiplied by its x.
constexpr std::size_t kRunicLightScaleParam = 0x52554e43;

/// `CGenericModel::setLightOverride` on every entity under
/// `node` and what hangs off their bones: each sub-entity's light `scale`, and
/// whether its render-behind outline is hidden (the record's `[0]`).
void set_node_light(Ogre::SceneNode* node, float scale, bool outline_hidden);
/// The same on one entity and what hangs off its bones.
void set_entity_light(Ogre::Entity* entity, float scale, bool outline_hidden);

/// Whether the last set_node_light() hid `entity`'s outline; false if never.
bool entity_outline_hidden(const Ogre::Entity* entity);

}  // namespace tl::render
