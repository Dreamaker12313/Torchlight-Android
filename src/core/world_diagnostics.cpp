// The gameplay half of render/renderer_diagnostics.cpp (core/world.h).
#include "core/platform.h"
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

// PORT: diagnostics, a stutter the five-second averages would hide: a frame
// over 50 ms, split as run() times it.
void World::log_long_frame(unsigned long long drive_us, unsigned long long advance_us,
                           unsigned long long render_us, unsigned long long total_us) {
    if (total_us <= 50000) return;
    TL_LOG_INFO("long frame: %.1f ms (drive %.1f, advance %.1f, render %.1f)",
                static_cast<double>(total_us) * 0.001, static_cast<double>(drive_us) * 0.001,
                static_cast<double>(advance_us) * 0.001, static_cast<double>(render_us) * 0.001);
}

}  // namespace tl::core
