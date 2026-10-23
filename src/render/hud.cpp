#include "render/hud.h"

#include "core/log.h"
#include "render/overlay_set.h"
#include "render/text_layout.h"
#include "render/text_scale.h"

#include <OgreDataStream.h>
#include <OgreFont.h>
#include <OgreFontManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreOverlay.h>
#include <OgreOverlayContainer.h>
#include <OgreOverlayElement.h>
#include <OgreOverlayManager.h>
#include <OgreOverlaySystem.h>
#include <OgrePanelOverlayElement.h>
#include <OgrePixelFormat.h>
#include <OgreResourceGroupManager.h>
#include <OgreSceneManager.h>
#include <OgreTechnique.h>
#include <OgreTextAreaOverlayElement.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>
#include <OgreTextureUnitState.h>

#include <algorithm>
#include <cmath>

namespace tl::render {
namespace {

/// The resource group the game's archive is mounted in. Kept in step with
/// renderer.cpp's own `kGameGroup`; the font lives in `Pak.zip`.
constexpr const char* kGameGroup = "Torchlight";

/// The game's own typeface, out of its own archive.
constexpr const char* kFontFile = "Torchlight Regular.ttf";
constexpr const char* kFontName = "tl/font/torchlight";

/// How tall the text is drawn, as a fraction of the viewport height (about 22 pixels at 720p).
constexpr float kLineHeight = 0.030f;

/// How tall a floaty line is drawn. The 1.25 is the original's: `FLOATYTEXT` gets a text-event
/// scale of 1.25 where a damage number gets 1.0 (`CQuestDialogMenu::setNPC` against
/// `CCharacter::rollAttack`, both via `CGameUI::getTextEventObject`); the base is the box's.
constexpr float kFloatyHeight = kLineHeight * 1.25f;

/// How wide a floaty line may run before it wraps, as a fraction of the viewport (the original's
/// text-event window is built in code).
constexpr float kFloatyWrap = 0.45f;

/// The performance line: its height and its top-left corner, as fractions of
/// the viewport. The port's own overlay, not the original's.
constexpr float kStatsHeight = 0.026f;
constexpr float kStatsX = 0.006f;
constexpr float kStatsY = 0.006f;

/// One white pixel, to colour a panel with: a `Panel` ignores `setColour()` and a pass with no
/// texture unit draws black under GL3+, so the colour is a manual texture-stage argument.
Ogre::TexturePtr white_pixel() {
    auto& textures = Ogre::TextureManager::getSingleton();
    const char* kName = "tl/hud/white";
    Ogre::TexturePtr existing = textures.getByName(kName, Ogre::RGN_INTERNAL);
    if (existing) return existing;

    Ogre::TexturePtr texture = textures.createManual(kName, Ogre::RGN_INTERNAL, Ogre::TEX_TYPE_2D,
                                                     1, 1, 0, Ogre::PF_BYTE_RGBA);
    const Ogre::HardwarePixelBufferSharedPtr buffer = texture->getBuffer();
    buffer->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    auto* pixel = static_cast<uint8_t*>(buffer->getCurrentLock().data);
    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
    buffer->unlock();
    return texture;
}

/// A flat colour for a panel: one material per colour, because the colour lives
/// in the material and not in the element.
Ogre::MaterialPtr solid_material(const std::string& name, const Ogre::ColourValue& colour) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr existing = materials.getByName(name, Ogre::RGN_INTERNAL);
    if (existing) return existing;

    Ogre::MaterialPtr material = materials.create(name, Ogre::RGN_INTERNAL);
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setDepthCheckEnabled(false);
    pass->setDepthWriteEnabled(false);
    pass->setCullingMode(Ogre::CULL_NONE);
    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);

    Ogre::TextureUnitState* unit = pass->createTextureUnitState();
    unit->setTexture(white_pixel());
    unit->setColourOperationEx(Ogre::LBX_SOURCE1, Ogre::LBS_MANUAL, Ogre::LBS_CURRENT, colour);
    unit->setAlphaOperation(Ogre::LBX_SOURCE1, Ogre::LBS_MANUAL, Ogre::LBS_CURRENT, colour.a);
    return material;
}

