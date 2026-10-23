// Overlay setters that leave an element alone when nothing changes. OGRE's own mark the element
// stale whatever they are handed, rebuilding its vertices and buffer (every glyph of a caption);
// the HUD and floor labels are applied every frame. Each compares against the getter that reads
// back what its setter stores, in the element's own metrics mode.
#pragma once

#include <OgreColourValue.h>
#include <OgreOverlayElement.h>
#include <OgrePanelOverlayElement.h>
#include <OgreTextAreaOverlayElement.h>

namespace tl::render {

float ui_text_scale();

inline void set_caption(Ogre::OverlayElement* element, const Ogre::DisplayString& text) {
    if (element->getCaption() != text) element->setCaption(text);
}

inline void set_position(Ogre::OverlayElement* element, Ogre::Real left, Ogre::Real top) {
    if (element->getLeft() != left || element->getTop() != top) element->setPosition(left, top);
}

inline void set_dimensions(Ogre::OverlayElement* element, Ogre::Real width, Ogre::Real height) {
    if (element->getWidth() != width || element->getHeight() != height) {
        element->setDimensions(width, height);
    }
}

inline void set_uv(Ogre::PanelOverlayElement* panel, Ogre::Real u1, Ogre::Real v1, Ogre::Real u2,
                   Ogre::Real v2) {
    Ogre::Real a = 0, b = 0, c = 0, d = 0;
    panel->getUV(a, b, c, d);
    if (a != u1 || b != v1 || c != u2 || d != v2) panel->setUV(u1, v1, u2, v2);
}

inline void set_colour(Ogre::TextAreaOverlayElement* text, const Ogre::ColourValue& colour) {
    if (text->getColourTop() != colour || text->getColourBottom() != colour) {
        text->setColour(colour);
    }
}

/// Only a relative element's height reads back as it was set: in pixel modes
/// the setter truncates to whole pixels and the getter answers in another unit.
inline void set_char_height(Ogre::TextAreaOverlayElement* text, Ogre::Real height) {
    if (text == nullptr) return;
    if (text->getMetricsMode() != Ogre::GMM_RELATIVE || text->getCharHeight() != height) {
        text->setCharHeight(height);
    }
}

}  // namespace tl::render
