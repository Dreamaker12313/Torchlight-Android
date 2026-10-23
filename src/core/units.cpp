#include "core/units.h"

#include <algorithm>
#include <cctype>

namespace tl::core {
namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string key_of(const std::string& name) {
    std::string out = name;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace

// INVENTED: a unit is a pet when its `UNITTYPE` is `PET`, by name.
bool UnitStats::is_pet() const { return key_of(unittype) == "pet"; }

// `CResourceManager::createNewItem`:
// `CREATEAS`, upper-cased.
bool ItemStats::is_gold() const { return key_of(create_as) == "gold"; }

// PORT: an item kept in the port's book by name and GUID.
void ItemBook::add(const ItemStats& stats) {
    if (stats.name.empty()) return;
    const std::string key = key_of(stats.name);
    const auto replaced = by_name_.find(key);
    if (replaced != by_name_.end() && replaced->second.unit_guid != -1) {
        by_guid_.erase(replaced->second.unit_guid);
    }
    by_name_[key] = stats;
    if (stats.unit_guid != -1) by_guid_[stats.unit_guid] = key;
}

// PORT: an item of the port's book by name.
const ItemStats* ItemBook::find(const std::string& name) const {
    const auto it = by_name_.find(key_of(name));
    return it == by_name_.end() ? nullptr : &it->second;
}

// PORT: an item of the port's book by GUID.
const ItemStats* ItemBook::find_by_guid(int64_t guid) const {
    if (guid == -1) return nullptr;
    const auto it = by_guid_.find(guid);
    return it == by_guid_.end() ? nullptr : find(it->second);
}

// PORT: an item of the port's book by its display name (headless runs).
const ItemStats* ItemBook::find_by_display_name(const std::string& display_name) const {
    const std::string wanted = key_of(display_name);
    if (wanted.empty()) return nullptr;
    for (const auto& entry : by_name_) {
        if (key_of(entry.second.display_name) == wanted) return &entry.second;
    }
    return nullptr;
}

// PORT: a unit kept in the port's book by name.
void UnitBook::add(const std::string& name, const UnitStats& stats) {
    if (name.empty()) return;
    by_name_[key_of(name)] = stats;
}

// PORT: a unit of the port's book by name.
const UnitStats* UnitBook::find(const std::string& name) const {
    const auto it = by_name_.find(name);
    return it == by_name_.end() ? nullptr : &it->second;
}

// PORT: the book's order, `key_of`'s lower case compared byte by byte as `std::string` does
// (`std::tolower` in the "C" locale, which the game never leaves, folds A-Z alone).
bool UnitBook::NameLess::operator()(const std::string& a, const std::string& b) const {
    const auto fold = [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return static_cast<unsigned char>(u >= 'A' && u <= 'Z' ? u + ('a' - 'A') : u);
    };
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char x = fold(a[i]);
        const unsigned char y = fold(b[i]);
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

}  // namespace tl::core