/// A panel material that samples a texture as it is, rather than a flat colour.
Ogre::MaterialPtr textured_material(const std::string& name, const Ogre::TexturePtr& texture,
                                    Ogre::TextureFilterOptions filtering) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr existing = materials.getByName(name, Ogre::RGN_INTERNAL);
    if (existing) materials.remove(existing);

    Ogre::MaterialPtr material = materials.create(name, Ogre::RGN_INTERNAL);
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setDepthCheckEnabled(false);
    pass->setDepthWriteEnabled(false);
    pass->setCullingMode(Ogre::CULL_NONE);
    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);

    Ogre::TextureUnitState* unit = pass->createTextureUnitState();
    unit->setTexture(texture);
    // The texture as it is, not modulated against "current": under RTSS an overlay panel arrives
    // with an alpha this pass never set, which drew every pixel transparent.
    unit->setColourOperationEx(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    unit->setAlphaOperation(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    // A portrait is 128 pixels drawn at about 80 and wants smoothing; the
    // caller says which.
    unit->setTextureFiltering(filtering);
    return material;
}

}  // namespace

Hud::~Hud() { destroy(); }

bool Hud::load_portraits(const std::string& path) {
    if (path.empty()) return false;

    std::vector<uint8_t> bytes;
    try {
        Ogre::DataStreamPtr stream =
            Ogre::ResourceGroupManager::getSingleton().openResource(path, kGameGroup);
        if (!stream) return false;
        bytes.resize(static_cast<size_t>(stream->size()));
        if (!bytes.empty()) stream->read(bytes.data(), bytes.size());
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("interface: could not read '%s': %s", path.c_str(), e.getDescription().c_str());
        return false;
    }

    portrait_set_ = tl::formats::parse_imageset(bytes.data(), bytes.size());
    if (!portrait_set_.ok) {
        TL_LOG_WARN("interface: %s: %s", path.c_str(), portrait_set_.error.c_str());
        return false;
    }
    try {
        portrait_texture_ =
            Ogre::TextureManager::getSingleton().load(portrait_set_.image_file, kGameGroup);
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("interface: could not load '%s': %s", portrait_set_.image_file.c_str(),
                    e.getDescription().c_str());
        return false;
    }
    if (!portrait_texture_) return false;

    portrait_material_ =
        textured_material("tl/hud/portrait/mat", portrait_texture_, Ogre::TFO_BILINEAR);
    size_t heads = 0;
    for (const tl::formats::ImagesetImage& image : portrait_set_.images) {
        if (image.name.rfind("portrait_", 0) == 0) ++heads;
    }
    TL_LOG_INFO("interface: %zu portrait(s) from %s", heads, portrait_set_.image_file.c_str());
    return true;
}

