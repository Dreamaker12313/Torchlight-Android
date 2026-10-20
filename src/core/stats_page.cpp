#include "core/stats_page.h"

#include "core/character_sheet.h"

#include <algorithm>

namespace tl::core {

namespace {

constexpr StatType kStats[4] = {StatType::Strength, StatType::Dexterity, StatType::Magic,
                                StatType::Defense};

}  // namespace

void open_stats_page(StatsPage& page, int unspent) {
    // CStatsMenuXenon::vfunc_7, the open branch.
    if (page.open) return;
    page.open = true;
    for (int& count : page.pending) count = 0;
    page.available = unspent;
    page.dirty = true;
}

void close_stats_page(StatsPage& page, CharacterProgression& player) {
    // CStatsMenuXenon::vfunc_7, the close branch:.
    // The pending counts stay until the next opening.
    if (!page.open) return;
    page.open = false;
    commit_stats_page(page, player);
}

void commit_stats_page(const StatsPage& page, CharacterProgression& player) {
    //
    for (int i = 0; i < 4; ++i) {
        for (int n = 0; n < page.pending[i]; ++n) {
            if (player.unspent_stat_points() > 0) player.move_stat_point(kStats[i], 1);
        }
    }
}

void revert_stats_page(const StatsPage& page, CharacterProgression& player) {
    //
    for (int i = 0; i < 4; ++i) {
        for (int n = 0; n < page.pending[i]; ++n) {
            if (player.base_stat(kStats[i]) > 2) player.move_stat_point(kStats[i], -1);
        }
    }
}

bool stats_page_add_point(StatsPage& page, int unspent) {
    // CStatsMenuXenon's handler, case 5.
    if (unspent == 0) return false;
    bool moved = false;
    if (page.available > 0) {
        ++page.pending[page.selected];
        --page.available;
        moved = true;
    }
    page.dirty = true;
    return moved;
}

bool stats_page_remove_point(StatsPage& page, int unspent) {
    // with a negative step: no refresh either when nothing moves.
    if (unspent == 0) return false;
    if (page.pending[page.selected] == 0) return false;
    --page.pending[page.selected];
    ++page.available;
    page.dirty = true;
    return true;
}

bool stats_page_step(StatsPage& page, int direction) {
    // refreshes.
    const int before = page.selected;
    page.selected = std::max(0, std::min(3, page.selected + direction));
    page.dirty = true;
    return page.selected != before;
}

uint32_t stats_figure_colour(int difference) {
    //
    if (difference > 0) return 0xFFC0C0FFu;
    if (difference < 0) return 0xFFFF0000u;
    return 0xFFFFFFFFu;
}

StatsPageView refresh_stats_page(StatsPage& page, CharacterProgression& player,
                                 CharacterEffects effects, const Inventory& inventory,
                                 const UnitTypeHierarchy& unit_types, const StatBook* graphs,
                                 const AttackDescription* own_attack) {
    // CStatsXenon::updateStats.
    StatsPageView view;
    page.dirty = false;
    commit_stats_page(page, player);

    // Windows 4-7: the attribute's getters,
    // coloured against the base read after the commit.
    for (int i = 0; i < 4; ++i) {
        const int total = player.stat(kStats[i], effects);
        view.points[i] = std::to_string(total);
        view.points_colour[i] = stats_figure_colour(total - player.base_stat(kStats[i]));
    }

    // Windows 8-11: `minimumDamageForDisplay` "-"
    // `maximumDamageForDisplay`, coloured against the maximum without effects.
    const DisplayInputs in =
        make_display_inputs(player, inventory, unit_types, effects, graphs, own_attack);
    auto range = [](int low, int high) { return std::to_string(low) + "-" + std::to_string(high); };
    // Melee (1,0,x) and ranged (0,0,x).
    for (int i = 0; i < 2; ++i) {
        const bool melee = i == 0;
        const int low = minimum_damage_for_display(in, melee, false, false);
        const int high = maximum_damage_for_display(in, melee, false, false);
        const int plain = maximum_damage_for_display(in, melee, false, true);
        view.damage[i] = range(low, high);
        view.damage_colour[i] = stats_figure_colour(high - plain);
    }
    {
        // Magic: the low end the smaller nonzero of the pair.
        const int low_ranged = minimum_damage_for_display(in, false, true, false);
        const int low_melee = minimum_damage_for_display(in, true, true, false);
        int low = low_ranged;
        if (low_ranged == 0) low = low_melee;
        else if (low_melee != 0) low = low_ranged < low_melee ? low_ranged : low_melee;
        const int high = std::max(maximum_damage_for_display(in, false, true, false),
                                  maximum_damage_for_display(in, true, true, false));
        const int plain = std::max(maximum_damage_for_display(in, false, true, true),
                                   maximum_damage_for_display(in, true, true, true));
        view.damage[2] = range(low, high);
        view.damage_colour[2] = stats_figure_colour(high - plain);
    }
    {
        // Armour: `minimumAC` "-" `AC`, coloured by
        // `AC` less `baseArmorBonus`.
        const int base = player.base_armour_bonus(effects);
        const int ac = player.armour_class(effects);
        view.damage[3] = range(CharacterProgression::display_minimum(ac), ac);
        view.damage_colour[3] = stats_figure_colour(ac - base);
    }

    // Window 12: `+0x9c`. 13-16: `damageDefense` of poison (5),
    // fire (2), ice (3) and electric (4). All "FFFFFFFF".
    view.to_spend = std::to_string(page.available);
    constexpr int kResistanceTypes[4] = {5, 2, 3, 4};
    for (int i = 0; i < 4; ++i) {
        view.resistances[i] = std::to_string(player.damage_defense(kResistanceTypes[i], effects));
    }

    // Windows 19-22: "+" and the count, shown while it is above 0.
    for (int i = 0; i < 4; ++i) {
        view.added[i] = "+" + std::to_string(page.pending[i]);
        view.added_shown[i] = page.pending[i] > 0;
    }

    // Windows 23-29, from the points as the commit left them: `setAlpha`.
    const bool can_add = player.unspent_stat_points() > 0;
    const bool can_remove = page.pending[page.selected] > 0;
    view.add_alpha = can_add ? 1.0f : 0.25f;
    view.remove_alpha = can_remove ? 1.0f : 0.25f;
    view.spend_shown = can_remove || can_add;
    for (int i = 0; i < 4; ++i) view.help_shown[i] = page.selected == i && view.spend_shown;

    revert_stats_page(page, player);
    return view;
}

}  // namespace tl::core
