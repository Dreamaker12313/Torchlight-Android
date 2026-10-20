#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace tl::core {
namespace {

const char* level_tag(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Off: return "OFF  ";
    }
    return "?????";
}

/// Trim to the basename so log lines stay readable; __FILE__ is an absolute
/// path in this build.
const char* short_file(const char* path) {
    const char* out = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') out = p + 1;
    }
    return out;
}

bool g_open = false;
LogLevel g_min_level = LogLevel::Debug;

}  // namespace

void log_set_min_level(LogLevel level) { g_min_level = level; }

void log_init(const std::string& path_hint) {
    detail::sink_open(path_hint);
    g_open = true;
}

void log_shutdown() {
    if (!g_open) return;
    detail::sink_close();
    g_open = false;
}

void log_write(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < g_min_level) return;
    char prefix[256];
    std::snprintf(prefix, sizeof(prefix), "[%s] %s:%d: ", level_tag(level), short_file(file), line);
    detail::sink_write(level, std::string(prefix) + message);
}

void log_writef(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_min_level) return;  // before the formatting, not after it
    va_list ap;
    va_start(ap, fmt);
    va_list ap_probe;
    va_copy(ap_probe, ap);
    const int needed = std::vsnprintf(nullptr, 0, fmt, ap_probe);
    va_end(ap_probe);

    std::string message;
    if (needed > 0) {
        std::vector<char> buf(static_cast<size_t>(needed) + 1);
        std::vsnprintf(buf.data(), buf.size(), fmt, ap);
        message.assign(buf.data(), static_cast<size_t>(needed));
    }
    va_end(ap);

    log_write(level, file, line, message);
}

}  // namespace tl::core
