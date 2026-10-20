// Call counts for the parity report. Built only with -DTL_CALL_COUNTS=ON (which adds
// -finstrument-functions): counts `__cyg_profile_func_enter` per function and writes them at exit
// to `call_counts.txt` (or `$TL_CALL_COUNTS_FILE`) as runtime addresses, with the module's base.
#if defined(TL_CALL_COUNTS) && defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

/// An open-addressed table of (function, count), lock-free because the audio
/// thread runs instrumented code too. 2^17 slots against a few thousand
/// functions keeps probes short.
constexpr std::size_t kSlots = std::size_t{1} << 17;
struct Slot {
    std::atomic<std::uintptr_t> function{0};
    std::atomic<std::uint64_t> calls{0};
};
Slot g_slots[kSlots];
std::atomic<bool> g_registered{false};

__attribute__((no_instrument_function)) void write_counts() {
    const char* path = std::getenv("TL_CALL_COUNTS_FILE");
    std::FILE* out = std::fopen(path != nullptr ? path : "call_counts.txt", "w");
    if (out == nullptr) return;
    std::fprintf(out, "base %llx\n",
                 static_cast<unsigned long long>(
                     reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr))));
    for (const Slot& slot : g_slots) {
        const std::uintptr_t function = slot.function.load(std::memory_order_relaxed);
        if (function == 0) continue;
        std::fprintf(out, "%llx %llu\n", static_cast<unsigned long long>(function),
                     static_cast<unsigned long long>(slot.calls.load(std::memory_order_relaxed)));
    }
    std::fclose(out);
}

}  // namespace

extern "C" {

__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void* function, void*) {
    if (!g_registered.exchange(true, std::memory_order_relaxed)) std::atexit(write_counts);
    const auto key = reinterpret_cast<std::uintptr_t>(function);
    std::size_t at = static_cast<std::size_t>((key >> 4) * 2654435761u) & (kSlots - 1);
    for (std::size_t probe = 0; probe < kSlots; ++probe) {
        Slot& slot = g_slots[at];
        std::uintptr_t held = slot.function.load(std::memory_order_relaxed);
        if (held == 0) {
            std::uintptr_t empty = 0;
            if (slot.function.compare_exchange_strong(empty, key, std::memory_order_relaxed)) {
                held = key;
            } else {
                held = empty;
            }
        }
        if (held == key) {
            slot.calls.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        at = (at + 1) & (kSlots - 1);
    }
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*, void*) {}

}  // extern "C"

#endif  // TL_CALL_COUNTS && _WIN32
