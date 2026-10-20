#include "core/quest.h"

#include <algorithm>
#include <cctype>

namespace tl::core {
namespace {

// `STRINGS::StringUpper`, the form quest and unit names are compared in.
// PORT: ASCII upper-casing of the port's narrow strings.
std::string upper(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

/// The quest a gate field names, or empty when the gate is not used. Read by
/// index: the node's `strings` drop empties.
// PORT: asset parsing, a gate field's quest name read at its index.
std::string gate(const tl::formats::CmpNode& node, size_t index) {
    const tl::formats::CmpField* field = node.field(index);
    return field != nullptr ? field->as_text() : std::string();
}

}  // namespace

// PORT: the port's record of each quest's state by name.
QuestStatus QuestState::status(const std::string& quest) const {
    if (quest.empty()) return QuestStatus::Unknown;
    const auto it = by_name_.find(upper(quest));
    return it == by_name_.end() ? QuestStatus::Unknown : it->second;
}

// PORT: the port's record of each quest's state by name.
void QuestState::set(const std::string& quest, QuestStatus status) {
    // "" would match every unauthored gate.
    if (quest.empty()) return;
    by_name_[upper(quest)] = status;
}

// PORT: the port's record of each quest's state by name.
std::vector<std::string> QuestState::names_with(QuestStatus status) const {
    std::vector<std::string> out;
    for (const auto& entry : by_name_) {
        if (entry.second == status) out.push_back(entry.first);
    }
    return out;
}

// PORT: a headless verification switch (`--quest name[:active|:complete]`) parsed.
bool QuestState::apply_specification(const std::string& text) {
    const size_t colon = text.rfind(':');
    if (colon == std::string::npos) {
        // No suffix means complete.
        set(text, QuestStatus::Complete);
        return !text.empty();
    }

    const std::string name = text.substr(0, colon);
    const std::string what = upper(text.substr(colon + 1));
    if (name.empty()) return false;
    if (what == "COMPLETE") {
        set(name, QuestStatus::Complete);
        return true;
    }
    if (what == "ACTIVE") {
        set(name, QuestStatus::Active);
        return true;
    }
    return false;
}

// PORT: whether a `Group` node's quest gates name this quest (which groups a
// quest change has to look at again).
bool group_gates_on(const tl::formats::CmpNode& node, const std::string& quest) {
    if (node.type != tl::formats::cmp_type::kGroup || quest.empty()) return false;
    namespace group = tl::formats::cmp_field::group;
    const std::string wanted = upper(quest);
    for (const size_t index : {group::kComplete, group::kActive, group::kActiveOrComplete,
                               group::kNotComplete, group::kNotActiveOrComplete}) {
        const std::string named = gate(node, index);
        if (!named.empty() && upper(named) == wanted) return true;
    }
    return false;
}

// `CRandomGroup::canGroupBeCreatedBasedOffOfQuests`:
// every authored gate passes. Its first test, an editor flag, is the editor's.
bool group_is_open(const tl::formats::CmpNode& node, const QuestState& quests) {
    if (node.type != tl::formats::cmp_type::kGroup) return true;
    namespace group = tl::formats::cmp_field::group;

    const std::string complete = gate(node, group::kComplete);
    if (!complete.empty() && !quests.is_complete(complete)) return false;

    const std::string active = gate(node, group::kActive);
    if (!active.empty() && !quests.is_active(active)) return false;

    const std::string either = gate(node, group::kActiveOrComplete);
    if (!either.empty() && !(quests.is_active(either) || quests.is_complete(either))) {
        return false;
    }

    const std::string not_complete = gate(node, group::kNotComplete);
    if (!not_complete.empty() && quests.is_complete(not_complete)) return false;

    const std::string neither = gate(node, group::kNotActiveOrComplete);
    if (!neither.empty() && (quests.is_active(neither) || quests.is_complete(neither))) {
        return false;
    }

    return true;
}

}  // namespace tl::core
