// `media/unitdata.raw`: the Xbox 360 release's unit list (the PC's
// `masterresourceunits.dat`), little-endian, one `CDataGroupUnit` (0x80 bytes) per unit.
// `CUnitResourceList::CUnitResourceList`'s loader;
// strings read as does.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

/// One `CDataGroupUnit` as the file gives it.
struct UnitDataEntry {
    uint32_t group = 0;  ///< `ERESOURCE_GROUPS`: 0 items, 1 monsters, 2 players, 3 props
    int64_t guid = -1;
    std::string name;  ///< UTF-8; empty for a base
    std::string file;  ///< `MEDIA/UNITS/...DAT`, as stored
    bool create_as_equipment = false;
    uint32_t level = 0;
    uint32_t min_level = 0;
    uint32_t max_level = 0;
    int32_t rarity = 0;
    int32_t rarity_hardcore = 0;
    std::string unit_type;
};

struct UnitData {
    /// Every entry in the file's order, bases included (the caller files them).
    std::vector<UnitDataEntry> entries;
    /// False when the file ends before its four groups do; what was read is kept
    /// (the console's guarded reads leave the rest nameless, so never filed).
    bool complete = false;
};

/// The four groups' entries, in order.
UnitData parse_unit_data(const uint8_t* data, size_t size);

}  // namespace tl::formats
