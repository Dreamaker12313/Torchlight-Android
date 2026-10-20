#include "core/affix_book.h"

#include "core/randomizer.h"

#include <utility>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper_ascii(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return text;
}

}  // namespace

void AffixBook::clear() {
    // `CEffectGroupManager::clean`: the
    // cached lists first, then every affix.
    candidate_cache_.clear();
    entries_.clear();
    index_.clear();
    made_.clear();
}

bool AffixBook::add(Affix affix) {
    // The console's folder load (under `reload`): keyed by
    // the upper-cased `NAME` ("NA" by default); a later file of the same name replaces the
    // earlier.
    std::string key = upper_ascii(affix.name.empty() ? std::string("NA") : affix.name);
    // Every file keeps its own row in the index tables the roll walks; the
    // cached candidate lists hold row numbers, so they outlive an add.
    entries_.push_back(std::move(affix));
    made_.push_back(0);
    const auto [it, inserted] = index_.insert_or_assign(std::move(key), entries_.size() - 1);
    (void)it;
    return inserted;
}

const Affix* AffixBook::get_affix(const std::string& name) const {
    // `CEffectGroupManager::getAffix`.
    const auto it = index_.find(upper_ascii(name));
    return it == index_.end() ? nullptr : &entries_[it->second];
}

// PORT: an affix's row in `AFFIXES.RAW`'s order, by name.
int32_t AffixBook::row_of(const std::string& name) const {
    const auto it = index_.find(upper_ascii(name));
    return it == index_.end() ? -1 : static_cast<int32_t>(it->second);
}

void AffixBook::make(size_t index, const EffectRolls& rolls) const {
    // `CEffectGroupManager::getAffix` and the pick in
    // `rollAndCreateRandomAffixes` read a row on first use;
    // each `[EFFECT]` ends in a roll (`CEffect::CEffect`).
    //
    if (index >= entries_.size() || made_[index] != 0) return;
    made_[index] = 1;
    for (const Effect& authored : entries_[index].effects) {
        Effect effect = authored;
        rolls.roll(effect);
    }
}

std::vector<std::string> AffixBook::affix_names() const {
    // `CEffectGroupManager::getAffixNames`.
    std::vector<std::string> names;
    names.reserve(index_.size());
    for (const auto& entry : index_) names.push_back(entry.first);
    return names;
}

bool affix_can_be_applied_to_unit_type(const Affix& affix, uint32_t type,
                                       const UnitTypeHierarchy& unit_types) {
    // `CAffix::canBeAppliedToUnitType`.
    for (uint32_t allowed : affix.unit_types) {
        if (unit_types.isa(type, allowed)) return true;
    }
    return false;
}

Affix clone_affix_at_level(const Affix& affix, uint32_t level) {
    // `CAffix::CAffix(CAffix*, unsigned int)`,
    // without the rolls.
    return copy_affix(affix, level);
}

std::vector<Affix> AffixBook::roll_and_create_random_affixes(uint32_t type, uint32_t level,
                                                             int32_t budget, RunicRandom& rng,
                                                             const UnitTypeHierarchy& unit_types,
                                                             const EffectRolls& rolls) const {
    // `CEffectGroupManager::rollAndCreateRandomAffixes`.
    // No early return on the budget: the type's list is
    // built first, on its first roll, and kept: every row in
    // load order (a duplicate name is two rows) whose unit types the type ISAs **every one
    // of** (not `canBeAppliedToUnitType`'s any-of; an empty list passes), with a `WEIGHT`.
    auto cached = candidate_cache_.find(type);
    if (cached == candidate_cache_.end()) {
        std::vector<size_t> list;
        for (size_t row = 0; row < entries_.size(); ++row) {
            const Affix& entry = entries_[row];
            bool every = true;
            for (const uint32_t listed : entry.listed_unit_types) {
                if (!unit_types.isa(type, listed)) {
                    every = false;
                    break;
                }
            }
            if (!every || entry.weight == 0) continue;
            list.push_back(row);
        }
        cached = candidate_cache_.emplace(type, std::move(list)).first;
    }
    const std::vector<size_t>& candidates = cached->second;

    std::vector<Affix> out;
    Randomizer picker(RandomizerType::kNormal);
    const int32_t at = static_cast<int32_t>(level);
    for (size_t i = 0; i < candidates.size(); ++i) {
        const Affix& affix = entries_[candidates[i]];
        if (affix.min_spawn_range <= at && at < affix.max_spawn_range) {
            picker.add_choice(static_cast<int32_t>(i), affix.weight);
        }
    }

    while (picker.choice_count() != 0 && 0 < budget) {
        // `CRandomizer::getRandom` draws
        // once; `removeChoice` swaps the last
        // choice into the picked one's place.
        const int32_t pick = picker.get_random(rng);
        picker.remove_choice(static_cast<uint32_t>(pick));
        // The row's file made on first use, then `new CAffix(row,
        // level)`.
        const size_t row = tarray_at(candidates, static_cast<size_t>(pick));
        make(row, rolls);
        Affix clone = copy_affix(entries_[row], level, rolls);
        // Too big while other choices remain: the clone is deleted and nothing
        // is spent. The last choice is taken whatever it costs.
        if (budget < clone.slots_occupy && picker.choice_count() != 0) continue;
        budget -= clone.slots_occupy;
        out.push_back(std::move(clone));
    }
    return out;
}

std::optional<Affix> new_affix_by_name(const AffixBook& book, const std::string& name,
                                       uint32_t level, const EffectRolls& rolls) {
    // `CResourceManager::getNewAffixByName`:
    // `getAffix` (which makes the row), then `new CAffix(affix, level)`.
    const int32_t row = book.row_of(name);
    if (row < 0) return std::nullopt;
    book.make(static_cast<size_t>(row), rolls);
    return copy_affix(book.row(static_cast<size_t>(row)), level, rolls);
}

}  // namespace tl::core
