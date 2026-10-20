#include "render/menu_chrome.h"

#include "core/log.h"

#include <OgreDataStream.h>
#include <OgreFont.h>
#include <OgreFontManager.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreResourceGroupManager.h>
#include <OgreTechnique.h>
#include <OgreTextureUnitState.h>

namespace tl::render {

Ogre::MaterialPtr build_menu_material(const std::string& name, const Ogre::TexturePtr& texture,
                                      float alpha) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    if (Ogre::MaterialPtr existing = materials.getByName(name, Ogre::RGN_INTERNAL)) {
        materials.remove(existing);
    }
    Ogre::MaterialPtr material = materials.create(name, Ogre::RGN_INTERNAL);
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setDepthCheckEnabled(false);
    pass->setDepthWriteEnabled(false);
    pass->setCullingMode(Ogre::CULL_NONE);
    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);

    Ogre::TextureUnitState* unit = pass->createTextureUnitState();
    unit->setTexture(texture);
    unit->setColourOperationEx(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    if (alpha < 1.0f) {
        // A CEGUI window's `Alpha`, which multiplies everything it draws.
        unit->setAlphaOperation(Ogre::LBX_MODULATE, Ogre::LBS_TEXTURE, Ogre::LBS_MANUAL, 1.0f,
                                alpha);
    } else {
        unit->setAlphaOperation(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
    }
    // Clamped, because every panel here draws a sub-rectangle of an atlas and a
    // wrapping sampler bleeds the neighbouring image in along the seam.
    unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
    return material;
}

float authored_height(const tl::formats::Imageset& reference, float fallback) {
    return reference.ok && reference.native_height > 0 ? static_cast<float>(reference.native_height)
                                                       : fallback;
}

namespace {
bool g_console_game_data = false;
}  // namespace

void set_console_game_data(bool console) { g_console_game_data = console; }
bool console_game_data() { return g_console_game_data; }

bool read_menu_resource(const std::string& path, const std::string& group,
                        std::vector<uint8_t>& bytes) {
    // The console-UI extraction's renamed sheet: beside an Xbox pak, absent as expected.
    if (g_console_game_data && path.find("xbox_itemicons13") != std::string::npos) return false;
    try {
        Ogre::DataStreamPtr stream =
            Ogre::ResourceGroupManager::getSingleton().openResource(path, group);
        if (!stream) return false;
        bytes.resize(static_cast<size_t>(stream->size()));
        if (!bytes.empty()) stream->read(bytes.data(), bytes.size());
        return true;
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("menu: could not read '%s': %s", path.c_str(), e.getDescription().c_str());
        return false;
    }
}

bool ensure_menu_font() {
    auto& fonts = Ogre::FontManager::getSingleton();
    if (fonts.getByName(kFontName, Ogre::RGN_INTERNAL)) return true;
    try {
        Ogre::FontPtr font = fonts.create(kFontName, Ogre::RGN_INTERNAL);
        font->setType(Ogre::FT_TRUETYPE);
        font->setSource(kFontFile);
        font->setTrueTypeSize(48);
        font->setTrueTypeResolution(96);
        font->addCodePointRange(Ogre::Font::CodePointRange(32, 255));
        font->load();
        return true;
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("menu: could not build a font from '%s': %s", kFontFile,
                    e.getDescription().c_str());
        return false;
    }
}

float font_cell_per_em() {
    // The same TTF at the same size on every renderer this process builds, so
    // once is enough - and the value outlives a `Renderer::restart()` that
    // throws the font itself away.
    static float measured = 0.0f;
    if (measured > 0.0f) return measured;
    Ogre::FontPtr font = Ogre::FontManager::getSingleton().getByName(kFontName, Ogre::RGN_INTERNAL);
    if (!font || font->getTrueTypeSize() <= 0.0f || font->getTrueTypeResolution() == 0) {
        return 1.0f;
    }
    font->load();
    const Ogre::MaterialPtr& material = font->getMaterial();
    if (!material || material->getNumTechniques() == 0) return 1.0f;
    Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    if (pass == nullptr || pass->getNumTextureUnitStates() == 0) return 1.0f;
    const Ogre::TexturePtr& atlas = pass->getTextureUnitState(0)->_getTexturePtr();
    if (!atlas || atlas->getHeight() == 0) return 1.0f;
    // Every glyph's rectangle is one cell tall; `M` is as good as any.
    const Ogre::Font::UVRect& uv = font->getGlyphTexCoords('M');
    const float cell = (uv.bottom - uv.top) * static_cast<float>(atlas->getHeight());
    const float em =
        font->getTrueTypeSize() * static_cast<float>(font->getTrueTypeResolution()) / 72.0f;
    if (!(cell > 0.0f) || !(em > 0.0f)) return 1.0f;
    measured = cell / em;
    TL_LOG_INFO("interface font: a glyph cell is %.3f em", static_cast<double>(measured));
    return measured;
}

}  // namespace tl::render
