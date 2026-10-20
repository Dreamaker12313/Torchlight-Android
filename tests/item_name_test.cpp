// An item's full and plain name: CEquipment::getFullItemName and
// CEquipment::getItemName.
#include "core/item_name.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

namespace {

using tl::core::Affix;
using tl::core::ItemStats;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

UnitTypeHierarchy make_test_hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection types;
    types.name = "UNITTYPES";

    auto add_type = [&](const std::string& name, uint32_t id,
                        const std::vector<uint32_t>& parents) {
        AdmSection s;
        s.name = name;
        AdmRecord id_rec;
        id_rec.key = "ID";
        id_rec.type = AdmType::Integer;
        id_rec.integer = id;
        s.records.push_back(id_rec);
        for (size_t i = 0; i < parents.size(); ++i) {
            AdmRecord p_rec;
            p_rec.key = (i == 0) ? "child" : ("child" + std::to_string(i));
            p_rec.type = AdmType::Integer;
            p_rec.integer = parents[i];
            s.records.push_back(p_rec);
        }
        types.children.push_back(s);
    };

    add_type("any", 0, {});
    add_type("Weapon", 8, {0});
    add_type("Armor", 13, {0});
    add_type("UNIQUE", tl::core::unit_type::kUnique, {0});
    add_type("MAGIC", tl::core::unit_type::kMagic, {0});
    add_type("MAGIC SWORD", 301, {8, tl::core::unit_type::kMagic, 0});
    add_type("UNIQUE SWORD", 302, {8, tl::core::unit_type::kUnique, 0});

    root.children.push_back(types);
    UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

ItemStats make_test_item(const std::string& display_name,
                         const std::string& unittype = "MAGIC SWORD") {
    ItemStats item;
    item.name = "TEST_ITEM";
    item.display_name = display_name;
    item.base_name = display_name;
    item.unittype = unittype;
    item.identified = true;
    return item;
}

ItemStats::RolledAffix make_rolled_affix(const std::string& name, const std::string& prefix,
                                         const std::string& suffix, int rank) {
    Affix affix;
    affix.name = name;
    affix.prefix = prefix;
    affix.suffix = suffix;
    affix.rank = rank;
    return ItemStats::RolledAffix{affix, 1};
}

TL_TEST(item_name_is_the_unidentified_name_while_unidentified) {
    ItemStats item = make_test_item("Testblade");
    item.unidentified_name = "Unidentified Sword";
    item.identified = false;

    // While unidentified with a non-empty unidentified name, getItemName returns it.
    TL_CHECK_EQ(tl::core::item_name(item), std::string("Unidentified Sword"));

    // When identified, getItemName returns the base name.
    item.identified = true;
    TL_CHECK_EQ(tl::core::item_name(item), std::string("Testblade"));

    // When unidentified with an empty unidentified name, getItemName returns the base name.
    item.identified = false;
    item.unidentified_name.clear();
    TL_CHECK_EQ(tl::core::item_name(item), std::string("Testblade"));
}

TL_TEST(item_name_is_the_name_once_identified) {
    ItemStats item = make_test_item("Testblade");
    item.unidentified_name = "Mysterious Item";
    item.identified = true;
    TL_CHECK_EQ(tl::core::item_name(item), std::string("Testblade"));
}

TL_TEST(full_name_without_affixes_is_the_base_name) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Testblade");
    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Testblade"));
    TL_CHECK(item.name_part_2e0.empty());
    TL_CHECK(item.name_part_2e8.empty());
}

TL_TEST(full_name_takes_the_prefix_and_suffix) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Testblade");
    item.affixes.push_back(make_rolled_affix("TEST_AFFIX", "Glowing [ITEM]", "[ITEM] of Tests", 1));

    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Glowing Testblade of Tests"));
    TL_CHECK_EQ(item.name_part_2e0, std::string("Glowing [ITEM]"));
    TL_CHECK_EQ(item.name_part_2e8, std::string("[ITEM] of Tests"));
}

TL_TEST(full_name_picks_the_prefix_by_rank) {
    const UnitTypeHierarchy types = make_test_hierarchy();

    // 1. Differing ranks: lower rank first, higher rank second. Higher rank wins.
    {
        ItemStats item = make_test_item("Testblade");
        item.affixes.push_back(make_rolled_affix("LOW", "Sharp [ITEM]", "", 1));
        item.affixes.push_back(make_rolled_affix("HIGH", "Keen [ITEM]", "", 2));
        const std::string full = tl::core::item_full_name(item, false, types);
        TL_CHECK_EQ(full, std::string("Keen Testblade"));
        TL_CHECK_EQ(item.name_part_2e0, std::string("Keen [ITEM]"));
    }

    // 2. Differing ranks: higher rank first, lower rank second. Higher rank still wins.
    {
        ItemStats item = make_test_item("Testblade");
        item.affixes.push_back(make_rolled_affix("HIGH", "Keen [ITEM]", "", 2));
        item.affixes.push_back(make_rolled_affix("LOW", "Sharp [ITEM]", "", 1));
        const std::string full = tl::core::item_full_name(item, false, types);
        TL_CHECK_EQ(full, std::string("Keen Testblade"));
        TL_CHECK_EQ(item.name_part_2e0, std::string("Keen [ITEM]"));
    }

    // 3. Tie in rank: affix->rank <= best_rank causes jump past prefix assignment,
    // so the earlier affix of the highest rank wins.
    {
        ItemStats item = make_test_item("Testblade");
        item.affixes.push_back(make_rolled_affix("FIRST", "First [ITEM]", "", 2));
        item.affixes.push_back(make_rolled_affix("SECOND", "Second [ITEM]", "", 2));
        const std::string full = tl::core::item_full_name(item, false, types);
        TL_CHECK_EQ(full, std::string("First Testblade"));
        TL_CHECK_EQ(item.name_part_2e0, std::string("First [ITEM]"));
    }
}

