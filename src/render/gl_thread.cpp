// The GL driver's own thread; see gl_thread.h.
#include "render/gl_thread.h"

#include "core/log.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#if defined(TL_PLATFORM_SWITCH)
// Defined by the patched switch-mesa: 1 while the last context it made runs glthread. Weak, so
// the stock library links and leaves its address null.
extern "C" __attribute__((weak)) int mesa_switch_glthread_active;

// The patched Mesa's counters (`struct mesa_switch_glthread_stats`; the layout must stay the
// same). Weak for the same reason.
extern "C" {
struct mesa_switch_glthread_stats {
    std::uint64_t sync_ns;
    std::uint64_t full_ns;
    std::uint64_t heap_bytes;
    std::uint32_t syncs;
    std::uint32_t heap_uploads;
    struct {
        const char* func;
        std::uint32_t count;
    } by_func[16];
};
__attribute__((weak)) extern struct mesa_switch_glthread_stats mesa_switch_glthread_stats;
}
#endif

#if defined(TL_PLATFORM_SWITCH) || defined(TL_PLATFORM_ANDROID)
// The OGRE GLES2 render system's shader totals from the port's OGRE patch (the layout must stay
// the same). Weak, so an OGRE built without it still links.
extern "C" {
struct ogre_gles2_shader_stats_t {
    unsigned long long compile_ns, link_ns, worst_compile_ns, worst_link_ns;
    unsigned compiles, links;
};
__attribute__((weak)) extern struct ogre_gles2_shader_stats_t ogre_gles2_shader_stats;
}
#endif

namespace tl::render {
namespace {

/// -1 until first asked: the file is read once, the menu decides after that.
int g_wanted = -1;

}  // namespace

bool GlThread::wanted() {
    if (g_wanted < 0) {
#if defined(TL_PLATFORM_SWITCH)
        std::FILE* off = std::fopen("sdmc:/switch/torchlight/no_glthread", "rb");
        g_wanted = off == nullptr ? 1 : 0;
        if (off != nullptr) std::fclose(off);
#else
        g_wanted = 0;
#endif
    }
    return g_wanted == 1;
}

void GlThread::set_wanted(bool on) {
    if (wanted() == on) return;
    g_wanted = on ? 1 : 0;
    TL_LOG_INFO("gl thread: %s from the next level", on ? "on" : "off");
}

bool GlThread::driver_supports() {
#if defined(TL_PLATFORM_SWITCH)
    return &mesa_switch_glthread_active != nullptr;
#else
    return false;
#endif
}

// PORT: the Switch's Mesa GL thread asked for (platform).
void GlThread::request() {
#if defined(TL_PLATFORM_SWITCH)
    if (!driver_supports()) {
        TL_LOG_INFO(
            "gl thread: devkitPro's switch-mesa has none (scripts/build_mesa.py builds one that "
            "does)");
        return;
    }
    // Read by the driver as it makes the context. Core 1 is also its default:
    // the game has 0, and the mixer and the log writer share 2.
    setenv("mesa_glthread", wanted() ? "true" : "false", 1);
    setenv("mesa_glthread_core", "1", 1);
#endif
}

bool GlThread::started() {
#if defined(TL_PLATFORM_SWITCH)
    return driver_supports() && mesa_switch_glthread_active != 0;
#else
    return false;
#endif
}

bool GlThread::take_costs(Costs& out) {
#if defined(TL_PLATFORM_SWITCH)
    if (&mesa_switch_glthread_stats == nullptr) return false;
    // The counters only grow; each call reports the difference.
    static struct mesa_switch_glthread_stats last {};
    const struct mesa_switch_glthread_stats now = mesa_switch_glthread_stats;
    out.syncs = now.syncs - last.syncs;
    out.sync_ms = static_cast<double>(now.sync_ns - last.sync_ns) / 1.0e6;
    out.full_ms = static_cast<double>(now.full_ns - last.full_ns) / 1.0e6;
    out.heap_uploads = now.heap_uploads - last.heap_uploads;
    out.heap_kib = static_cast<double>(now.heap_bytes - last.heap_bytes) / 1024.0;
    std::vector<std::pair<std::uint32_t, const char*>> calls;
    for (const auto& entry : now.by_func) {
        if (entry.func == nullptr) break;
        std::uint32_t before = 0;
        for (const auto& old : last.by_func) {
            if (old.func == entry.func) before = old.count;
        }
        if (entry.count > before) calls.emplace_back(entry.count - before, entry.func);
    }
    std::sort(calls.begin(), calls.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    out.by_call.clear();
    for (size_t i = 0; i < calls.size() && i < 4; ++i) {
        if (!out.by_call.empty()) out.by_call += ", ";
        out.by_call += std::string(calls[i].second) + ' ' + std::to_string(calls[i].first);
    }
    last = now;
    return true;
#else
    (void)out;
    return false;
#endif
}

// PORT: the shader compile and link costs the OGRE patch keeps, by log window.
bool GlThread::take_shader_costs(ShaderCosts& out) {
#if defined(TL_PLATFORM_SWITCH) || defined(TL_PLATFORM_ANDROID)
    if (&ogre_gles2_shader_stats == nullptr) return false;
    // Read and reset: the worst figures are per window.
    ogre_gles2_shader_stats_t& s = ogre_gles2_shader_stats;
    out.compiles = s.compiles;
    out.compile_ms = static_cast<double>(s.compile_ns) / 1.0e6;
    out.worst_compile_ms = static_cast<double>(s.worst_compile_ns) / 1.0e6;
    out.links = s.links;
    out.link_ms = static_cast<double>(s.link_ns) / 1.0e6;
    out.worst_link_ms = static_cast<double>(s.worst_link_ns) / 1.0e6;
    s = ogre_gles2_shader_stats_t{};
    return true;
#else
    (void)out;
    return false;
#endif
}

}  // namespace tl::render
