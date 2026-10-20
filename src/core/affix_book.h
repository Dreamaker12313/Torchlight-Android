// The affix book: `CEffectGroupManager`, transcribed. Affix definitions by name,
// candidates per unit type, and weighted random rolls for a type, level and budget.
#pragma once

#include "core/effects.h"
#include "core/random.h"
#include "core/unit_types.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tl::core {

/// `CEffectGroupManager`'s affix map: a `std::map` keyed by the upper-cased
/// `NAME`, so it is **walked in name order** whatever order the files were read in.
///
/// `CEffectGroupManager::CEffectGroupManager(folder)`
/// reloads at once. `reload`
/// walks the file index and sets `map[name] = index` per file:
/// only duplicates depend on file order, and the last file of a name wins.
class AffixBook {
public:
    AffixBook() = default;
    /// A copy starts with an empty candidate cache: the cached lists point into
    /// the source's own map.
    AffixBook(const AffixBook& other)
        : entries_(other.entries_), index_(other.index_), made_(other.made_) {}
    AffixBook& operator=(const AffixBook& other) {
        if (this != &other) {
            entries_ = other.entries_;
            index_ = other.index_;
            made_ = other.made_;
            candidate_cache_.clear();
        }
        return *this;
    }
    AffixBook(AffixBook&&) = default;
    AffixBook& operator=(AffixBook&&) = default;

    /// Number of distinct affixes in the book.
    size_t size() const { return index_.size(); }
    bool empty() const { return index_.empty(); }

    /// `CEffectGroupManager::clean`: clears
    /// the candidate cache and the affix map.
    void clear();

    /// One file's worth of the console's folder load.
    /// Returns false when the name was already taken and its affix replaced.
    bool add(Affix affix);

    /// `CEffectGroupManager::getAffix`:
    /// upper-cases `name` and returns the stored affix itself, or null.
    const Affix* get_affix(const std::string& name) const;

    /// `CEffectGroupManager::getAffixNames`: the upper-cased names,
    /// in the map's order.
    std::vector<std::string> affix_names() const;

    /// `CEffectGroupManager::rollAndCreateRandomAffixes`.
    /// The candidates for `type` (built on its
    /// first roll whatever the budget, kept until `clear()`) with
    /// `MIN_SPAWN_RANGE <= level < MAX_SPAWN_RANGE` go into a `CRandomizer` at their
    /// weight; while it has choices and `budget` is above 0, one `getRandom` picks,
    /// removes and clones one at `level`. A clone over what is left is dropped
    /// **unless it was the last choice**, which is taken and the budget goes
    /// negative. Returns the clones in pick order. With `rolls` on, each pick also
    /// draws `g_RandVolatile` as the console does (`make`, then `copy_affix`).
    std::vector<Affix> roll_and_create_random_affixes(
        uint32_t type, uint32_t level, int32_t budget, RunicRandom& rng,
        const UnitTypeHierarchy& unit_types, const EffectRolls& rolls = EffectRolls{}) const;

    /// The row `get_affix` answers with, or -1.
    int32_t row_of(const std::string& name) const;
    const Affix& row(size_t index) const { return entries_[index]; }
    size_t rows() const { return entries_.size(); }

    /// Whether a row's `CAffix` has been read: on first use (`getAffix`,
    /// a pick) it is, which rolls each `[EFFECT]`'s
    /// value once (`CEffect::CEffect(CDataGroup*, ...)`). `make`
    /// does that for a row not yet made.
    bool made(size_t index) const { return index < made_.size() && made_[index] != 0; }
    void set_made(size_t index, bool made) const {
        if (index < made_.size()) made_[index] = made ? 1 : 0;
    }
    void make(size_t index, const EffectRolls& rolls) const;

private:
    /// One per file, in the folder load's order: a duplicate name is two entries.
    std::vector<Affix> entries_;
    /// `+0x08`: the upper-cased name to its last file's index.
    std::map<std::string, size_t> index_;

    /// +0x38, one per row: made (`make`).
    mutable std::vector<uint8_t> made_;
    /// +0x48: candidate lists keyed by unit type id. Emptied by `clear()`.
    /// The pointers stay valid across `add`: map nodes do not move.
    mutable std::map<uint32_t, std::vector<size_t>> candidate_cache_;  ///< rows of `entries_`
};

/// `CAffix::canBeAppliedToUnitType(unsigned int)`:
/// any of the affix's unit types that `type`
/// ISAs; an empty list accepts nothing.
bool affix_can_be_applied_to_unit_type(const Affix& affix, uint32_t type,
                                       const UnitTypeHierarchy& unit_types);

/// `CAffix::CAffix(CAffix*, unsigned int)`:
/// `clone`, which copies the whole affix,
/// then `setLevel` unless the level is -1.
Affix clone_affix_at_level(const Affix& affix, uint32_t level);

/// `CResourceManager::getNewAffixByName`: the
/// named affix cloned at `level`, or nothing when the book has no such name.
/// With `rolls` on, the row is made first (`AffixBook::make`, `getAffix`) and
/// the clone rolls as `copy_affix` says.
std::optional<Affix> new_affix_by_name(const AffixBook& book, const std::string& name,
                                       uint32_t level, const EffectRolls& rolls = EffectRolls{});

}  // namespace tl::core
