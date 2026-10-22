#include "core/section_profile.h"

namespace tl::core {

SectionTotals& section_totals() {
    static SectionTotals totals;
    return totals;
}

const char* section_name(Section section) {
    switch (section) {
        case Section::Update: return "update";
        case Section::Animation: return "animation";
        case Section::UnitKeys: return "unit keys";
        case Section::Particles: return "particles";
        case Section::Spawners: return "spawners";
        case Section::Missiles: return "missiles";
        case Section::Disposal: return "disposal";
        case Section::Fight: return "fight";
        case Section::EffectsSkills: return "effects+skills";
        case Section::Script: return "script";
        case Section::ScriptEvents: return "script events";
        case Section::Characters: return "characters";
        case Section::Ai: return "ai";
        case Section::Pathing: return "pathing";
        case Section::Motion: return "motion";
        case Section::Count: break;
    }
    return "?";
}

SectionScope::~SectionScope() {
    const auto spent = std::chrono::steady_clock::now() - start_;
    SectionTotals& totals = section_totals();
    const int i = static_cast<int>(section_);
    totals.ns[i] +=
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(spent).count());
    ++totals.calls[i];
}

}  // namespace tl::core
