// The event queue kept moving through synchronous loading (core/platform.h).
#include "core/log.h"
#include "core/platform.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace tl::core {

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<std::thread::id> g_event_thread{};
Clock::time_point g_last_pump{};
bool g_pumping = false;

}  // namespace

void mark_event_thread() {
    g_event_thread.store(std::this_thread::get_id());
    g_last_pump = Clock::now();
}

void keep_responsive() {
    if (std::this_thread::get_id() != g_event_thread.load(std::memory_order_relaxed)) return;
    // A pump that reaches back here (an event's handler loading something) waits its turn.
    if (g_pumping) return;
    const Clock::time_point now = Clock::now();
    const auto since = now - g_last_pump;
    if (since < std::chrono::milliseconds(100)) return;
    // A stretch the system may have noticed: a player's log says where.
    if (since > std::chrono::seconds(2)) {
        TL_LOG_INFO("events: %lld ms without a pump (synchronous loading)",
                    static_cast<long long>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(since).count()));
    }
    g_pumping = true;
    pump_events();
    // The console's applet messages are read here (`appletMainLoop`); its answer is kept by
    // should_keep_running for the frame loop to act on.
    (void)should_keep_running();
    g_pumping = false;
}

void note_events_pumped() {
    if (std::this_thread::get_id() == g_event_thread.load(std::memory_order_relaxed)) {
        g_last_pump = Clock::now();
    }
}

}  // namespace tl::core
