// The stat lines of a skill level and the stat bonuses it takes: the effects half of the
// skill screen's description. `CSkillProperty::getSkillStats`
// and `CSkillProperty::fillOutStatBonuses`.
#include "core/runic_strings.h"
#include "core/skill_text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tl::core {
namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return text;
}

/// `gSKILL_EVENT_TYPE_NAMES`, in the order `CSkillProperty`'s constructor
/// allocates the eleven event groups. `getSkillStats` walks the groups in this
/// order, so it is the order the lines come out in - not file order.
const char* const kEventGroupNames[] = {
    "EVENT_START",       "EVENT_END",       "EVENT_TRIGGER",     "EVENT_TRIGGER_TWO",
    "EVENT_UNITHIT",     "EVENT_UNITDIE",   "EVENT_MISSILEHIT",  "EVENT_MISSILEDIE",
    "EVENT_DIEBYEFFECT", "EVENT_CASTERDIE", "EVENT_UNIT_CREATE",
};
constexpr int kEventGroupCount = 11;

/// One `[EVENT_*]` block as the description needs it (`STATSHIDDEN`, `USEDPS`,
/// `WEAPONDAMAGEPCT`/100, the effect-and-affixes block); every event is read.
/// PORT: the fields gathered for the description.
struct DisplayEvent {
    bool stats_hidden = false;
    bool use_dps = false;
    float weapon_damage = 0.0f;
    SkillEffectAndAffixes block;
    /// `+0x110`/`+0x118`: the `EXECUTE_SKILL` names, which print their own
    /// skill's stats under "Secondary Effects".
    std::vector<std::string> execute_skills;
};

/// The property's events, grouped and ordered as `CSkillProperty`'s constructor
/// leaves them.
std::vector<DisplayEvent> property_events(const tl::formats::AdmSection& section,
                                          const EffectTypeTable& types) {
    std::vector<DisplayEvent> events;
    for (int group = 0; group < kEventGroupCount; ++group) {
        for (const tl::formats::AdmSection& child : section.children) {
            if (upper(child.name) != kEventGroupNames[group]) continue;
            DisplayEvent event;
            event.stats_hidden = child.bool_or("STATSHIDDEN", false);
            event.use_dps = child.bool_or("USEDPS", false);
            // `CSkillEvent::CSkillEvent`: times 0.01f.
            const float pct = child.float_or("WEAPONDAMAGEPCT", 0.0f) * 0.01f;
            event.weapon_damage = pct > 0.0f ? pct : 0.0f;
            event.block = parse_skill_effect_and_affixes(child, types);
            // `[EXECUTE_SKILL]` is a block, not a key: `CExecuteSkillProps`'s constructor
            // reads one `SKILL` out of it.
            for (const tl::formats::AdmSection& props : child.children) {
                if (upper(props.name) != "EXECUTE_SKILL") continue;
                // Every block counts (`+0xa4`), named or not.
                event.execute_skills.push_back(props.string_or("SKILL", ""));
            }
            events.push_back(std::move(event));
        }
    }
    return events;
}

/// `CSkillEvent::getDisplayStats(CSkillProperty*)`: the event's own
/// block, unless `STATSHIDDEN`.
std::string event_display_stats(const DisplayEvent& event, const EffectDisplayContext& effects) {
    if (event.stats_hidden) return std::string();
    return skill_effect_and_affixes_display_stats(event.block, effects);
}

/// DIVERGENCE: the original's secondary-effects recursion
/// (`CExecuteSkillProps::getSkillStats`) has no bound, so a skill executing itself never
/// returns; the cap keeps a mod's cycle from hanging.
constexpr int kMaxSecondaryDepth = 4;

