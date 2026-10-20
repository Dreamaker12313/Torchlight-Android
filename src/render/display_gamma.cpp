#include "render/display_gamma.h"

#include <algorithm>
#include <cmath>

#include <OgreCamera.h>
#include <OgreManualObject.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreRenderTarget.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreTechnique.h>
#include <OgreViewport.h>

#include "core/log.h"
#include "core/settings_menu.h"

namespace tl::render {

namespace {

using tl::core::GammaBlend;

const char* material_name(GammaBlend blend) {
    switch (blend) {
        case GammaBlend::MultiplyAdd: return "tl/gamma/multiply_add";
        case GammaBlend::MultiplySubtract: return "tl/gamma/multiply_subtract";
        case GammaBlend::Amplify: return "tl/gamma/amplify";
    }
    return "tl/gamma/multiply_add";
}

/// The three blends of `core::GammaBlend`. The colour comes from the quad's
/// vertices; the destination's alpha is kept (a dump's PNG keeps its own).
void ensure_material(GammaBlend blend) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    const char* name = material_name(blend);
    if (materials.getByName(name, Ogre::RGN_INTERNAL)) return;

    Ogre::MaterialPtr material = materials.create(name, Ogre::RGN_INTERNAL);
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setDepthCheckEnabled(false);
    pass->setDepthWriteEnabled(false);
    pass->setCullingMode(Ogre::CULL_NONE);
    pass->setFog(true, Ogre::FOG_NONE);
    pass->setVertexColourTracking(Ogre::TVC_DIFFUSE);
    switch (blend) {
        case GammaBlend::MultiplyAdd:  // src * 1 + dst * src.a
            pass->setSeparateSceneBlending(Ogre::SBF_ONE, Ogre::SBF_SOURCE_ALPHA, Ogre::SBF_ZERO,
                                           Ogre::SBF_ONE);
            break;
        case GammaBlend::MultiplySubtract:  // dst * src.a - src * 1
            pass->setSeparateSceneBlending(Ogre::SBF_ONE, Ogre::SBF_SOURCE_ALPHA, Ogre::SBF_ZERO,
                                           Ogre::SBF_ONE);
            pass->setSeparateSceneBlendingOperation(Ogre::SBO_REVERSE_SUBTRACT, Ogre::SBO_ADD);
            break;
        case GammaBlend::Amplify:  // src * dst + dst * 1
            pass->setSeparateSceneBlending(Ogre::SBF_DEST_COLOUR, Ogre::SBF_ONE, Ogre::SBF_ZERO,
                                           Ogre::SBF_ONE);
            break;
    }
}

/// A vertex colour reaches the GPU as a byte and `ColourValue::getAsABGR` truncates; centred on
/// the nearest byte instead.
float nearest_byte(float v) {
    const long n = std::clamp(std::lround(v * 255.0f), 0L, 255L);
    return (static_cast<float>(n) + 0.5f) / 255.0f;
}

}  // namespace

bool DisplayGamma::create(Ogre::Root* root, Ogre::RenderWindow* window,
                          const tl::core::GameSettings* settings) {
    if (root == nullptr || window == nullptr || settings == nullptr) return false;
    root_ = root;
    window_ = window;
    settings_ = settings;

    ensure_material(GammaBlend::MultiplyAdd);
    ensure_material(GammaBlend::MultiplySubtract);
    ensure_material(GammaBlend::Amplify);

    // Its own scene, so the extra viewport walks four quads and not the
    // level's graph, and none of the level's listeners hear it.
    scene_ = root_->createSceneManager();
    camera_ = scene_->createCamera("tl/gamma/camera");
    scene_->getRootSceneNode()->createChildSceneNode()->attachObject(camera_);

    for (int i = 0; i < kMaxSteps; ++i) {
        Ogre::ManualObject* quad = scene_->createManualObject();
        quad->setUseIdentityProjection(true);
        quad->setUseIdentityView(true);
        quad->setQueryFlags(0);
        // A queue each, so the blends run in order. Not from `RENDER_QUEUE_MAIN`: a transparent
        // pass there is moved to `RENDER_QUEUE_TRANSPARENTS` (`RenderQueue::addRenderable`).
        quad->setRenderQueueGroup(static_cast<Ogre::uint8>(Ogre::RENDER_QUEUE_6 + i));
        quad->setVisible(false);
        scene_->getRootSceneNode()->attachObject(quad);
        quads_[static_cast<size_t>(i)] = quad;
    }

    attach(window_);
    window_->addListener(this);
    return true;
}

void DisplayGamma::destroy() {
    if (window_ != nullptr) {
        window_->removeListener(this);
        if (window_->hasViewportWithZOrder(kViewportZOrder))
            window_->removeViewport(kViewportZOrder);
    }
    if (root_ != nullptr && scene_ != nullptr) root_->destroySceneManager(scene_);
    root_ = nullptr;
    window_ = nullptr;
    scene_ = nullptr;
    camera_ = nullptr;
    quads_.fill(nullptr);
}

void DisplayGamma::attach(Ogre::RenderTarget* target) {
    if (camera_ == nullptr || target == nullptr || target->hasViewportWithZOrder(kViewportZOrder)) {
        return;
    }
    Ogre::Viewport* viewport = target->addViewport(camera_, kViewportZOrder);
    viewport->setClearEveryFrame(false);
    viewport->setOverlaysEnabled(false);
    viewport->setSkiesEnabled(false);
    viewport->setShadowsEnabled(false);
    viewport->setMaterialScheme(Ogre::MSN_SHADERGEN);
    refresh();
}

void DisplayGamma::refresh() {
    if (settings_ == nullptr || scene_ == nullptr) return;
    if (!watch_.changed(*settings_)) return;
    rebuild(watch_.sent());
}

void DisplayGamma::rebuild(const tl::core::GammaPair& pair) {
    const tl::core::GammaLine line = tl::core::gamma_line(pair.brightness, pair.contrast);
    const std::vector<tl::core::GammaBlendStep> steps = tl::core::gamma_blend_steps(line);

    for (size_t i = 0; i < quads_.size(); ++i) {
        Ogre::ManualObject* quad = quads_[i];
        quad->clear();
        if (i >= steps.size()) {
            quad->setVisible(false);
            continue;
        }
        const tl::core::GammaBlendStep& step = steps[i];
        // MultiplyAdd / MultiplySubtract: rgb the offset, alpha the factor.
        // Amplify: rgb the factor.
        const float rgb =
            nearest_byte(step.blend == GammaBlend::Amplify ? step.factor : step.offset);
        const Ogre::ColourValue colour(rgb, rgb, rgb, nearest_byte(step.factor));
        quad->begin(material_name(step.blend), Ogre::RenderOperation::OT_TRIANGLE_STRIP,
                    Ogre::RGN_INTERNAL);
        quad->position(-1.0f, -1.0f, 0.0f);
        quad->colour(colour);
        quad->position(1.0f, -1.0f, 0.0f);
        quad->colour(colour);
        quad->position(-1.0f, 1.0f, 0.0f);
        quad->colour(colour);
        quad->position(1.0f, 1.0f, 0.0f);
        quad->colour(colour);
        quad->end();
        quad->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
        quad->setVisible(true);
    }

    TL_LOG_INFO("display gamma: brightness %d, contrast %d -> %.4f x %+.4f, %zu blend(s)",
                pair.brightness, pair.contrast, static_cast<double>(line.scale),
                static_cast<double>(line.offset), steps.size());
}

}  // namespace tl::render
