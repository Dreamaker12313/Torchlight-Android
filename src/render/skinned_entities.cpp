// See skinned_entities.h.
#include "render/skinned_entities.h"

#include <OgreEntity.h>
#include <OgreMesh.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>

namespace tl::render {

namespace {

bool names_a_skeleton(const Ogre::Entity* entity) {
    const Ogre::MeshPtr& mesh = entity->getMesh();
    return mesh && mesh->hasSkeleton();
}

}  // namespace

SkinnedEntities& skinned_entities() {
    // Never destroyed: a Root's teardown still destroys entities through it.
    static auto* const registry = new SkinnedEntities();
    return *registry;
}

void SkinnedEntities::install(Ogre::Root& root) {
    Ogre::MovableObjectFactory* current = root.getMovableObjectFactory(Ogre::MOT_ENTITY);
    if (current == this) return;
    inner_ = current;
    listed_.clear();
    view_stale_ = true;
    root.addMovableObjectFactory(this, /*overrideExisting=*/true);
}

const Ogre::String& SkinnedEntities::getType() const { return Ogre::MOT_ENTITY; }

Ogre::MovableObject* SkinnedEntities::createInstanceImpl(const Ogre::String& name,
                                                         const Ogre::NameValuePairList* params) {
    // The manager is told by our caller, after this returns.
    Ogre::MovableObject* made = inner_->createInstance(name, nullptr, params);
    auto* entity = static_cast<Ogre::Entity*>(made);
    if (names_a_skeleton(entity)) {
        listed_.emplace(name, entity);
        view_stale_ = true;
    }
    return made;
}

void SkinnedEntities::destroyInstance(Ogre::MovableObject* object) {
    if (listed_.erase({object->getName(), static_cast<Ogre::Entity*>(object)}) != 0) {
        view_stale_ = true;
    }
    inner_->destroyInstance(object);
}

void SkinnedEntities::rescan() {
    if (inner_ == nullptr) return;
    for (const auto& [unused, scene] : Ogre::Root::getSingleton().getSceneManagers()) {
        for (const auto& entry : scene->getMovableObjects(Ogre::MOT_ENTITY)) {
            auto* entity = static_cast<Ogre::Entity*>(entry.second);
            if (names_a_skeleton(entity) && listed_.emplace(entry.first, entity).second) {
                view_stale_ = true;
            }
        }
    }
}

const std::vector<Ogre::Entity*>& SkinnedEntities::of(const Ogre::SceneManager* scene) {
    if (view_stale_ || view_scene_ != scene) {
        view_.clear();
        for (const auto& [name, entity] : listed_) {
            if (entity->_getManager() == scene) view_.push_back(entity);
        }
        view_scene_ = scene;
        view_stale_ = false;
    }
    return view_;
}

}  // namespace tl::render
