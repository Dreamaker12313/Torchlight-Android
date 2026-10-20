#include "core/unit_themes.h"

#include "core/random.h"
#include "core/skill_animation.h"

#include <cctype>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string to_upper(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}

}  // namespace

UnitThemeParticle UnitThemeParticle::load(const tl::formats::AdmSection& block) {
    // `CUnitThemeParticle::load`.
    UnitThemeParticle particle;
    particle.file = block.string_or("FILE", "");
    particle.count = block.integer_or("COUNT", 1);
    particle.unit_type = block.string_or("UNITTYPE", "ANY");
    particle.attaches = block.bool_or("ATTACHES", true);
    // `FOLLOWS` defaults to `!attaches` when not authored:
    particle.follows = block.bool_or("FOLLOWS", !particle.attaches);
    particle.bone = block.string_or("BONE", "");
    return particle;
}

void UnitThemeBook::reload(const std::vector<const tl::formats::AdmFile*>& files) {
    // `CUnitThemes::reload`: one theme per file with a `NAME`,
    // in list order. `media/UNITTHEMES.DAT` is not one of them.
    themes_.clear();
    for (const auto* file : files) {
        if (!file) continue;
        UnitTheme theme;
        theme.name = to_upper(file->root.string_or("NAME", ""));
        if (theme.name.empty()) continue;
        // DIVERGENCE: no `GUID` keeps -1; the original draws one (`UTILITIES::createUniqueGuid`)
        // and writes the file back (`CDataGroup::SaveToFile`). The port never writes game data.
        theme.guid = file->root.guid_or("GUID", -1);
        for (const auto& child : file->root.children) {
            if (child.name == "PARTICLE") {
                theme.particles.push_back(UnitThemeParticle::load(child));
            }
        }
        themes_.push_back(std::move(theme));
    }
}

void UnitThemeBook::reload(const std::vector<tl::formats::AdmFile>& files) {
    std::vector<const tl::formats::AdmFile*> ptrs;
    ptrs.reserve(files.size());
    for (const auto& f : files) {
        ptrs.push_back(&f);
    }
    reload(ptrs);
}

// `CUnitThemeManager`'s lookup by name or GUID. PORT: over the port's list.
const UnitTheme* UnitThemeBook::get_unit_theme(const std::string& name) const {
    for (const auto& theme : themes_) {
        if (theme.name == name) return &theme;
    }
    return nullptr;
}

// `CUnitThemeManager`'s lookup by name or GUID. PORT: over the port's list.
const UnitTheme* UnitThemeBook::get_unit_theme(int64_t guid) const {
    for (const auto& theme : themes_) {
        if (theme.guid == guid) return &theme;
    }
    return nullptr;
}

bool UnitThemeSet::add_unit_theme(const UnitTheme* theme) {
    // `CBaseUnit::addUnitTheme(CUnitTheme*)`.
    // 1. Checks own (+0x138); if already present, duplicate refused.
    for (const auto* t : own_) {
        if (t == theme) return false;
    }

    // 2. Checks removed (+0x120); if present, removes it via swap-with-last (rescuing it).
    for (size_t i = 0; i < removed_.size(); ++i) {
        if (removed_[i] == theme) {
            removed_[i] = removed_.back();
            removed_.pop_back();
            break;
        }
    }

    // 3. Appends to added (+0x108).
    added_.push_back(theme);

    // 4. Appends to own (+0x138).
    own_.push_back(theme);
    return true;
}

bool UnitThemeSet::add_unit_theme(const std::string& name, const UnitThemeBook* book) {
    // `CBaseUnit::addUnitTheme(const std::wstring&)
    // `.
    if (!book) return false;
    const UnitTheme* theme = book->get_unit_theme(name);
    return add_unit_theme(theme);
}

bool UnitThemeSet::remove_unit_theme(const UnitTheme* theme) {
    // `CBaseUnit::removeUnitTheme(CUnitTheme*)`.
    // 1. Checks own (+0x138); if not found, returns immediately.
    size_t own_idx = static_cast<size_t>(-1);
    for (size_t i = 0; i < own_.size(); ++i) {
        if (own_[i] == theme) {
            own_idx = i;
            break;
        }
    }
    if (own_idx == static_cast<size_t>(-1)) return false;

    // 2. Appends to removed (+0x120).
    removed_.push_back(theme);

    // 3. If present in added (+0x108), removes it via swap-with-last.
    for (size_t i = 0; i < added_.size(); ++i) {
        if (added_[i] == theme) {
            added_[i] = added_.back();
            added_.pop_back();
            break;
        }
    }

    // 4. Removes from own (+0x138) via swap-with-last.
    own_[own_idx] = own_.back();
    own_.pop_back();
    return true;
}

