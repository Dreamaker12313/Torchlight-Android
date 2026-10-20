#include "core/weapon_procs.h"

#include "core/item_effects.h"
#include "core/stats.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

/// The character's own casts that write `+0x708` first: the bits
/// of 0x80000083 over `type - 0x4a`.
bool sets_lockout(int type) {
    const unsigned offset = static_cast<unsigned>(type - kEffectCastSkill);
    return offset < 0x20u && ((1ull << offset) & 0x80000083ull) != 0;
}

/// One list's rolls, as both `executeProcs` walk it: each effect of `type`
/// draws, and wins when `value(0)` is not below the draw. By index, the count
/// read again each time round, and the effect copied before `found`
/// runs: a cast can add to the list it is walking.
template <typename Found>
void roll_list(const std::vector<LiveEffect>& list, int type, const StatBook& graphs,
               RunicRandom& rolls, const Found& found) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].effect.type != type) continue;
        const Effect effect = list[i].effect;
        const int roll = rolls.integer_between(0, 100);
        if (effect_column_value(effect, 0, graphs) < static_cast<float>(roll)) continue;
        found(effect);
    }
}

}  // namespace

bool effect_teaches_skill(int type) {
    // CBaseUnit::activateEffect
    const unsigned offset = static_cast<unsigned>(type - kEffectCastSkill);
    return offset < 0x20u && ((1ull << offset) & 0x80000087ull) != 0;
}

uint32_t skill_level_from_manager(uint32_t level, uint32_t level_count) {
    // CSkill::_setLevelOfSkillFromSkillManager
    //
    if (level == 0xffffffffu) return 1;
    const uint32_t most = level_count != 0 ? level_count : 1;
    return most < level ? most : level;
}

SkillCopy* SkillCopies::add(const std::string& name, uint32_t level, const SkillFileLookup& files) {
    // CBaseUnit::addSkillByName
    const std::optional<SkillFileFacts> file = files ? files(name) : std::nullopt;
    if (!file) return nullptr;
    const std::string key = upper(file->name);
    SkillCopy* copy = nullptr;
    if (one_per_name_) {
        for (SkillCopy& held : copies_) {
            if (upper(held.name) == key) copy = &held;
        }
    }
    if (copy == nullptr) {
        // CSkillManager::addSkill:
        // `CSkill::CSkill` of the template,
        // its `HIDDEN` and `CHARGES`, then onto the known list.
        SkillCopy made;
        made.name = file->name;
        made.hidden = file->hidden;
        made.charges = file->charges;
        copies_.push_back(made);
        copy = &copies_.back();
    }
    // CSkillManager::setSkillLevel: every copy of the name.
    const uint32_t taken = skill_level_from_manager(level, file->level_count);
    for (SkillCopy& held : copies_) {
        if (upper(held.name) == key) held.level = taken;
    }
    return copy;
}

const SkillCopy* SkillCopies::find(const std::string& name, uint32_t level) const {
    // CSkillManager::getSkill
    const std::string key = upper(name);
    for (const SkillCopy& held : copies_) {
        if (upper(held.name) == key && held.level == level) return &held;
    }
    return nullptr;
}

void activate_effect(Effect& effect, SkillCopies& skills, const StatBook& graphs,
                     const SkillFileLookup& files) {
    // CBaseUnit::activateEffect
    if (!effect_teaches_skill(effect.type)) return;
    // the level read before anything, then `(int)value(1)`, 0 being -1.
    const uint32_t level = effect.level;
    int32_t charges = static_cast<int32_t>(effect_column_value(effect, 1, graphs));
    if (charges == 0) charges = -1;
    SkillCopy* copy = skills.add(effect.name, level, files);
    if (copy == nullptr) return;
    copy->charges = charges;
    // `getSkillLevel(copy)` into `+0x10`; -1 is 0, and so is anything above 1000.
    const uint32_t taken = copy->level;
    effect.level = (taken == 0xffffffffu || 1000u < taken) ? 0u : taken;
}