bool Hud::create(Ogre::SceneManager* scene, unsigned width, unsigned height,
                 const std::string& portraits) {
    destroy();
    if (scene == nullptr) return false;
    scene_ = scene;
    width_ = width;
    height_ = height;

    // The OverlaySystem first: its constructor creates the `FontManager` and the element
    // factories.
    try {
        system_ = new Ogre::OverlaySystem();
        scene_->addRenderQueueListener(system_);
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("no interface: %s", e.getDescription().c_str());
        destroy();
        return false;
    }

    // The font, out of the game's own archive. FreeType is built for both
    // targets, so this is one code path rather than a desktop convenience.
    auto& fonts = Ogre::FontManager::getSingleton();
    Ogre::FontPtr font = fonts.getByName(kFontName, Ogre::RGN_INTERNAL);
    if (!font) {
        try {
            font = fonts.create(kFontName, Ogre::RGN_INTERNAL);
            font->setType(Ogre::FT_TRUETYPE);
            font->setSource(kFontFile);
            font->setTrueTypeSize(32);
            font->setTrueTypeResolution(96);
            font->addCodePointRange(Ogre::Font::CodePointRange(32, 255));
            font->load();
        } catch (const Ogre::Exception& e) {
            TL_LOG_WARN("no interface: could not build a font from '%s': %s", kFontFile,
                        e.getDescription().c_str());
            font.reset();
        }
    }
    if (!font) {
        destroy();
        return false;
    }

    try {
        auto& overlays = Ogre::OverlayManager::getSingleton();
        overlay_ = overlays.create("tl/hud");
        // Above anything else an overlay might draw.
        overlay_->setZOrder(500);

        const Ogre::ColourValue black(0.0f, 0.0f, 0.0f, 1.0f);
        const Ogre::ColourValue box(0.0f, 0.0f, 0.0f, 0.65f);
        auto panel = [&](const char* name, const Ogre::ColourValue& colour) {
            auto* element =
                static_cast<Ogre::OverlayContainer*>(overlays.createOverlayElement("Panel", name));
            element->setMetricsMode(Ogre::GMM_RELATIVE);
            element->setMaterial(solid_material(std::string(name) + "/mat", colour));
            overlay_->add2D(element);
            return element;
        };
        top_bar_ = panel("tl/hud/top", black);
        bottom_bar_ = panel("tl/hud/bottom", black);
        box_ = panel("tl/hud/box", box);

        // The speaker's portrait, cut out of the imageset the unit's `PORTRAIT`
        // names. A child of the box, so it moves and hides with it.
        if (load_portraits(portraits)) {
            portrait_ = static_cast<Ogre::OverlayContainer*>(
                overlays.createOverlayElement("Panel", "tl/hud/portrait"));
            portrait_->setMetricsMode(Ogre::GMM_RELATIVE);
            portrait_->setMaterial(portrait_material_);
            box_->addChild(portrait_);
            portrait_->hide();
        }
        auto text = [&](const char* name, float height, const Ogre::ColourValue& colour) {
            auto* element = static_cast<Ogre::TextAreaOverlayElement*>(
                overlays.createOverlayElement("TextArea", name));
            element->setMetricsMode(Ogre::GMM_RELATIVE);
            element->setFontName(kFontName, Ogre::RGN_INTERNAL);
            set_char_height(element, height * ui_text_scale());
            element->setColour(colour);
            box_->addChild(element);
            return element;
        };
        // The speaker's name in the game's own quest-title amber, the line in
        // white. `|cFFFFBA00` is the colour Torchlight's own quest names carry.
        speaker_ = text("tl/hud/speaker", kLineHeight, Ogre::ColourValue(1.0f, 0.73f, 0.0f));
        line_ = text("tl/hud/line", kLineHeight, Ogre::ColourValue::White);

        // The floaty line is not a child of the box and has an overlay of its own above it:
        // within one overlay OGRE orders transparent renderables by pass, not by element. White
        // at full alpha is what `setNPC` passes for both text-event colours. A `Panel` with no
        // material draws only its children (`PanelOverlayElement::_updateRenderQueue`), so the
        // container at the origin makes the text's position plain viewport units.
        floaty_overlay_ = overlays.create("tl/hud/floaty");
        floaty_overlay_->setZOrder(501);
        floaty_box_ = static_cast<Ogre::OverlayContainer*>(
            overlays.createOverlayElement("Panel", "tl/hud/floatybox"));
        floaty_box_->setMetricsMode(Ogre::GMM_RELATIVE);
        floaty_box_->setPosition(0.0f, 0.0f);
        floaty_box_->setDimensions(1.0f, 1.0f);
        floaty_overlay_->add2D(floaty_box_);

        floaty_pool_.fill(nullptr);
        for (size_t i = 0; i < kMaxFloaties; ++i) {
            const std::string elem_name = "tl/hud/floaty/text/" + std::to_string(i);
            auto* elem = static_cast<Ogre::TextAreaOverlayElement*>(
                overlays.createOverlayElement("TextArea", elem_name));
            elem->setMetricsMode(Ogre::GMM_RELATIVE);
            elem->setFontName(kFontName, Ogre::RGN_INTERNAL);
            set_char_height(elem, kFloatyHeight * ui_text_scale());
            elem->setColour(Ogre::ColourValue::White);
            elem->setAlignment(Ogre::TextAreaOverlayElement::Center);
            elem->hide();
            floaty_box_->addChild(elem);
            floaty_pool_[i] = elem;
        }
        floaty_overlay_->show();

        // The performance line: an overlay of its own at the top of OGRE's
        // range (`Overlay::setZOrder` asserts < 650), above the death screen
        // (600-602) and every menu. Hidden until asked for.
        stats_overlay_ = overlays.create("tl/hud/stats");
        stats_overlay_->setZOrder(640);
        stats_box_ = static_cast<Ogre::OverlayContainer*>(
            overlays.createOverlayElement("Panel", "tl/hud/statsbox"));
        stats_box_->setMetricsMode(Ogre::GMM_RELATIVE);
        stats_box_->setPosition(0.0f, 0.0f);
        stats_box_->setDimensions(1.0f, 1.0f);
        stats_overlay_->add2D(stats_box_);
        const auto stats_line = [&](const char* name, const Ogre::ColourValue& colour, float x,
                                    float y) {
            auto* element = static_cast<Ogre::TextAreaOverlayElement*>(
                overlays.createOverlayElement("TextArea", name));
            element->setMetricsMode(Ogre::GMM_RELATIVE);
            element->setFontName(kFontName, Ogre::RGN_INTERNAL);
            set_char_height(element, kStatsHeight * ui_text_scale());
            element->setColour(colour);
            element->setPosition(x, y);
            stats_box_->addChild(element);
            return element;
        };
        // The shadow first, so the text is drawn over it.
        stats_drop_ = stats_line("tl/hud/stats/drop", black, kStatsX + 0.0015f, kStatsY + 0.002f);
        stats_text_ =
            stats_line("tl/hud/stats/text", Ogre::ColourValue(1.0f, 1.0f, 0.4f), kStatsX, kStatsY);
        stats_overlay_->hide();

        overlay_->show();
        layout();
        TL_LOG_INFO("interface: %s at %u x %u", kFontFile, width_, height_);
        return true;
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("no interface: %s", e.getDescription().c_str());
        destroy();
        return false;
    }
}

