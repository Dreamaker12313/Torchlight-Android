// The GL driver's own thread (Mesa's glthread), on the console: the GL calls are recorded on the
// game's thread and executed by a worker on core 1. Only the patched switch-mesa can start it;
// with the stock library everything here is inert.
//
// OGRE stops mapping buffers when it runs (a `glMapBufferRange` waits for the worker to drain):
// without RSC_MAPBUFFER it keeps a copy of each buffer and uploads with `glBufferData` /
// `glBufferSubData`, which glthread queues. The copies cost memory.
#pragma once

#include <string>

namespace tl::render {

class GlThread {
public:
    /// Whether to ask for it. On unless `sdmc:/switch/torchlight/no_glthread` exists, or the
    /// menu's GL THREAD row says otherwise. Process-wide: takes effect at `Renderer::restart()`.
    static bool wanted();
    static void set_wanted(bool on);

    /// Whether the linked driver can start it at all (the patched switch-mesa).
    static bool driver_supports();

    /// Before the render window, and so the GL context, is created.
    static void request();

    /// After it: whether the driver started it for the context just made.
    static bool started();

    /// What glthread cost the game's thread since the previous call, from the patched Mesa's
    /// counters: syncs (waits for the worker to empty its queue) and their time, waits for room
    /// in a full queue, large uploads queued as a copy, and the calls that synced most. False
    /// when the driver keeps no such counters.
    struct Costs {
        unsigned syncs = 0;
        double sync_ms = 0.0;
        double full_ms = 0.0;
        unsigned heap_uploads = 0;
        double heap_kib = 0.0;
        std::string by_call;  ///< "BufferSubData 120, GetIntegerv 30, ..."
    };
    static bool take_costs(Costs& out);

    /// What shader compiles and program links cost the game's thread since the previous call,
    /// from the patched OGRE GLES2 render system's totals. False where it is not built.
    struct ShaderCosts {
        unsigned compiles = 0;
        double compile_ms = 0.0;
        double worst_compile_ms = 0.0;
        unsigned links = 0;
        double link_ms = 0.0;
        double worst_link_ms = 0.0;
    };
    static bool take_shader_costs(ShaderCosts& out);
};

}  // namespace tl::render