void execute_procs(int type, const ProcHolder& character, const std::array<ProcHolder, 12>& worn,
                   const StatBook& graphs, RunicRandom& rolls,
                   const std::function<void(const ProcCast&)>& cast) {
    // CCharacter::executeProcs
    if (character.effects != nullptr && character.skills != nullptr &&
        character.effects->has_effect(type)) {
        const unsigned offset = static_cast<unsigned>(type - kEffectCastSkill);
        for (EffectActivation list : {EffectActivation::Passive, EffectActivation::Dynamic}) {
            roll_list(
                character.effects->effects(list), type, graphs, rolls, [&](const Effect& effect) {
                    const SkillCopy* skill = character.skills->find(effect.name, effect.level);
                    // nothing for a type outside 0x4a..0x69.
                    if (skill == nullptr || 0x1fu < offset) return;
                    ProcCast out;
                    out.skill = skill;
                    if (sets_lockout(type)) {
                        out.lockout = true;
                    } else if (type == kEffectCastSkillAtTarget) {
                        out.from_target = true;
                    } else {
                        return;
                    }
                    cast(out);
                });
        }
    }
    // CInventory::executeProcs: each equip location in turn,
    // its three lists. `CEquipment::executeProcs` is inlined on the console
    // (a copy specialised to type 0x4a).
    for (int location = 0; location < 12; ++location) {
        const ProcHolder& item = worn[static_cast<size_t>(location)];
        if (item.effects == nullptr || item.skills == nullptr || !item.effects->has_effect(type)) {
            continue;
        }
        for (EffectActivation list :
             {EffectActivation::Passive, EffectActivation::Dynamic, EffectActivation::Transfer}) {
            roll_list(item.effects->effects(list), type, graphs, rolls, [&](const Effect& effect) {
                const SkillCopy* skill = item.skills->find(effect.name, effect.level);
                if (skill == nullptr) return;
                ProcCast out;
                out.skill = skill;
                out.location = location;
                cast(out);
            });
        }
    }
}

SkillCopies weapon_known_skills(const ItemStats& item, const EffectManager::Context& context,
                                const StatBook& graphs, const SkillFileLookup& files) {
    // CBaseUnit::unitInit and
    // CBaseUnit::activateEffect, as far as
    // the known list.
    SkillCopies skills;
    weapon_proc_effects(item, context, graphs, files, &skills);
    return skills;
}

EffectManager weapon_proc_effects(const ItemStats& item, const EffectManager::Context& context,
                                  const StatBook& graphs, const SkillFileLookup& files,
                                  SkillCopies* skills) {
    SkillCopies local;
    SkillCopies& known = skills != nullptr ? *skills : local;
    EffectManager::Context taught = context;
    taught.on_activate = [&known, &graphs, &files](Effect& effect) {
        activate_effect(effect, known, graphs, files);
    };
    EffectManager manager = item_effect_manager(item, taught, [&item, &known, &files]() {
        // CBaseUnit::unitInit: each
        // `[SKILL]`, `addSkillByName(NAME, true)`, `ENABLED` onto `+0x6d`, then
        // `setSkillLevel` at `LEVEL`.
        for (const ItemStats::Skill& block : item.skills) {
            SkillCopy* copy = known.add(block.name, block.level, files);
            if (copy != nullptr) copy->enabled = block.enabled;
        }
    });
    // The hook holds this function's locals; the manager goes back without it.
    manager.set_context(context);
    return manager;
}

WeaponSkillPoints weapon_skill_points(const Vec3& hand, const Vec3& position, float radius,
                                      const Vec3& forward) {
    // CCharacter::doWeaponSkill
    const float dx = hand.x - position.x;
    const float dz = hand.z - position.z;
    float reach = std::sqrt(dx * dx + 0.0f + dz * dz);
    const float least = 0.6f * radius;
    if (!(least < reach)) reach = least;  // `maxss`
    WeaponSkillPoints out;
    out.start = Vec3{forward.x * reach + position.x, (forward.y * reach + position.y) + 0.5f,
                     forward.z * reach + position.z};
    out.aim = Vec3{10.0f * forward.x + out.start.x, 10.0f * forward.y + out.start.y,
                   10.0f * forward.z + out.start.z};
    return out;
}

bool weapon_skill_runs(const SkillCopy& skill) {
    // CCharacter::doWeaponSkill, its test at
    //
    return skill.enabled && !skill.hidden;
}

Vec3 weapon_proc_point(const Vec3& attacker, const Vec3& target, float target_radius) {
    // CCharacter::doWeaponProcs
    float x = attacker.x - target.x;
    float y = 0.0f;
    float z = attacker.z - target.z;
    const float length = std::sqrt(x * x + 0.0f + z * z);
    if (1e-08 < static_cast<double>(length)) {
        const float inverse = 1.0f / length;
        x *= inverse;
        y *= inverse;
        z *= inverse;
    }
    return Vec3{(x * target_radius + target.x) + 0.0f * 1.25f,
                (y * target_radius + target.y) + 1.0f * 1.25f,
                (z * target_radius + target.z) + 0.0f * 1.25f};
}

}  // namespace tl::core
