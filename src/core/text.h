// The game's inline colour markup: `|cAARRGGBB` opens a run, `|u` closes it.
#pragma once

#include <string>
#include <vector>

namespace tl::core {

/// Strip the colour runs from a display string, leaving the text. A `|` that opens neither
/// (a `|c` without eight hex digits included) is ordinary text.
std::string strip_markup(const std::string& text);

/// One stretch of a display string that is all one colour.
struct MarkupRun {
    std::string text;

    /// True inside a `|c ... |u` pair; otherwise the caller's colour applies.
    bool coloured = false;

    /// 0..1; `|c` spells alpha first, then red, green, blue.
    float a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
};

/// Split a display string into its colour runs, in order; an unclosed run runs to the end.
/// The runs' text concatenated is strip_markup()'s result.
std::vector<MarkupRun> markup_runs(const std::string& text);

}  // namespace tl::core
