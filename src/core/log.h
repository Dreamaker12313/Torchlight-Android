// Logging abstraction. Log statements must never be platform-conditional: call TL_LOG* from
// anywhere; the platform sink decides where the text goes (desktop: stdout; Switch: a file on
// the SD card, flushed after every line so it survives a crash).
#pragma once

#include <string>

namespace tl::core {

/// `Off` is only a minimum level (log_set_min_level): nothing is logged at it.
enum class LogLevel { Debug, Info, Warning, Error, Off };

/// Open the platform sink. Called once at startup, before anything else logs.
/// `path_hint` is used only by sinks that write to a file; desktop ignores it.
void log_init(const std::string& path_hint);

/// Flush and close the sink. Safe to call without log_init().
void log_shutdown();

void log_write(LogLevel level, const char* file, int line, const std::string& message);

/// Lines below `level` are dropped (default: none are). For tools that run the
/// engine code in bulk - the fuzzer - rather than for the game.
void log_set_min_level(LogLevel level);

/// printf-style. Kept separate so callers can use whichever reads better.
// MinGW's default format checker uses MSVC semantics, under which %zu is not
// recognised. gnu_printf selects the semantics the implementation actually has.
#if defined(__MINGW32__)
#define TL_PRINTF_FMT(f, a) __attribute__((format(gnu_printf, f, a)))
#elif defined(__GNUC__)
#define TL_PRINTF_FMT(f, a) __attribute__((format(printf, f, a)))
#else
#define TL_PRINTF_FMT(f, a)
#endif

void log_writef(LogLevel level, const char* file, int line, const char* fmt, ...)
    TL_PRINTF_FMT(4, 5);

// --- platform sink, implemented once per platform in src/platform/<p>/ -------
namespace detail {
void sink_open(const std::string& path_hint);
void sink_write(LogLevel level, const std::string& line);
void sink_close();
}  // namespace detail

}  // namespace tl::core

#define TL_LOG(level, ...) ::tl::core::log_writef((level), __FILE__, __LINE__, __VA_ARGS__)

#define TL_LOG_DEBUG(...) TL_LOG(::tl::core::LogLevel::Debug, __VA_ARGS__)
#define TL_LOG_INFO(...) TL_LOG(::tl::core::LogLevel::Info, __VA_ARGS__)
#define TL_LOG_WARN(...) TL_LOG(::tl::core::LogLevel::Warning, __VA_ARGS__)
#define TL_LOG_ERROR(...) TL_LOG(::tl::core::LogLevel::Error, __VA_ARGS__)
