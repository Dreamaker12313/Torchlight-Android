// Where the game's thread spends its time on the desktop, sampled as the console's sampler does
// (platform/switch/platform_profiler.cpp), into the same file format: a thread pauses the game's
// thread every 2 ms, reads its registers, unwinds its stack through the image's unwind tables, and
// resumes it.
//
// Windows only, and only while `TL_PROFILE` names the file to write. Samples are offsets into the
// image (the program counter, 0 where the console has its link register, then the return
// addresses); a program counter in a system library (the C runtime's allocator, the GL driver) is
// 0xFFFFFFFF, its callers in the program after it. profiler_mark() labels what follows.
//
// Nothing that can take a lock (malloc, stdio, logging) runs while the game's thread is paused.
#include "core/platform.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// After windows.h, which it needs.
#include <timeapi.h>

#include "core/log.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tl::core {
namespace {

constexpr DWORD kIntervalMs = 2;
constexpr int kMaxDepth = 40;
constexpr uint16_t kMarker = 0xffff;
constexpr uint32_t kOutside = 0xffffffffu;

struct Sample {
    uint16_t depth = 0;
    uint16_t thread = 0;
    uint32_t offsets[kMaxDepth] = {};
};

struct Sampler {
    std::FILE* out = nullptr;
    HANDLE target = nullptr;
    uintptr_t image = 0;
    uintptr_t image_end = 0;
    uintptr_t stack_low = 0;  // the game thread's stack, which the walk reads
    uintptr_t stack_high = 0;
    std::thread worker;
    std::atomic<bool> stop{false};
    std::mutex file_lock;  // the worker's samples against the game thread's marks
};

Sampler* g_sampler = nullptr;

/// The paused thread's program counter and return addresses, by the unwind tables. Reads its stack
/// in place; allocates nothing.
uint16_t walk(const CONTEXT& paused, const Sampler& s, uint32_t* offsets) {
    CONTEXT context = paused;
    const auto ours = [&s](DWORD64 pc) { return pc >= s.image && pc < s.image_end; };
    uint16_t depth = 0;
    offsets[depth++] = ours(context.Rip) ? static_cast<uint32_t>(context.Rip - s.image) : kOutside;
    offsets[depth++] = 0;  // the console's link register slot
    // Up the stack through the libraries' frames too, keeping ours, until the stack ends.
    for (int frames = 0; frames < 128 && depth < kMaxDepth; ++frames) {
        if (context.Rsp < s.stack_low || context.Rsp + 8 > s.stack_high) break;
        DWORD64 base = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
        if (function == nullptr) {
            // A leaf: its return address is on top of the stack.
            // NOLINTNEXTLINE(performance-no-int-to-ptr): the paused thread's stack pointer
            context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
            context.Rsp += 8;
        } else {
            void* handler_data = nullptr;
            DWORD64 frame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, function, &context,
                             &handler_data, &frame, nullptr);
        }
        if (context.Rip == 0) break;
        if (ours(context.Rip)) offsets[depth++] = static_cast<uint32_t>(context.Rip - s.image);
    }
    return depth;
}

void run(Sampler* s) {
    timeBeginPeriod(1);
    Sample sample;
    CONTEXT context;
    while (!s->stop.load(std::memory_order_relaxed)) {
        Sleep(kIntervalMs);
        if (SuspendThread(s->target) == static_cast<DWORD>(-1)) break;
        std::memset(&context, 0, sizeof context);
        context.ContextFlags = CONTEXT_FULL;
        sample.depth = 0;
        if (GetThreadContext(s->target, &context) != 0) {
            sample.depth = walk(context, *s, sample.offsets);
        }
        ResumeThread(s->target);
        const std::lock_guard<std::mutex> hold(s->file_lock);
        std::fwrite(&sample.depth, sizeof(uint16_t), 1, s->out);
        std::fwrite(&sample.thread, sizeof(uint16_t), 1, s->out);
        std::fwrite(sample.offsets, sizeof(uint32_t), sample.depth, s->out);
    }
    timeEndPeriod(1);
}

}  // namespace

void start_profiler() {
    const char* path = std::getenv("TL_PROFILE");
    if (path == nullptr || *path == '\0' || g_sampler != nullptr) return;
    auto* s = new Sampler;
    s->out = std::fopen(path, "wb");
    if (s->out == nullptr) {
        TL_LOG_WARN("profiler: cannot write %s", path);
        delete s;
        return;
    }
    std::fwrite("TLPROF1\n", 1, 8, s->out);
    HMODULE module = GetModuleHandleA(nullptr);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const char*>(module) + dos->e_lfanew);
    s->image = reinterpret_cast<uintptr_t>(module);
    s->image_end = s->image + nt->OptionalHeader.SizeOfImage;
    // This thread's stack: its reservation's start, and the top the TIB records.
    MEMORY_BASIC_INFORMATION region{};
    VirtualQuery(&region, &region, sizeof region);
    s->stack_low = reinterpret_cast<uintptr_t>(region.AllocationBase);
    s->stack_high =
        reinterpret_cast<uintptr_t>(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s->target,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                    0);
    g_sampler = s;
    s->worker = std::thread(run, s);
    std::atexit(stop_profiler);
    TL_LOG_INFO("profiler: sampling the game's thread every %lu ms into %s",
                static_cast<unsigned long>(kIntervalMs), path);
}

void stop_profiler() {
    Sampler* s = g_sampler;
    if (s == nullptr) return;
    s->stop.store(true);
    if (s->worker.joinable()) s->worker.join();
    std::fclose(s->out);
    CloseHandle(s->target);
    g_sampler = nullptr;
    delete s;
}

void profiler_mark(const char* label) {
    Sampler* s = g_sampler;
    if (s == nullptr) return;
    const uint16_t length = static_cast<uint16_t>(std::strlen(label));
    const std::lock_guard<std::mutex> hold(s->file_lock);
    std::fwrite(&kMarker, sizeof kMarker, 1, s->out);
    std::fwrite(&length, sizeof length, 1, s->out);
    std::fwrite(label, 1, length, s->out);
}

}  // namespace tl::core

#else

namespace tl::core {

// Sampled on Windows only; a Linux desktop has perf.
void start_profiler() {}
void stop_profiler() {}
void profiler_mark(const char* label) { (void)label; }

}  // namespace tl::core

#endif
