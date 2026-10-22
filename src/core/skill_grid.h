// The skills tab's grid for a class file laid out for the PC's three-column skill tree.
#pragma once

#include "core/skills.h"

#include <cstddef>
#include <vector>

namespace tl::core {

/// Re-packs a class's `[SKILL]` cells onto the console's two-column grid when the
/// file uses a third column; a file already on two columns is left as it is.
/// Row by row and pane by pane: a row's skills take columns 0 and 1 in their
/// `COLUMN` order; a row with three gives its leftmost to the next row, where
/// it goes first. A skill carried past the last row takes the lowest free cell
/// of its pane. Only `ROW` and `COLUMN` change: they place a skill on the tab
/// and nothing else.
///
/// Returns how many skills found no cell (their pane is full); those keep
/// row and column -1 and stay off the tab.
size_t pack_two_column_skill_grid(std::vector<ClassSkill>& skills, bool* repacked = nullptr);

}  // namespace tl::core
