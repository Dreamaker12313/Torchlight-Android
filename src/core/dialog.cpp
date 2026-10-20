#include "core/dialog.h"

#include <algorithm>
#include <cctype>

namespace tl::core {
namespace {

// PORT: ASCII upper-casing, the case-blind compare of quest and unit names.
std::string upper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

/// Which sections to look in, best first, for a quest in this state; `PASSIVE`
/// is last in every state. INVENTED, a stand-in for `CQuest::getQuestDialog`
/// where no quest log is there to ask.
std::vector<const char*> preference(QuestStatus status) {
    switch (status) {
        case QuestStatus::Complete: return {"COMPLETE", "RETURN", "PASSIVE"};
        case QuestStatus::Active: return {"RETURN", "COMPLETE", "PASSIVE"};
        case QuestStatus::Unknown:
        default: return {"INTRO", "PASSIVE", "RETURN"};
    }
}

}  // namespace

// PORT: a dialog line kept by quest (the port's book of every quest's lines).
void DialogBook::add(const std::string& quest, DialogLine line) {
    by_quest_[upper(quest)].push_back(std::move(line));
}

// PORT: a diagnostic count of the book's lines.
size_t DialogBook::line_count() const {
    size_t n = 0;
    for (const auto& entry : by_quest_) n += entry.second.size();
    return n;
}

// INVENTED: a quest's line for a unit by the section order of `preference`, for a
// machine with no quest log to ask (the tests); in the game the quest log's
// `dialog_for` answers.
const DialogLine* DialogBook::find(const std::string& quest, const std::string& unit,
                                   QuestStatus status) const {
    const auto it = by_quest_.find(upper(quest));
    if (it == by_quest_.end()) return nullptr;
    const std::vector<DialogLine>& lines = it->second;

    const std::string want = upper(unit);
    for (const char* section : preference(status)) {
        for (const DialogLine& line : lines) {
            if (line.section != section) continue;
            if (want.empty()) return &line;
            if (upper(line.unit) == want) return &line;
        }
    }

    // A controller that names no unit takes whatever the quest says, in file
    // order; one that names a unit with nothing to say stays quiet.
    if (!want.empty()) return nullptr;
    return lines.empty() ? nullptr : &lines.front();
}

// INVENTED: the first quest, in name order, with a line for the unit, for a
// machine with no quest log to ask (the tests).
const DialogLine* DialogBook::find_for_unit(const std::string& unit, const QuestState& quests,
                                            std::string* quest_name) const {
    if (quest_name != nullptr) quest_name->clear();
    if (unit.empty()) return nullptr;

    // Across quests in `by_quest_`'s (a `std::map`: name) order, within a quest find()'s
    // section preference.
    for (const auto& entry : by_quest_) {
        const DialogLine* line = find(entry.first, unit, quests.status(entry.first));
        if (line == nullptr) continue;
        if (quest_name != nullptr) *quest_name = entry.first;
        return line;
    }
    return nullptr;
}

// `CQuestDialogMenu::updateDialogTags`'s tags:
// a `<LOOKAT=NAME>` reported and every `<...>` taken out of the text shown.
// INVENTED: the tag reading is the port's; only `LOOKAT` is acted on.
std::string strip_dialog_directives(const std::string& line, std::string* look_at) {
    if (look_at != nullptr) look_at->clear();
    if (line.find('<') == std::string::npos) return line;

    std::string out;
    out.reserve(line.size());
    for (size_t i = 0; i < line.size();) {
        if (line[i] != '<') {
            out.push_back(line[i++]);
            continue;
        }
        const size_t close = line.find('>', i);
        // An unmatched '<' is ordinary text.
        if (close == std::string::npos) {
            out.push_back(line[i++]);
            continue;
        }

        const std::string directive = line.substr(i + 1, close - i - 1);
        const size_t equals = directive.find('=');
        if (look_at != nullptr && equals != std::string::npos) {
            std::string key = directive.substr(0, equals);
            for (char& c : key) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            if (key == "LOOKAT") *look_at = directive.substr(equals + 1);
        }
        i = close + 1;
    }
    return out;
}

float DialogBook::box_time(const std::string& text, float sound_seconds) {
    // `CQuestDialogMenu::setNPC`: the reveal runs for `max(len * 0.015, 5.0)`
    // seconds.
    constexpr float kSecondsPerCharacter = 0.015f;
    constexpr float kMinimumReveal = 5.0f;
    const float reveal = static_cast<float>(text.size()) * kSecondsPerCharacter;
    float held = reveal > kMinimumReveal ? reveal : kMinimumReveal;
    if (sound_seconds > held) held = sound_seconds;
    return held;
}

}  // namespace tl::core
