// A unit's `[SKILL]` blocks (see the header).
#include "core/unit_skills_reader.h"

#include "core/level_memory.h"

namespace tl::core {

// `CBaseUnit::unitInit`.
std::vector<LevelScript::UnitSkill> read_unit_skills(
    const tl::formats::AdmFile& file,
    const std::function<const SkillDef*(const std::string& upper_name)>& skill,
    const std::function<void(const std::string& name)>& not_found) {
    std::vector<LevelScript::UnitSkill> out;
    if (!file.ok) return out;
    // `LEVEL`, `CHARGES` and `ENABLED` off the unit are each block's default; `LEVEL`'s own
    // default is the unit's level, which `LevelScript::load_unit_skills` puts in.
    const bool unit_level = file.root.find("LEVEL") != nullptr;
    const uint32_t level_default = file.root.integer_or("LEVEL", 0);
    const uint32_t charges_default = file.root.integer_or("CHARGES", 0xffffffffu);
    const bool enabled_default = file.root.bool_or("ENABLED", true);
    for (const tl::formats::AdmSection& group : file.root.children) {
        if (upper_ascii(group.name) != "SKILL") continue;
        const std::string name = group.string_or("NAME", "");
        const SkillDef* def = skill ? skill(upper_ascii(name)) : nullptr;
        if (def == nullptr) {
            if (not_found) not_found(name);
            continue;
        }
        LevelScript::UnitSkill s;
        s.known.def = def;
        s.known.entry.name = def->name;
        // `setSkillLevel(LEVEL)`; a monster's effective level is that level.
        s.level_from_unit = !unit_level && group.find("LEVEL") == nullptr;
        const uint32_t level = group.integer_or("LEVEL", level_default);
        s.known.invested = level;
        s.known.effective = level;
        s.known.enabled = group.bool_or("ENABLED", enabled_default);
        s.cast_chance = static_cast<int32_t>(group.integer_or("CHANCE", 0x28));
        s.cancel_chance = static_cast<int32_t>(group.integer_or("CANCEL_CHANCE", 0));
        s.known.charges = static_cast<int32_t>(group.integer_or("CHARGES", charges_default));
        out.push_back(std::move(s));
    }
    return out;
}

}  // namespace tl::core
