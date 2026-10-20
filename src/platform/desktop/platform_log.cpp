// Desktop log sink: stdout.
#include "core/log.h"

#include <cstdio>

namespace tl::core::detail {

void sink_open(const std::string&) {
    // Nothing to open. path_hint is ignored on desktop.
}

void sink_write(LogLevel level, const std::string& line) {
    std::FILE* out = (level == LogLevel::Error) ? stderr : stdout;
    std::fprintf(out, "%s\n", line.c_str());
    // Flushed line by line, as the console's sink is, so a log survives a hard crash
    // (a redirected stdout is block-buffered).
    std::fflush(out);
}

void sink_close() {
    std::fflush(stdout);
    std::fflush(stderr);
}

}  // namespace tl::core::detail