/// `CSkillProperty::getSkillStats`, over one
/// property's section.
std::string stats_for_section(const tl::formats::AdmSection& section,
                              const tl::formats::AdmSection& event_section, uint32_t number,
                              const EffectDisplayContext& effects, int depth) {
    if (effects.types == nullptr || effects.graphs == nullptr) return std::string();

    // The four headings, the keys `CStringTranslate::getTranslateString` is called with
    //;
    // an untranslated key is the English text (trailing space on `Inflicts `).
    const std::string g_weapon_damage = "Weapon Damage";
    const std::string g_weapon_dps = "of Weapon DPS";
    const std::string g_inflicts = "Inflicts ";
    const std::string g_secondary_effects = "Secondary Effects";

    std::string stats;
    bool appended = false;

    // 1. The property's own effect-and-affixes block (+0x20).
    if (has_skill_effect_and_affixes(section)) {
        stats += skill_effect_and_affixes_display_stats(
            parse_skill_effect_and_affixes(section, *effects.types), effects);
        appended = !stats.empty();
    }

    // The property's event lists (`+0x34`): its own events, or its parent's
    // when it has none, the pre-built clones skipped
    // (`+0x118`).
    const std::vector<DisplayEvent> events = property_events(event_section, *effects.types);

    // 2. The weapon-damage line, from the **first** event that carries a
    //    `WEAPONDAMAGEPCT`: the original breaks out of both loops there, so a
    //    second one is never printed.
    for (const DisplayEvent& event : events) {
        if (event.weapon_damage == 0.0f) continue;
        if (appended) stats += "\n";
        const std::string number = value_as_string(event.weapon_damage * 100.0f);
        if (event.use_dps) {
            stats = stats + g_inflicts + number + "% " + g_weapon_dps + "\n";
        } else {
            stats = stats + g_weapon_damage + ": " + number + "%\n";
        }
        appended = false;
        break;
    }

    // 3. Each event's own lines.
    for (const DisplayEvent& event : events) {
        const std::string line = event_display_stats(event, effects);
        if (line.empty()) continue;
        if (appended) stats += "\n";
        stats += line;
        appended = true;
    }
    if (!stats.empty() && stats.back() == '\n') stats.pop_back();

    // 4. The "Secondary Effects" header, when any visible event executes a
    //    skill of its own.
    bool any_secondary = false;
    for (const DisplayEvent& event : events) {
        if (!event.execute_skills.empty()) any_secondary = true;
    }
    if (any_secondary && !stats.empty()) {
        stats = stats + "\n  -" + g_secondary_effects + "-";
    }

    // 5. `CExecuteSkillProps::getSkillStats`: for each event not
    //    a clone and not `STATSHIDDEN`, each executed skill on its own line, described at this
    //    property's own number (so `[LEVEL3]` prints its `[LEVEL3]`). With no owner or no
    //    skill manager the line is empty but the line break still goes on.
    if (depth < kMaxSecondaryDepth) {
        for (const DisplayEvent& event : events) {
            if (event.stats_hidden) continue;
            for (const std::string& name : event.execute_skills) {
                const SkillProperty* executed = effects.skill_level_property
                                                    ? effects.skill_level_property(name, number)
                                                    : nullptr;
                std::string line;
                if (executed != nullptr && executed->section != nullptr) {
                    line = stats_for_section(*executed->section, executed->event_section(),
                                             executed->number, effects, depth + 1);
                }
                stats = stats + "\n" + line;
            }
        }
    }
    return stats;
}

}  // namespace

std::string skill_stats_text(const SkillProperty& property, const EffectDisplayContext& effects) {
    // `CSkillProperty::getSkillStats`.
    if (property.section == nullptr) return std::string();
    return stats_for_section(*property.section, property.event_section(), property.number, effects,
                             0);
}

std::array<float, kEffectStatCount> skill_stat_bonuses_all(const KnownSkill& skill,
                                                           const EffectDisplayContext& effects) {
    // `CSkillProperty::fillOutStatBonuses(float (&)[6], bool)`: the
    // property's own block and then every event's, each with the max rule, so
    // several effects on one skill combine rather than overwrite.
    std::array<float, kEffectStatCount> out{};
    if (effects.types == nullptr) return out;
    const SkillProperty* property = skill.property(skill.effective);
    if (property == nullptr || property->section == nullptr) return out;

    const SkillEffectAndAffixes own =
        parse_skill_effect_and_affixes(*property->section, *effects.types);
    skill_effect_and_affixes_fill_out_stat_bonuses(own, effects, out, true);
    for (const DisplayEvent& event : property_events(property->event_section(), *effects.types)) {
        skill_effect_and_affixes_fill_out_stat_bonuses(event.block, effects, out, true);
    }
    return out;
}

std::array<float, kSkillStatBonusCount> skill_stat_bonuses(const KnownSkill& skill,
                                                           const EffectDisplayContext& effects) {
    // Which four of the six the screen shows, in order: indices 0, 1, 3, 2,
    // `MELEE` (strength), `RANGED` (dexterity), `MAGIC`, `DEFENSE`.
    const std::array<float, kEffectStatCount> all = skill_stat_bonuses_all(skill, effects);
    return {
        all[static_cast<size_t>(EffectStat::Melee)], all[static_cast<size_t>(EffectStat::Ranged)],
        all[static_cast<size_t>(EffectStat::Magic)], all[static_cast<size_t>(EffectStat::Defense)]};
}

}  // namespace tl::core
