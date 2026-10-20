#include "core/death.h"

#include "formats/float_convert.h"

#include <algorithm>

namespace tl::core {
namespace {

// `CMasterResourceManager::experienceGate`/`fameGate`: level 0 is 0; a level past
// the curve's control points reads the last one; truncated (`fctiwz`).
// PORT: a curve the book has not got reads 0 (the console's is loaded at start).
int gate(const StatBook& graphs, const char* name, int level) {
    if (level == 0) return 0;
    const StatGraph* graph = graphs.find(name);
    if (graph == nullptr) return 0;
    const int points = static_cast<int>(graph->size());
    if (points < level) level = points;
    return tl::formats::ppc_fctiwz(graph->at(static_cast<float>(level)));
}

// PORT: a `[TOKEN]` of a text replaced by a number.
std::string replaced(std::string text, const std::string& token, int value) {
    const size_t at = text.find(token);
    if (at != std::string::npos) text.replace(at, token.size(), std::to_string(value));
    return text;
}

}  // namespace

// CMasterResourceManager::experienceGate
int experience_gate(const StatBook& graphs, int level) {
    return gate(graphs, kExperienceGateGraph, level);
}

// CMasterResourceManager::fameGate
int fame_gate(const StatBook& graphs, int level) { return gate(graphs, kFameGateGraph, level); }

// CDieMenu::setOpen: a tenth of the current
// level's experience span, a tenth of the fame level's span, a tenth of the
// gold. Integer division.
DeathPenalty death_screen_figures(const StatBook& graphs, int level, int fame_level, int gold) {
    DeathPenalty figures;
    figures.experience = (experience_gate(graphs, level) - experience_gate(graphs, level - 1)) / 10;
    figures.fame = (fame_gate(graphs, fame_level) - fame_gate(graphs, fame_level - 1)) / 10;
    figures.gold = gold / 10;
    return figures;
}

// CDieMenu::onClick
DeathPenalty death_penalty(ResurrectChoice choice, const StatBook& graphs, int level,
                           int fame_level, int gold) {
    const DeathPenalty all = death_screen_figures(graphs, level, fame_level, gold);
    DeathPenalty taken;
    switch (choice) {
        case ResurrectChoice::Here:
            taken.experience = all.experience;
            taken.fame = all.fame;
            break;
        case ResurrectChoice::Entrance: taken.gold = all.gold; break;
        case ResurrectChoice::Town: break;
    }
    return taken;
}

// The translate keys `setOpen` reads (UTF-32 literals).
std::string resurrect_here_text(const DeathPenalty& figures) {
    std::string text = "Resurrect here, but lose [XP] Experience and [FAME] Fame.";
    text = replaced(text, "[XP]", figures.experience);
    return replaced(text, "[FAME]", figures.fame);
}

// `CDieMenu::onClick`'s second choice, its
// text with the gold put in. PORT: the PC's English text.
std::string resurrect_entrance_text(const DeathPenalty& figures) {
    return replaced("Resurrect at the entrance to this floor, but lose [GOLD] Gold.", "[GOLD]",
                    figures.gold);
}

// `CDieMenu::onClick`'s third choice. PORT: the
// PC's English text.
std::string resurrect_town_text() { return "Resurrect in town and lose nothing."; }

// `g_Resurrect` and `g_Death`.
std::string death_screen_title(bool hardcore) { return hardcore ? "Death" : "Resurrect"; }

}  // namespace tl::core
