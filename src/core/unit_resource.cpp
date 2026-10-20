#include "core/unit_resource.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace tl::core {

namespace {

using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

/// `CDataGroup::GetDataValue(name, EMPTY_WSTRING)`: the value's text, empty
/// when the group has none of that name.
std::string text_of(const AdmSection& group, const std::string& key) {
    return group.string_or(key, std::string());
}

/// `CDataGroup::SetDataValue(name, text, translate)`: the first value of that
/// name set, or one added at the end.
void set_text(AdmSection& group, const std::string& key, const std::string& text, bool translate) {
    for (AdmRecord& record : group.records) {
        if (record.key != key) continue;
        record.type = translate ? AdmType::Translate : AdmType::String;
        record.text = text;
        return;
    }
    AdmRecord added;
    added.key = key;
    added.type = translate ? AdmType::Translate : AdmType::String;
    added.text = text;
    group.records.push_back(std::move(added));
}

/// `CDataGroup::SetDataValue(name, bool)`.
void set_bool(AdmSection& group, const std::string& key, bool value) {
    for (AdmRecord& record : group.records) {
        if (record.key != key) continue;
        record.type = AdmType::Bool;
        record.integer = value ? 1u : 0u;
        return;
    }
    AdmRecord added;
    added.key = key;
    added.type = AdmType::Bool;
    added.integer = value ? 1u : 0u;
    group.records.push_back(std::move(added));
}

}  // namespace

void copy_data_group(AdmSection& into, const AdmSection& from) {
    // CDataGroup::CopyDataGroup. The values `into` holds now,
    // by position: the list CopyDataValue searches and
    // shortens.
    std::vector<size_t> pending;
    pending.reserve(into.records.size());
    for (size_t i = 0; i < into.records.size(); ++i) pending.push_back(i);

    for (const AdmRecord& value : from.records) {
        bool copied = false;
        for (size_t at = 0; at < pending.size(); ++at) {
            if (into.records[pending[at]].key != value.key) continue;
            const size_t target = pending[at];
            // Out of the list, the last one into its place.
            pending[at] = pending.back();
            pending.pop_back();
            into.records[target] = value;  // CDataValue::Copy
            copied = true;
            break;
        }
        if (!copied) into.records.push_back(value);
    }
    // AddDataGroup makes a new sub-group
    // each time, and the copy into an empty group is every value and group of
    // the source.
    for (const AdmSection& group : from.children) {
        AdmSection added;
        added.name = group.name;
        copy_data_group(added, group);
        into.children.push_back(std::move(added));
    }
}

void parse_child_file(const std::string& path, bool top_level, const UnitFileHost& host,
                      AdmSection& out) {
    // CUnitResourceList::parseChildFile
    AdmSection own;
    if (host.load) host.load(path, own);

    const bool dont_create = own.bool_or("DONTCREATE", false);
    const std::string base = text_of(own, "BASEFILE");
    std::string guid = text_of(own, "UNIT_GUID");
    const std::string display = text_of(own, "DISPLAYNAME");

    if ((guid.empty() || display.empty()) && top_level) {
        bool name_it = true;
        if (guid.empty()) {
            if (host.unique_guid) {
                guid = host.unique_guid();
                set_text(own, "UNIT_GUID", guid, false);
            }
        } else if (dont_create) {
            name_it = false;
        }
        if (name_it && display.empty()) {
            const std::string name = text_of(own, "NAME");
            if (!name.empty()) set_text(own, "DISPLAYNAME", name, true);
        }
    }

    if (base.empty()) {
        out = std::move(own);
    } else {
        AdmSection merged;
        parse_child_file(base, false, host, merged);
        // The base's group is what the result is; it takes this file's name
        // only where the base file gave none.
        if (merged.name.empty()) merged.name = own.name;
        copy_data_group(merged, own);
        // `SetDataValue(L"UNIT_GUID", guid)`: with a GUID of its own the copy
        // has already put it there; without one the base's is wiped.
        if (own.find("UNIT_GUID") == nullptr || guid.empty()) {
            set_text(merged, "UNIT_GUID", guid, false);
        }
        out = std::move(merged);
    }
    set_bool(out, "DONTCREATE", dont_create);
}

void load_file(const std::string& path, ResourceGroup group, const UnitFileHost& host,
               AdmSection& out) {
    // CUnitResourceList::loadFile
    parse_child_file(path, true, host, out);
    auto add_text = [&out](const char* key, const std::string& text) {
        AdmRecord added;
        added.key = key;
        added.type = AdmType::String;
        added.text = text;
        out.records.push_back(std::move(added));
    };
    // FILESYSTEM::GetFileName: what follows the last separator.
    const size_t slash = path.find_last_of("/\\");
    add_text("FILEITEM", slash == std::string::npos ? path : path.substr(slash + 1));
    add_text("DATFILE", path);
    AdmRecord resource_group;
    resource_group.key = "RESOURCEGROUP";
    resource_group.type = AdmType::Unsigned;
    resource_group.integer = static_cast<uint32_t>(group);
    out.records.push_back(std::move(resource_group));
    add_text("BASEFILE", std::string());
}

}  // namespace tl::core
