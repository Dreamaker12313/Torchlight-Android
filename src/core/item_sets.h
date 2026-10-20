// Item sets: `CSets` and `CSet`, transcribed.
//
// One `CSet` per file under `media/sets/`: a `NAME`, a `DISPLAYNAME`, and one
// bonus per `[AFFIX]` block - an affix, the level it is applied at, and how many
// pieces of the set must be worn for it. An item joins a set through its own
// `SET` key (`ItemStats::set`); what the pieces worn are worth is the
// inventory's business (`Inventory::calculate_effect_values`, `core/inventory.h`).
//
// No OGRE and no file I/O: the caller parses the files and hands them over in
// `CFileSystem::getFileList`'s order.
#pragma once

#include "core/affix_book.h"
#include "core/effects.h"
#include "formats/adm.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// One `[AFFIX]` block of a set (0x18 bytes, allocated in `CSet::load`).
struct ItemSetBonus {
    /// +0x00: the book's affix cloned at `level` (`CAffix::CAffix(CAffix*,
    /// unsigned)`). Nothing in the effect pass reads
    /// it - the inventory adds the bonus by name - so it is what a description
    /// would print.
    Affix affix;
    std::string affix_name;  ///< +0x08, `AFFIX`, as authored
    uint32_t level = 1;      ///< +0x10, `AFFIXLEVEL`, default 1
    int32_t count = 1;       ///< +0x14, `COUNT`, default 1: pieces worn
};

/// `CSet` (0x30 bytes).
struct ItemSet {
    std::vector<ItemSetBonus> bonuses;  ///< +0x08 (count +0x10, capacity +0x14)
    std::string name;                   ///< +0x20, `NAME`, default empty
    std::string display_name;           ///< +0x28, `DISPLAYNAME`, default empty
};

/// `CSet::load(CDataGroup*, CEffectGroupManager*)`:
/// `NAME`, `DISPLAYNAME`, then every
/// `[AFFIX]` child in file order. A bonus whose `AFFIX` the book does not know
/// (`CEffectGroupManager::getAffix`) is dropped.
ItemSet load_item_set(const tl::formats::AdmSection& root, const AffixBook& affixes);

/// `CSets` (`g_pSets`): every set, in load order, and the index by name.
class ItemSetBook {
public:
    /// `CSets::reload(CEffectGroupManager*)`,
    /// which `CSets::CSets` runs once: the list is emptied, then one
    /// set per file, in the order given. **The name index is not emptied** -
    /// the original never clears its map - so a name a previous load knew keeps
    /// its old index.
    void reload(const std::vector<const tl::formats::AdmFile*>& files, const AffixBook& affixes);

    /// One file's worth of `reload`: the set goes on the list, and its `NAME`
    /// is indexed to its position. **A duplicate `NAME` takes the later
    /// position** (the map node is found or inserted, then its value
    /// overwritten); both sets stay on the list.
    void add(ItemSet set);

    /// `CSets::getSet(const wchar_t*)`: an
    /// exact, case-sensitive lookup, null for an unknown name or an index past
    /// the list.
    const ItemSet* get_set(const std::string& name) const;
    /// `CSets::getSet(unsigned)`.
    const ItemSet* get_set(uint32_t index) const;

    // PORT: an accessor.
    size_t size() const { return sets_.size(); }
    const std::vector<ItemSet>& sets() const { return sets_; }

private:
    std::vector<ItemSet> sets_;                ///< +0x18 (count +0x20)
    std::map<std::string, uint32_t> by_name_;  ///< +0x30
};

}  // namespace tl::core
