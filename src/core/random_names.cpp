// `CRandomNames`. See `random_names.h`.
#include "core/random_names.h"

#include <cctype>

namespace tl::core {
namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
bool same_key(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

// `CDataGroup::GetDataValuesMatchingName`: every value of that key, in file
// order.
std::vector<std::string> values_named(const tl::formats::AdmSection& group, const char* key) {
    std::vector<std::string> out;
    for (const tl::formats::AdmRecord& record : group.records) {
        if (same_key(record.key, key)) out.push_back(record.text);
    }
    return out;
}

// A `TArrayList` read: an index past the list gives its first entry.
// A `TArrayList` read: an index past the list gives its first entry. PORT: on the port's vector.
const std::string& entry_at(const std::vector<std::string>& list, uint32_t index) {
    static const std::string kNone;
    if (list.empty()) return kNone;
    return index < list.size() ? list[index] : list[0];
}

}  // namespace

// `CRandomNames::load(const wchar_t*)`.
void RandomNames::load(const tl::formats::AdmSection& root) {
    sets_.clear();
    picker_ = Randomizer(RandomizerType::kNormal);
    title_chance_ = 100;
    title_prefixes_.clear();
    title_suffixes_.clear();
    for (const tl::formats::AdmSection& group : root.children) {
        if (!same_key(group.name, "NAME")) continue;
        Set set;
        const int32_t weight = static_cast<int32_t>(group.integer_or("WEIGHT", 1));
        picker_.add_choice(static_cast<int32_t>(sets_.size()), weight);
        set.prefixes = values_named(group, "PREFIX");
        set.center_chance = static_cast<int32_t>(group.integer_or("CENTERCHANCE", 0));
        set.center = group.string_or("CENTER", "'");
        set.suffixes = values_named(group, "SUFFIX");
        sets_.push_back(std::move(set));
    }
    for (const tl::formats::AdmSection& group : root.children) {
        if (!same_key(group.name, "TITLE")) continue;
        title_chance_ = static_cast<int32_t>(group.integer_or("CHANCE", 100));
        title_prefixes_ = values_named(group, "PREFIX");
        title_suffixes_ = values_named(group, "SUFFIX");
        break;
    }
}

// `CRandomNames::generateName(bool)`.
std::string RandomNames::generate_name(bool with_title, RunicRandom& rng) const {
    std::string name;
    if (sets_.empty()) return name;
    const uint32_t picked = static_cast<uint32_t>(picker_.get_random(rng));
    const Set& set = picked < sets_.size() ? sets_[picked] : sets_[0];
    const uint32_t prefix =
        static_cast<uint32_t>(rng.integer_between(0, static_cast<int>(set.prefixes.size()) - 1));
    const uint32_t suffix =
        static_cast<uint32_t>(rng.integer_between(0, static_cast<int>(set.suffixes.size()) - 1));
    if (0 < set.center_chance && rng.integer_between(0, 100) <= set.center_chance) {
        name = entry_at(set.prefixes, prefix) + set.center + entry_at(set.suffixes, suffix);
    } else {
        name = entry_at(set.prefixes, prefix) + entry_at(set.suffixes, suffix);
    }
    if (with_title && rng.integer_between(0, 100) <= title_chance_ && !title_suffixes_.empty()) {
        const uint32_t title = static_cast<uint32_t>(
            rng.integer_between(0, static_cast<int>(title_suffixes_.size()) - 1));
        name = name + " " + entry_at(title_suffixes_, title);
    }
    return name;
}

}  // namespace tl::core
