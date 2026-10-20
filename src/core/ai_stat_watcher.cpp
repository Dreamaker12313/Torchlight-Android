#include "core/ai_stat_watcher.h"

#include <cctype>
#include <cmath>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string uppercased(std::string str) {
    for (char& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return str;
}

/// `STRINGS::getStringIndex`: the upper-cased text's index in `names`, or
/// `fallback` when it is not there. Every enum key of the watcher goes through
/// it with the field's current value as the fallback.
uint32_t string_index(const std::string& text, const char* const* names, uint32_t count,
                      uint32_t fallback) {
    const std::string upper = uppercased(text);
    for (uint32_t i = 0; i < count; ++i) {
        if (upper == names[i]) return i;
    }
    return fallback;
}

// `g_AISTAT_TYPE_NAMES`, `g_AISTAT_LOGIC_NAMES`, `g_AISTAT_TARGET_NAMES`,
// `g_AIFLAG_TYPE_NAMES` and `KALIGNMENT_STRINGS`.
const char* const kStatNames[] = {"NONE", "HP", "MANA", "HP PCT", "MANA PCT", "ACTIVE UNITS"};
const char* const kLogicNames[] = {"BELOW", "ABOVE"};
const char* const kTargetNames[] = {"SELF", "FORMATION", "AREA", "AREAUNITTYPES", "AREAFORMATION"};
const char* const kFlagNames[] = {"AWARE",        "BERSERK",          "CANNOT INTERRUPT",
                                  "FRIGHTEN",     "NO LINE OF SIGHT", "NEVER CHANGE TARGET",
                                  "CANNOT TARGET"};
const char* const kAlignmentNames[] = {"NEUTRAL", "GOOD",        "EVIL",       "ALL",
                                       "BERSERK", "EVILBERSERK", "GOODBERSERK"};

/// `GetDataValue(group, name)` then the value word itself,
/// so a number
/// authored as an integer is its bits as a float.
float word_float_or(const formats::AdmSection& section, const char* key, float fallback) {
    const formats::AdmRecord* record = section.find(key);
    return record != nullptr ? record->word_as_float() : fallback;
}

bool word_bool_or(const formats::AdmSection& section, const char* key, bool fallback) {
    const formats::AdmRecord* record = section.find(key);
    return record != nullptr ? record->word_first_byte() != 0 : fallback;
}

}  // namespace

// CAIStatWatcher::CAIStatWatcher
std::vector<AIStatWatcherDef> parse_ai_stat_watchers(const formats::AdmSection& ai_section,
                                                     const UnitTypeHierarchy& hierarchy) {
    std::vector<AIStatWatcherDef> out;
    for (const formats::AdmSection& child : ai_section.children) {
        if (child.name != "STAT") continue;
        AIStatWatcherDef def;
        // Each enum key is upper-cased and looked up with the field's current
        // value as the fallback; `STAT`'s fallback is 0 (`NONE`).
        def.stat = static_cast<AIStatType>(
            string_index(child.string_or("STAT", "NONE"), kStatNames, 6, 0));
        def.logic = static_cast<AIStatLogic>(string_index(
            child.string_or("LOGIC", "BELOW"), kLogicNames, 2, static_cast<uint32_t>(def.logic)));

        // `FLAG` and `SKILL` are lists, every value in file order. An unknown
        // flag name falls back to index 0 and is still appended: it reads as
        // `AWARE`. A skill is kept whatever its text (a `CAISkill` per value).
        for (const formats::AdmRecord& rec : child.records) {
            if (rec.key == "FLAG") {
                def.flags.push_back(static_cast<AIFlag>(string_index(rec.text, kFlagNames, 7, 0)));
            }
        }
        for (const formats::AdmRecord& rec : child.records) {
            if (rec.key == "SKILL") def.skills.push_back(rec.text);
        }

        def.target = static_cast<AIStatTarget>(string_index(
            child.string_or("TARGET", "SELF"), kTargetNames, 5, static_cast<uint32_t>(def.target)));
        def.target_alignment = string_index(child.string_or("TARGET_ALIGNMENT", "ALL"),
                                            kAlignmentNames, 7, def.target_alignment);
        // `CHierarchy::getTypeIDByName`; `ANY` is type 0 in `UNITTYPES.HIE`.
        const std::string tut = uppercased(child.string_or("TARGETUNITTYPE", "ANY"));
        if (tut != "ANY") def.target_unittype = hierarchy.id_of(tut);

        def.target_area = word_float_or(child, "TARGETAREA", def.target_area);
        def.duration = word_float_or(child, "DURATION", def.duration);
        def.value = word_float_or(child, "VALUE", def.value);

        def.only_once = word_bool_or(child, "ONLYONCE", def.only_once);
        // Quirk: ONLYONCE false with VALUE exactly 0 becomes true.
        if (!def.only_once && def.value == 0.0f) {
            def.only_once = true;
        }

        def.animation = uppercased(child.string_or("ANIMATION", ""));
        def.enable_on_event = word_bool_or(child, "ENABLE_ON_EVENT", def.enable_on_event);
        def.include_living = word_bool_or(child, "INCLUDE_LIVING", def.include_living);
        def.include_dead = word_bool_or(child, "INCLUDE_DEAD", def.include_dead);

        out.push_back(std::move(def));
    }
    return out;
}

