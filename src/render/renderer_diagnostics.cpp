// The renderer's diagnostics: timing windows, long frames and the scene, for the log.
#include "core/platform.h"
#include "core/section_profile.h"
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

// PORT: diagnostics, the end of a timing window: its sections (printed when `print`), and
// both they and OGRE's phases start over.
void Renderer::Impl::log_sections(unsigned frames, bool print) {
    if (print) print_sections(frames);
    tl::core::section_totals() = tl::core::SectionTotals{};
    frame_phases.reset();
}

// PORT: diagnostics, a headless run's own timing windows, every 300 frames (it never reaches
// run()'s).
void Renderer::Impl::step_headless_sections() {
    if (!config.headless || ++headless_profile_frames <= 300) return;
    log_sections(headless_profile_frames - 1, true);
    headless_profile_frames = 1;
}

// PORT: diagnostics, the update's sections (core/section_profile.h) and OGRE's phases a frame
// over a timing window, and what the level holds that they scale with.
void Renderer::Impl::print_sections(unsigned frames) {
    const double n = frames > 0 ? static_cast<double>(frames) : 1.0;
    const tl::core::SectionTotals& totals = tl::core::section_totals();
    char line[512];
    int used = std::snprintf(line, sizeof line, "  sections (ms a frame):");
    for (int i = 0; i < static_cast<int>(tl::core::Section::Count); ++i) {
        if (used >= static_cast<int>(sizeof line)) break;
        used += std::snprintf(line + used, sizeof line - static_cast<size_t>(used), " %s %.2f",
                              tl::core::section_name(static_cast<tl::core::Section>(i)),
                              static_cast<double>(totals.ns[i]) / n / 1e6);
    }
    TL_LOG_INFO("%s", line);
    // Per character: how many ran a frame and what each cost.
    using tl::core::Section;
    const auto per = [&](Section s) {
        const int i = static_cast<int>(s);
        return totals.calls[i] > 0
                   ? static_cast<double>(totals.ns[i]) / static_cast<double>(totals.calls[i]) / 1e3
                   : 0.0;
    };
    const auto calls = [&](Section s) {
        return static_cast<double>(totals.calls[static_cast<int>(s)]) / n;
    };
    TL_LOG_INFO(
        "  per character: ai %.1f a frame at %.1f us, pathing %.1f at %.1f us, "
        "motion %.1f at %.1f us; render: particles+controllers %.2f, scene graph %.2f, "
        "cull+animate %.2f, draw calls %.2f ms",
        calls(Section::Ai), per(Section::Ai), calls(Section::Pathing), per(Section::Pathing),
        calls(Section::Motion), per(Section::Motion),
        static_cast<double>(frame_phases.controllers) / n / 1000.0,
        static_cast<double>(frame_phases.scene_graph) / n / 1000.0,
        static_cast<double>(frame_phases.cull) / n / 1000.0,
        static_cast<double>(frame_phases.draw) / n / 1000.0);
    const size_t running = static_cast<size_t>(
        std::count_if(nested_scripts.begin(), nested_scripts.end(),
                      [](const NestedScript& machine) { return machine.running; }));
    TL_LOG_INFO(
        "  held: %zu linked machine(s), %zu running, %zu on the level's list; %zu "
        "particle system(s), %zu waiting to die out; %zu skill event(s), %zu missile(s)",
        nested_scripts.size(), running, level_machines.size(), particle_systems.size(),
        transient_particles.size(), live_skill_events.size(), missiles.size());
}

}  // namespace tl::render
