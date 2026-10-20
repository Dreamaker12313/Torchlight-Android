// Brightness and contrast over the finished frame.
//
// DIVERGENCE (platform): the console sends its ramp to the display hardware
// (`core/display_gamma.h`); a homebrew Switch has none and a desktop window should not change the
// monitor's. The ramp is a clamped line, so the last viewport of the window, in a scene manager of
// its own, blends `core::gamma_blend_steps` over everything drawn before it with up to four
// full-screen quads (vertex colours). Nothing is drawn at the identity.
#pragma once

#include <array>

#include <OgreRenderTargetListener.h>

#include "core/display_gamma.h"

namespace Ogre {
class Camera;
class ManualObject;
class RenderTarget;
class RenderWindow;
class Root;
class SceneManager;
class Viewport;
}  // namespace Ogre

namespace tl::core {
struct GameSettings;
}

namespace tl::render {

class DisplayGamma : public Ogre::RenderTargetListener {
public:
    /// After the window's own viewports. `settings` outlives this object.
    bool create(Ogre::Root* root, Ogre::RenderWindow* window,
                const tl::core::GameSettings* settings);
    /// Before Root goes.
    void destroy();

    /// The same blend as the last viewport of another target (a frame dump).
    void attach(Ogre::RenderTarget* target);

    /// Brings the quads up to the settings when their ints moved (the
    /// console's per-frame check), and before an attached target is drawn.
    void refresh();

    // PORT: the gamma overlay refreshed before a frame (drawing).
    void preRenderTargetUpdate(const Ogre::RenderTargetEvent&) override { refresh(); }

private:
    void rebuild(const tl::core::GammaPair& pair);

    static constexpr int kMaxSteps = 4;
    /// Above the minimap's 2 and anything a menu adds.
    static constexpr int kViewportZOrder = 100;

    Ogre::Root* root_ = nullptr;
    Ogre::RenderWindow* window_ = nullptr;
    Ogre::SceneManager* scene_ = nullptr;
    Ogre::Camera* camera_ = nullptr;
    std::array<Ogre::ManualObject*, kMaxSteps> quads_{};
    const tl::core::GameSettings* settings_ = nullptr;
    tl::core::GammaWatch watch_;
};

}  // namespace tl::render