TL_TEST(full_name_skips_a_negative_rank) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Testblade");
    item.affixes.push_back(make_rolled_affix("CURSED", "Cursed [ITEM]", "[ITEM] of Doom", -1));

    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Testblade"));
    TL_CHECK(item.name_part_2e0.empty());
    TL_CHECK(item.name_part_2e8.empty());
}

TL_TEST(full_name_replaces_item_placeholder) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Widget");
    item.affixes.push_back(make_rolled_affix("CUSTOM", "Super [ITEM] Deluxe", "", 1));

    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Super Widget Deluxe"));
    TL_CHECK_EQ(item.name_part_2e0, std::string("Super [ITEM] Deluxe"));
}

TL_TEST(full_name_leaves_a_set_item_alone) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Setblade");
    item.set = "TEST_SET";
    item.affixes.push_back(make_rolled_affix("AFFIX", "Glowing [ITEM]", "[ITEM] of Tests", 1));

    // A set item skips affix resolution and returns the base name.
    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Setblade"));
    TL_CHECK(item.name_part_2e0.empty());
    TL_CHECK(item.name_part_2e8.empty());
}

TL_TEST(full_name_of_a_unique) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Uniqueblade", "UNIQUE SWORD");
    item.affixes.push_back(make_rolled_affix("AFFIX", "Glowing [ITEM]", "[ITEM] of Tests", 1));

    // A unique item (ISA kUnique / 0x36) skips affix resolution and returns the base name.
    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Uniqueblade"));
    TL_CHECK(item.name_part_2e0.empty());
    TL_CHECK(item.name_part_2e8.empty());
}

TL_TEST(full_name_handles_the_brace_span) {
    const UnitTypeHierarchy types = make_test_hierarchy();

    // 1. Male tag in base name matches male tag in affix suffix.
    {
        ItemStats item = make_test_item("{MALE}Testblade");
        item.affixes.push_back(make_rolled_affix(
            "AFFIX", "", "[ITEM] {MALE}of the King{/MALE}{FEMALE}of the Queen{/FEMALE}", 1));

        const std::string full = tl::core::item_full_name(item, false, types);
        TL_CHECK_EQ(full, std::string("Testblade of the King"));
    }

    // 2. Female tag in base name matches female tag in affix suffix.
    {
        ItemStats item = make_test_item("{FEMALE}Testblade");
        item.affixes.push_back(make_rolled_affix(
            "AFFIX", "", "[ITEM] {MALE}of the King{/MALE}{FEMALE}of the Queen{/FEMALE}", 1));

        const std::string full = tl::core::item_full_name(item, false, types);
        TL_CHECK_EQ(full, std::string("Testblade of the Queen"));
    }
}

TL_TEST(full_name_keeps_the_saved_parts_without_affixes) {
    const UnitTypeHierarchy types = make_test_hierarchy();
    ItemStats item = make_test_item("Testblade");
    // Loaded copy from save: affixes are empty, but saved name parts are populated.
    item.name_part_2e0 = "Ancient [ITEM]";
    item.name_part_2e8 = "[ITEM] of Ages";

    const std::string full = tl::core::item_full_name(item, false, types);
    TL_CHECK_EQ(full, std::string("Ancient Testblade of Ages"));
    TL_CHECK_EQ(item.name_part_2e0, std::string("Ancient [ITEM]"));
    TL_CHECK_EQ(item.name_part_2e8, std::string("[ITEM] of Ages"));
}

TL_TEST(full_name_brace_loop_keeps_its_quirks) {
    const UnitTypeHierarchy types = make_test_hierarchy();

    // A mismatched tag whose `}` is one of the last two characters ends the
    // loop, even with a `}` after it.
    {
        ItemStats item = make_test_item("{MALE}Testblade");
        item.affixes.push_back(make_rolled_affix("AFFIX", "", "[ITEM] {X}}", 1));
        TL_CHECK_EQ(tl::core::item_full_name(item, false, types), std::string("Testblade {X}}"));
    }

    // A matching closing tag with no `}` after it erases to the end.
    {
        ItemStats item = make_test_item("{MALE}Testblade");
        item.affixes.push_back(make_rolled_affix("AFFIX", "", "[ITEM] {MALE}of the King{/MALE", 1));
        TL_CHECK_EQ(tl::core::item_full_name(item, false, types),
                    std::string("Testblade of the King"));
    }
}

}  // namespace