// PORT: the watchers parsed with no unit types to resolve against.
std::vector<AIStatWatcherDef> parse_ai_stat_watchers(const formats::AdmSection& ai_section) {
    static const UnitTypeHierarchy kEmptyHierarchy;
    return parse_ai_stat_watchers(ai_section, kEmptyHierarchy);
}

// PORT: a unit's `[AI]` block found and parsed, once.
void parse_unit_ai_watchers(std::vector<AIStatWatcherDef>& out, const formats::AdmSection& root,
                            const UnitTypeHierarchy& hierarchy) {
    if (!out.empty()) return;
    for (const formats::AdmSection& child : root.children) {
        if (child.name == "AI") {
            out = parse_ai_stat_watchers(child, hierarchy);
            break;
        }
    }
}

// CAIStatWatcher::CAIStatWatcher
AIStatWatcher::AIStatWatcher(AIStatWatcherDef def, AIStatWatcherHooks hooks)
    : def_(std::move(def)), hooks_(std::move(hooks)) {
    for (const std::string& skill_name : def_.skills) {
        skills_.push_back(std::make_unique<AISkill>(skill_name));
    }
}

// CAIStatWatcher::update
void AIStatWatcher::update(float dt) {
    // if ONLYONCE is set and owner is not alive, return early.
    if (def_.only_once && hooks_.alive && !hooks_.alive()) return;

    // evaluate stat value.
    if (def_.stat == AIStatType::None) return;

    float stat_val = 0.0f;
    switch (def_.stat) {
        case AIStatType::Hp: stat_val = hooks_.hp ? static_cast<float>(hooks_.hp()) : 0.0f; break;
        case AIStatType::Mana:
            stat_val = hooks_.mana ? static_cast<float>(hooks_.mana()) : 0.0f;
            break;
        case AIStatType::HpPct: {
            const int32_t hp = hooks_.hp ? hooks_.hp() : 0;
            const int32_t max_hp = hooks_.max_hp ? hooks_.max_hp() : 1;
            stat_val = (static_cast<float>(hp) / static_cast<float>(max_hp)) * 100.0f;
            break;
        }
        case AIStatType::ManaPct: {
            const int32_t mana = hooks_.mana ? hooks_.mana() : 0;
            const int32_t max_mana = hooks_.max_mana ? hooks_.max_mana() : 1;
            stat_val = (static_cast<float>(mana) / static_cast<float>(max_mana)) * 100.0f;
            break;
        }
        case AIStatType::ActiveUnits: {
            // checks character has level and character count > 0; 0.0f if no level.
            if (hooks_.has_level && hooks_.has_level()) {
                stat_val = hooks_.active_units_count
                               ? hooks_.active_units_count(def_.target_unittype,
                                                           def_.target_alignment, def_.target_area,
                                                           def_.include_living, def_.include_dead)
                               : 0.0f;
            } else {
                stat_val = 0.0f;
            }
            break;
        }
        default: stat_val = 0.0f; break;
    }

    // threshold comparison, both BELOW and ABOVE inclusive; a NaN
    // meets neither.
    bool condition_met = false;
    if (!std::isnan(stat_val) && !std::isnan(def_.value)) {
        if (def_.logic == AIStatLogic::Below) {
            condition_met = (stat_val <= def_.value);
        } else {
            condition_met = (stat_val >= def_.value);
        }
    }

    if (condition_met) {
        if (def_.enable_on_event) {
            add_flag();
            add_skill();
        } else {
            remove_flag();
            remove_skill();
        }
    } else {
        // check if duration is exactly zero.
        // If duration != 0.0f or NaN, flags expire on their own; no removal performed.
        if (!std::isnan(def_.duration) && def_.duration == 0.0f) {
            if (def_.enable_on_event) {
                remove_flag();
                remove_skill();
            } else {
                add_flag();
                add_skill();
            }
        }
    }
}

