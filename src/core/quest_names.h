// The quest code's name helpers, shared by quest_book.cpp and quest_parse.cpp.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace tl::core::quest_names {

// `STRINGS::StringUpper`, the form quest and unit names are compared in.
// PORT: ASCII upper-casing of the port's narrow strings.
inline std::string upper(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// `STRINGS::StringUpper` on both, compared: the console's case-blind name test.
// PORT: ASCII, on the port's narrow strings.
inline bool same_name(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace tl::core::quest_names
