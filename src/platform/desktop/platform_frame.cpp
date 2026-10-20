// Desktop event pump. OGRE creates the window and its WindowProc but nothing dispatches to it;
// without this the window stops responding.
#include "core/platform.h"

#include "core/log.h"

#include <cstdio>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX  // libstdc++'s <string> defines it on MinGW
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tl::core {

// The working directory: `--game-data game_data`, `torchlight.log`, `save/`.
const std::string& platform_data_root() {
    static const std::string root;
    return root;
}

// OGRE makes the window itself.
void platform_window_params(std::map<std::string, std::string>& params) { (void)params; }

// In the log and on stderr, and on Windows in a message box: a player who
// double-clicked the program has no terminal to read.
void platform_error_message(const std::string& summary, const std::string& detail) {
    TL_LOG_ERROR("%s", summary.c_str());
    std::fprintf(stderr, "%s\n\n%s\n", summary.c_str(), detail.c_str());
#if defined(_WIN32)
    const std::string text = summary + "\n\n" + detail;
    MessageBoxA(nullptr, text.c_str(), "Torchlight Rekindled", MB_OK | MB_ICONERROR);
#endif
}

void pump_events() {
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#endif
    // X11 and Cocoa are pumped inside OGRE's own GLX/Cocoa window code, so
    // there is nothing to do on those platforms.
}

bool should_keep_running() {
    // Desktop has no external lifecycle; the window closing ends the loop.
    return true;
}

bool memory_usage(unsigned long long& used, unsigned long long& total) {
    // A desktop process has no meaningful ceiling; no number rather than a meaningless one.
    (void)used;
    (void)total;
    return false;
}

void set_loading_boost(bool on) {
    // The desktop's clocks are the operating system's business.
    (void)on;
}

bool core_busy(float busy[kCoreCount]) {
    // Scheduled by the operating system across however many cores it has;
    // nothing here decides which, so there is nothing to report.
    (void)busy;
    return false;
}

bool clock_rates(unsigned& cpu_mhz, unsigned& gpu_mhz, unsigned& memory_mhz) {
    (void)cpu_mhz;
    (void)gpu_mhz;
    (void)memory_mhz;
    return false;
}

// The desktop has real profilers; the sampler is the console's.
void start_profiler() {}
void stop_profiler() {}
void profiler_mark(const char* label) { (void)label; }

void start_gpu_waits() {}
bool gpu_wait_times(GpuWaitTimes& game, GpuWaitTimes& others) {
    (void)game;
    (void)others;
    return false;
}
const char* gpu_wait_name(int kind) {
    (void)kind;
    return "";
}

}  // namespace tl::core
