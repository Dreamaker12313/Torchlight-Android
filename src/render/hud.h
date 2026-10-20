// The interface layer: the letterbox bars and the line a character is speaking, drawn with the
// game's own font (`media/UI/Torchlight Regular.ttf`) through OGRE's Overlay and FreeType. Given a
// scene manager and a viewport size it draws; it owns nothing of the level.
#pragma once

#include <OgrePrerequisites.h>

#include <OgreMaterial.h>
#include <OgreTexture.h>

#include "core/interactive_menu.h"
#include "formats/cegui.h"

#include <array>
#include <string>
#include <vector>

namespace Ogre {
class Overlay;
class OverlayContainer;
class OverlayElement;
class OverlaySystem;
class SceneManager;
class TextAreaOverlayElement;
}  // namespace Ogre

namespace tl::render {

/// One floating text item on the screen.
struct FloatyItem {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float alpha = 1.0f;
    Ogre::ColourValue colour = Ogre::ColourValue::White;
    float char_height = 0.030f;
};

/// The letterbox bars and the dialogue box.
class Hud {
public:
    ~Hud();

    /// Build the overlay against a scene manager. False when the font or the overlay could not be
    /// made, which is not fatal. `portraits` is the archive path of the speakers' portrait
    /// imageset (`media/UI/itemicons/itemicons.imageset`); without it the box draws text alone.
    bool create(Ogre::SceneManager* scene, unsigned width, unsigned height,
                const std::string& portraits);

    void destroy();
    bool ready() const { return overlay_ != nullptr; }

    /// The bars a `Game State Controller` asks for: `CInteractiveMenu::
    /// setVisible` (`tl::core::InteractiveBars`). They slide; `letterboxed()`
    /// is whether they were asked for (`+0x1c`), not whether they are drawn.
    void set_letterboxed(bool on);
    bool letterboxed() const { return bars_.open; }
    /// `CInteractiveMenu::update`, once a frame.
    void update_bars(float delta);

    /// What is being said, by whom, and which portrait goes beside it. An empty line hides the
    /// box; an empty or unknown `portrait` draws the box without one.
    void set_line(const std::string& speaker, const std::string& text,
                  const std::string& portrait = std::string());

    /// Set all currently visible floating text items.
    void set_floaties(const std::vector<FloatyItem>& items);

    /// A line floating over its speaker's head (`FLOATYTEXT`). `x`, `y` are where the head
    /// projects to, 0..1 across the viewport; `alpha` 1 at the start of the fade, 0 at the end.
    /// Empty text or an off-screen position hides it. Independent of set_line(): the box is
    /// `CQuestDialogMenu`'s window, the floaty line a `CTextEvent`.
    void set_floaty(const std::string& text, float x, float y, float alpha);

    /// Re-lay-out for a new viewport size.
    void resize(unsigned width, unsigned height);

    /// The performance line in the top-left corner (frame rate and
    /// resolution). The port's own: the original has no such overlay. Drawn
    /// above every other overlay, so no menu covers it.
    void set_stats(const std::string& text);
    void set_stats_visible(bool on);
    bool stats_visible() const { return stats_visible_; }

private:
    void layout();
    void layout_floaties();

    /// Read the imageset that holds the speakers' portraits and build the
    /// material that draws from it. False when there is nothing to draw with,
    /// which is not an error: the box works without one.
    bool load_portraits(const std::string& path);

    /// Split `text` into lines that fit `width` pixels, at spaces and authored `\n`. `height` is
    /// the character height it will be drawn at: the measure is a per-glyph advance times it.
    std::vector<std::string> wrap(const std::string& text, float width, float height) const;

    Ogre::SceneManager* scene_ = nullptr;
    Ogre::OverlaySystem* system_ = nullptr;
    Ogre::Overlay* overlay_ = nullptr;
    Ogre::OverlayContainer* top_bar_ = nullptr;
    Ogre::OverlayContainer* bottom_bar_ = nullptr;
    Ogre::OverlayContainer* box_ = nullptr;
    Ogre::TextAreaOverlayElement* speaker_ = nullptr;
    Ogre::TextAreaOverlayElement* line_ = nullptr;
    /// The speakers' portraits: the imageset that names the rectangles, the
    /// texture they are cut from, and the panel that draws one.
    tl::formats::Imageset portrait_set_;
    Ogre::TexturePtr portrait_texture_;
    Ogre::MaterialPtr portrait_material_;
    Ogre::OverlayContainer* portrait_ = nullptr;
    std::string portrait_name_;

    static constexpr size_t kMaxFloaties = 32;
    Ogre::Overlay* floaty_overlay_ = nullptr;
    Ogre::OverlayContainer* floaty_box_ = nullptr;
    std::array<Ogre::TextAreaOverlayElement*, kMaxFloaties> floaty_pool_{};
    std::vector<FloatyItem> floaties_;

    Ogre::Overlay* stats_overlay_ = nullptr;
    Ogre::OverlayContainer* stats_box_ = nullptr;
    Ogre::TextAreaOverlayElement* stats_text_ = nullptr;
    Ogre::TextAreaOverlayElement* stats_drop_ = nullptr;  ///< its shadow
    bool stats_visible_ = false;

    unsigned width_ = 0;
    unsigned height_ = 0;
    tl::core::InteractiveBars bars_;

    std::string speaker_text_;
    std::string line_text_;
};

}  // namespace tl::render
