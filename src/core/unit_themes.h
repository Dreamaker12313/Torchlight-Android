// Unit themes: `CUnitThemes`, `CUnitTheme`, `CUnitThemeParticle`, and `CBaseUnit`'s four theme
// lists. No OGRE and no file I/O: the caller hands the parsed files over in
// `CFileSystem::getFileList`'s order.
#pragma once

#include "core/animation_manifest.h"
#include "formats/adm.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

/// `CUnitThemeParticle` (0x30 bytes): one `[PARTICLE]` block of a theme file.
struct UnitThemeParticle {
    std::string file;    ///< +0x10, `FILE`, default ""
    uint32_t count = 1;  ///< +0x18, `COUNT`, default 1
    /// +0x1c, `UNITTYPE`, default "ANY": kept as the name, not the type ID
    /// `CHierarchy::getTypeIDByName` gives.
    std::string unit_type = "ANY";
    bool attaches = true;  ///< +0x20, `ATTACHES`, default true
    bool follows = false;  ///< +0x21, `FOLLOWS`, default !attaches
    std::string bone;      ///< +0x28, `BONE`, default ""

    /// `CUnitThemeParticle::load`
    static UnitThemeParticle load(const tl::formats::AdmSection& block);
};

/// `CUnitTheme` (0x38 bytes). `NEEDSTOSAVE`, authored in a few theme files, is
/// not read by `CUnitThemes::reload`, so it is not kept.
struct UnitTheme {
    std::vector<UnitThemeParticle> particles;  ///< +0x10
    std::string name;                          ///< +0x28, `NAME`, upper-cased
    int64_t guid = -1;                         ///< +0x30, `GUID`, default -1
};

/// `CUnitThemes` (`CUnitThemes::getSingleton`): the table of
/// `media/unitthemes/*.dat`. The original has no lookup method: every caller
/// scans `+0x18` inline, by exact name (`wmemcmp`, no upper-casing) or by GUID,
/// first match. `get_unit_theme` is that scan.
class UnitThemeBook {
public:
    /// `CUnitThemes::reload`, fed the files
    /// in `CFileSystem::getFileList`'s order.
    void reload(const std::vector<const tl::formats::AdmFile*>& files);
    void reload(const std::vector<tl::formats::AdmFile>& files);

    const UnitTheme* get_unit_theme(const std::string& name) const;
    const UnitTheme* get_unit_theme(int64_t guid) const;

    // PORT: an accessor of the port's own bookkeeping.
    size_t size() const { return themes_.size(); }
    const std::vector<UnitTheme>& themes() const { return themes_; }

private:
    std::vector<UnitTheme> themes_;  ///< +0x18, count +0x20
};

/// `CBaseUnit`'s four theme lists: added and removed since the last update, the unit's own, and
/// its effects'. A name or GUID the table lacks is stored and announced as null, as in the
/// original; a callback must allow for it.
class UnitThemeSet {
public:
    using ThemeUpdatedCallback = std::function<void(const UnitTheme*, bool /*removed*/)>;

    /// `CBaseUnit::addUnitTheme(CUnitTheme*)`. The original returns
    /// nothing; `false` here means the theme was already held.
    bool add_unit_theme(const UnitTheme* theme);

    /// `CBaseUnit::addUnitTheme(const std::wstring&)
    /// `
    bool add_unit_theme(const std::string& name, const UnitThemeBook* book);

    /// `CBaseUnit::removeUnitTheme(CUnitTheme*)`. `false`: not held.
    bool remove_unit_theme(const UnitTheme* theme);

    /// `CBaseUnit::removeUnitTheme(const std::wstring&)
    /// `
    bool remove_unit_theme(const std::string& name, const UnitThemeBook* book);

    /// `CBaseUnit::removeUnitTheme(long long)`
    bool remove_unit_theme(int64_t guid, const UnitThemeBook* book);

    /// `CBaseUnit::hasUnitTheme(CUnitTheme*)`
    bool has_unit_theme(const UnitTheme* theme) const;

    /// `CBaseUnit::hasUnitTheme(const std::wstring&)
    /// `
    bool has_unit_theme(const std::string& name, const UnitThemeBook* book) const;

    /// `CBaseUnit::hasUnitTheme(long long)`
    bool has_unit_theme(int64_t guid, const UnitThemeBook* book) const;

    /// `CBaseUnit::unitInitThemes`: `root` is
    /// the unit file's root section (`CBaseUnit +0x1b0`).
    void init_themes(const tl::formats::AdmSection& root, const UnitThemeBook* book);

    /// `CBaseUnit::update`, the theme half.
    /// The caller passes the effect manager's list (`EffectManager::themes()`);
    /// the original skips the effect half for a unit without an effect manager.
    void update(const std::vector<const UnitTheme*>& effect_manager_themes,
                const ThemeUpdatedCallback& callback = nullptr);

    const std::vector<const UnitTheme*>& added_themes() const { return added_; }
    const std::vector<const UnitTheme*>& removed_themes() const { return removed_; }
    const std::vector<const UnitTheme*>& own_themes() const { return own_; }
    const std::vector<const UnitTheme*>& effect_themes() const { return effect_; }

private:
    std::vector<const UnitTheme*> added_;    ///< +0x108
    std::vector<const UnitTheme*> removed_;  ///< +0x120
    std::vector<const UnitTheme*> own_;      ///< +0x138
    std::vector<const UnitTheme*> effect_;   ///< +0x150
};

