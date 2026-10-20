// `CSets` / `CSet`.
#include "core/item_sets.h"

namespace tl::core {

ItemSet load_item_set(const tl::formats::AdmSection& root, const AffixBook& affixes) {
    // `CSet::load(CDataGroup*, CEffectGroupManager*)`.
    //
    ItemSet set;
    set.name = root.string_or("NAME", "");
    set.display_name = root.string_or("DISPLAYNAME", "");
    // `GetDataGroupsMatchingName(L"AFFIX")`, the children in file order. The
    // list's grow step (`+0x18`, the block count plus one) changes nothing
    // anyone can observe and is not modelled.
    for (const tl::formats::AdmSection& group : root.children) {
        if (group.name != "AFFIX") continue;
        const std::string name = group.string_or("AFFIX", "");
        const int32_t count = static_cast<int32_t>(group.integer_or("COUNT", 1));
        const uint32_t level = group.integer_or("AFFIXLEVEL", 1);
        const Affix* affix = affixes.get_affix(name);
        if (affix == nullptr) continue;
        ItemSetBonus bonus;
        bonus.affix = clone_affix_at_level(*affix, level);
        bonus.affix_name = name;
        bonus.level = level;
        bonus.count = count;
        set.bonuses.push_back(std::move(bonus));
    }
    return set;
}

void ItemSetBook::reload(const std::vector<const tl::formats::AdmFile*>& files,
                         const AffixBook& affixes) {
    // `CSets::reload(CEffectGroupManager*)`:
    // the list is freed and emptied; `+0x30`, the name map, is left as it was.
    sets_.clear();
    for (const tl::formats::AdmFile* file : files) {
        if (file == nullptr) continue;
        add(load_item_set(file->root, affixes));
    }
}

// `CSets::reload(CEffectGroupManager*)`'s per-file
// tail: `map[name] = count` whichever way the search went, then the set appended.
// PORT: the port's book of sets.
void ItemSetBook::add(ItemSet set) {
    by_name_[set.name] = static_cast<uint32_t>(sets_.size());
    sets_.push_back(std::move(set));
}

const ItemSet* ItemSetBook::get_set(const std::string& name) const {
    // `CSets::getSet(const wchar_t*)`.
    const auto it = by_name_.find(name);
    if (it == by_name_.end()) return nullptr;
    return get_set(it->second);
}

const ItemSet* ItemSetBook::get_set(uint32_t index) const {
    // `CSets::getSet(unsigned)`.
    return index < sets_.size() ? &sets_[index] : nullptr;
}

}  // namespace tl::core
