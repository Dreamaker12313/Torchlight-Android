// The text-crawl cinematics: `media/cinematics/*.dat`, `CCinematics`, and the
// arithmetic of `CCinematicMenu` (art, crawling text, a recording, Continue).
//
// Three things open one, all through `CGameUI::setCinematicOpen(name)`:
//
//   * `CGameClient::setGameState` for a new
//     character, with the class's `INTRO_CINEMATIC`
//     (`CCharacter::getIntroCinematic`);
//   * `CGameClient::performWarp` on arriving at
//     a level whose template names a `CINEMATIC` (`warp_plays_cinematic` below);
//   * a layout's `Cinematic` node (`CCinematicObject::play`).
//
// No OGRE and no file I/O: whoever has the archive open does the reading.
#pragma once

#include "formats/adm.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// One `[CINEMATIC]` file: `CCinematic`, 0x20 bytes of four strings.
struct Cinematic {
    std::string name;   ///< +0x00 `NAME`, the key `setCinematicOpen` is given
    std::string text;   ///< +0x08 `TEXT`, with literal backslash-n between paragraphs
    std::string image;  ///< +0x10 `IMAGE`, an imageset whose `image` is the art
    std::string sound;  ///< +0x18 `SOUND`, a sound definition name
};

/// `CCinematic::load(CDataGroup*)`.
Cinematic load_cinematic(const tl::formats::AdmSection& group);

/// `CCinematics`: every file under `media/cinematics/`, found by `NAME`.
/// `CCinematics::CCinematics` (on the console made by
/// `CMasterResourceManager::CMasterResourceManager`): the list
/// (`+0x18`, growing by 10) and the map (`+0x38`) empty, `g_pCinematics` set
/// and `reload` at once.
class Cinematics {
public:
    /// `CCinematics::reload`, one file's
    /// worth: on the list, and its name indexed to it - the last file of a
    /// name wins.
    void add(Cinematic cinematic);
    const std::vector<Cinematic>& list() const { return list_; }

    /// `CCinematics::getCinematic(wchar_t const*)`: an exact match,
    /// case and all, or null.
    const Cinematic* get(const std::string& name) const;

    size_t size() const { return list_.size(); }

private:
    std::vector<Cinematic> list_;
    std::map<std::string, uint32_t> index_;
};

/// The text `CCinematicMenu::setText` hands
/// the window: every literal backslash-n becomes a newline.
std::string cinematic_display_text(const std::string& text);

/// The crawl's length in seconds, from the same function: 0.015 s
/// for each byte of the UTF-8 display text, and never under 10.
float cinematic_crawl_seconds(const std::string& display_text);

/// The state `CCinematicMenu::updateTextDisplay`
/// keeps: `+0xd8` the time crawled and
/// `+0xdc` its length.
struct CinematicCrawl {
    float elapsed = 0.0f;   ///< +0xd8
    float duration = 0.0f;  ///< +0xdc

    /// One call of `updateTextDisplay(delta)`: returns how far below its resting
    /// place the text window's top sits, in pixels. `window_height` is the
    /// text window's own height (`DialogText`, 200 at the native 768).
    float step(float delta, float window_height);

    bool finished() const { return !(0.0f < duration && elapsed < duration); }
};

/// The time `CCinematicMenu::update` feeds
/// the crawl this frame: ten times the frame's while a mouse button is held,
/// the whole crawl on the frame one is pressed, otherwise the frame's.
float cinematic_crawl_delta(float delta, bool held, bool pressed, float duration);

/// The music's share while a cinematic is up (`CCinematicMenu +0xf8`, from
/// `update`): it sinks to a quarter at one unit a
/// second while the menu is open and climbs back to 1 at the same rate once it
/// has closed.
float cinematic_music_level(float level, float delta, bool open);

/// `CGameClient::performWarp`: the arrival plays the level
/// template's `CINEMATIC` when the player has never been deeper in the
/// destination dungeon than this depth, the template names one, and the level
/// was not put back from a remembered state (`CLevel +0x1a1`).
bool warp_plays_cinematic(int32_t max_depth, int32_t depth, const std::string& cinematic,
                          bool level_restored);

}  // namespace tl::core
