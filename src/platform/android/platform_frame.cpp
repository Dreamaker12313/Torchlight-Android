// Android frame lifecycle. pump_events() drains the glue's looper once a frame; the window going
// away or the activity being destroyed is what should_keep_running() reports.
#include "core/platform.h"

#include "core/log.h"

#include "core/touch_pad.h"
#include "platform/android/android_glue.h"

#include <android_native_app_glue.h>
#include <malloc.h>
#include <unistd.h>

namespace tl::core {

// `/sdcard/Android/data/<package>/files/`: the activity's externalDataPath.
const std::string& platform_data_root() {
    static const std::string root = [] {
        const char* path = android::app()->activity->externalDataPath;
        return path != nullptr ? std::string(path) + "/" : std::string();
    }();
    return root;
}

// The launcher activity (android/java) checks the folder before the engine
// starts, so what reaches here is said in the log (and logcat).
void platform_error_message(const std::string& summary, const std::string& detail) {
    TL_LOG_ERROR("%s: %s", summary.c_str(), detail.c_str());
}

// The activity owns the window; OGRE's AndroidEGLWindow reads both as numbers.
void platform_window_params(std::map<std::string, std::string>& params) {
    android_app* glue = android::app();
    params["externalWindowHandle"] = std::to_string(reinterpret_cast<std::size_t>(glue->window));
    params["androidConfig"] = std::to_string(reinterpret_cast<std::size_t>(glue->config));
}

void pump_events() {
    android_app* glue = android::app();
    android_poll_source* source = nullptr;
    int events = 0;
    while (glue->destroyRequested == 0 &&
           ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
        if (source != nullptr) source->process(glue, source);
    }
    // The window's size is the space touches arrive in, and a foldable phone
    // changes it when it folds.
    if (glue->window != nullptr) {
        touch_pad().set_screen(static_cast<float>(ANativeWindow_getWidth(glue->window)),
                               static_cast<float>(ANativeWindow_getHeight(glue->window)));
    }
}

bool should_keep_running() {
    return android::app()->destroyRequested == 0 && !android::window_lost();
}

bool memory_usage(unsigned long long& used, unsigned long long& total) {
    // What the program has allocated, against the phone's memory. GL objects
    // live in the driver's own allocations and are not in this figure.
    const struct mallinfo info = mallinfo();
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || page_size <= 0) return false;
    used = static_cast<unsigned long long>(info.uordblks);
    total = static_cast<unsigned long long>(pages) * static_cast<unsigned long long>(page_size);
    return true;
}

// Android's clocks and cores are the system's; there is nothing to set and,
// without root, nothing reliable to read.
void set_loading_boost(bool on) { (void)on; }

bool core_busy(float busy[kCoreCount]) {
    (void)busy;
    return false;
}

bool clock_rates(unsigned& cpu_mhz, unsigned& gpu_mhz, unsigned& memory_mhz) {
    (void)cpu_mhz;
    (void)gpu_mhz;
    (void)memory_mhz;
    return false;
}

// The sampler and the GPU-wait counters are the console's.
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
