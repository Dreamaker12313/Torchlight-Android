// Spawn classes (`CSpawnClass`, `CSpawnClassData`, `CSpawningData`, `CSpawnClassParser`): weighted
// entries naming a unit, a unit type or another spawn class, and their roll.
#pragma once

#include "core/random.h"
#include "core/randomizer.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "formats/adm.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tl::core {

class SpawnClass;
class SpawnClassData;

/// `CSpawningData` (0x30 bytes, built in `CSpawnClass::addSpawnClassData` from `[OBJECT]`).
struct SpawningData {
    int32_t weight = -1;           ///< +0x10 `WEIGHT`, default -1, authored 0 stored as -1
    int32_t rarity_override = -1;  ///< +0x14 `RARITY_OVERRIDE`, default -1
    /// +0x18: bit 0 FORCEENCHANT, bit 1 NO_UNIQUE_ITEMS,
    ///        bit 2 NO_MAGIC_ITEMS, bit 4 IGNORE_RANGE
    uint32_t flags = 0;
    int32_t min_count = 1;    ///< +0x1c `MINCOUNT`, default 1
    int32_t max_count = 1;    ///< +0x20 `MAXCOUNT`, default 1
    int32_t level_bonus = 0;  ///< +0x24 `LEVEL_BONUS`, default 0
    float magic_find = 0.0f;  ///< +0x28 `MAGICFIND`, default 0.0f
};

/// Unit metadata evaluated by `CSpawnClassData::fillRandomizer`.
///
struct SpawnUnitData {
    std::string name;
    std::string unittype;
    int32_t rarity = 1;            ///< `RARITY`, default 1
    int32_t rarity_hardcore = -1;  ///< `RARITY_HARDCORE`, -1 if unauthored
    std::string create_as;         ///< `CREATEAS`, default empty
    int32_t level = 1;             ///< `LEVEL`, default 1
    int32_t min_level = 0;         ///< `MINLEVEL`, default 0
    int32_t max_level = 99999;     ///< `MAXLEVEL`, default 99999 (0 for `CREATEAS:EQUIPMENT`)

    /// `CREATEAS` is `EQUIPMENT`, the one value `fillRandomizer` tests for:
    /// such a unit is gated by the item spawn range and split by
    /// quality; every other by `MINLEVEL`/`MAXLEVEL` alone.
    bool is_equipment() const { return create_as == "EQUIPMENT"; }

    static SpawnUnitData from_adm(const tl::formats::AdmSection& unit_section);
};

/// Character context passed to `rollSpawnClass` and `fillRandomizer`.
struct SpawnCharacterContext {
    /// `CBaseUnit::ISA(41)` (kMerchant, 0x29) expands level window by [-4, +4]
    bool is_merchant = false;
    bool is_hardcore = false;  ///< `player + 0xa15`
    /// The monster level of the character's level (`fillRandomizer`)
    /// when the roll has none; not the character's own level.
    int32_t level = 1;
    /// maximum of `getEffectValue(0x1b, 7)` on finder, pet (+0x640), and pet's pet
    float magic_find = 0.0f;
};

/// External environment / callbacks supplied by the caller (renderer or unit test).
struct SpawnContext {
    const UnitTypeHierarchy* unit_types = nullptr;
    const StatBook* stats = nullptr;

    /// Rarity weights and scaling from `CGameGlobals` / `GLOBALS.DAT`.
    int32_t normal_item_weight = 5000;  ///< `CGameGlobals +0x18` NORMAL_ITEM_WEIGHT
    int32_t magic_item_weight = 120;    ///< `CGameGlobals +0x1c` MAGIC_ITEM_WEIGHT
    int32_t unique_item_weight = 18;    ///< `CGameGlobals +0x20` UNIQUE_ITEM_WEIGHT
    /// `MAGICFIND_MAGIC_INFLUENCE` / `MAGICFIND_UNIQUE_INFLUENCE` as `reload` leaves them: the
    /// file's figure times 0.01f.
    float magicfind_magic_influence = 1.5f;
    float magicfind_unique_influence = 2.0f;

    std::function<const SpawnClass*(const std::string& name)> get_spawn_class;
    std::function<std::vector<SpawnUnitData>(uint32_t type_id)> get_units_by_type;
    std::function<const SpawnUnitData*(const std::string& name)> get_unit_by_name;
};

/// The five `CGameGlobals` figures a spawn roll reads (`reload`); the
/// context's own stand when the file has none.
void read_spawn_weights(const tl::formats::AdmFile& globals, SpawnContext& context);

/// `CSpawnClassData`; `CSpawnClassData::CSpawnClassData`.
/// The `ITEM_SPAWN_RANGE_*` graphs it fetches are read through the stats when it rolls.
class SpawnClassData {
public:
    SpawnClassData() = default;
    explicit SpawnClassData(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }
    // PORT: an accessor of the port's own bookkeeping.
    const std::vector<SpawnUnitData>& units() const { return units_; }
    // PORT: an accessor of the port's own bookkeeping.
    const SpawnClass* sub_class() const { return sub_class_; }

    /// `UNITTYPE:NONE` (`createSpawnClassDataByUnitType`):
    /// `rollSpawnClass` stops when it draws one, so its weight is the chance of nothing.
    bool is_none() const { return is_none_; }
    void set_none() { is_none_ = true; }

    /// `CSpawnClassData::addSpawnClass`.
    void add_spawn_class(const SpawnClass* sub_class);

    /// `CSpawnClassData::addUnitDataObject`.
    void add_unit_data_object(SpawnUnitData unit);

    /// `CSpawnClassData::addUnitDataObjects`.
    void add_unit_data_objects(const std::vector<SpawnUnitData>& new_units);

