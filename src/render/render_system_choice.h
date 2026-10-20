// Which of OGRE's render systems the console brings up. The console runs GLES2; a build
// configured with -DTL_SWITCH_GL3PLUS=ON links GL3Plus beside it (the same Mesa through a desktop
// GL core context). It measured no faster, so such a build still starts on GLES2 and GL3Plus has
// to be asked for. Every other build has one render system, and everything here is inert.
#pragma once

namespace tl::render {

class RenderSystemChoice {
public:
    /// Whether this build carries both render systems (TL_SWITCH_GL3PLUS).
    static bool selectable();

    /// GL3Plus rather than GLES2: off unless `sdmc:/switch/torchlight/gl3plus` exists at start or
    /// the menu's RENDERER row asks. Process-wide: takes effect at `Renderer::restart()`.
    static bool gl3plus_wanted();
    static void set_gl3plus_wanted(bool on);
};

}  // namespace tl::render
