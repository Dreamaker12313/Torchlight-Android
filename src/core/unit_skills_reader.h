// A unit's `[SKILL]` blocks as `CBaseUnit::unitInit` adds them to its skill manager.
#pragma once

#include "core/script.h"
#include "formats/adm.h"

#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// `CBaseUnit::unitInit`: every `[SKILL]` block of the
/// unit's merged data group, in order, each found by its upper-cased `NAME`; `not_found`
/// hears of one that is not (`addSkillByName` passes over it).
std::vector<LevelScript::UnitSkill> read_unit_skills(
    const tl::formats::AdmFile& file,
    const std::function<const SkillDef*(const std::string& upper_name)>& skill,
    const std::function<void(const std::string& name)>& not_found = {});

}  // namespace tl::core
