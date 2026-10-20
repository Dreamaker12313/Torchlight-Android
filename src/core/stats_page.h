// The console's Stats column, `CStatsXenon`. Points placed are kept pending: every refresh
// puts them on the character, reads the figures and takes them off; only closing keeps them.
#pragma once

#include "core/character.h"
#include "core/character_effects.h"
#include "core/units.h"

#include <cstdint>
#include <string>

namespace tl::core {

class Inventory;
class UnitTypeHierarchy;
class StatBook;

/// `CStatsXenon`'s state. Indices are `StatType`'s: strength, dexterity, magic,
/// defense, which is also the order of the page's four panels.
struct StatsPage {
    /// `CStatsMenuXenon`'s open state, which `vfunc_7` compares
    /// before doing anything.
    bool open = false;
    int pending[4] = {0, 0, 0, 0};  ///< `+0x8c` strength .. `+0x98` defense
    int available = 0;              ///< `+0x9c`, points still to place
    int selected = 0;               ///< `+0xa4`, 0 at construction, kept across openings
    bool dirty = false;             ///< `+0xb8`, a refresh is due
};

/// `CStatsMenuXenon::vfunc_7` (`setOpen`) with true: pending cleared, `available`
/// the character's unspent points. A refresh follows (from `vfunc_8`).
void open_stats_page(StatsPage& page, int unspent);

/// `CStatsMenuXenon::vfunc_7` with false: the pending points put on the
/// character for good. Nothing when the page is not open.
void close_stats_page(StatsPage& page, CharacterProgression& player);

/// each pending point onto its attribute while the character
/// has an unspent one.
void commit_stats_page(const StatsPage& page, CharacterProgression& player);

/// each pending point back off its attribute, but only while
/// the attribute is above 2.
void revert_stats_page(const StatsPage& page, CharacterProgression& player);

/// The page's handler, code 5 (A): one more pending on the selection.
/// True when a point moved (`POINTASSIGN`). A refresh is due while points are unspent.
bool stats_page_add_point(StatsPage& page, int unspent);

/// with -1, the handler's code 8 (Y): one pending point off the
/// selected attribute. True when one moved (`POINTASSIGN`).
bool stats_page_remove_point(StatsPage& page, int unspent);

/// the handler's codes 0x11 (-1, up) and 0x12 (+1, down): the selection
/// clamped to 0..3, then a refresh. True when it changed (`NAVIGATE`).
bool stats_page_step(StatsPage& page, int direction);

/// the function's `TextColour`: `FFc0c0ff` when the figure is above what it
/// is compared with, `FFFF0000` below, `FFFFFFFF` equal. ARGB.
uint32_t stats_figure_colour(int difference);

/// What one refresh writes, window by window. The arrays
/// follow `StatType`.
struct StatsPageView {
    std::string points[4];        ///< `strPts`.. the attribute with effects
    uint32_t points_colour[4]{};  ///< against the attribute before effects
    /// `strDMG`, `dexDMG`, `magicDMG`, `defArmor`: "min-max".
    std::string damage[4];
    /// the maximum against it without effects; armour against `baseArmorBonus`
    uint32_t damage_colour[4]{};
    std::string to_spend;  ///< `ptsToSpend`, `available`
    /// `poisonRes`, `fireRes`, `iceRes`, `eletricRes`: `damageDefense` of 5, 2, 3, 4.
    std::string resistances[4];
    std::string added[4];        ///< `strPtsAdded`..: "+" and the pending count
    bool added_shown[4]{};       ///< the pending count is above 0
    float add_alpha = 0.25f;     ///< `addPoint`: 1 with unspent points left after the pending ones
    float remove_alpha = 0.25f;  ///< `removePoint`: 1 with points pending on the selection
    bool spend_shown = false;    ///< `spendPoints`: either of the two
    bool help_shown[4]{};        ///< `StrHelp`..: the selection's, when `spend_shown`
};

/// `CStatsXenon::updateStats`: commit, read every figure, revert (in that
/// order, so the revert's "above 2" applies), clear `dirty`. `effects` are the totals.
StatsPageView refresh_stats_page(StatsPage& page, CharacterProgression& player,
                                 CharacterEffects effects, const Inventory& inventory,
                                 const UnitTypeHierarchy& unit_types, const StatBook* graphs,
                                 const AttackDescription* own_attack);

}  // namespace tl::core
