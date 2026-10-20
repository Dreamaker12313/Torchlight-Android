#include "core/spawn_class.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace tl::core {

namespace {

// ASCII upper-casing.
// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string to_upper(std::string str) {
    for (char& c : str) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return str;
}

}  // namespace

// --------------------------------------------------------------------------
// SpawnUnitData

void read_spawn_weights(const tl::formats::AdmFile& globals, SpawnContext& context) {
    const tl::formats::AdmSection& root = globals.root;
    context.normal_item_weight = static_cast<int32_t>(
        root.integer_or("NORMAL_ITEM_WEIGHT", static_cast<uint32_t>(context.normal_item_weight)));
    context.magic_item_weight = static_cast<int32_t>(
        root.integer_or("MAGIC_ITEM_WEIGHT", static_cast<uint32_t>(context.magic_item_weight)));
    context.unique_item_weight = static_cast<int32_t>(
        root.integer_or("UNIQUE_ITEM_WEIGHT", static_cast<uint32_t>(context.unique_item_weight)));
    // `reload` scales each influence by 0.01f, read or held.
    context.magicfind_magic_influence =
        root.float_or("MAGICFIND_MAGIC_INFLUENCE", context.magicfind_magic_influence * 100.0f) *
        0.01f;
    context.magicfind_unique_influence =
        root.float_or("MAGICFIND_UNIQUE_INFLUENCE", context.magicfind_unique_influence * 100.0f) *
        0.01f;
}

SpawnUnitData SpawnUnitData::from_adm(const tl::formats::AdmSection& unit_section) {
    SpawnUnitData u;
    u.name = unit_section.string_or("NAME", "");
    u.unittype = unit_section.string_or("UNITTYPE", "");
    u.rarity = static_cast<int32_t>(unit_section.integer_or("RARITY", 1));
    if (const auto* rec = unit_section.find("RARITY_HARDCORE")) {
        u.rarity_hardcore = static_cast<int32_t>(rec->integer);
    } else {
        u.rarity_hardcore = -1;
    }
    u.create_as = unit_section.string_or("CREATEAS", "");
    u.level = static_cast<int32_t>(unit_section.integer_or("LEVEL", 1));
    u.min_level = static_cast<int32_t>(unit_section.integer_or("MINLEVEL", 0));
    // `fillRandomizer`: `MAXLEVEL` defaults to 0 for
    // `EQUIPMENT`, else 99999.
    u.max_level =
        static_cast<int32_t>(unit_section.integer_or("MAXLEVEL", u.is_equipment() ? 0 : 99999));
    return u;
}

// --------------------------------------------------------------------------
// SpawnClassData

// CSpawnClassData::addSpawnClass
void SpawnClassData::add_spawn_class(const SpawnClass* sub_class) { sub_class_ = sub_class; }

// CSpawnClassData::addUnitDataObject
void SpawnClassData::add_unit_data_object(SpawnUnitData unit) { units_.push_back(std::move(unit)); }

// CSpawnClassData::addUnitDataObjects
void SpawnClassData::add_unit_data_objects(const std::vector<SpawnUnitData>& new_units) {
    units_.insert(units_.end(), new_units.begin(), new_units.end());
}

// CSpawnClassData::calculateChoices
int32_t SpawnClassData::calculate_choices(const SpawnCharacterContext* character, int32_t level,
                                          int32_t level_bonus, int32_t rarity_override,
                                          int32_t flags, const SpawnContext& context) {
    if (sub_class_ != nullptr) {
        return sub_class_->calculate_number_of_choices(character, level, level_bonus,
                                                       rarity_override, flags, context);
    }
    if (units_.empty()) {
        return 0;
    }
    if (cached_flags_ == flags && cached_level_ == level && cached_level_bonus_ == level_bonus &&
        cached_rarity_override_ == rarity_override) {
        return static_cast<int32_t>(normal_randomizer_.choice_count() +
                                    magic_randomizer_.choice_count() +
                                    unique_randomizer_.choice_count());
    }

    normal_randomizer_.clear();
    magic_randomizer_.clear();
    unique_randomizer_.clear();
    equipment_indices_.clear();

    cached_level_ = level;
    cached_level_bonus_ = level_bonus;
    cached_rarity_override_ = rarity_override;
    cached_flags_ = flags;

    fill_randomizer(character, level, level_bonus, rarity_override, flags, context);
    return static_cast<int32_t>(normal_randomizer_.choice_count() +
                                magic_randomizer_.choice_count() +
                                unique_randomizer_.choice_count());
}

