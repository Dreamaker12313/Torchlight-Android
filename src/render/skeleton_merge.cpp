// A worn piece's animations: an animation rig's tracks copied onto a skeleton that lacks
// some of its bones, each key rebased onto the piece's own binding pose. Split from
// renderer.cpp; declared in renderer_impl.h.
#include "render/renderer_impl.h"

namespace tl::render::renderer_detail {

namespace {

/// inverse(DestBind) * SrcBind for one bone, as `_mergeSkeletonAnimations()` spells it.
struct BindDelta {
    Ogre::Vector3 translate = Ogre::Vector3::ZERO;
    Ogre::Quaternion rotate = Ogre::Quaternion::IDENTITY;
    Ogre::Vector3 scale = Ogre::Vector3::UNIT_SCALE;
    bool identity = true;
};

BindDelta bind_delta(const Ogre::Bone* from, const Ogre::Bone* to) {
    BindDelta delta;
    delta.translate = from->getInitialPosition() - to->getInitialPosition();
    delta.rotate = to->getInitialOrientation().Inverse() * from->getInitialOrientation();
    delta.scale = from->getInitialScale() / to->getInitialScale();
    constexpr Ogre::Real kTolerance = 1e-3f;
    Ogre::Vector3 axis;
    Ogre::Radian angle;
    delta.rotate.ToAngleAxis(angle, axis);
    delta.identity = delta.translate.positionEquals(Ogre::Vector3::ZERO, kTolerance) &&
                     delta.scale.positionEquals(Ogre::Vector3::UNIT_SCALE, kTolerance) &&
                     Ogre::Math::RealEqual(angle.valueRadians(), 0.0f, kTolerance);
    return delta;
}

/// A source track's keys, rebased by the bone's delta.
void copy_rebased_track(const Ogre::NodeAnimationTrack* track, Ogre::NodeAnimationTrack* copy,
                        const BindDelta& delta) {
    copy->setUseShortestRotationPath(track->getUseShortestRotationPath());
    // `getNodeKeyFrame` takes an unsigned short: a count past 65535
    // could not be indexed, and would have made this loop endless.
    const auto key_count = static_cast<unsigned short>(
        std::min<size_t>(track->getNumKeyFrames(), std::numeric_limits<unsigned short>::max()));
    for (unsigned short k = 0; k < key_count; ++k) {
        const Ogre::TransformKeyFrame* key = track->getNodeKeyFrame(k);
        Ogre::TransformKeyFrame* made = copy->createNodeKeyFrame(key->getTime());
        made->setTranslate(delta.identity ? key->getTranslate()
                                          : delta.translate + key->getTranslate());
        made->setRotation(delta.identity ? key->getRotation() : delta.rotate * key->getRotation());
        made->setScale(delta.identity ? key->getScale() : delta.scale * key->getScale());
    }
}

/// A bone the animation leaves alone still sits on the source's binding pose:
/// a two-key track holding the delta.
void hold_delta(Ogre::NodeAnimationTrack* track, Ogre::Real length, const BindDelta& delta) {
    for (const Ogre::Real time : {Ogre::Real(0), length}) {
        Ogre::TransformKeyFrame* key = track->createNodeKeyFrame(time);
        key->setTranslate(delta.translate);
        key->setRotation(delta.rotate);
        key->setScale(delta.scale);
    }
}

}  // namespace

/// Copy animations onto a skeleton, keeping only the tracks it has bones for.
///
/// **`Skeleton::_mergeSkeletonAnimations()` grows the destination**, and that is
/// fatal here. Read it in `external/ogre/OgreMain/src/OgreSkeleton.cpp`: any
/// source bone whose mapped handle is past the end of the destination is
/// `createBone()`d into it, re-parented, and then `deriveRootBone()` runs. Every
/// `SkeletonInstance` already cloned from that master keeps the bone list it
/// built at clone time, and every `Entity` sized its bone-matrix array from
/// `getNumBones()` when it was created - so the next `_getBoneMatrices()` writes
/// past the end of that array. That is the segfault the Vanquisher's chain and
/// assassin sets produced the moment their idle was linked.
///
/// The bones in question are real and the mismatch is ordinary: a worn set is
/// rigged only for the part of the body it covers, so `Vanquisher_chain_set.
/// skeleton` genuinely lacks bones that `Idle.SKELETON` animates. A track for a
/// bone a mesh does not have has nothing to move, so dropping it is not an
/// approximation - it is what the piece would do anyway if the bone existed and
/// nothing were skinned to it.
///
/// Every kept key is rebased onto the destination's binding pose with OGRE's own
/// delta, as `_mergeSkeletonAnimations()` does for the body: without it a worn set
/// applied the animation rig's keys to its own rest pose and stood nearly still
/// beside a moving body.
///
/// Returns the number of tracks left out, so the caller can say so.
size_t merge_shared_animations(const Ogre::SkeletonPtr& master, const Ogre::Skeleton* source,
                               const Ogre::Skeleton::BoneHandleMap& bones,
                               const Ogre::StringVector& wanted) {
    const unsigned short destination_bones = master->getNumBones();
    const auto source_bones =
        static_cast<unsigned short>(std::min<size_t>(bones.size(), source->getNumBones()));
    std::vector<BindDelta> deltas(source_bones);
    for (unsigned short handle = 0; handle < source_bones; ++handle) {
        if (bones[handle] < destination_bones) {
            deltas[handle] = bind_delta(source->getBone(handle), master->getBone(bones[handle]));
        }
    }

    size_t dropped = 0;
    for (const Ogre::String& name : wanted) {
        const Ogre::Animation* from = source->getAnimation(name);
        if (from == nullptr) continue;
        Ogre::Animation* to = master->createAnimation(name, from->getLength());
        to->setInterpolationMode(from->getInterpolationMode());
        to->setRotationInterpolationMode(from->getRotationInterpolationMode());

        for (unsigned short source_handle = 0; source_handle < source_bones; ++source_handle) {
            const unsigned short handle = bones[source_handle];
            const bool tracked = from->hasNodeTrack(source_handle);
            if (handle >= destination_bones) {
                if (tracked) ++dropped;
                continue;
            }
            if (!tracked && deltas[source_handle].identity) continue;
            Ogre::NodeAnimationTrack* copy = to->createNodeTrack(handle, master->getBone(handle));
            if (tracked) {
                copy_rebased_track(from->getNodeTrack(source_handle), copy, deltas[source_handle]);
            } else {
                hold_delta(copy, to->getLength(), deltas[source_handle]);
            }
        }
    }
    return dropped;
}

}  // namespace tl::render::renderer_detail
