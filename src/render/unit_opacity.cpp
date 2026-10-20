// A character's fade drawn; see unit_opacity.h.
#include "render/unit_opacity.h"

#include <OgreEntity.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreRenderQueue.h>
#include <OgreSceneNode.h>
#include <OgreSubEntity.h>
#include <OgreTagPoint.h>
#include <OgreTechnique.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tl::render {
namespace {

const char* const kFadeKey = "tl/opacity";
/// The opacity itself, kept for entity_opacity().
const char* const kOpacityKey = "tl/opacity_value";

/// An entity's materials, their faded clones and each clone's source diffuse alphas (pass
/// 0, technique order), in sub-entity order, and the queue group it is given back. Alphas,
/// not passes: RTSS remakes a clone's shader technique, so a held pass would go stale.
struct Faded {
    std::vector<Ogre::MaterialPtr> own;
    std::vector<Ogre::MaterialPtr> clones;
    std::vector<std::vector<float>> alphas;
    Ogre::uint8 group = Ogre::RENDER_QUEUE_MAIN;
    bool on = false;
};

unsigned g_clones = 0;

void set_entity_opacity(Ogre::Entity* entity, float raw_opacity) {
    Ogre::UserObjectBindings& tags = entity->getUserObjectBindings();
    // Each record keeps the opacity as given (`+0x134`, what the shadow pass
    // and the render-behind test read)...
    tags.setUserAny(kOpacityKey, Ogre::Any(raw_opacity));
    // The console fades in twelve steps: CGenericModel::setOpacity takes
    // opacity * 11 truncated (fctidz, low byte) and uses the material reInitialize
    // built for that step, step * 0.09090909 opaque; 11 is the model's own.
    const auto step = static_cast<std::uint8_t>(static_cast<std::int64_t>(raw_opacity * 11.0f));
    const float opacity = step >= 11 ? 1.0f : static_cast<float>(step) * 0.09090909f;
    const Ogre::Any& held = tags.getUserAny(kFadeKey);
    std::shared_ptr<Faded> faded =
        held.has_value() ? Ogre::any_cast<std::shared_ptr<Faded>>(held) : nullptr;

    if (!(opacity < 1.0f)) {
        if (faded == nullptr || !faded->on) return;
        const size_t n = std::min(faded->own.size(), entity->getNumSubEntities());
        for (size_t i = 0; i < n; ++i) entity->getSubEntity(i)->setMaterial(faded->own[i]);
        entity->setRenderQueueGroup(faded->group);
        faded->on = false;
        return;
    }

    if (faded == nullptr || faded->own.size() != entity->getNumSubEntities()) {
        faded = std::make_shared<Faded>();
        tags.setUserAny(kFadeKey, Ogre::Any(faded));
    }
    if (!faded->on) {
        // Materials can change under an entity (the wardrobe, a highlight):
        // whatever it wears now is what it gets back.
        bool same = faded->clones.size() == entity->getNumSubEntities();
        for (size_t i = 0; same && i < faded->own.size(); ++i) {
            same = faded->own[i] == entity->getSubEntity(i)->getMaterial();
        }
        if (!same) {
            faded->own.clear();
            faded->clones.clear();
            faded->alphas.clear();
            for (Ogre::SubEntity* sub : entity->getSubEntities()) {
                const Ogre::MaterialPtr& own = sub->getMaterial();
                faded->own.push_back(own);
                Ogre::MaterialPtr clone =
                    own->clone(own->getName() + "/tl_fade/" + std::to_string(++g_clones));
                // Without the source's RTSS techniques (as highlight_entity() clones): a
                // frozen copy would never take GPU skinning. RTSS makes the clone's own on
                // its first draw.
                for (unsigned t = clone->getNumTechniques(); t-- > 0;) {
                    if (clone->getTechnique(t)->getSchemeName() != Ogre::MSN_DEFAULT)
                        clone->removeTechnique(t);
                }
                std::vector<float> alphas;
                // Pass 0 of each technique, as setOpacity fades only pass 0: blended,
                // rejecting only alpha <= 5 (cut-out fur and foliage would vanish). Depth
                // write left as it was, as setOpacity leaves it.
                for (Ogre::Technique* technique : clone->getTechniques()) {
                    if (technique->getNumPasses() == 0) continue;
                    Ogre::Pass* pass = technique->getPass(0);
                    if (!pass->isTransparent()) pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    pass->setAlphaRejectSettings(Ogre::CMPF_GREATER, 5, false);
                    alphas.push_back(pass->getDiffuse().a);
                }
                faded->clones.push_back(clone);
                faded->alphas.push_back(std::move(alphas));
            }
        }
        for (size_t i = 0; i < faded->clones.size(); ++i) {
            entity->getSubEntity(i)->setMaterial(faded->clones[i]);
        }
        // DIVERGENCE: OGRE 14 draws the blended scenery in group 95, after the main group;
        // a faded model goes there too, as OGRE 1.6 sorted it with that scenery.
        faded->group = entity->getRenderQueueGroup();
        entity->setRenderQueueGroup(Ogre::RENDER_QUEUE_TRANSPARENTS);
        faded->on = true;
    }
    // Pass 0 of every technique the clone has now: its source techniques, each
    // from its own alpha, and RTSS's, made from the first source technique.
    for (size_t i = 0; i < faded->clones.size(); ++i) {
        const std::vector<float>& alphas = faded->alphas[i];
        const float first = alphas.empty() ? 1.0f : alphas.front();
        size_t source = 0;
        for (Ogre::Technique* technique : faded->clones[i]->getTechniques()) {
            if (technique->getNumPasses() == 0) continue;
            float alpha = first;
            if (technique->getSchemeName() == Ogre::MSN_DEFAULT) {
                if (source < alphas.size()) alpha = alphas[source];
                ++source;
            }
            Ogre::Pass* pass = technique->getPass(0);
            Ogre::ColourValue diffuse = pass->getDiffuse();
            diffuse.a = alpha * opacity;
            pass->setDiffuse(diffuse);
        }
    }
}

void fade_entity(Ogre::Entity* entity, float opacity) {
    set_entity_opacity(entity, opacity);
    for (Ogre::MovableObject* object : entity->getAttachedObjects()) {
        if (auto* worn = dynamic_cast<Ogre::Entity*>(object)) fade_entity(worn, opacity);
    }
}

}  // namespace