// CSpawnClassData::fillRandomizer
void SpawnClassData::fill_randomizer(const SpawnCharacterContext* character, int32_t level,
                                     int32_t level_bonus, int32_t rarity_override, int32_t flags,
                                     const SpawnContext& context) {
    int32_t effective_level = -1;
    bool is_hardcore = false;
    int32_t delta_min = 0;
    int32_t delta_max = 0;

    if (character == nullptr) {
        if (level > 0) {
            effective_level = std::max(1, level + level_bonus);
        } else {
            effective_level = -1;
        }
    } else {
        // With a character the level cache keys are invalidated (0xfffffc19, -999).
        cached_level_ = -999;
        cached_level_bonus_ = -999;
        is_hardcore = character->is_hardcore;
        int32_t base_level = (level > 0) ? level : std::max(1, character->level);
        if (character->is_merchant) {
            delta_min = -4;
            delta_max = 4;
        }
        effective_level = std::max(1, base_level + level_bonus);
    }

    for (size_t i = 0; i < units_.size(); ++i) {
        const auto& unit = units_[i];
        int32_t rarity = unit.rarity;
        if (is_hardcore && unit.rarity_hardcore != -1) {
            rarity = unit.rarity_hardcore;
        }
        if (rarity_override != -1) {
            rarity = rarity_override;
        }

        // No level: straight to the loop tail - no type split, no flag checks, rarity != 0.
        if (effective_level == -1) {
            if (rarity != 0) {
                normal_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
            }
            continue;
        }

        // Not `EQUIPMENT`: gated by MINLEVEL/MAXLEVEL in either
        // order (or a rarity override), normal randomizer only.
        if (!unit.is_equipment()) {
            // a `MAXLEVEL` of 0 is 99999.
            const int32_t max_level = unit.max_level == 0 ? 99999 : unit.max_level;
            const int32_t low = std::min(unit.min_level, max_level);
            const int32_t high = std::max(unit.min_level, max_level);
            if (((low <= effective_level && effective_level <= high) || rarity_override != -1) &&
                rarity != 0) {
                normal_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
            }
            continue;
        }

        // Equipment: its own range, or the `ITEM_SPAWN_RANGE` graphs at its `LEVEL` without a
        // `MAXLEVEL`; a merchant widens it by 4.
        bool eligible = false;
        if ((flags & 0x10) != 0) {
            // IGNORE_RANGE (0x10)
            eligible = true;
        } else {
            int32_t min_lvl = unit.min_level;
            int32_t max_lvl = unit.max_level;
            if (max_lvl == 0 && context.stats != nullptr) {
                // the two curves at the unit's
                // `LEVEL`, at least 1.
                const float at = static_cast<float>(std::max(unit.level, 1));
                min_lvl = static_cast<int32_t>(
                    std::floor(context.stats->value("ITEM_SPAWN_RANGE_MINIMUM", at)));
                max_lvl = static_cast<int32_t>(
                    std::ceil(context.stats->value("ITEM_SPAWN_RANGE_MAXIMUM", at)));
            }
            if (min_lvl + delta_min <= effective_level && effective_level <= max_lvl + delta_max) {
                eligible = true;
            }
        }

        if (!eligible || rarity <= 0) {
            continue;
        }

        uint32_t type_id = UnitTypeHierarchy::kUnknown;
        if (context.unit_types != nullptr && !unit.unittype.empty()) {
            type_id = context.unit_types->id_of(unit.unittype);
        }

        auto isa = [&](uint32_t want) -> bool {
            if (context.unit_types == nullptr) {
                return false;
            }
            if (type_id != UnitTypeHierarchy::kUnknown) {
                return context.unit_types->isa(type_id, want);
            }
            if (!unit.unittype.empty()) {
                return context.unit_types->isa(unit.unittype, want);
            }
            return false;
        };

        if (isa(54 /* 0x36 UNIQUE */)) {
            if ((flags & 2) == 0) {  // NO_UNIQUE_ITEMS
                unique_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
            }
        } else if (isa(55 /* 0x37 MAGIC */)) {
            if ((flags & 4) == 0) {  // NO_MAGIC_ITEMS
                magic_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
            }
        } else {
            if ((flags & 1) != 0) {  // FORCEENCHANT
                // Equipment categories: 8 Weapon, 13 Armor, 17 Ring, 160, 24 Necklace
                const bool is_equip = isa(8) || isa(13) || isa(17) || isa(160) || isa(24);
                if (is_equip) {
                    const uint32_t choice_idx =
                        normal_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
                    equipment_indices_.push_back(static_cast<int32_t>(choice_idx));
                }
            } else {
                normal_randomizer_.add_choice(static_cast<int32_t>(i), rarity);
            }
        }
    }
}

