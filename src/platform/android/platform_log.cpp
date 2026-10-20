// Android log sink: logcat (tag "torchlight") while the phone is attached, and
// a file in the data root (`torchlight.log`) for a run that was not watched.
// Both are written line by line, the file flushed each time, so a crash keeps
// everything up to its last line - the same rule as the other targets' sinks.
#include "core/log.h"

#include <android/log.h>

#include <cstdio>
#include <mutex>
#include <string>

namespace tl::core::detail {

namespace {

std::mutex g_mutex;
std::FILE* g_file = nullptr;

}  // namespace

void sink_open(const std::string& path_hint) {
    const std::lock_guard<std::mutex> lock(g_mutex);
    g_file = std::fopen(path_hint.c_str(), "wb");
    if (g_file == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, "torchlight", "could not open %s; logcat only",
                            path_hint.c_str());
    }
}

void sink_write(LogLevel level, const std::string& line) {
    const int priority = level == LogLevel::Error     ? ANDROID_LOG_ERROR
                         : level == LogLevel::Warning ? ANDROID_LOG_WARN
                         : level == LogLevel::Debug   ? ANDROID_LOG_DEBUG
                                                      : ANDROID_LOG_INFO;
    __android_log_write(priority, "torchlight", line.c_str());
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file == nullptr) return;
    std::fputs(line.c_str(), g_file);
    std::fputc('\n', g_file);
    std::fflush(g_file);
}

void sink_close() {
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != nullptr) std::fclose(g_file);
    g_file = nullptr;
}

}  // namespace tl::core::detail
