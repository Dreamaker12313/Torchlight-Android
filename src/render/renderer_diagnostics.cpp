// The renderer's diagnostics: timing windows, long frames and the scene, for the log.
#include "core/platform.h"
#include "render/gl_thread.h"
#include "render/renderer_impl.h"

namespace tl::render {

// PORT: diagnostics, the end of a timing window: glthread's waits, the shaders made, and
// where the driver waited on the GPU. Each is taken every window, so the next starts empty.
void Renderer::Impl::log_driver_costs(bool print, unsigned frames) {
    const double n = frames > 0 ? static_cast<double>(frames) : 1.0;
    tl::render::GlThread::Costs gl_costs;
    const bool have_gl_costs = tl::render::GlThread::take_costs(gl_costs);
    tl::render::GlThread::ShaderCosts shader_costs;
    const bool new_shaders = tl::render::GlThread::take_shader_costs(shader_costs) &&
                             (shader_costs.compiles > 0 || shader_costs.links > 0);
    tl::core::GpuWaitTimes waits_game;
    tl::core::GpuWaitTimes waits_others;
    const bool have_waits = tl::core::gpu_wait_times(waits_game, waits_others);
    if (!print) {
        if (!new_shaders) return;
        TL_LOG_INFO(
            "  shaders in the load and the first window: %u compiled in %.1f ms (worst "
            "%.1f), %u linked in %.1f ms (worst %.1f)",
            shader_costs.compiles, shader_costs.compile_ms, shader_costs.worst_compile_ms,
            shader_costs.links, shader_costs.link_ms, shader_costs.worst_link_ms);
        return;
    }
    // What glthread made the game's thread wait for, a frame, and the large uploads it
    // queued as copies.
    if (have_gl_costs) {
        TL_LOG_INFO(
            "  gl thread: %.1f syncs (%.2f ms) and %.2f ms waiting on a full queue a "
            "frame; "
            "%.1f large uploads (%.0f KiB) queued as copies; syncs by call: %s",
            static_cast<double>(gl_costs.syncs) / n, gl_costs.sync_ms / n, gl_costs.full_ms / n,
            static_cast<double>(gl_costs.heap_uploads) / n, gl_costs.heap_kib / n,
            gl_costs.by_call.empty() ? "none" : gl_costs.by_call.c_str());
    }
    // New shaders over the window, and what they cost the game's
    // thread in all (not a frame): the compile-once question.
    if (new_shaders) {
        TL_LOG_INFO(
            "  shaders: %u compiled in %.1f ms (worst %.1f), %u linked in %.1f ms "
            "(worst %.1f), on the game's thread",
            shader_costs.compiles, shader_costs.compile_ms, shader_costs.worst_compile_ms,
            shader_costs.links, shader_costs.link_ms, shader_costs.worst_link_ms);
    }
    // Where the driver waited on the GPU, a frame: on the game's thread
    // and on the others (glthread's worker), by call.
    if (have_waits) {
        for (int who = 0; who < 2; ++who) {
            const tl::core::GpuWaitTimes& w = who == 0 ? waits_game : waits_others;
            char line[480];
            int used = std::snprintf(
                line, sizeof line, "  gpu waits, %s:", who == 0 ? "game thread" : "other threads");
            for (int kind = 0;
                 kind < tl::core::kGpuWaitKinds && used < static_cast<int>(sizeof line); ++kind) {
                used += std::snprintf(line + used, sizeof line - static_cast<size_t>(used),
                                      " %s %.2f ms (%.1f);", tl::core::gpu_wait_name(kind),
                                      w.ms[kind] / n, static_cast<double>(w.calls[kind]) / n);
            }
            TL_LOG_INFO("%s", line);
        }
    }
}

// PORT: diagnostics, a stutter the five-second averages would hide: a frame
// over 50 ms, split as run() times it.
void Renderer::Impl::log_long_frame(unsigned long long drive_us, unsigned long long advance_us,
                                    unsigned long long render_us, unsigned long long total_us) {
    if (total_us <= 50000) return;
    TL_LOG_INFO("long frame: %.1f ms (drive %.1f, advance %.1f, render %.1f)",
                static_cast<double>(total_us) * 0.001, static_cast<double>(drive_us) * 0.001,
                static_cast<double>(advance_us) * 0.001, static_cast<double>(render_us) * 0.001);
}

// PORT: diagnostics, what the scene holds; logged every timing window and as a
// level begins (what the last one left behind).
void Renderer::Impl::log_scene_census(const char* when) const {
    if (scene == nullptr) return;
    size_t nodes = 0;
    std::vector<Ogre::Node*> stack{scene->getRootSceneNode()};
    while (!stack.empty()) {
        Ogre::Node* n = stack.back();
        stack.pop_back();
        ++nodes;
        for (Ogre::Node* child : n->getChildren()) stack.push_back(child);
    }
    size_t pu_total = 0, pu_started = 0, pu_attached = 0;
    count_particle_systems(scene, pu_total, pu_started, pu_attached);
    const auto count = [&](const char* type) { return scene->getMovableObjects(type).size(); };
    const size_t map_billboards = automap_initialized ? automap.billboard_count() : 0;
    TL_LOG_INFO(
        "  scene%s: %zu nodes in the graph; %zu entities, %zu particle systems (%zu started, %zu "
        "attached), "
        "%zu billboard sets, %zu lights, %zu manual objects; automap %zu billboards",
        when, nodes, count("Entity"), pu_total, pu_started, pu_attached, count("BillboardSet"),
        count("Light"), count("ManualObject"), map_billboards);
}

}  // namespace tl::render
