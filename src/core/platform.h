// The per-frame half of the platform layer: the event queue, whether to keep running, and
// what each target can say about its memory, clocks and cores.
#pragma once

#include <chrono>
#include <map>
#include <string>

namespace tl::core {

/// Where this target keeps the game data, the log, the saves and the caches. Ends in a
/// separator, or is empty for the working directory: the console's `sdmc:/switch/torchlight/`,
/// Android's `/sdcard/Android/data/<package>/files/`.
const std::string& platform_data_root();

/// The desktop's: the folder the running program is in, ending in a separator, or empty
/// when it cannot be told. The devices keep what comes with the program elsewhere.
std::string platform_program_folder();

/// What `Ogre::Root::createRenderWindow` needs added to its parameters: nothing but on
/// Android (`externalWindowHandle`, `androidConfig`).
void platform_window_params(std::map<std::string, std::string>& params);

/// Tell the player something they have to fix before the game can run - a file
/// missing from where it belongs - in the platform's own way, and return once
/// they have read it. `summary` is one line; `detail` says what to put where.
/// The console shows its system error dialog; the desktop and Android log it
/// (Android's launcher has checked the folder before the engine starts).
void platform_error_message(const std::string& summary, const std::string& detail);

/// Service the platform's event queue. Call once per frame.
void pump_events();

/// False once the platform wants the application to stop. Desktop always
/// returns true; the console reports the applet's own lifecycle.
bool should_keep_running();

/// The thread that runs the frames and pumps their events: the only one `keep_responsive`
/// pumps on (Android's looper belongs to it). Call once, from that thread.
void mark_event_thread();

/// Each platform's `pump_events` says it has run, so `keep_responsive` leaves the frames'
/// own pumping alone.
void note_events_pumped();

/// For long synchronous work (a level loading): on the event thread, at most every 100 ms,
/// `pump_events` and `should_keep_running`, so the system does not take the game for hung
/// (Windows' "Not Responding", Android's ANR, the Switch's HOME button unanswered). Cheap to
/// call often; does nothing on another thread.
void keep_responsive();

/// The frame rate the game holds itself to (config.txt's `fps_limit`; 0 for none), told to the
/// display where it takes a request: Android's `ANativeWindow_setFrameRate`, which lets a 120 Hz
/// screen run at 60. Elsewhere nothing.
void platform_set_frame_rate(float fps);

/// Sleep until `deadline` as closely as the platform's timers allow (the frame limiter).
void platform_sleep_until(std::chrono::steady_clock::time_point deadline);

/// How much of the process's heap is in use, in bytes, and how much it has. False when the
/// platform cannot say (the desktop).
bool memory_usage(unsigned long long& used, unsigned long long& total);

/// Raise the CPU clock for a stretch of synchronous loading, or put it back
/// (`appletSetCpuBoostMode`; `Renderer::Impl::FrameClock`). It lowers the GPU clock, so never
/// on while frames are drawn. Idempotent; a no-op on the desktop and for an applet.
void set_loading_boost(bool on);

/// The cores a process may run on, for core_busy().
constexpr int kCoreCount = 3;

/// How busy each core was since the previous call, 0..1 per core, from the kernel's idle
/// counts. The first call only samples and returns false; the desktop always does.
bool core_busy(float busy[kCoreCount]);

/// The clocks the CPU, GPU and memory are running at right now, in MHz (an overclock shows).
/// False on the desktop, and when the clock service refuses the process.
bool clock_rates(unsigned& cpu_mhz, unsigned& gpu_mhz, unsigned& memory_mhz);

/// Sample where the calling thread spends its time, when the platform can and
/// `sdmc:/switch/torchlight/profile` exists; a no-op otherwise. Call once, from the game's
/// thread.
void start_profiler();
void stop_profiler();

/// Label the samples taken from now on: the level, a test phase.
void profiler_mark(const char* label);

/// Place the calling thread as the game's `index`-th worker (core/job_pool.h): on the console
/// the first goes to core 2 above the mixer, the rest to core 1 below Mesa's own thread. Call
/// from the worker itself. A no-op where the system schedules.
void prepare_worker_thread(unsigned index);

/// Where the GL driver waits on the GPU, by kind (`gpu_wait_name`), as time and calls since
/// the last call, for the game's thread and for every other. False on the desktop.
/// start_gpu_waits() names the game's thread; call it from it.
constexpr int kGpuWaitKinds = 9;
struct GpuWaitTimes {
    double ms[kGpuWaitKinds] = {};
    unsigned calls[kGpuWaitKinds] = {};
};
void start_gpu_waits();
bool gpu_wait_times(GpuWaitTimes& game, GpuWaitTimes& others);
const char* gpu_wait_name(int kind);

}  // namespace tl::core
