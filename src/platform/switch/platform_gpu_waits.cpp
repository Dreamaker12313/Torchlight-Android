// Where the GL driver waits on the GPU, timed from the outside: libdrm_nouveau's buffer map and
// wait, its push-buffer kick and space, Mesa's fence wait and libnx's GPU fence wait, each a
// link-time wrap (`--wrap=`). Time and count go to the game's thread or to the others (nearly
// always the glthread worker), read by gpu_wait_times().
#include "core/platform.h"

#include <atomic>
#include <cstdint>

#include <switch.h>

namespace tl::core {
namespace {

/// The thread that calls start_gpu_waits(): the game's.
Handle g_game_thread = INVALID_HANDLE;

struct Counter {
    std::atomic<std::uint64_t> ticks{0};
    std::atomic<std::uint32_t> calls{0};
};
/// [kind][0 game thread, 1 others]
Counter g_counters[kGpuWaitKinds][2];

class Timed {
public:
    explicit Timed(int kind) : kind_(kind), start_(armGetSystemTick()) {}
    ~Timed() {
        Counter& c = g_counters[kind_][threadGetCurHandle() == g_game_thread ? 0 : 1];
        c.ticks.fetch_add(armGetSystemTick() - start_, std::memory_order_relaxed);
        c.calls.fetch_add(1, std::memory_order_relaxed);
    }
    Timed(const Timed&) = delete;
    Timed& operator=(const Timed&) = delete;

private:
    int kind_;
    std::uint64_t start_;
};

}  // namespace

void start_gpu_waits() { g_game_thread = threadGetCurHandle(); }

const char* gpu_wait_name(int kind) {
    static const char* const kNames[kGpuWaitKinds] = {"bo map",   "bo wait", "fence wait",
                                                      "nv fence", "kick",    "push space",
                                                      "dequeue",  "queue",   "condvar"};
    return kind >= 0 && kind < kGpuWaitKinds ? kNames[kind] : "?";
}

bool gpu_wait_times(GpuWaitTimes& game, GpuWaitTimes& others) {
    static std::uint64_t last_ticks[kGpuWaitKinds][2] = {};
    static std::uint32_t last_calls[kGpuWaitKinds][2] = {};
    const double ms_per_tick = 1000.0 / static_cast<double>(armGetSystemTickFreq());
    for (int kind = 0; kind < kGpuWaitKinds; ++kind) {
        for (int who = 0; who < 2; ++who) {
            const std::uint64_t ticks = g_counters[kind][who].ticks.load(std::memory_order_relaxed);
            const std::uint32_t calls = g_counters[kind][who].calls.load(std::memory_order_relaxed);
            GpuWaitTimes& out = who == 0 ? game : others;
            out.ms[kind] = static_cast<double>(ticks - last_ticks[kind][who]) * ms_per_tick;
            out.calls[kind] = calls - last_calls[kind][who];
            last_ticks[kind][who] = ticks;
            last_calls[kind][who] = calls;
        }
    }
    return true;
}

}  // namespace tl::core

// The wrapped calls. Opaque pointers: the port includes neither libdrm's nor
// Mesa's headers, and these are the same arguments passed straight through.
using tl::core::Timed;
extern "C" {
int __real_nouveau_bo_map(void* bo, std::uint32_t access, void* client);
int __real_nouveau_bo_wait(void* bo, std::uint32_t access, void* client);
bool __real_nouveau_fence_wait(void* fence, void* debug);
int __real_nouveau_pushbuf_kick(void* push, void* channel);
int __real_nouveau_pushbuf_space(void* push, std::uint32_t dwords, std::uint32_t relocs,
                                 std::uint32_t pushes);
Result __real_nvFenceWait(NvFence* fence, s32 timeout_us);
Result __real_nwindowDequeueBuffer(NWindow* nw, s32* out_slot, NvMultiFence* out_fence);
Result __real_nwindowQueueBuffer(NWindow* nw, s32 slot, const NvMultiFence* fence);
Result __real_condvarWaitTimeout(CondVar* c, Mutex* m, u64 timeout);

Result __wrap_nwindowDequeueBuffer(NWindow* nw, s32* out_slot, NvMultiFence* out_fence) {
    Timed t(6);
    return __real_nwindowDequeueBuffer(nw, out_slot, out_fence);
}
Result __wrap_nwindowQueueBuffer(NWindow* nw, s32 slot, const NvMultiFence* fence) {
    Timed t(7);
    return __real_nwindowQueueBuffer(nw, slot, fence);
}
Result __wrap_condvarWaitTimeout(CondVar* c, Mutex* m, u64 timeout) {
    Timed t(8);
    return __real_condvarWaitTimeout(c, m, timeout);
}

int __wrap_nouveau_bo_map(void* bo, std::uint32_t access, void* client) {
    Timed t(0);
    return __real_nouveau_bo_map(bo, access, client);
}
int __wrap_nouveau_bo_wait(void* bo, std::uint32_t access, void* client) {
    Timed t(1);
    return __real_nouveau_bo_wait(bo, access, client);
}
bool __wrap_nouveau_fence_wait(void* fence, void* debug) {
    Timed t(2);
    return __real_nouveau_fence_wait(fence, debug);
}
Result __wrap_nvFenceWait(NvFence* fence, s32 timeout_us) {
    Timed t(3);
    return __real_nvFenceWait(fence, timeout_us);
}
int __wrap_nouveau_pushbuf_kick(void* push, void* channel) {
    Timed t(4);
    return __real_nouveau_pushbuf_kick(push, channel);
}
int __wrap_nouveau_pushbuf_space(void* push, std::uint32_t dwords, std::uint32_t relocs,
                                 std::uint32_t pushes) {
    Timed t(5);
    return __real_nouveau_pushbuf_space(push, dwords, relocs, pushes);
}
}