// CSpawnClassData::rollSpawnClassData
void SpawnClassData::roll_spawn_class_data(std::vector<std::string>& out_units,
                                           std::vector<bool>* out_flags,
                                           const SpawnCharacterContext* character,
                                           const SpawnCharacterContext* finder, int32_t level,
                                           int32_t level_bonus, int32_t rarity_override,
                                           int32_t magic_find, uint32_t flags, RunicRandom& rng,
                                           const SpawnContext& context) {
    if (sub_class_ != nullptr) {
        sub_class_->roll_spawn_class(out_units, out_flags, character, finder, level, 0xffffffffu,
                                     level_bonus, rarity_override, magic_find, flags, rng, context);
        return;
    }

    calculate_choices(character, level, level_bonus, rarity_override, static_cast<int32_t>(flags),
                      context);
    if (normal_randomizer_.choice_count() == 0 && magic_randomizer_.choice_count() == 0 &&
        unique_randomizer_.choice_count() == 0) {
        return;
    }

    // Magic find, truncated to unsigned.
    uint32_t finder_mf = 0;
    if (finder != nullptr) {
        finder_mf = static_cast<uint32_t>(std::max(0.0f, finder->magic_find));
    }
    const uint32_t param_mf = static_cast<uint32_t>(std::max(0, magic_find));
    const uint32_t total_mf = finder_mf + param_mf;
    const bool apply_mf = (finder_mf != 0 || param_mf != 0);

    Randomizer rarity_picker(RandomizerType::kNormal);
    if (unique_randomizer_.choice_count() != 0) {
        int32_t weight = context.unique_item_weight;
        if (apply_mf) {
            // ((mf * influence) * 0.01f) * weight,
            // rounded up.
            const float bonus = std::ceil(
                ((static_cast<float>(total_mf) * context.magicfind_unique_influence) * 0.01f) *
                static_cast<float>(context.unique_item_weight));
            weight += static_cast<int32_t>(bonus);
        }
        rarity_picker.add_choice(2, weight);
    }
    if (magic_randomizer_.choice_count() != 0) {
        int32_t weight = context.magic_item_weight;
        if (apply_mf) {
            const float bonus = std::ceil(
                ((static_cast<float>(total_mf) * context.magicfind_magic_influence) * 0.01f) *
                static_cast<float>(context.magic_item_weight));
            weight += static_cast<int32_t>(bonus);
        }
        rarity_picker.add_choice(1, weight);
    }
    if (normal_randomizer_.choice_count() != 0) {
        rarity_picker.add_choice(0, context.normal_item_weight);
    }

    if (rarity_picker.choice_count() == 0) {
        return;
    }

    const int32_t picked_rarity = rarity_picker.get_random(rng);
    int32_t local_choice_index = -1;
    int32_t unit_idx = 0;
    if (picked_rarity == 1) {
        unit_idx = magic_randomizer_.get_random(rng);
    } else if (picked_rarity == 2) {
        unit_idx = unique_randomizer_.get_random(rng);
    } else {
        unit_idx = normal_randomizer_.get_random(rng, &local_choice_index);
    }

    if (unit_idx < 0 || static_cast<size_t>(unit_idx) >= units_.size()) {
        return;
    }

    out_units.push_back(units_[static_cast<size_t>(unit_idx)].name);

    if (out_flags != nullptr) {
        if (picked_rarity == 0 && !equipment_indices_.empty()) {
            bool found_enchant = false;
            for (int32_t eq_idx : equipment_indices_) {
                if (eq_idx == local_choice_index) {
                    found_enchant = true;
                    break;
                }
            }
            out_flags->push_back(found_enchant);
        } else {
            out_flags->push_back(false);
        }
    }
}

// --------------------------------------------------------------------------
// SpawnClass

// CSpawnClass::addSpawnClassData
void SpawnClass::add_spawn_class_data(SpawningData spawning_data,
                                      std::shared_ptr<SpawnClassData> data) {
    entries_.push_back(SpawnClassEntry{spawning_data, std::move(data)});
}

