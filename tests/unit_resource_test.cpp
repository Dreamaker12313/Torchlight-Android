// A unit file over its `BASEFILE` (`core/unit_resource.h`):
// `CUnitResourceList::parseChildFile` and
// the copy it makes, `CDataGroup::CopyDataGroup`.
#include "core/unit_resource.h"
#include "test_support.h"

#include <map>
#include <string>
#include <vector>

namespace {

using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord text(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord flag(const std::string& key, bool value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Bool;
    r.integer = value ? 1u : 0u;
    return r;
}

AdmSection block(const std::string& name, std::vector<AdmRecord> records) {
    AdmSection s;
    s.name = name;
    s.records = std::move(records);
    return s;
}

std::vector<std::string> keys(const AdmSection& s) {
    std::vector<std::string> out;
    for (const AdmRecord& r : s.records) out.push_back(r.key + "=" + r.text);
    return out;
}

tl::core::UnitFileHost host_of(const std::map<std::string, AdmSection>& files,
                               int* guids = nullptr) {
    tl::core::UnitFileHost host;
    host.load = [&files](const std::string& path, AdmSection& out) {
        const auto found = files.find(path);
        if (found != files.end()) out = found->second;
    };
    if (guids != nullptr) {
        host.unique_guid = [guids]() { return std::to_string(1000 + (*guids)++); };
    }
    return host;
}

}  // namespace

// A child's value replaces the base's where the base had it and is appended
// where it had not; the base's order stands.
TL_TEST(a_copied_value_replaces_in_place_and_a_new_one_goes_to_the_end) {
    AdmSection into = block("UNIT", {text("A", "1"), text("B", "2"), text("C", "3")});
    const AdmSection from = block("UNIT", {text("D", "9"), text("B", "8")});
    tl::core::copy_data_group(into, from);
    const std::vector<std::string> want{"A=1", "B=8", "C=3", "D=9"};
    TL_CHECK(keys(into) == want);
}

// Each value the group had is replaced once: a second of the same name goes to the next,
// then to the end.
TL_TEST(a_name_twice_in_the_source_takes_the_next_of_that_name_or_the_end) {
    AdmSection into = block("UNIT", {text("A", "1"), text("A", "2"), text("B", "3")});
    const AdmSection from = block(
        "UNIT", {text("A", "x"), text("A", "y"), text("A", "z"), text("N", "n"), text("N", "m")});
    tl::core::copy_data_group(into, from);
    const std::vector<std::string> want{"A=x", "A=y", "B=3", "A=z", "N=n", "N=m"};
    TL_CHECK(keys(into) == want);
}

// The pending list closes a gap with its last entry, so a second copy of a name does not
// always land on the second. Kept.
TL_TEST(the_pending_list_closes_a_gap_with_its_last_entry) {
    AdmSection into =
        block("UNIT", {text("B", "b"), text("A", "1"), text("C", "c"), text("A", "2")});
    const AdmSection from = block("UNIT", {text("B", "B"), text("A", "x")});
    tl::core::copy_data_group(into, from);
    // Pending was [B, A1, C, A2]; taking B out leaves [A2, A1, C], so `A=x`
    // lands on the *last* `A` the group had.
    const std::vector<std::string> want{"B=B", "A=1", "C=c", "A=x"};
    TL_CHECK(keys(into) == want);
}

// Sub-groups are appended, never merged: the base's blocks, then the child's.
TL_TEST(a_copied_block_is_appended_after_the_blocks_already_there) {
    AdmSection into = block("UNIT", {});
    into.children.push_back(block("SKILL", {text("NAME", "BASE SKILL")}));
    AdmSection from = block("UNIT", {});
    from.children.push_back(block("SKILL", {text("NAME", "OWN SKILL")}));
    AdmSection nested = block("EFFECT", {text("TYPE", "DAMAGE")});
    nested.children.push_back(block("INNER", {text("K", "v")}));
    from.children.push_back(nested);
    tl::core::copy_data_group(into, from);
    TL_CHECK_EQ(into.children.size(), size_t{3});
    TL_CHECK(into.children[0].string_or("NAME", "") == "BASE SKILL");
    TL_CHECK(into.children[1].string_or("NAME", "") == "OWN SKILL");
    TL_CHECK(into.children[2].name == "EFFECT");
    TL_CHECK_EQ(into.children[2].children.size(), size_t{1});
    TL_CHECK(into.children[2].children[0].string_or("K", "") == "v");
}

// The whole chain: a file over its base over that base's base. The base's
// GUID is never the child's, and `DONTCREATE` is the file's own.
TL_TEST(a_unit_file_is_copied_over_its_basefile_chain) {
    std::map<std::string, AdmSection> files;
    files["root.dat"] =
        block("UNIT", {text("NAME", "ROOT"), text("UNIT_GUID", "1"), flag("DONTCREATE", true),
                       text("SPEED", "4"), text("DISPLAYNAME", "Root")});
    files["root.dat"].children.push_back(block("SKILL", {text("NAME", "ROOT SKILL")}));
    files["mid.dat"] = block("UNIT", {text("NAME", "MID"), text("BASEFILE", "root.dat"),
                                      flag("DONTCREATE", true), text("ARMOR", "7")});
    files["mid.dat"].children.push_back(block("SKILL", {text("NAME", "MID SKILL")}));
    files["leaf.dat"] = block("UNIT", {text("NAME", "LEAF"), text("BASEFILE", "mid.dat"),
                                       text("UNIT_GUID", "33"), text("SPEED", "9")});
    const tl::core::UnitFileHost host = host_of(files);

    AdmSection leaf;
    tl::core::parse_child_file("leaf.dat", true, host, leaf);
    TL_CHECK(leaf.name == "UNIT");
    TL_CHECK(leaf.string_or("NAME", "") == "LEAF");
    TL_CHECK(leaf.string_or("SPEED", "") == "9");
    TL_CHECK(leaf.string_or("ARMOR", "") == "7");
    TL_CHECK(leaf.string_or("UNIT_GUID", "") == "33");
    TL_CHECK(leaf.string_or("BASEFILE", "") == "mid.dat");
    TL_CHECK(!leaf.bool_or("DONTCREATE", true));
    // The leaf had no display name of its own: top-level and not `DONTCREATE`,
    // it takes its `NAME`, and that goes over the root's.
    TL_CHECK(leaf.string_or("DISPLAYNAME", "") == "LEAF");
    TL_CHECK_EQ(leaf.children.size(), size_t{2});
    TL_CHECK(leaf.children[0].string_or("NAME", "") == "ROOT SKILL");
    TL_CHECK(leaf.children[1].string_or("NAME", "") == "MID SKILL");

    // The middle file has no GUID of its own: the root's is wiped, not kept.
    AdmSection mid;
    tl::core::parse_child_file("mid.dat", false, host, mid);
    TL_CHECK(mid.string_or("UNIT_GUID", "x").empty());
    TL_CHECK(mid.bool_or("DONTCREATE", false));
    TL_CHECK(mid.string_or("DISPLAYNAME", "") == "Root");
}

// A top-level file with no GUID gets one; `DONTCREATE` has no display name; a missing base
// is empty.
TL_TEST(a_top_level_file_gets_a_guid_and_its_name_for_display) {
    std::map<std::string, AdmSection> files;
    files["a.dat"] = block("UNIT", {text("NAME", "A")});
    files["b.dat"] =
        block("UNIT", {text("NAME", "B"), text("UNIT_GUID", "5"), flag("DONTCREATE", true)});
    files["c.dat"] =
        block("UNIT", {text("NAME", "C"), text("UNIT_GUID", "6"), text("BASEFILE", "gone.dat")});
    files["d.dat"] = block("UNIT", {text("UNIT_GUID", "7"), flag("DONTCREATE", true)});
    int guids = 0;
    const tl::core::UnitFileHost host = host_of(files, &guids);

    AdmSection a;
    tl::core::parse_child_file("a.dat", true, host, a);
    TL_CHECK(a.string_or("UNIT_GUID", "") == "1000");
    TL_CHECK(a.string_or("DISPLAYNAME", "") == "A");
    TL_CHECK_EQ(guids, 1);

    AdmSection b;
    tl::core::parse_child_file("b.dat", true, host, b);
    TL_CHECK(b.string_or("DISPLAYNAME", "none") == "none");
    TL_CHECK(b.bool_or("DONTCREATE", false));

    AdmSection c;
    tl::core::parse_child_file("c.dat", true, host, c);
    TL_CHECK(c.string_or("NAME", "") == "C");
    TL_CHECK(c.string_or("UNIT_GUID", "") == "6");
    TL_CHECK(c.string_or("DISPLAYNAME", "") == "C");

    // Not top-level: nothing is made up.
    AdmSection a_as_base;
    tl::core::parse_child_file("a.dat", false, host, a_as_base);
    TL_CHECK(a_as_base.string_or("UNIT_GUID", "none") == "none");
    TL_CHECK(a_as_base.string_or("DISPLAYNAME", "none") == "none");
    TL_CHECK_EQ(guids, 1);
}

// `CUnitResourceList::loadFile`: `FILEITEM`, `DATFILE`,
// `RESOURCEGROUP` and a second, empty `BASEFILE` appended in that order.
TL_TEST(load_file_appends_the_four_values_the_list_adds) {
    std::map<std::string, AdmSection> files;
    files["MEDIA/UNITS/ITEMS/BASE.DAT"] = block("UNIT", {text("NAME", "BASE"), text("VALUE", "1")});
    files["MEDIA/UNITS/ITEMS/AXE.DAT"] =
        block("UNIT", {text("NAME", "AXE"), text("UNIT_GUID", "5"),
                       text("BASEFILE", "MEDIA/UNITS/ITEMS/BASE.DAT")});
    const tl::core::UnitFileHost host = host_of(files);

    AdmSection axe;
    tl::core::load_file("MEDIA/UNITS/ITEMS/AXE.DAT", tl::core::ResourceGroup::Props, host, axe);
    const size_t n = axe.records.size();
    TL_CHECK(n >= 4);
    TL_CHECK(axe.records[n - 4].key == "FILEITEM" && axe.records[n - 4].text == "AXE.DAT");
    TL_CHECK(axe.records[n - 3].key == "DATFILE" &&
             axe.records[n - 3].text == "MEDIA/UNITS/ITEMS/AXE.DAT");
    TL_CHECK(axe.records[n - 2].key == "RESOURCEGROUP" &&
             axe.records[n - 2].type == AdmType::Unsigned && axe.records[n - 2].integer == 3u);
    TL_CHECK(axe.records[n - 1].key == "BASEFILE" && axe.records[n - 1].text.empty());
    TL_CHECK(axe.string_or("BASEFILE", "") == "MEDIA/UNITS/ITEMS/BASE.DAT");
    TL_CHECK(axe.string_or("VALUE", "") == "1");
    TL_CHECK(axe.string_or("UNIT_GUID", "") == "5");
}
