#include "core/logic_names.h"

#include <cstddef>

namespace tl::core {

namespace {

/// The index of `name` in `names`, the first that is equal, or 0.
template <size_t N>
int32_t id_by_string(const char* const (&names)[N], const std::string& name) {
    for (size_t i = 0; i < N; ++i) {
        if (name == names[i]) return static_cast<int32_t>(i);
    }
    return 0;
}

}  // namespace

int32_t loop_type_id_by_string(const std::string& name) {
    static const char* const kNames[] = {"No Loop", "Cycle", "Back and Forth"};
    return id_by_string(kNames, name);
}

int32_t counter_type_id_by_string(const std::string& name) {
    static const char* const kNames[] = {
        "Activate only once",        "Activate and reset",
        "Activate on each add",      "Activate on each add and reset",
        "Activate on each subtract", "Activate on each subtract and reset"};
    return id_by_string(kNames, name);
}

int32_t choice_type_id_by_string(const std::string& name) {
    static const char* const kNames[] = {"WEIGHT", "RANDOM"};
    return id_by_string(kNames, name);
}

int32_t timeline_mod_id_by_string(const std::string& name) {
    static const char* const kNames[] = {"None", "Set", "Multiply"};
    return id_by_string(kNames, name);
}

int32_t game_state_id_by_string(const std::string& name) {
    static const char* const kNames[] = {
        "NONE",
        "PAUSE PLAYER",
        "PAUSE GAME",
        "INTERACTIVE UI(BARS AT TOP AND BOTTOM)",
        "HIDE PLAYER",
        "DISABLE SKILLS",
        "CLOSE INGAME UI",
    };
    return id_by_string(kNames, name);
}

uint32_t random_group_type_from_string(const std::string& name) {
    static const char* const kNames[] = {"ALL", "Weight", "Random Chance"};
    return static_cast<uint32_t>(id_by_string(kNames, name));
}

uint32_t difficulty_id_by_string(const std::string& name) {
    if (name.empty()) return 0xe;
    if (name == "EASY") return 0;
    if (name == "NORMAL") return 1;
    if (name == "HARD") return 2;
    if (name == "VERY HARD") return 3;
    return 0xe;
}

}  // namespace tl::core