    /// `CSpawnClassData::getUnitObjectCount`.
    uint32_t get_unit_object_count() const { return static_cast<uint32_t>(units_.size()); }

    /// `CSpawnClassData::calculateChoices`.
    int32_t calculate_choices(const SpawnCharacterContext* character, int32_t level,
                              int32_t level_bonus, int32_t rarity_override, int32_t flags,
                              const SpawnContext& context);

    /// `CSpawnClassData::fillRandomizer`.
    void fill_randomizer(const SpawnCharacterContext* character, int32_t level, int32_t level_bonus,
                         int32_t rarity_override, int32_t flags, const SpawnContext& context);

    /// `CSpawnClassData::rollSpawnClassData`.
    void roll_spawn_class_data(std::vector<std::string>& out_units, std::vector<bool>* out_flags,
                               const SpawnCharacterContext* character,
                               const SpawnCharacterContext* finder, int32_t level,
                               int32_t level_bonus, int32_t rarity_override, int32_t magic_find,
                               uint32_t flags, RunicRandom& rng, const SpawnContext& context);

    // Randomizer accessors for testing
    const Randomizer& normal_randomizer() const { return normal_randomizer_; }
    const Randomizer& magic_randomizer() const { return magic_randomizer_; }
    const Randomizer& unique_randomizer() const { return unique_randomizer_; }

private:
    std::string name_;                       ///< +0x10
    std::vector<SpawnUnitData> units_;       ///< +0x18 (data), +0x20 (count)
    const SpawnClass* sub_class_ = nullptr;  ///< +0x58
    bool is_none_ = false;                   ///< +0x60

    // Cache keys (default -999 = 0xfffffc19)
    int32_t cached_level_ = -999;            ///< +0x30
    int32_t cached_level_bonus_ = -999;      ///< +0x34
    int32_t cached_rarity_override_ = -999;  ///< +0x38
    int32_t cached_flags_ = -999;            ///< +0x3c

    Randomizer normal_randomizer_{RandomizerType::kNormal};  ///< +0x40
    Randomizer magic_randomizer_{RandomizerType::kNormal};   ///< +0x48 (ISA 0x37 MAGIC)
    Randomizer unique_randomizer_{RandomizerType::kNormal};  ///< +0x50 (ISA 0x36 UNIQUE)

    std::vector<int32_t> equipment_indices_;  ///< +0x68 (data), +0x70 (count)
};

/// One entry in `CSpawnClass`.
struct SpawnClassEntry {
    SpawningData spawning_data;
    std::shared_ptr<SpawnClassData> data;
};

/// `CSpawnClass`.
class SpawnClass {
public:
    /// `CSpawnClass::CSpawnClass(wchar_t const*)` (X360 inline in
    /// `CSpawnClassParser::createEmptySpawnClass`).
    SpawnClass() = default;
    explicit SpawnClass(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }
    /// The file root's `LEVEL_BONUS`, which `createEmptySpawnClass` writes; nothing reads it.
    int32_t level_bonus() const { return level_bonus_; }
    void set_level_bonus(int32_t bonus) { level_bonus_ = bonus; }
    const std::vector<SpawnClassEntry>& entries() const { return entries_; }

    /// `CSpawnClass::addSpawnClassData`.
    void add_spawn_class_data(SpawningData spawning_data, std::shared_ptr<SpawnClassData> data);

    /// `CSpawnClass::calculateNumberOfChoices`.
    int32_t calculate_number_of_choices(const SpawnCharacterContext* character, int32_t level,
                                        int32_t level_bonus, int32_t rarity_override, int32_t flags,
                                        const SpawnContext& context) const;

    /// `CSpawnClass::rollSpawnClass`.
    uint32_t roll_spawn_class(std::vector<std::string>& out_units, std::vector<bool>* out_flags,
                              const SpawnCharacterContext* character,
                              const SpawnCharacterContext* finder, int32_t level, uint32_t seed,
                              int32_t level_bonus, int32_t rarity_override, int32_t magic_find,
                              uint32_t flags, RunicRandom& rng, const SpawnContext& context) const;

private:
    std::string name_;                      ///< +0x28
    std::vector<SpawnClassEntry> entries_;  ///< +0x10 (data), +0x18 (count)
    int32_t level_bonus_ = 0;               ///< +0x30
};

// --------------------------------------------------------------------------
// Parser helpers (CSpawnClassParser)

/// `CSpawnClassParser::createSpawnClassData`.
std::shared_ptr<SpawnClassData> create_spawn_class_data(
    const tl::formats::AdmSection& object_section, const SpawnContext& context);

/// `CSpawnClassParser::createSpawnClassDataByUnitType`.
///
std::shared_ptr<SpawnClassData> create_spawn_class_data_by_unit_type(
    const std::string& type_name, const tl::formats::AdmSection& object_section,
    const SpawnContext& context);

/// `CSpawnClassParser::createSpawnClassDataWithUnit`.
///
std::shared_ptr<SpawnClassData> create_spawn_class_data_with_unit(
    const std::string& unit_name, const tl::formats::AdmSection& object_section,
    const SpawnContext& context);

/// `CSpawnClassParser::createSpawnClassDataRefSpawnClass`.
///
std::shared_ptr<SpawnClassData> create_spawn_class_data_ref_spawn_class(
    const std::string& class_name, const tl::formats::AdmSection& object_section,
    const SpawnContext& context);

/// `CSpawnClassParser::fillEmptySpawnClass`.
SpawnClass parse_spawn_class(const tl::formats::AdmSection& root, const SpawnContext& context);

}  // namespace tl::core
