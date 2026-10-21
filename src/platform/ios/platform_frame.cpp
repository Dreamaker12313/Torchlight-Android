#include "core/platform.h"
#include "core/log.h"
#include "platform/ios/ios_app.h"
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <thread>

namespace tl::core {
const std::string& platform_data_root() {
    static const std::string root = [] {
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Documents/" : std::string();
    }();
    return root;
}
void platform_window_params(std::map<std::string, std::string>& params) {
    params["externalViewHandle"] = std::to_string(reinterpret_cast<std::uintptr_t>(tl_ios_view_handle()));
    params["externalViewControllerHandle"] = std::to_string(reinterpret_cast<std::uintptr_t>(tl_ios_controller_handle()));
}
void platform_error_message(const std::string& summary, const std::string& detail) {
    TL_LOG_ERROR("%s: %s", summary.c_str(), detail.c_str());
}
void pump_events() {
    while (!tl_ios_app_active()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
bool should_keep_running() { return true; }
bool memory_usage(unsigned long long& used, unsigned long long& total) { (void)used; (void)total; return false; }
void set_loading_boost(bool on) { (void)on; }
bool core_busy(float busy[kCoreCount]) { (void)busy; return false; }
bool clock_rates(unsigned& cpu_mhz, unsigned& gpu_mhz, unsigned& memory_mhz) { (void)cpu_mhz; (void)gpu_mhz; (void)memory_mhz; return false; }
void start_profiler() {}
void stop_profiler() {}
void profiler_mark(const char* label) { (void)label; }
void start_gpu_waits() {}
bool gpu_wait_times(GpuWaitTimes& game, GpuWaitTimes& others) { (void)game; (void)others; return false; }
const char* gpu_wait_name(int kind) { (void)kind; return ""; }
}  // namespace tl::core