void Hud::destroy() {
    if (stats_overlay_ != nullptr) {
        auto& overlays = Ogre::OverlayManager::getSingleton();
        stats_overlay_->hide();
        if (stats_box_ != nullptr) {
            for (Ogre::TextAreaOverlayElement* element : {stats_drop_, stats_text_}) {
                if (element == nullptr) continue;
                stats_box_->removeChild(element->getName());
                overlays.destroyOverlayElement(element);
            }
            stats_overlay_->remove2D(stats_box_);
            overlays.destroyOverlayElement(stats_box_);
        }
        overlays.destroy(stats_overlay_);
    }
    stats_overlay_ = nullptr;
    stats_box_ = nullptr;
    stats_text_ = nullptr;
    stats_drop_ = nullptr;
    stats_visible_ = false;

    if (floaty_overlay_ != nullptr) {
        auto& overlays = Ogre::OverlayManager::getSingleton();
        floaty_overlay_->hide();
        if (floaty_box_ != nullptr) {
            for (size_t i = 0; i < kMaxFloaties; ++i) {
                if (floaty_pool_[i] != nullptr) {
                    floaty_box_->removeChild(floaty_pool_[i]->getName());
                    overlays.destroyOverlayElement(floaty_pool_[i]);
                    floaty_pool_[i] = nullptr;
                }
            }
            floaty_overlay_->remove2D(floaty_box_);
            overlays.destroyOverlayElement(floaty_box_);
            floaty_box_ = nullptr;
        }
        overlays.destroy(floaty_overlay_);
    }
    floaty_overlay_ = nullptr;
    floaty_box_ = nullptr;
    floaty_pool_.fill(nullptr);
    floaties_.clear();

    if (overlay_ != nullptr) {
        auto& overlays = Ogre::OverlayManager::getSingleton();
        overlay_->hide();
        // Children first: an element still parented cannot be destroyed.
        if (box_ != nullptr) {
            if (speaker_ != nullptr) box_->removeChild(speaker_->getName());
            if (line_ != nullptr) box_->removeChild(line_->getName());
            if (portrait_ != nullptr) {
                box_->removeChild(portrait_->getName());
                overlays.destroyOverlayElement(portrait_);
                portrait_ = nullptr;
            }
        }
        for (Ogre::OverlayElement* element : {static_cast<Ogre::OverlayElement*>(speaker_),
                                              static_cast<Ogre::OverlayElement*>(line_)}) {
            if (element != nullptr) overlays.destroyOverlayElement(element);
        }
        for (Ogre::OverlayContainer* container : {top_bar_, bottom_bar_, box_}) {
            if (container == nullptr) continue;
            overlay_->remove2D(container);
            overlays.destroyOverlayElement(container);
        }
        overlays.destroy(overlay_);
    }
    if (system_ != nullptr && scene_ != nullptr) {
        scene_->removeRenderQueueListener(system_);
    }
    delete system_;

    scene_ = nullptr;
    system_ = nullptr;
    overlay_ = nullptr;
    top_bar_ = nullptr;
    bottom_bar_ = nullptr;
    box_ = nullptr;
    speaker_ = nullptr;
    line_ = nullptr;
    portrait_ = nullptr;
    portrait_set_ = tl::formats::Imageset{};
    portrait_texture_.reset();
    portrait_material_.reset();
    portrait_name_.clear();
}

