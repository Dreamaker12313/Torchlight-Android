#include "formats/unit_data.h"

#include "formats/byte_reader.h"

namespace tl::formats {

namespace {

/// The four resource groups the loader walks.
constexpr uint32_t kGroups = 4;

/// One string as reads it: a u16 count of UTF-16 code units, the
/// units, and the result cut at its first U+0000 (wcslen).
std::string read_string(ByteReader& in) {
    const uint16_t count = in.u16();
    if (!in.ok() || count == 0) return {};
    std::string text = in.utf16le(count);
    const size_t nul = text.find('\0');
    if (nul != std::string::npos) text.resize(nul);
    return text;
}

}  // namespace

// `CUnitResourceList`'s loader of `UNITDATA.RAW`, the
// reading half; the filing half (nameless entries freed, a later GUID or name
// replacing an earlier one) is the caller's.
UnitData parse_unit_data(const uint8_t* data, size_t size) {
    UnitData out;
    ByteReader in(data, size);
    for (uint32_t group = 0; group < kGroups; ++group) {
        const uint32_t count = in.u32();
        if (!in.ok()) return out;
        for (uint32_t i = 0; i < count; ++i) {
            UnitDataEntry entry;
            entry.group = group;
            entry.guid = static_cast<int64_t>(in.u64());
            entry.name = read_string(in);
            entry.file = read_string(in);
            entry.create_as_equipment = in.u8() != 0;
            entry.level = in.u32();
            entry.min_level = in.u32();
            entry.max_level = in.u32();
            entry.rarity = static_cast<int32_t>(in.u32());
            entry.rarity_hardcore = static_cast<int32_t>(in.u32());
            entry.unit_type = read_string(in);
            if (!in.ok()) return out;
            out.entries.push_back(std::move(entry));
        }
    }
    out.complete = true;
    return out;
}

}  // namespace tl::formats
