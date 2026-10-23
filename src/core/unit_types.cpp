#include "core/unit_types.h"

#include <algorithm>
#include <cctype>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// `CHierarchy::parseHierachy`.
void UnitTypeHierarchy::parse(const tl::formats::AdmSection& root) {
    by_name_.clear();
    ancestors_.clear();
    const tl::formats::AdmSection* types = nullptr;
    root.visit([&](const tl::formats::AdmSection& section) {
        if (types == nullptr && upper(section.name) == "UNITTYPES") types = &section;
    });
    if (types == nullptr) return;
    for (const tl::formats::AdmSection& group : types->children) {
        const tl::formats::AdmRecord* id = group.find("ID");
        if (id == nullptr) continue;
        by_name_[upper(group.name)] = id->integer;
        std::vector<uint32_t>& list = ancestors_[id->integer];
        for (const tl::formats::AdmRecord& record : group.records) {
            if (upper(record.key) == "ID") continue;
            list.push_back(record.integer);
        }
    }
}

// `CHierarchy::getTypeIDByName`.
uint32_t UnitTypeHierarchy::id_of(const std::string& name) const {
    const auto it = by_name_.find(name);
    return it == by_name_.end() ? kUnknown : it->second;
}

// PORT: the order of `upper`'s keys, compared byte by byte as `std::string` does
// (`std::toupper` in the "C" locale, which the game never leaves, folds a-z alone).
bool UnitTypeHierarchy::NameLess::operator()(const std::string& a, const std::string& b) const {
    const auto fold = [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return static_cast<unsigned char>(u >= 'a' && u <= 'z' ? u - ('a' - 'A') : u);
    };
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char x = fold(a[i]);
        const unsigned char y = fold(b[i]);
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

std::string UnitTypeHierarchy::name_of(uint32_t id) const {
    for (const auto& [n, type_id] : by_name_) {
        if (type_id == id) return n;
    }
    return {};
}

// `CHierarchy::ISA(unsigned, unsigned)`.
bool UnitTypeHierarchy::isa(uint32_t type, uint32_t want) const {
    if (type == want) return true;
    const auto it = ancestors_.find(type);
    if (it == ancestors_.end()) return false;
    return std::find(it->second.begin(), it->second.end(), want) != it->second.end();
}

}  // namespace tl::core
