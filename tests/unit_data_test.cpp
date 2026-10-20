// `src/formats/unit_data.cpp` - `media/unitdata.raw`, the Xbox 360 release's unit
// list. Fixtures built here: the shipped file is game data.
#include "formats/unit_data.h"

#include "test_support.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

/// One entry as the file lays it out.
struct Entry {
    uint64_t guid = 0;
    std::u16string name;
    std::u16string file;
    uint8_t equipment = 0;
    uint32_t level = 0, min_level = 0, max_level = 0;
    int32_t rarity = 0, rarity_hardcore = 0;
    std::u16string type;
};

void put_le(std::vector<uint8_t>& out, uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

/// A u16 count of UTF-16 code units, then the units.
void put_string(std::vector<uint8_t>& out, const std::u16string& text) {
    put_le(out, text.size(), 2);
    for (char16_t c : text) put_le(out, c, 2);
}

void put_entry(std::vector<uint8_t>& out, const Entry& e) {
    put_le(out, e.guid, 8);
    put_string(out, e.name);
    put_string(out, e.file);
    out.push_back(e.equipment);
    put_le(out, e.level, 4);
    put_le(out, e.min_level, 4);
    put_le(out, e.max_level, 4);
    put_le(out, static_cast<uint32_t>(e.rarity), 4);
    put_le(out, static_cast<uint32_t>(e.rarity_hardcore), 4);
    put_string(out, e.type);
}

/// Four groups, each its count and entries.
std::vector<uint8_t> make_list(const std::vector<std::vector<Entry>>& groups) {
    std::vector<uint8_t> out;
    for (size_t g = 0; g < 4; ++g) {
        const std::vector<Entry> none;
        const std::vector<Entry>& entries = g < groups.size() ? groups[g] : none;
        put_le(out, entries.size(), 4);
        for (const Entry& e : entries) put_entry(out, e);
    }
    return out;
}

}  // namespace

TL_TEST(unit_data_reads_four_groups_in_order) {
    Entry base{
        0x58335fec306811deULL, u"", u"MEDIA/UNITS/ITEMS/BASE.DAT", 1, 1, 0, 0, 0, 0, u"ITEM"};
    Entry vest{42, u"STARTING VEST", u"MEDIA/UNITS/ITEMS/ARMOR/STARTINGVEST.DAT", 1, 1, 1, 3, 5,
               -2, u"ARMOR"};
    Entry imp{static_cast<uint64_t>(-739636840147775010LL),
              u"ALCHEMIST IMP",
              u"MEDIA/UNITS/MONSTERS/ALCHEMISTIMP/ALCHEMISTIMP.DAT",
              0,
              1,
              0,
              0,
              1,
              1,
              u"MONSTER"};
    const std::vector<uint8_t> raw = make_list({{base, vest}, {imp}});

    const tl::formats::UnitData data = tl::formats::parse_unit_data(raw.data(), raw.size());
    TL_CHECK(data.complete);
    TL_CHECK_EQ(data.entries.size(), size_t{3});
    TL_CHECK(data.entries[0].name.empty());
    TL_CHECK_EQ(data.entries[0].file, std::string("MEDIA/UNITS/ITEMS/BASE.DAT"));
    TL_CHECK_EQ(data.entries[0].guid, static_cast<int64_t>(0x58335fec306811deULL));
    TL_CHECK_EQ(data.entries[2].group, 1u);
    TL_CHECK_EQ(data.entries[2].guid, int64_t{-739636840147775010LL});
    TL_CHECK(!data.entries[2].create_as_equipment);
}

TL_TEST(unit_data_reads_an_entry_s_numbers) {
    Entry vest{42, u"STARTING VEST", u"V.DAT", 1, 1, 1, 3, 5, -2, u"ARMOR"};
    const std::vector<uint8_t> raw = make_list({{vest}});
    const tl::formats::UnitData data = tl::formats::parse_unit_data(raw.data(), raw.size());
    TL_CHECK_EQ(data.entries.size(), size_t{1});
    const tl::formats::UnitDataEntry& e = data.entries[0];
    TL_CHECK_EQ(e.name, std::string("STARTING VEST"));
    TL_CHECK(e.create_as_equipment);
    TL_CHECK_EQ(e.level, 1u);
    TL_CHECK_EQ(e.min_level, 1u);
    TL_CHECK_EQ(e.max_level, 3u);
    TL_CHECK_EQ(e.rarity, 5);
    TL_CHECK_EQ(e.rarity_hardcore, -2);
    TL_CHECK_EQ(e.unit_type, std::string("ARMOR"));
}

// copies the units it read with wcslen: a string ends at its first U+0000.
TL_TEST(unit_data_string_stops_at_its_first_nul) {
    Entry odd{7, std::u16string(u"AB\0CD", 5), u"F.DAT", 0, 0, 0, 0, 0, 0, u"T"};
    const std::vector<uint8_t> raw = make_list({{odd}});
    const tl::formats::UnitData data = tl::formats::parse_unit_data(raw.data(), raw.size());
    TL_CHECK(data.complete);
    TL_CHECK_EQ(data.entries.size(), size_t{1});
    TL_CHECK_EQ(data.entries[0].name, std::string("AB"));
    TL_CHECK_EQ(data.entries[0].file, std::string("F.DAT"));
}

// A short file keeps what it read; the rest would be nameless on the console.
TL_TEST(unit_data_short_file_keeps_what_it_read) {
    std::vector<uint8_t> raw;
    put_le(raw, 3, 4);
    put_entry(raw, Entry{1, u"ONE", u"ONE.DAT", 0, 0, 0, 0, 0, 0, u"T"});
    put_le(raw, 2, 8);  // the second entry stops after its GUID
    const tl::formats::UnitData data = tl::formats::parse_unit_data(raw.data(), raw.size());
    TL_CHECK(!data.complete);
    TL_CHECK_EQ(data.entries.size(), size_t{1});
    TL_CHECK_EQ(data.entries[0].name, std::string("ONE"));
}