class RunicRandom;

/// `CCharacter::CParticleAnimationTrigger` (0x60 bytes), made by a theme or a key of the model's
/// base skeleton (`CCharacter::loadModel`). Other animations' keys
/// are kept apart (`Renderer::Impl::unit_particle_triggers`).
struct ThemeParticleTrigger {
    /// +0x18: the caller's particle handle, -1 when `CParticlePreloader::GetParticle` gave none
    /// (the trigger stays, never started).
    int particle = -1;
    /// +0x40, the bone, as authored and then the name of the one
    /// `configureParticleTrigger` found.
    std::string bone;
    /// +0x48 non-null: a bone was found (with `attaches`, a node for the particle to ride).
    bool bone_found = false;
    const UnitTheme* theme = nullptr;  ///< +0x50
    bool attaches = true;              ///< +0x58
    bool follows = false;              ///< +0x59
    /// +0x5a: a key of a `BIND` base skeleton, worn while shown (`setVisible`).
    ///
    bool bind = false;
    /// +0x20: `CCharacter::die` stops it. A
    /// theme's always; a key's when it is `SPAWNPARTICLE_STOP_ON_DEATH`.
    bool stop_on_death = false;
    /// +0x28..+0x30: the key's `BONEOFFSET`, where the particle sits on its node
    /// (`CPositionableObject::setPosition`); zero for a theme's.
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;
    /// The particle file as `GetParticle` was asked for it, upper-cased.
    std::string file;
};

/// What `createSpecificUnitThemeParticles` and `configureParticleTrigger` ask
/// of the unit and its skeleton.
struct ThemeParticleUnit {
    /// `CBaseUnit::ISA` against a particle's `UNITTYPE` name.
    std::function<bool(const std::string&)> isa;
    /// The unit has an entity with a skeleton (vtable +0x1e0, then +0x130).
    bool has_skeleton = false;
    /// `Skeleton::getNumBones` (vtable +0x188).
    uint16_t bone_count = 0;
    /// `Skeleton::getBone(unsigned short)` (vtable +0x1a8): the bone's name, or
    /// empty when there is none at that index.
    std::function<std::string(uint16_t)> bone_name;
    /// `Skeleton::hasBone` (vtable +0x1b8).
    std::function<bool(const std::string&)> has_bone;
    /// `CParticlePreloader::LoadParticle` + `GetParticle` for an upper-cased
    /// file: a handle, or -1.
    std::function<int(const std::string&)> get_particle;
};

/// One thing `unitThemeUpdated`, `setVisible` or `die` does to a trigger's
/// particle.
struct ThemeParticleStep {
    size_t trigger = 0;
    /// `CParticle::Start` after placing it, or `CParticle::Stop`.
    bool start = true;
    /// `CParticle::Stop`'s argument: what is out goes at once rather than living out its life.
    bool at_once = false;
    /// `setVisible(false, ...)` on a trigger that `FOLLOWS` (+0x59): the
    /// particle is put at (0, -99999, 0), out of sight, rather than stopped.
    bool park = false;
};

/// `CCharacter +0x738` for themes: the particles a unit's themes hang on it.
class ThemeParticleTriggers {
public:
    /// `CCharacter::unitThemeUpdated`, the handler
    /// `UnitThemeSet::update` calls. Steps in list order; placing and starting are the caller's.
    std::vector<ThemeParticleStep> unit_theme_updated(const UnitTheme* theme, bool removed,
                                                      const ThemeParticleUnit& unit,
                                                      RunicRandom& volatile_rng);

    /// `CCharacter::loadModel`, its trigger loop over the base
    /// skeleton's `SPAWNPARTICLE` keys. `bind`: that animation is `BIND`. Nothing is started.
    void load_model(const std::vector<AnimationTag>& keys, bool bind, const UnitThemeBook* book,
                    const ThemeParticleUnit& unit, RunicRandom& volatile_rng);

    /// `CCharacter::setVisible(bool, bool)`, the trigger walk,
    /// on a change only (the caller returns early otherwise). `ai_state` is its `+0x330`;
    /// `looping` is `CParticle::looping` on the caller's handle.
    std::vector<ThemeParticleStep> set_visible(
        bool visible, bool at_once, int32_t ai_state,
        const std::function<bool(const UnitTheme*)>& has_theme,
        const std::function<bool(int)>& looping = {}) const;

    /// `CCharacter::die`: every trigger with
    /// a particle that stops on death (+0x20) is stopped.
    std::vector<ThemeParticleStep> die() const;

    /// `CCharacter::stopParticles(bool)`: following and
    /// `BIND` triggers stopped (also `CMonsterDescriptor::InputLogicEvent`'s `Hide`).
    std::vector<ThemeParticleStep> stop_particles(bool at_once) const;

    const std::vector<ThemeParticleTrigger>& triggers() const { return triggers_; }

private:
    /// `CCharacter::createSpecificUnitThemeParticles
    /// `
    void create_specific(const UnitTheme* theme, const ThemeParticleUnit& unit,
                         RunicRandom& volatile_rng);
    /// `CCharacter::configureParticleTrigger
    /// `, the bone half.
    static void configure(ThemeParticleTrigger& trigger, uint32_t index,
                          const ThemeParticleUnit& unit, RunicRandom& volatile_rng);

    std::vector<ThemeParticleTrigger> triggers_;
};

}  // namespace tl::core
