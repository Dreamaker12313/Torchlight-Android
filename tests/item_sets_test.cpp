// `CSets` and `CSet`, loaded from hand-made files shaped as `media/sets/*.dat` compiles.
#include "core/item_sets.h"
#include "core/affix_book.h"
#include "core/item_roll.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::Affix;
using tl::core::AffixBook;
using tl::core::ItemSet;
using tl::core::ItemSetBook;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord integer(const std::string& key, uint32_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Integer;
    r.integer = value;
    return r;
}

/// One `[AFFIX]` block; `count`/`level` of 0 leave the key out.
AdmSection bonus(const std::string& affix, uint32_t count, uint32_t level) {
    AdmSection block;
    block.name = "AFFIX";
    block.records.push_back(str("AFFIX", affix));
    if (count != 0) block.records.push_back(integer("COUNT", count));
    if (level != 0) block.records.push_back(integer("AFFIXLEVEL", level));
    return block;
}

AdmFile set_file(const std::string& name, const std::string& display,
                 const std::vector<AdmSection>& bonuses) {
    AdmFile file;
    file.ok = true;
    file.root.name = "SET";
    file.root.records.push_back(str("NAME", name));
    file.root.records.push_back(str("DISPLAYNAME", display));
    file.root.children = bonuses;
    return file;
}

/// A book with two affixes. `SPAWN` 0..100 so a clone keeps its level.
AffixBook book() {
    AffixBook out;
    for (const char* name : {"SET_STR", "SET_DEX"}) {
        Affix affix;
        affix.name = name;
        affix.min_spawn_range = 0;
        affix.max_spawn_range = 100;
        out.add(affix);
    }
    return out;
}

TL_TEST(item_set_reads_its_bonuses_with_the_defaults_and_drops_an_unknown_affix) {
    const AffixBook affixes = book();
    // COUNT and AFFIXLEVEL absent default to 1; an affix the book does
    // not hold is dropped.
    const AdmFile file =
        set_file("Mercenary", "Mercenary's Garb",
                 {bonus("SET_STR", 0, 0), bonus("NO_SUCH_AFFIX", 2, 3), bonus("SET_DEX", 3, 7)});
    const ItemSet set = tl::core::load_item_set(file.root, affixes);
    TL_CHECK_EQ(set.name, std::string("Mercenary"));
    TL_CHECK_EQ(set.display_name, std::string("Mercenary's Garb"));
    TL_CHECK_EQ(set.bonuses.size(), 2u);
    TL_CHECK_EQ(set.bonuses[0].affix_name, std::string("SET_STR"));
    TL_CHECK_EQ(set.bonuses[0].count, 1);
    TL_CHECK_EQ(set.bonuses[0].level, 1u);
    TL_CHECK_EQ(set.bonuses[1].affix_name, std::string("SET_DEX"));
    TL_CHECK_EQ(set.bonuses[1].count, 3);
    TL_CHECK_EQ(set.bonuses[1].level, 7u);
    // The entry holds the book's affix cloned at the bonus level.
    TL_CHECK_EQ(set.bonuses[1].affix.name, std::string("SET_DEX"));
}

TL_TEST(item_set_book_looks_names_up_exactly_and_by_index) {
    const AffixBook affixes = book();
    const AdmFile first = set_file("Mercenary", "A", {bonus("SET_STR", 2, 1)});
    const AdmFile second = set_file("Warlord", "B", {bonus("SET_DEX", 2, 1)});
    ItemSetBook sets;
    sets.reload({&first, &second}, affixes);
    TL_CHECK_EQ(sets.size(), 2u);
    TL_CHECK(sets.get_set("Warlord") == sets.get_set(1u));
    TL_CHECK(sets.get_set("Mercenary") == sets.get_set(0u));
    // A `wmemcmp` key: another case is another name, and a miss is null.
    TL_CHECK(sets.get_set("warlord") == nullptr);
    TL_CHECK(sets.get_set("Nobody") == nullptr);
    TL_CHECK(sets.get_set(2u) == nullptr);
}

TL_TEST(item_set_book_indexes_a_duplicate_name_to_the_later_file_and_lists_both) {
    const AffixBook affixes = book();
    const AdmFile first = set_file("Mercenary", "first", {bonus("SET_STR", 2, 1)});
    const AdmFile second = set_file("Mercenary", "second", {bonus("SET_DEX", 2, 1)});
    ItemSetBook sets;
    sets.reload({&first, &second}, affixes);
    // Both are on the list (+0x20 counts two) ...
    TL_CHECK_EQ(sets.size(), 2u);
    TL_CHECK_EQ(sets.get_set(0u)->display_name, std::string("first"));
    // ... and the map node's value was overwritten with the second's index.
    TL_CHECK_EQ(sets.get_set("Mercenary")->display_name, std::string("second"));
}

TL_TEST(item_roll_data_reads_the_set_name_default_empty) {
    AdmSection section;
    section.name = "ITEM";
    {
        tl::core::ItemStats item;
        tl::core::read_item_roll_data(item, section);
        TL_CHECK(item.set.empty());
    }
    section.records.push_back(str("SET", "Mercenary"));
    tl::core::ItemStats item;
    tl::core::read_item_roll_data(item, section);
    TL_CHECK_EQ(item.set, std::string("Mercenary"));
}

}  // namespace