void set_node_opacity(Ogre::SceneNode* node, float opacity) {
    if (node == nullptr) return;
    for (Ogre::MovableObject* object : node->getAttachedObjects()) {
        if (auto* entity = dynamic_cast<Ogre::Entity*>(object)) fade_entity(entity, opacity);
    }
    for (Ogre::Node* child : node->getChildren()) {
        set_node_opacity(dynamic_cast<Ogre::SceneNode*>(child), opacity);
    }
}

void set_node_casts_shadows(Ogre::SceneNode* node, bool on) {
    if (node == nullptr) return;
    // The flag on what hangs off its bones too: OGRE 14 asks each attached
    // object's own flags of the viewport's mask (`MovableObject::isVisible`).
    const auto cast = [on](Ogre::MovableObject* movable) {
        movable->setCastShadows(on);
        if (on) {
            movable->addVisibilityFlags(kVisibilityShadowMap);
        } else {
            movable->removeVisibilityFlags(kVisibilityShadowMap);
        }
    };
    for (Ogre::MovableObject* object : node->getAttachedObjects()) {
        auto* entity = dynamic_cast<Ogre::Entity*>(object);
        if (entity == nullptr) continue;
        cast(entity);
        for (Ogre::MovableObject* worn : entity->getAttachedObjects()) cast(worn);
    }
    for (Ogre::Node* child : node->getChildren()) {
        set_node_casts_shadows(dynamic_cast<Ogre::SceneNode*>(child), on);
    }
}

namespace {
/// The outline flag set_entity_light() keeps for entity_outline_hidden().
const char* const kOutlineHiddenKey = "tl/outline_hidden";
}  // namespace

void set_entity_light(Ogre::Entity* entity, float scale, bool outline_hidden) {
    if (entity == nullptr) return;
    // Every record's SubEntity `+0x1c`; the outline flag only where a record
    // has a render-behind material, which here is every record of a model
    // set_render_behind() ran on.
    const Ogre::Vector4 value(scale, 0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < entity->getNumSubEntities(); ++i) {
        entity->getSubEntity(i)->setCustomParameter(kRunicLightScaleParam, value);
    }
    entity->getUserObjectBindings().setUserAny(kOutlineHiddenKey, Ogre::Any(outline_hidden));
    for (Ogre::MovableObject* object : entity->getAttachedObjects()) {
        if (auto* worn = dynamic_cast<Ogre::Entity*>(object)) {
            set_entity_light(worn, scale, outline_hidden);
        }
    }
}

void set_node_light(Ogre::SceneNode* node, float scale, bool outline_hidden) {
    if (node == nullptr) return;
    for (Ogre::MovableObject* object : node->getAttachedObjects()) {
        if (auto* entity = dynamic_cast<Ogre::Entity*>(object)) {
            set_entity_light(entity, scale, outline_hidden);
        }
    }
    for (Ogre::Node* child : node->getChildren()) {
        set_node_light(dynamic_cast<Ogre::SceneNode*>(child), scale, outline_hidden);
    }
}

bool entity_outline_hidden(const Ogre::Entity* entity) {
    if (entity == nullptr) return false;
    const Ogre::Any& held = entity->getUserObjectBindings().getUserAny(kOutlineHiddenKey);
    return held.has_value() && Ogre::any_cast<bool>(held);
}

float entity_opacity(const Ogre::Entity* entity) {
    if (entity == nullptr) return 1.0f;
    const Ogre::Any& held = entity->getUserObjectBindings().getUserAny(kOpacityKey);
    return held.has_value() ? Ogre::any_cast<float>(held) : 1.0f;
}

}  // namespace tl::render