// CSpawnClass::calculateNumberOfChoices
int32_t SpawnClass::calculate_number_of_choices(const SpawnCharacterContext* character,
                                                int32_t level, int32_t level_bonus,
                                                int32_t rarity_override, int32_t flags,
                                                const SpawnContext& context) const {
    int32_t total = 0;
    for (const auto& entry : entries_) {
        if (entry.data != nullptr) {
            total += entry.data->calculate_choices(character, level, level_bonus, rarity_override,
                                                   flags, context);
        }
    }
    return total;
}

// CSpawnClass::rollSpawnClass
uint32_t SpawnClass::roll_spawn_class(std::vector<std::string>& out_units,
                                      std::vector<bool>* out_flags,
                                      const SpawnCharacterContext* character,
                                      const SpawnCharacterContext* finder, int32_t level,
                                      uint32_t seed, int32_t level_bonus, int32_t rarity_override,
                                      int32_t magic_find, uint32_t flags, RunicRandom& rng,
                                      const SpawnContext& context) const {
    Randomizer weighted(RandomizerType::kNormal);
    const bool flag_bit = (flags & 1) != 0;

    for (size_t j = 0; j < entries_.size(); ++j) {
        const auto& entry = entries_[j];
        if (entry.spawning_data.weight == -1) {
            // the count drawn inline, at least 1, and 1 when the
            // caller's flags carry 0x20.
            int copies =
                rng.integer_between(entry.spawning_data.min_count, entry.spawning_data.max_count);
            if (copies < 1) copies = 1;
            if ((flags & 0x20u) != 0) copies = 1;
            for (int c = 0; c < copies; ++c) {
                if (entry.data != nullptr && entry.data->sub_class() == nullptr &&
                    entry.data->units().size() == 1) {
                    out_units.push_back(entry.data->units()[0].name);
                    if (out_flags != nullptr) {
                        out_flags->push_back(flag_bit);
                    }
                } else if (entry.data != nullptr) {
                    entry.data->roll_spawn_class_data(
                        out_units, out_flags, character, finder, level,
                        std::max(entry.spawning_data.level_bonus, level_bonus),
                        std::max(entry.spawning_data.rarity_override, rarity_override),
                        std::max(static_cast<int32_t>(entry.spawning_data.magic_find), magic_find),
                        flags | entry.spawning_data.flags, rng, context);
                }
            }
        } else {
            weighted.add_choice(static_cast<int32_t>(j), entry.spawning_data.weight);
        }
    }

    if (weighted.choice_count() != 0) {
        if (seed != 0xffffffffu) {
            Randomizer::set_random_seed(rng, seed);
        }
        while (weighted.choice_count() != 0) {
            const uint32_t pick = static_cast<uint32_t>(weighted.get_random(rng));
            weighted.remove_choice(pick);
            if (pick >= entries_.size()) continue;
            const auto& picked = entries_[pick];
            if (picked.data == nullptr) continue;

            // the entry's flags or-ed with the caller's.
            const int32_t choices = picked.data->calculate_choices(
                character, level, picked.spawning_data.level_bonus,
                picked.spawning_data.rarity_override,
                static_cast<int32_t>(flags | picked.spawning_data.flags), context);

            if (choices != 0) {
                int copies = rng.integer_between(picked.spawning_data.min_count,
                                                 picked.spawning_data.max_count);
                if (copies < 1) copies = 1;
                for (int c = 0; c < copies; ++c) {
                    picked.data->roll_spawn_class_data(
                        out_units, out_flags, character, finder, level,
                        std::max(picked.spawning_data.level_bonus, level_bonus),
                        std::max(picked.spawning_data.rarity_override, rarity_override),
                        std::max(static_cast<int32_t>(picked.spawning_data.magic_find), magic_find),
                        flags | picked.spawning_data.flags, rng, context);
                }
                break;  // Jump to epilogue (jmp a75608)
            }
            // a picked `UNITTYPE:NONE` ends the roll with nothing; any other entry
            // without a choice is passed over.
            if (picked.data->is_none()) break;
        }
    }

    return static_cast<uint32_t>(out_units.size());
}

// --------------------------------------------------------------------------
// Parser helpers (CSpawnClassParser)

// CSpawnClassParser::createSpawnClassData
std::shared_ptr<SpawnClassData> create_spawn_class_data(
    const tl::formats::AdmSection& object_section, const SpawnContext& context) {
    const std::string unittype = object_section.string_or("UNITTYPE", "");
    const std::string unit = object_section.string_or("UNIT", "");
    const std::string spawnclass = object_section.string_or("SPAWNCLASS", "");

    if (!unittype.empty()) {
        return create_spawn_class_data_by_unit_type(unittype, object_section, context);
    }
    if (!unit.empty()) {
        return create_spawn_class_data_with_unit(unit, object_section, context);
    }
    if (!spawnclass.empty()) {
        return create_spawn_class_data_ref_spawn_class(spawnclass, object_section, context);
    }
    return nullptr;
}

