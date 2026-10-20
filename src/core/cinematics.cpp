#include "core/cinematics.h"

namespace tl::core {

// CCinematic::load
Cinematic load_cinematic(const tl::formats::AdmSection& group) {
    Cinematic out;
    out.name = group.string_or("NAME", "");
    out.text = group.string_or("TEXT", "");
    out.image = group.string_or("IMAGE", "");
    out.sound = group.string_or("SOUND", "");
    return out;
}

// CCinematics::reload, the insertion.
void Cinematics::add(Cinematic cinematic) {
    // `CCinematics::reload`: every file goes on
    // the list, and its `NAME` is indexed to it over any earlier file of the
    // same name - the last one wins.
    index_[cinematic.name] = static_cast<uint32_t>(list_.size());
    list_.push_back(std::move(cinematic));
}

// CCinematics::getCinematic
const Cinematic* Cinematics::get(const std::string& name) const {
    const auto found = index_.find(name);
    if (found == index_.end() || found->second >= list_.size()) return nullptr;
    return &list_[found->second];
}

// CCinematicMenu::setText, the
// `replaceWString(text, L"\\n", L"\n")`.
std::string cinematic_display_text(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            out.push_back('\n');
            ++i;
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

// CCinematicMenu::setText.
float cinematic_crawl_seconds(const std::string& display_text) {
    const float seconds = static_cast<float>(display_text.size()) * 0.015f;
    return 10.0f < seconds ? seconds : 10.0f;
}

// CCinematicMenu::updateTextDisplay
float CinematicCrawl::step(float delta, float window_height) {
    if (0.0f < duration && elapsed < duration) {
        elapsed += delta;
        const float travel = window_height + 50.0f;
        return travel - (elapsed / duration) * travel;
    }
    return 0.0f;
}

// CCinematicMenu::update.
float cinematic_crawl_delta(float delta, bool held, bool pressed, float duration) {
    if (held) return delta * 10.0f;
    if (pressed) return duration;
    return delta;
}

// CCinematicMenu::update.
float cinematic_music_level(float level, float delta, bool open) {
    if (!open) {
        if (level < 1.0f) {
            const float raised = level + delta;
            return raised <= 1.0f ? raised : 1.0f;
        }
        return level;
    }
    if (level <= 0.25f) return level;
    const float lowered = level - delta;
    return 0.25f <= lowered ? lowered : 0.25f;
}

// CGameClient::performWarp,
//
bool warp_plays_cinematic(int32_t max_depth, int32_t depth, const std::string& cinematic,
                          bool level_restored) {
    if (max_depth <= depth && !cinematic.empty()) return !level_restored;
    return false;
}

}  // namespace tl::core