void Hud::resize(unsigned width, unsigned height) {
    width_ = width;
    height_ = height;
    set_char_height(speaker_, kLineHeight * ui_text_scale());
    set_char_height(line_, kLineHeight * ui_text_scale());
    for (Ogre::TextAreaOverlayElement* elem : floaty_pool_) {
        set_char_height(elem, kFloatyHeight * ui_text_scale());
    }
    set_char_height(stats_text_, kStatsHeight * ui_text_scale());
    set_char_height(stats_drop_, kStatsHeight * ui_text_scale());
    layout();
    layout_floaties();
}

// PORT: the debug FPS overlay's text.
void Hud::set_stats(const std::string& text) {
    if (stats_text_ == nullptr) return;
    stats_text_->setCaption(text);
    stats_drop_->setCaption(text);
}

// PORT: the debug FPS overlay shown or hidden.
void Hud::set_stats_visible(bool on) {
    stats_visible_ = on;
    if (stats_overlay_ == nullptr) return;
    if (on) {
        stats_overlay_->show();
    } else {
        stats_overlay_->hide();
    }
}

void Hud::set_letterboxed(bool on) {
    bars_.set_visible(on);
    layout();
}

void Hud::update_bars(float delta) {
    const bool was_visible = bars_.window_visible;
    const float top = bars_.top_y;
    const float bottom = bars_.bottom_y;
    bars_.update(delta);
    if (bars_.window_visible != was_visible || bars_.top_y != top || bars_.bottom_y != bottom) {
        layout();
    }
}

void Hud::set_line(const std::string& speaker, const std::string& text,
                   const std::string& portrait) {
    speaker_text_ = speaker;
    line_text_ = text;
    portrait_name_ = portrait;
    layout();
}

void Hud::set_floaties(const std::vector<FloatyItem>& items) {
    floaties_ = items;
    layout_floaties();
}

void Hud::set_floaty(const std::string& text, float x, float y, float alpha) {
    if (text.empty() || alpha <= 0.0f) {
        set_floaties({});
        return;
    }
    FloatyItem item;
    item.text = text;
    item.x = x;
    item.y = y;
    item.alpha = alpha;
    item.colour = Ogre::ColourValue::White;
    item.char_height = kFloatyHeight;
    set_floaties({item});
}

std::vector<std::string> Hud::wrap(const std::string& text, float width, float height) const {
    // One measure, shared with the loading screen's tip; see render/text_layout.h
    // for why it is the glyph's *advance* and not its aspect ratio.
    Ogre::FontPtr font;
    if (line_ != nullptr) {
        font = line_->getFont();
    } else if (floaty_pool_[0] != nullptr) {
        font = floaty_pool_[0]->getFont();
    }
    return wrap_text(font, text, width, height, static_cast<float>(width_),
                     static_cast<float>(height_));
}