bool UnitThemeSet::remove_unit_theme(const std::string& name, const UnitThemeBook* book) {
    // `CBaseUnit::removeUnitTheme(const std::wstring&)
    // `.
    if (!book) return false;
    const UnitTheme* theme = book->get_unit_theme(name);
    return remove_unit_theme(theme);
}

bool UnitThemeSet::remove_unit_theme(int64_t guid, const UnitThemeBook* book) {
    // `CBaseUnit::removeUnitTheme(long long)`.
    if (!book) return false;
    const UnitTheme* theme = book->get_unit_theme(guid);
    return remove_unit_theme(theme);
}

bool UnitThemeSet::has_unit_theme(const UnitTheme* theme) const {
    // `CBaseUnit::hasUnitTheme(CUnitTheme*)`.
    // 1. Checks effect (+0x150) first.
    for (const auto* t : effect_) {
        if (t == theme) return true;
    }

    // 2. Checks own (+0x138) second.
    for (const auto* t : own_) {
        if (t == theme) return true;
    }
    return false;
}

bool UnitThemeSet::has_unit_theme(const std::string& name, const UnitThemeBook* book) const {
    // `CBaseUnit::hasUnitTheme(const std::wstring&)
    // `.
    if (!book) return false;
    const UnitTheme* theme = book->get_unit_theme(name);
    return has_unit_theme(theme);
}

bool UnitThemeSet::has_unit_theme(int64_t guid, const UnitThemeBook* book) const {
    // `CBaseUnit::hasUnitTheme(long long)`.
    if (!book) return false;
    const UnitTheme* theme = book->get_unit_theme(guid);
    return has_unit_theme(theme);
}

void UnitThemeSet::init_themes(const tl::formats::AdmSection& root, const UnitThemeBook* book) {
    // `CBaseUnit::unitInitThemes`: every string value of
    // `[THEMES]`, whatever its key. Also `CCharacter::unitInit`, `CBreakable::unitInit`,
    // `CTriggerUnit::unitInit` (its callers).
    for (const auto& child : root.children) {
        if (child.name != "THEMES") continue;
        for (const auto& rec : child.records) {
            if (rec.type == tl::formats::AdmType::String ||
                rec.type == tl::formats::AdmType::Translate) {
                add_unit_theme(to_upper(rec.text), book);
            }
        }
        return;
    }
}

void UnitThemeSet::update(const std::vector<const UnitTheme*>& effect_manager_themes,
                          const ThemeUpdatedCallback& callback) {
    // `CBaseUnit::update`, the theme half. The caller runs
    // `CSkillManager::update` before it and `CEffectManager::updateAffixes` between the halves.

    // 1. Announced removed themes (+0x120) with removed=true, then clear list.
    for (const auto* theme : removed_) {
        if (callback) callback(theme, true);
    }
    removed_.clear();

    // 2. Announce added themes (+0x108) with removed=false, then clear list.
    for (const auto* theme : added_) {
        if (callback) callback(theme, false);
    }
    added_.clear();

    // 3. The effect manager's themes (+0x2c8): new ones announced and appended.
    for (const auto* theme : effect_manager_themes) {
        bool found = false;
        for (const auto* t : effect_) {
            if (t == theme) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (callback) callback(theme, false);
            effect_.push_back(theme);
        }
    }

    // Then the ones it no longer has announced and removed, swap-with-last.
    for (size_t i = 0; i < effect_.size();) {
        const auto* theme = effect_[i];
        bool found = false;
        for (const auto* em_t : effect_manager_themes) {
            if (em_t == theme) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (callback) callback(theme, true);
            effect_[i] = effect_.back();
            effect_.pop_back();
            // Re-check same index because back() element moved into the hole.
        } else {
            ++i;
        }
    }
}

