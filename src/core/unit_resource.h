// A unit file over its `BASEFILE` chain: `CUnitResourceList::parseChildFile`,
// for the units `media/masterresourceunits.dat` does not hold and a mod's.
// No file I/O: the caller hands over a loader.
#pragma once

#include "formats/adm.h"

#include <cstdint>
#include <functional>
#include <string>

namespace tl::core {

/// `CDataGroup::CopyDataGroup` (inlined on the console), with `CopyDataValue`
/// and `AddDataGroup`. Each value of `from`
/// replaces in place the first not yet replaced value of that name `into` had before the copy, or
/// is appended; each sub-group is appended, never merged. Names compare as stored.
void copy_data_group(tl::formats::AdmSection& into, const tl::formats::AdmSection& from);

struct UnitFileHost {
    /// `CDataGroup::LoadFile`: the file's root group into `out`; a missing file leaves it empty.
    std::function<void(const std::string& path, tl::formats::AdmSection& out)> load;
    /// `UTILITIES::createUniqueGuid` as text (`STRINGS::GetValueAsWString`), asked only for a
    /// top-level file with no `UNIT_GUID`. May be unset.
    std::function<std::string()> unique_guid;
};

/// `CUnitResourceList::parseChildFile(path, topLevel)`: the
/// unit `path` describes, its `BASEFILE` chain under it. A base's `UNIT_GUID` is never inherited,
/// and `DONTCREATE` is this file's own.
void parse_child_file(const std::string& path, bool top_level, const UnitFileHost& host,
                      tl::formats::AdmSection& out);

/// The unit list's four groups, in the order the console's `UNITDATA.RAW`
/// holds them and the value `RESOURCEGROUP` takes.
enum class ResourceGroup : uint32_t { Items = 0, Monsters = 1, Players = 2, Props = 3 };

/// `CUnitResourceList::loadFile`, the half that makes the record:
/// `parse_child_file(path, true)`, then `FILEITEM` (`FILESYSTEM::GetFileName`), `DATFILE`,
/// `RESOURCEGROUP` and an empty `BASEFILE` appended (`CDataGroup::AddDataValue`, not set), so a
/// reader of the first `BASEFILE` still sees the file's.
void load_file(const std::string& path, ResourceGroup group, const UnitFileHost& host,
               tl::formats::AdmSection& out);

}  // namespace tl::core
