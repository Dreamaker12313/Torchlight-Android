// The death screen, from the Xbox 360 release's `media/UI/resurrect_xenon.layout`. What the screen
// does is `CDieMenu` (`core/death.h`); this is how it looks on a pad.
#pragma once

#include "formats/cegui.h"
#include "formats/uilayout.h"

#include <OgrePrerequisites.h>

#include <OgreMaterial.h>
#include <OgreTexture.h>

#include <map>
#include <string>
#include <vector>

namespace Ogre {
class Overlay;
class OverlayContainer;
class PanelOverlayElement;
class TextAreaOverlayElement;
}  // namespace Ogre

namespace tl::render {

struct DeathScreenState {
    std::string title;
    std::string choices[3];
    /// A hardcore character: every choice is hidden (`CDieMenu::setOpen`) and the
    /// layout's `hardcoretext` is shown.
    bool hardcore = false;
    /// The highlighted choice, 0 to 2.
    int selected = 0;
};

class DeathScreen {
public:
    DeathScreen() = default;
    ~DeathScreen();

    DeathScreen(const DeathScreen&) = delete;
    DeathScreen& operator=(const DeathScreen&) = delete;

    /// False, with one logged line, when the console interface data is not mounted.
    bool create(const std::string& group, unsigned width, unsigned height);
    void destroy();
    bool ready() const { return built_; }

    void show();
    void hide();
    bool visible() const { return visible_; }

    void set_state(const DeathScreenState& state);
    const DeathScreenState& state() const { return state_; }

    void resize(unsigned width, unsigned height);

private:
    struct Node {
        float off_x = 0.0f, off_y = 0.0f;
        float wide = 0.0f, tall = 0.0f;
        float scale_wide = 0.0f, scale_tall = 0.0f;
        int h_anchor = 0, v_anchor = 0;
        int parent = -1;

        Ogre::PanelOverlayElement* panel = nullptr;
        Ogre::TextAreaOverlayElement* text = nullptr;
        Ogre::TextAreaOverlayElement* drop = nullptr;

        std::string normal;  ///< `NORMAL` / `IMAGE`
        std::string hover;   ///< `HOVER`
        std::string caption;
        float points = 0.0f;
        int h_text = 0;
        int v_text = 0;
        bool wrap = false;
        std::string widget;
        bool hidden = false;
        bool runtime_hidden = false;

        float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;
    };

    struct Sprite {
        const tl::formats::ImagesetImage* rect = nullptr;
        Ogre::TexturePtr texture;
        Ogre::MaterialPtr material;
    };

    bool load_art(const std::string& group);
    void build(const tl::formats::UiObject& object, int parent, size_t& serial,
               bool inherited_hidden);
    void add_item_text_frame(int owner, size_t& serial);
    void paint(Node& node, const std::string& image);
    void layout();
    bool is_hidden(int index) const;
    const Sprite* sprite(const std::string& name) const;
    int find_widget(const std::string& name) const;

    std::vector<tl::formats::Imageset> sets_;
    std::vector<Ogre::TexturePtr> textures_;
    std::vector<Ogre::MaterialPtr> materials_;
    std::map<std::string, Sprite> sprites_;
    std::vector<Node> nodes_;

    Ogre::Overlay* overlay_ = nullptr;
    Ogre::Overlay* text_overlay_ = nullptr;
    Ogre::OverlayContainer* panel_root_ = nullptr;
    Ogre::OverlayContainer* text_root_ = nullptr;

    DeathScreenState state_;
    unsigned width_ = 0;
    unsigned height_ = 0;
    float native_height_ = 0.0f;
    bool built_ = false;
    bool visible_ = false;
};

}  // namespace tl::render