std::vector<ThemeParticleStep> ThemeParticleTriggers::unit_theme_updated(
    const UnitTheme* theme, bool removed, const ThemeParticleUnit& unit,
    RunicRandom& volatile_rng) {
    // `CCharacter::unitThemeUpdated`.
    if (!removed) {
        bool made = false;
        for (const ThemeParticleTrigger& trigger : triggers_) {
            if (trigger.theme == theme && trigger.particle >= 0) {
                made = true;
                break;
            }
        }
        if (!made) create_specific(theme, unit, volatile_rng);
    }
    std::vector<ThemeParticleStep> steps;
    for (size_t i = 0; i < triggers_.size(); ++i) {
        if (triggers_[i].theme != theme || triggers_[i].particle < 0) continue;
        steps.push_back(ThemeParticleStep{i, !removed});
    }
    return steps;
}

void ThemeParticleTriggers::create_specific(const UnitTheme* theme, const ThemeParticleUnit& unit,
                                            RunicRandom& volatile_rng) {
    // `CCharacter::createSpecificUnitThemeParticles`. A unit
    // without its model (`+0x200`) is the caller's to skip.
    if (theme == nullptr) return;
    for (const ThemeParticleTrigger& trigger : triggers_) {
        if (trigger.particle >= 0 && trigger.theme == theme) return;
    }
    for (const UnitThemeParticle& particle : theme->particles) {
        if (!unit.isa || !unit.isa(particle.unit_type)) continue;
        // `COUNT` 0 is one; `BONE:Every Bone` is one per bone of the skeleton,
        // and none on a skeleton without bones.
        uint32_t count = particle.count != 0 ? particle.count : 1;
        if (particle.bone == "Every Bone") {
            count = unit.bone_count;
            if (count == 0) continue;
        }
        const std::string file = to_upper(particle.file);
        for (uint32_t index = 0; index < count; ++index) {
            // The trigger is appended before the particle is asked for, so one
            // the preloader has not got stays in the list without a particle.
            triggers_.push_back(ThemeParticleTrigger{});
            const int handle = unit.get_particle ? unit.get_particle(file) : -1;
            if (handle < 0) continue;
            ThemeParticleTrigger& trigger = triggers_.back();
            trigger.particle = handle;
            trigger.file = file;
            trigger.stop_on_death = true;
            trigger.theme = theme;
            trigger.attaches = particle.attaches;
            trigger.follows = particle.follows;
            trigger.bone = particle.bone;
            configure(trigger, index, unit, volatile_rng);
        }
    }
}

void ThemeParticleTriggers::configure(ThemeParticleTrigger& trigger, uint32_t index,
                                      const ThemeParticleUnit& unit, RunicRandom& volatile_rng) {
    // `CCharacter::configureParticleTrigger`.
    // Without a skeleton, or with no bone named, the particle rides the unit's
    // own node; a named bone the skeleton lacks, likewise. `Random` draws one
    // bone per trigger on the volatile stream; `Every Bone` takes the trigger's
    // own index.
    trigger.bone_found = false;
    if (!unit.has_skeleton || trigger.bone.empty()) return;
    const bool every = trigger.bone == "Every Bone";
    const bool random = trigger.bone == "Random";
    if (!random && !every && !(unit.has_bone && unit.has_bone(trigger.bone))) return;
    std::string found;
    if (random || every) {
        uint64_t pick = index;
        if (random) {
            // `randomBetweenVolatile(0, (float)(int)(bones - 1))`, truncated
            // (`cvttss2si` to 64 bits) and taken as an unsigned short.
            const float draw = volatile_rng.between(
                0.0f, static_cast<float>(static_cast<int>(unit.bone_count) - 1));
            pick = static_cast<uint64_t>(static_cast<int64_t>(draw));
        }
        found =
            unit.bone_name ? unit.bone_name(static_cast<uint16_t>(pick & 0xffff)) : std::string();
    } else {
        found = trigger.bone;
    }
    if (found.empty()) return;
    trigger.bone = found;
    trigger.bone_found = true;
}

