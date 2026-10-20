// Laying authored text out in an OGRE overlay, measured the way OGRE lays it out (the
// dialogue box and the loading screen's tip).
#pragma once

#include "core/text.h"

#include <OgreFont.h>

#include <string>
#include <vector>

namespace tl::render {

/// Split `text` into lines that fit `width`, breaking at spaces.
///
/// All in the viewport-relative units of a GMM_RELATIVE element: `width` and `char_height`
/// are viewport fractions, `across`/`down` its pixel size. One glyph is
/// `advance * char_height * down / across` (OgreTextAreaOverlayElement uses the advance,
/// not the glyph's aspect ratio). Both a newline and a literal backslash-n (as the data
/// stores it) break a line. A null or unmeasurable font returns the text as one line.
std::vector<std::string> wrap_text(const Ogre::FontPtr& font, const std::string& text, float width,
                                   float char_height, float across, float down);

/// How wide `text` comes out, in the same units and per-glyph advance as wrap_text(). A
/// null font measures zero.
float measure_text(const Ogre::FontPtr& font, const std::string& text, float char_height,
                   float across, float down);

/// One stretch of a wrapped line that is all one colour, and where it sits.
struct TextSpan {
    std::string text;
    float x = 0.0f;  ///< from the left of the line, in viewport-relative units
    bool coloured = false;
    float a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
};

/// Wrap a run-coloured string (`|cAARRGGBB ... |u`) into lines of positioned spans: an
/// overlay text element has one colour, so each run is its own element. Wraps as
/// wrap_text() does; a run boundary is not a break.
std::vector<std::vector<TextSpan>> wrap_runs(const Ogre::FontPtr& font,
                                             const std::vector<tl::core::MarkupRun>& runs,
                                             float width, float char_height, float across,
                                             float down);

}  // namespace tl::render
