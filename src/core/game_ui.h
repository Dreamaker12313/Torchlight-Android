// What the game screen's interface decides, as opposed to how it is drawn
// (`render/game_hud.*`): the `+` over the bag that says points are waiting, and
// the level-up banner's words. The `+` is Xbox-only (`hud_xenon.layout`'s
// `AvailableSkillsIcon` and `AvailableSkillsGlowIcon`; `CGameUI::create`
// hides them, the in-game update drives them). The
// captions are `CPlayer::levelUp`'s.
#pragma once

#include <string>

namespace tl::core {

/// The `+` on the bag and its glow (`CGameUI +0x187c`, `+0x1880`), with their
/// timer and phase. Both windows keep the visibility they were last given.
struct AvailablePointsIcon {
    float timer = 0.0f;  ///< `+0x1874`, 0 from the constructor
    bool lit = false;    ///< `+0x1878`, 0 from the constructor
    bool plus = false;   ///< `+0x187c` visible; `CGameUI::create` hides it
    bool glow = false;   ///< `+0x1880` visible; hidden likewise

    /// One frame of the Xbox update: with no
    /// unspent stat or skill points both are hidden; otherwise the `+` is shown
    /// and the timer runs down, and below zero the phase flips and the timer
    /// goes back to 1.0. So the glow comes and goes over a steady `+`, once a
    /// second.
    void update(float delta, int unspent_stat_points, int unspent_skill_points);
};

/// `CPlayer::levelUp`: the big banner,
/// `translate("Level Up")` followed by "!", for 2.0
/// seconds.
std::string level_up_banner();

/// ... and the small one under it, `translate("Level") + " " + level + " " +
/// translate("reached")` with the level just reached
/// (`CCharacter +0x100`), for the same 2.0 seconds.
std::string level_up_caption(int level);

/// The seconds both level-up banners are held for: the float `setBigText` and
/// `setSmallText` are passed, 2.0.
inline constexpr float kLevelUpBannerSeconds = 2.0f;

}  // namespace tl::core
