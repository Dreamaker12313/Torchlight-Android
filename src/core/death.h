// The player's death and resurrection: what the death screen (`CDieMenu`)
// offers, what each choice costs, and how long after the fall it opens.
#pragma once

#include "core/stats.h"

#include <string>

namespace tl::core {

/// `CGameUI +0x1928`, written by `CDieMenu::update` from `CDieMenu +0xc4`.
enum class ResurrectChoice : int {
    Here = 0,      ///< `GUISELECT1` (14): lose experience and fame
    Entrance = 1,  ///< `GUISELECT2` (15): lose gold, stand at the level's start
    Town = 2,      ///< `GUISELECT3` (16): lose nothing, go to town
};

/// `CDieMenu +0xfc` experience, `+0xf8` fame, `+0x100` gold.
struct DeathPenalty {
    int experience = 0;
    int fame = 0;
    int gold = 0;
};

/// `CPlayer +0x884`, the delay before the death screen: 10 from the constructor,
/// held at 0.5 while a `DIE` animation plays, counted down after, and parked at
/// 999999 once the screen has opened.
inline constexpr float kDeathTimerStart = 10.0f;
inline constexpr float kDeathScreenDelay = 0.5f;
inline constexpr float kDeathTimerParked = 999999.0f;

int experience_gate(const StatBook& graphs, int level);
int fame_gate(const StatBook& graphs, int level);

/// The figures `CDieMenu::setOpen` writes into the three lines.
DeathPenalty death_screen_figures(const StatBook& graphs, int level, int fame_level, int gold);

/// What a click takes: only the chosen line's figures, the others zero.
DeathPenalty death_penalty(ResurrectChoice choice, const StatBook& graphs, int level,
                           int fame_level, int gold);

/// `setOpen`'s strings with `[XP]`, `[FAME]` and `[GOLD]` replaced.
std::string resurrect_here_text(const DeathPenalty& figures);
std::string resurrect_entrance_text(const DeathPenalty& figures);
std::string resurrect_town_text();
/// The screen's title: `Resurrect`, or `Death` for a hardcore character.
std::string death_screen_title(bool hardcore);

}  // namespace tl::core
