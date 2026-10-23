// PORT: the scene's entities that may carry a skeleton, kept as OGRE makes and destroys them, so
// the passes that want only those (GPU skinning, the poses) need not walk every entity each frame:
// in a dungeon that is thousands of props for a hundred characters.
//
// It stands in front of OGRE's own entity factory and hands every call on to it. An entity is
// listed when its mesh names a skeleton as it is made; `rescan()` lists what a mesh given a
// skeleton later re-initialises. Every user still asks `hasSkeleton()` itself: being listed is
// "may have one", never "has one".
#pragma once

#include <OgreMovableObject.h>
#include <OgrePrerequisites.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tl::render {

class SkinnedEntities final : public Ogre::MovableObjectFactory {
public:
    /// Puts the registry in front of `root`'s entity factory. Again for a new Root.
    void install(Ogre::Root& root);

    /// The listed entities of `scene`, in the order the scene's own map holds them (by name).
    const std::vector<Ogre::Entity*>& of(const Ogre::SceneManager* scene);

    /// After a mesh has been given a skeleton: lists every entity, in every scene, whose mesh
    /// names one.
    void rescan();

    const Ogre::String& getType() const override;
    void destroyInstance(Ogre::MovableObject* object) override;

private:
    Ogre::MovableObject* createInstanceImpl(const Ogre::String& name,
                                            const Ogre::NameValuePairList* params) override;

    Ogre::MovableObjectFactory* inner_ = nullptr;
    // By name, then pointer: one scene's entities in its map's order; names repeat across scenes.
    std::set<std::pair<std::string, Ogre::Entity*>> listed_;
    std::vector<Ogre::Entity*> view_;
    const Ogre::SceneManager* view_scene_ = nullptr;
    bool view_stale_ = true;
};

/// The process's one registry (OGRE's factories are the Root's, and outlive a scene).
SkinnedEntities& skinned_entities();

}  // namespace tl::render