void Hud::layout_floaties() {
    if (floaty_overlay_ == nullptr || floaty_box_ == nullptr) return;

    const size_t active_count = std::min(floaties_.size(), kMaxFloaties);
    for (size_t i = 0; i < active_count; ++i) {
        auto* elem = floaty_pool_[i];
        if (elem == nullptr) continue;
        const FloatyItem& item = floaties_[i];
        const bool on = !item.text.empty() && item.alpha > 0.0f && item.x > 0.0f && item.x < 1.0f &&
                        item.y > 0.0f && item.y < 1.0f;
        if (!on) {
            elem->hide();
            continue;
        }

        const float char_h =
            (item.char_height > 0.0f ? item.char_height : kFloatyHeight) * ui_text_scale();
        const std::vector<std::string> lines = wrap(item.text, kFloatyWrap, char_h);
        std::string body;
        for (size_t l = 0; l < lines.size(); ++l) {
            if (l > 0) body += "\n";
            body += lines[l];
        }
        const float tall = char_h * 1.35f * static_cast<float>(lines.size());
        set_char_height(elem, char_h);
        set_caption(elem, body);
        set_position(elem, item.x, item.y - tall);
        Ogre::ColourValue c = item.colour;
        c.a = item.colour.a * item.alpha;
        set_colour(elem, c);
        elem->show();
    }

    for (size_t i = active_count; i < kMaxFloaties; ++i) {
        if (floaty_pool_[i] != nullptr) {
            floaty_pool_[i]->hide();
        }
    }
}

void Hud::layout() {
    if (overlay_ == nullptr) return;

    // `interactivemenu.layout`'s two strips, where `CInteractiveMenu` has slid
    // them. DIVERGENCE: drawn black; the layout's `set:Loading
    // image:background` is not loaded here.
    top_bar_->setPosition(0.0f, bars_.top_y);
    top_bar_->setDimensions(1.0f, tl::core::InteractiveBars::kBarHeight);
    bottom_bar_->setPosition(0.0f, bars_.bottom_y);
    bottom_bar_->setDimensions(1.0f, tl::core::InteractiveBars::kBarHeight);
    if (bars_.window_visible) {
        top_bar_->show();
        bottom_bar_->show();
    } else {
        top_bar_->hide();
        bottom_bar_->hide();
    }

    // Floating text items over the world / speakers. Before the box's early return,
    // because the two are independent and the box being empty must not leave
    // floating text frozen on screen.
    layout_floaties();

    if (line_text_.empty()) {
        box_->hide();
        return;
    }

    // The box sits above the lower bar when there is one, so a cutscene's
    // letterboxing never covers what is being said.
    constexpr float kMargin = 0.06f;
    const float box_width = 1.0f - 2.0f * kMargin;
    const float padding = kLineHeight * 0.6f;

    // The speaker's portrait: a 128x128 cut-out, square on screen (width = height / aspect).
    const tl::formats::ImagesetImage* head = portrait_ != nullptr && !portrait_name_.empty()
                                                 ? portrait_set_.find(portrait_name_)
                                                 : nullptr;
    const float aspect =
        height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    const float head_tall = kLineHeight * 3.6f;
    const float head_wide = head_tall / aspect;
    const float text_left = head != nullptr ? 0.015f + head_wide + 0.015f : 0.015f;

    const std::vector<std::string> lines =
        wrap(line_text_, box_width - text_left - 0.015f, kLineHeight);

    const float text_height = kLineHeight * 1.35f * static_cast<float>(lines.size() + 1);
    // Never shorter than the portrait, or the head hangs out of the box.
    const float inner = head != nullptr && head_tall > text_height ? head_tall : text_height;
    const float box_height = inner + padding * 2.0f;
    const float bottom = bars_.bottom_edge();
    const float top = bottom - box_height - kMargin * 0.5f;

    box_->setPosition(kMargin, top);
    box_->setDimensions(box_width, box_height);
    box_->show();

    if (portrait_ != nullptr && head != nullptr) {
        const float tw = static_cast<float>(portrait_texture_->getWidth());
        const float th = static_cast<float>(portrait_texture_->getHeight());
        static_cast<Ogre::PanelOverlayElement*>(portrait_)->setUV(
            static_cast<float>(head->x) / tw, static_cast<float>(head->y) / th,
            static_cast<float>(head->x + head->width) / tw,
            static_cast<float>(head->y + head->height) / th);
        portrait_->setPosition(0.015f, padding);
        portrait_->setDimensions(head_wide, head_tall);
        portrait_->show();
    } else if (portrait_ != nullptr) {
        portrait_->hide();
    }

    speaker_->setPosition(text_left, padding);
    speaker_->setCaption(speaker_text_);

    std::string body;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) body += "\n";
        body += lines[i];
    }
    line_->setPosition(text_left, padding + kLineHeight * 1.35f);
    line_->setCaption(body);
}

}  // namespace tl::render
