// Quest state, and the level gates that read it. Quest names are compared
// case-insensitively. No OGRE and no file I/O.
#pragma once

#include "formats/cmp.h"

#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// How far the player has got with one quest; `Unknown` is "never mentioned".
enum class QuestStatus {
    Unknown,
    Active,
    Complete,
};

/// What the player has done, by quest name.
class QuestState {
public:
    QuestStatus status(const std::string& quest) const;

    // PORT: the port's record of each quest's state by name.
    bool is_active(const std::string& quest) const { return status(quest) == QuestStatus::Active; }
    // PORT: the port's record of each quest's state by name.
    bool is_complete(const std::string& quest) const {
        return status(quest) == QuestStatus::Complete;
    }

    /// Set a quest's state. An empty name is ignored, because an unauthored
    /// gate field compiles to the empty string and must not become a quest.
    void set(const std::string& quest, QuestStatus status);

    // PORT: the port's record of each quest's state by name.
    void activate(const std::string& quest) { set(quest, QuestStatus::Active); }
    // PORT: the port's record of each quest's state by name.
    void complete(const std::string& quest) { set(quest, QuestStatus::Complete); }

    /// Every quest in this state, in name order.
    std::vector<std::string> names_with(QuestStatus status) const;

    size_t size() const { return by_name_.size(); }

    /// Apply a `NAME` or `NAME:active` / `NAME:complete` specification, as the
    /// command line spells it. False when the suffix is not one of the two.
    bool apply_specification(const std::string& text);

private:
    /// Keyed by the upper-cased name.
    std::map<std::string, QuestStatus> by_name_;
};

/// Whether a `Group` node's subtree should be placed, given the quest state.
/// A node that is not a `Group`, or a group that names no quest, is always open.
bool group_is_open(const tl::formats::CmpNode& node, const QuestState& quests);

/// Whether a `Group` node names `quest` in any of its five gate fields, without
/// case: the groups that were waiting on it (`Renderer::Impl::report_gates_waiting_on`).
bool group_gates_on(const tl::formats::CmpNode& node, const std::string& quest);

}  // namespace tl::core
