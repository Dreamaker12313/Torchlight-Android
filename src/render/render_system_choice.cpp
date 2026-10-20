// The console's render system; see render_system_choice.h.
#include "render/render_system_choice.h"

#include "core/log.h"

#include <cstdio>

namespace tl::render {
namespace {

#if defined(TL_PLATFORM_SWITCH) && defined(TL_SWITCH_GL3PLUS)
/// -1 until first asked: the file is read once, the menu decides after that.
int g_gl3plus = -1;
#endif

}  // namespace

// PORT: whether the Switch build carries a second GL render system (platform).
bool RenderSystemChoice::selectable() {
#if defined(TL_PLATFORM_SWITCH) && defined(TL_SWITCH_GL3PLUS)
    return true;
#else
    return false;
#endif
}

bool RenderSystemChoice::gl3plus_wanted() {
#if defined(TL_PLATFORM_SWITCH) && defined(TL_SWITCH_GL3PLUS)
    if (g_gl3plus < 0) {
        std::FILE* on = std::fopen("sdmc:/switch/torchlight/gl3plus", "rb");
        g_gl3plus = on != nullptr ? 1 : 0;
        if (on != nullptr) std::fclose(on);
    }
    return g_gl3plus == 1;
#else
    return false;
#endif
}

void RenderSystemChoice::set_gl3plus_wanted(bool on) {
#if defined(TL_PLATFORM_SWITCH) && defined(TL_SWITCH_GL3PLUS)
    if (gl3plus_wanted() == on) return;
    g_gl3plus = on ? 1 : 0;
    TL_LOG_INFO("render system: %s from the next level", on ? "GL3Plus" : "GLES2");
#else
    (void)on;
#endif
}

}  // namespace tl::render