// CAIStatWatcher::addFlag
void AIStatWatcher::add_flag() {
    if (has_fired_ && def_.only_once) return;

    std::vector<int64_t> targets;
    get_targets(targets);
    if (targets.empty()) {
        has_fired_ = true;
        return;
    }

    // Outer loop over targets, inner loop over watcher's flag list.
    for (int64_t target_id : targets) {
        AIFlags* target_flags =
            hooks_.find_flags_by_id ? hooks_.find_flags_by_id(target_id) : nullptr;
        if (!target_flags) continue;

        bool target_modified = false;
        for (AIFlag flag : def_.flags) {
            if (target_flags->has(flag)) continue;
            target_flags->add(flag, def_.duration);
            target_modified = true;
        }

        if (target_modified) {
            recorded_target_guids_.push_back(target_id);
            if (!def_.animation.empty() && hooks_.play_animation) {
                hooks_.play_animation(target_id, def_.animation);
            }
        }
    }
    has_fired_ = true;
}

// CAIStatWatcher::removeFlag
void AIStatWatcher::remove_flag() {
    for (int64_t target_id : recorded_target_guids_) {
        AIFlags* target_flags =
            hooks_.find_flags_by_id ? hooks_.find_flags_by_id(target_id) : nullptr;
        if (!target_flags) continue;
        for (AIFlag flag : def_.flags) {
            target_flags->remove(flag);
        }
    }
    recorded_target_guids_.clear();
    // Note: does NOT clear has_fired_
}

// CAIStatWatcher::addSkill
void AIStatWatcher::add_skill() {
    if (has_fired_ && def_.only_once) return;

    bool any_added = false;
    for (const auto& skill : skills_) {
        if (!skill) continue;
        if (hooks_.has_ai_skill && hooks_.has_ai_skill(skill.get())) continue;
        if (hooks_.add_ai_skill) {
            hooks_.add_ai_skill(skill.get(), def_.duration);
        }
        any_added = true;
    }

    if (any_added && !def_.animation.empty() && hooks_.play_animation) {
        hooks_.play_animation(hooks_.owner_id ? hooks_.owner_id() : 0, def_.animation);
    }
    has_fired_ = true;
}

// CAIStatWatcher::removeSkill
void AIStatWatcher::remove_skill() {
    for (const auto& skill : skills_) {
        if (!skill) continue;
        if (hooks_.remove_ai_skill) {
            hooks_.remove_ai_skill(skill.get());
        }
    }
    // Note: does NOT clear has_fired_
}

// CAIStatWatcher::getTargets
void AIStatWatcher::get_targets(std::vector<int64_t>& out) {
    out.clear();
    switch (def_.target) {
        case AIStatTarget::Self:
            if (hooks_.owner_id) out.push_back(hooks_.owner_id());
            break;
        case AIStatTarget::Formation:
            if (hooks_.formation_units) {
                for (int64_t id : hooks_.formation_units()) out.push_back(id);
            }
            break;
        case AIStatTarget::Area:
            if (hooks_.area_targets) {
                for (int64_t id : hooks_.area_targets(def_.target_area, 0)) out.push_back(id);
            }
            break;
        case AIStatTarget::AreaUnitTypes:
            if (hooks_.area_targets) {
                for (int64_t id : hooks_.area_targets(def_.target_area, def_.target_unittype))
                    out.push_back(id);
            }
            break;
        case AIStatTarget::AreaFormation:
            if (hooks_.area_formation_targets) {
                for (int64_t id : hooks_.area_formation_targets(def_.target_area))
                    out.push_back(id);
            }
            break;
    }
}

}  // namespace tl::core