// CSpawnClassParser::createSpawnClassDataByUnitType
//
std::shared_ptr<SpawnClassData> create_spawn_class_data_by_unit_type(
    const std::string& type_name, const tl::formats::AdmSection& /*object_section*/,
    const SpawnContext& context) {
    const std::string upper_type = to_upper(type_name);
    uint32_t type_id = UnitTypeHierarchy::kUnknown;
    if (context.unit_types != nullptr) {
        type_id = context.unit_types->id_of(upper_type);
        if (type_id == UnitTypeHierarchy::kUnknown) {
            return nullptr;
        }
    }
    // `NONE` is marked and asks `CUnitResourceList` for nothing; any other type with no units
    // makes no entry.
    auto data = std::make_shared<SpawnClassData>(type_name);
    if (type_id == unit_type::kNone) {
        data->set_none();
        return data;
    }
    // A caller without a unit list (unit tests) gets an empty entry.
    if (context.get_units_by_type) {
        std::vector<SpawnUnitData> units = context.get_units_by_type(type_id);
        if (units.empty()) return nullptr;
        data->add_unit_data_objects(units);
    }
    return data;
}

// CSpawnClassParser::createSpawnClassDataWithUnit
//
std::shared_ptr<SpawnClassData> create_spawn_class_data_with_unit(
    const std::string& unit_name, const tl::formats::AdmSection& /*object_section*/,
    const SpawnContext& context) {
    auto data = std::make_shared<SpawnClassData>(unit_name);
    if (context.get_unit_by_name) {
        const SpawnUnitData* u = context.get_unit_by_name(unit_name);
        if (u != nullptr) {
            data->add_unit_data_object(*u);
        } else {
            SpawnUnitData fallback;
            fallback.name = unit_name;
            data->add_unit_data_object(std::move(fallback));
        }
    } else {
        SpawnUnitData fallback;
        fallback.name = unit_name;
        data->add_unit_data_object(std::move(fallback));
    }
    return data;
}

// CSpawnClassParser::createSpawnClassDataRefSpawnClass
//
std::shared_ptr<SpawnClassData> create_spawn_class_data_ref_spawn_class(
    const std::string& class_name, const tl::formats::AdmSection& /*object_section*/,
    const SpawnContext& context) {
    if (context.get_spawn_class) {
        const SpawnClass* sub = context.get_spawn_class(class_name);
        if (sub != nullptr) {
            auto data = std::make_shared<SpawnClassData>(class_name);
            data->add_spawn_class(sub);
            return data;
        }
    }
    return nullptr;
}

// CSpawnClassParser::fillEmptySpawnClass
SpawnClass parse_spawn_class(const tl::formats::AdmSection& root, const SpawnContext& context) {
    SpawnClass sc(root.string_or("NAME", ""));
    for (const auto& child : root.children) {
        if (child.name != "OBJECT") continue;
        SpawningData sd;
        if (const auto* rec = child.find("WEIGHT")) {
            int32_t w = static_cast<int32_t>(rec->integer);
            if (w == 0) w = -1;
            sd.weight = w;
        } else {
            sd.weight = -1;
        }
        if (const auto* rec = child.find("RARITY_OVERRIDE")) {
            sd.rarity_override = static_cast<int32_t>(rec->integer);
        }
        if (child.bool_or("FORCEENCHANT", false)) sd.flags |= 1;
        if (child.bool_or("NO_UNIQUE_ITEMS", false)) sd.flags |= 2;
        if (child.bool_or("NO_MAGIC_ITEMS", false)) sd.flags |= 4;
        if (child.bool_or("IGNORE_RANGE", false)) sd.flags |= 0x10;
        sd.min_count = static_cast<int32_t>(child.integer_or("MINCOUNT", 1));
        sd.max_count = static_cast<int32_t>(child.integer_or("MAXCOUNT", 1));
        sd.level_bonus = static_cast<int32_t>(child.integer_or("LEVEL_BONUS", 0));
        sd.magic_find = child.float_or("MAGICFIND", 0.0f);

        auto data = create_spawn_class_data(child, context);
        if (data != nullptr) {
            sc.add_spawn_class_data(sd, std::move(data));
        }
    }
    return sc;
}

}  // namespace tl::core
