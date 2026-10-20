// Tests for the `.adm` record reader, on files built byte by byte in the real files' shapes.
#include "formats/adm.h"
#include "test_support.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using tl::formats::AdmType;
using tl::formats::parse_adm;

struct Builder {
    std::vector<uint8_t> table;
    std::vector<uint8_t> records;
    uint32_t entries = 0;

    static void put_u32(std::vector<uint8_t>& out, uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    /// Add a string to the table and return its id.
    uint32_t intern(uint32_t id, const std::string& text) {
        put_u32(table, id);
        put_u32(table, static_cast<uint32_t>(text.size()));
        for (char c : text) {
            table.push_back(static_cast<uint8_t>(c));
            table.push_back(0);
        }
        ++entries;
        return id;
    }

    void record(uint32_t key_id, AdmType type, uint32_t raw) {
        put_u32(records, key_id);
        put_u32(records, static_cast<uint32_t>(type));
        put_u32(records, raw);
        ++record_count;
    }

    void record_float(uint32_t key_id, float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        record(key_id, AdmType::Float, bits);
    }

    uint32_t section_id = 0;
    uint32_t record_count = 0;

    /// Header, string table, root section header, then the records. There is no
    /// padding anywhere - the records start immediately after the last string.
    std::vector<uint8_t> build(uint32_t version = 1) const {
        std::vector<uint8_t> out;
        put_u32(out, version);
        put_u32(out, entries);
        out.insert(out.end(), table.begin(), table.end());
        put_u32(out, section_id);
        put_u32(out, record_count);
        out.insert(out.end(), records.begin(), records.end());
        put_u32(out, 0);  // no child sections
        return out;
    }

    /// A 16-byte record: the only type whose value is 64 bits.
    void record_guid(uint32_t key_id, int64_t v) {
        put_u32(records, key_id);
        put_u32(records, static_cast<uint32_t>(AdmType::Guid));
        const uint64_t bits = static_cast<uint64_t>(v);
        for (int i = 0; i < 8; ++i) records.push_back(static_cast<uint8_t>(bits >> (8 * i)));
        ++record_count;
    }
};

}  // namespace

TL_TEST(adm_rejects_an_unexpected_version) {
    Builder b;
    b.intern(3, "NAME");
    auto bytes = b.build(/*version=*/7);
    auto file = parse_adm(bytes.data(), bytes.size());
    TL_CHECK(!file.ok);
    TL_CHECK(file.error.find("version") != std::string::npos);
}

TL_TEST(adm_reads_each_value_type) {
    Builder b;
    b.section_id = b.intern(99, "LEVEL");
    const uint32_t k_name = b.intern(3, "NAME");
    const uint32_t k_skybox = b.intern(10, "SKYBOX");
    const uint32_t v_skybox = b.intern(11, "media/skyboxes/town/town_sky.mesh");
    const uint32_t k_red = b.intern(20, "DIRECTIONAL RED");
    const uint32_t k_intensity = b.intern(21, "DIRECTIONAL INTENSITY");
    const uint32_t k_populate = b.intern(22, "POPULATE");
    const uint32_t v_name = b.intern(23, "MainMenuTown");

    b.record(k_name, AdmType::String, v_name);
    b.record(k_skybox, AdmType::String, v_skybox);
    b.record(k_red, AdmType::Integer, 238);
    b.record_float(k_intensity, 1.9f);
    b.record(k_populate, AdmType::Bool, 1);

    auto bytes = b.build();
    auto file = parse_adm(bytes.data(), bytes.size());

    TL_CHECK(file.ok);
    TL_CHECK_EQ(file.records().size(), size_t{5});
    TL_CHECK_EQ(file.undecoded_bytes, size_t{0});

    TL_CHECK_EQ(file.string_or("NAME", ""), std::string("MainMenuTown"));
    TL_CHECK_EQ(file.string_or("SKYBOX", ""), std::string("media/skyboxes/town/town_sky.mesh"));
    TL_CHECK_EQ(file.integer_or("DIRECTIONAL RED", 0), uint32_t{238});
    TL_CHECK(file.float_or("DIRECTIONAL INTENSITY", 0.0f) > 1.89f);
    TL_CHECK(file.float_or("DIRECTIONAL INTENSITY", 0.0f) < 1.91f);
    TL_CHECK(file.bool_or("POPULATE", false));

    // Absent keys fall back rather than throwing; the schema is sparse.
    TL_CHECK_EQ(file.string_or("NOT PRESENT", "fallback"), std::string("fallback"));
    TL_CHECK_EQ(file.integer_or("NOT PRESENT", 42), uint32_t{42});
}

TL_TEST(adm_reads_a_colour_triple_as_zero_to_one) {
    Builder b;
    b.section_id = b.intern(99, "LEVEL");
    const uint32_t r = b.intern(1, "AMBIENT RED");
    const uint32_t g = b.intern(2, "AMBIENT GREEN");
    const uint32_t bl = b.intern(3, "AMBIENT BLUE");
    b.record(r, AdmType::Integer, 255);
    b.record(g, AdmType::Integer, 0);
    b.record(bl, AdmType::Integer, 51);

    auto bytes = b.build();
    auto file = parse_adm(bytes.data(), bytes.size());
    TL_CHECK(file.ok);

    float cr = -1.0f, cg = -1.0f, cb = -1.0f;
    TL_CHECK(file.colour("AMBIENT", cr, cg, cb));
    TL_CHECK(cr > 0.99f);
    TL_CHECK(cg < 0.01f);
    TL_CHECK(cb > 0.19f && cb < 0.21f);

    // A prefix with no components at all is reported absent, so a caller can
    // tell "not set" from "set to black".
    float x = 0.0f, y = 0.0f, zz = 0.0f;
    TL_CHECK(!file.colour("FOG", x, y, zz));
}

