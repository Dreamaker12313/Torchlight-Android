// Lower-casing helpers the archive lookups share.
#pragma once

#include <cctype>
#include <cstring>
#include <string>

namespace tl::core {

// PORT: a string lower-cased, for the port's own lookups.
inline std::string lowered(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// PORT: whether `text` ends with `suffix` (lower case), case aside.
inline bool ends_with_lower(const std::string& text, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (n > text.size()) return false;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[text.size() - n + i])) != suffix[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace tl::core
