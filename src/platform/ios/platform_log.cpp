#include <cstdio>
#include <mutex>
#include "core/log.h"

namespace tl::core::detail {
namespace {
std::mutex g_mutex;
std::FILE* g_file = nullptr;
}  // namespace
void sink_open(const std::string& path_hint) {
    const std::lock_guard<std::mutex> guard(g_mutex);
    g_file = std::fopen(path_hint.c_str(), "wb");
}
void sink_write(LogLevel level, const std::string& line) {
    (void)level;
    const std::lock_guard<std::mutex> guard(g_mutex);
    if (g_file == nullptr) return;
    std::fputs(line.c_str(), g_file);
    std::fputc('\n', g_file);
    std::fflush(g_file);
}
void sink_close() {
    const std::lock_guard<std::mutex> guard(g_mutex);
    if (g_file != nullptr) std::fclose(g_file);
    g_file = nullptr;
}
}  // namespace tl::core::detail
