// PORT: the skinned units' poses for the coming frame, worked out on the job pool before OGRE
// draws it (core/job_pool.h). OGRE works a pose out lazily, as it puts an entity in the render
// queue, one after another on the game's thread; this does the same work first, in parallel, and
// OGRE's own call finds the bones current for the frame and skips it. What it leaves alone, OGRE
// still poses as before: an entity off the camera, or sharing its skeleton. Only the bones are
// worked out here; a CPU blend and vertex animation, which write GL buffers, stay in OGRE's pass.
#include "core/job_pool.h"
#include "render/renderer_impl.h"
#include "render/skinned_entities.h"

#include <OgreAnimation.h>
#include <OgreAnimationState.h>
#include <OgreEntity.h>
#include <OgreSkeletonInstance.h>

namespace tl::render {

namespace {

/// Whether `entity` is one this pass may pose off the game's thread: its own skeleton instance
/// (two entities sharing one would pose it twice at once), and changed since its last pose.
/// Not `isHardwareAnimationEnabled`: asked before the frame, it answers for a material whose
/// shaders are not made yet, and OGRE keeps that answer - the entity would be skinned on the CPU.
bool posable(Ogre::Entity* entity) {
    if (!entity->hasSkeleton() || !entity->isInScene() || !entity->isVisible()) return false;
    if (entity->sharesSkeletonInstance()) return false;
    return entity->_isAnimationDirty();
}

}  // namespace

void Renderer::Impl::evaluate_poses() {
    tl::core::JobPool& pool = tl::core::job_pool();
    if (pool.workers() == 0 || scene == nullptr || camera == nullptr) return;
    const unsigned long long started = frame_phases.now();

    pose_jobs.clear();
    for (Ogre::Entity* entity : skinned_entities().of(scene)) {
        if (!posable(entity)) continue;
        // What the workers read and must not bring up to date themselves, done here: the
        // entity's node, whose transform the tag points on its bones combine with, and each
        // playing animation's keyframe index, which OGRE builds on first use and shares
        // between every entity of the skeleton.
        Ogre::SceneNode* node = entity->getParentSceneNode();
        if (node == nullptr) continue;
        node->_getDerivedPosition();
        if (!camera->isVisible(entity->getWorldBoundingBox(true))) continue;
        Ogre::SkeletonInstance* skeleton = entity->getSkeleton();
        for (Ogre::AnimationState* state :
             entity->getAllAnimationStates()->getEnabledAnimationStates()) {
            if (Ogre::Animation* animation =
                    skeleton->_getAnimationImpl(state->getAnimationName())) {
                animation->_getTimeIndex(state->getTimePosition());
            }
        }
        pose_jobs.push_back(entity);
    }

    pool.parallel_for(pose_jobs.size(), 4, [this](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) pose_jobs[i]->_cacheBoneMatricesForFrame();
    });
    posed_entities += pose_jobs.size();
    frame_phases.poses += frame_phases.now() - started;
}

}  // namespace tl::render
