// Small helpers of a level's loading: the layout's Player Start, a bind-pose animation name, the
// passability's log lines and dump, the memory line. The port's own.
#pragma once

#include "core/passability.h"
#include "formats/cmp.h"

#include <string>

namespace tl::core {

/// Where the character stands when a level is entered: the layout's Player Start, by name or by marker type.
const tl::formats::CmpNode* find_spawn(const tl::formats::CmpScene& layout);
/// An empty animation or `bind` (any case): the mesh's bind pose, no clip.
bool names_the_bind_pose(const std::string& animation);
/// The grid as a gdb dump of the original is written: a header, then one line per x column.
void write_passability_dump(const tl::core::PassabilityMap& grid, const std::string& path);
/// The passability lines `CLevel::calculatePassability` and `CLevel::populate` print.
void log_passability(const tl::core::PassabilityMap& grid, double seconds);
/// The process's memory, logged with `when`.
void log_memory(const char* when);

}  // namespace tl::core