void ThemeParticleTriggers::load_model(const std::vector<AnimationTag>& keys, bool bind,
                                       const UnitThemeBook* book, const ThemeParticleUnit& unit,
                                       RunicRandom& volatile_rng) {
    // `CCharacter::loadModel`, keyframe types
    // 4 and 5. The count is the key's `PARTICLECOUNT` (+0x48), or the
    // skeleton's bones when the key's bone (+0x20) is `Every Bone`.
    constexpr int kSpawnParticle = 4;
    constexpr int kSpawnParticleStopOnDeath = 5;
    for (const AnimationTag& key : keys) {
        const int type = keyframe_type_of(key.name);
        if (type != kSpawnParticle && type != kSpawnParticleStopOnDeath) continue;
        const uint32_t count = key.bone == "Every Bone" ? unit.bone_count : key.particle_count;
        const std::string file = to_upper(key.layout);
        for (uint32_t index = 0; index < count; ++index) {
            // Appended (`addValueIndex`, then the list) before the particle is
            // asked for, as `create_specific` does.
            triggers_.push_back(ThemeParticleTrigger{});
            const int handle = unit.get_particle ? unit.get_particle(file) : -1;
            if (handle < 0) continue;
            ThemeParticleTrigger& trigger = triggers_.back();
            trigger.particle = handle;
            trigger.file = file;
            trigger.offset_x = key.offset_x;
            trigger.offset_y = key.offset_y;
            trigger.offset_z = key.offset_z;
            trigger.stop_on_death = type == kSpawnParticleStopOnDeath;
            // +0x50 is the key's own theme (`CKeyframe +0x40`, its `UNITTHEME`).
            trigger.theme = book != nullptr && !key.unit_theme.empty()
                                ? book->get_unit_theme(key.unit_theme)
                                : nullptr;
            trigger.attaches = key.attaches;
            trigger.bone = key.bone;
            trigger.follows = false;
            // The animation's upper-cased name against "BIND".
            trigger.bind = bind;
            configure(trigger, index, unit, volatile_rng);
        }
    }
}

std::vector<ThemeParticleStep> ThemeParticleTriggers::set_visible(
    bool visible, bool at_once, int32_t ai_state,
    const std::function<bool(const UnitTheme*)>& has_theme,
    const std::function<bool(int)>& looping) const {
    // `CCharacter::setVisible(bool, bool)`,
    // the first loop. Only triggers that `FOLLOW` (+0x59) or are `BIND`'s
    // (+0x5a), with a particle.
    std::vector<ThemeParticleStep> steps;
    const bool dying = ai_state == 5 || ai_state == 6;
    for (size_t i = 0; i < triggers_.size(); ++i) {
        const ThemeParticleTrigger& trigger = triggers_[i];
        if ((!trigger.follows && !trigger.bind) || trigger.particle < 0) continue;
        if (trigger.bind) {
            // `CBaseUnit::hasUnitTheme` on the trigger's theme, when it has one.
            if (trigger.theme != nullptr && !(has_theme && has_theme(trigger.theme))) continue;
            if (!(trigger.stop_on_death && dying) && visible) {
                steps.push_back(ThemeParticleStep{i, true, false, false});
                continue;
            }
            steps.push_back(ThemeParticleStep{i, false, at_once, false});
        }
        // hidden, a trigger that follows goes to (0, -99999, 0).
        if (!visible && trigger.follows) steps.push_back(ThemeParticleStep{i, false, false, true});
    }
    // hidden at once, every other attaching trigger that does not loop
    // (`CParticle::looping`) is stopped at once.
    if (!visible && at_once) {
        for (size_t i = 0; i < triggers_.size(); ++i) {
            const ThemeParticleTrigger& trigger = triggers_[i];
            if (trigger.follows || trigger.bind || trigger.particle < 0) continue;
            if (looping && looping(trigger.particle)) continue;
            if (trigger.attaches) steps.push_back(ThemeParticleStep{i, false, true, false});
        }
    }
    return steps;
}

std::vector<ThemeParticleStep> ThemeParticleTriggers::die() const {
    // `CCharacter::die`, after the death
    // particle (`+0x4a8`).
    std::vector<ThemeParticleStep> steps;
    for (size_t i = 0; i < triggers_.size(); ++i) {
        if (!triggers_[i].stop_on_death || triggers_[i].particle < 0) continue;
        steps.push_back(ThemeParticleStep{i, false, false, false});
    }
    return steps;
}

std::vector<ThemeParticleStep> ThemeParticleTriggers::stop_particles(bool at_once) const {
    // `CCharacter::stopParticles(bool)`.
    std::vector<ThemeParticleStep> steps;
    for (size_t i = 0; i < triggers_.size(); ++i) {
        const ThemeParticleTrigger& t = triggers_[i];
        if (!t.follows && !t.bind) continue;
        if (t.particle < 0) continue;
        steps.push_back(ThemeParticleStep{i, false, at_once, false});
    }
    return steps;
}

}  // namespace tl::core