TL_TEST(adm_stops_at_a_nested_section_and_reports_the_remainder) {
    Builder b;
    b.section_id = b.intern(99, "LEVEL");
    const uint32_t key = b.intern(5, "CAMERAMULT");
    b.record_float(key, 1.0f);

    auto bytes = b.build();
    const size_t clean_size = bytes.size();

    // A nested [SECTION] block begins with an id that is not a key in the table.
    // The reader must stop there rather than misread it as records.
    Builder::put_u32(bytes, 2);
    Builder::put_u32(bytes, 6066);
    Builder::put_u32(bytes, 0);

    auto file = parse_adm(bytes.data(), bytes.size());
    TL_CHECK(file.ok);
    TL_CHECK_EQ(file.records().size(), size_t{1});
    TL_CHECK_EQ(file.undecoded_bytes, bytes.size() - clean_size);
    TL_CHECK(file.float_or("CAMERAMULT", 0.0f) > 0.99f);
}

TL_TEST(adm_handles_an_empty_record_section) {
    Builder b;
    b.section_id = b.intern(99, "LEVEL");
    b.intern(3, "NAME");
    auto bytes = b.build();
    auto file = parse_adm(bytes.data(), bytes.size());
    TL_CHECK(file.ok);
    TL_CHECK(file.records().empty());
    TL_CHECK_EQ(file.undecoded_bytes, size_t{0});
}

TL_TEST(adm_reads_a_sixteen_byte_guid_record_without_losing_alignment) {
    // The record after a GUID is what proves the size was right. Treating a
    // GUID as 12 bytes leaves the reader four bytes adrift, and everything
    // after it decodes as plausible nonsense rather than failing.
    Builder b;
    b.section_id = b.intern(99, "PIECE");
    const uint32_t k_guid = b.intern(1, "GUID");
    const uint32_t k_name = b.intern(2, "NAME");
    const uint32_t v_name = b.intern(3, "menu_billboard_terrain");

    b.record_guid(k_guid, -2087239461602848290LL);
    b.record(k_name, AdmType::String, v_name);

    auto bytes = b.build();
    auto file = parse_adm(bytes.data(), bytes.size());

    TL_CHECK(file.ok);
    TL_CHECK_EQ(file.section(), std::string("PIECE"));
    TL_CHECK_EQ(file.records().size(), size_t{2});
    TL_CHECK_EQ(file.root.guid_or("GUID", 0), int64_t{-2087239461602848290LL});
    TL_CHECK_EQ(file.string_or("NAME", ""), std::string("menu_billboard_terrain"));
    TL_CHECK_EQ(file.undecoded_bytes, size_t{0});
}

TL_TEST(adm_reads_nested_sections) {
    Builder b;
    b.section_id = b.intern(99, "TILESET");
    const uint32_t k_name = b.intern(1, "NAME");
    const uint32_t v_town = b.intern(2, "town");
    b.record(k_name, AdmType::String, v_town);

    // Build the parent by hand so a child section can be appended.
    std::vector<uint8_t> out;
    Builder::put_u32(out, 1);
    Builder::put_u32(out, b.entries + 2);
    out.insert(out.end(), b.table.begin(), b.table.end());
    // Two more strings for the child.
    Builder child;
    const uint32_t k_piece = 10, v_file = 11;
    Builder::put_u32(out, k_piece);
    Builder::put_u32(out, 5);
    for (char c : std::string("PIECE")) {
        out.push_back(static_cast<uint8_t>(c));
        out.push_back(0);
    }
    Builder::put_u32(out, v_file);
    Builder::put_u32(out, 4);
    for (char c : std::string("mesh")) {
        out.push_back(static_cast<uint8_t>(c));
        out.push_back(0);
    }

    Builder::put_u32(out, b.section_id);
    Builder::put_u32(out, b.record_count);
    out.insert(out.end(), b.records.begin(), b.records.end());
    Builder::put_u32(out, 1);        // one child
    Builder::put_u32(out, k_piece);  // child name
    Builder::put_u32(out, 0);        // child has no records
    Builder::put_u32(out, 0);        // child has no children

    auto file = parse_adm(out.data(), out.size());
    TL_CHECK(file.ok);
    TL_CHECK_EQ(file.section(), std::string("TILESET"));
    TL_CHECK_EQ(file.root.children.size(), size_t{1});
    TL_CHECK_EQ(file.root.children.front().name, std::string("PIECE"));
    TL_CHECK_EQ(file.undecoded_bytes, size_t{0});

    size_t visited = 0;
    file.visit([&](const tl::formats::AdmSection&) { ++visited; });
    TL_CHECK_EQ(visited, size_t{2});
}

// A corrupt record count is the file's word, not an allocation size: rejected where the
// records run out.
TL_TEST(adm_rejects_a_record_count_the_file_cannot_hold) {
    Builder b;
    b.section_id = b.intern(1, "LEVEL");
    b.record_count = 0xFFFFFFFFu;
    const auto bytes = b.build();
    const auto file = parse_adm(bytes.data(), bytes.size());
    TL_CHECK(!file.ok);
    TL_CHECK(file.error.find("records ran past the end") != std::string::npos);
}
