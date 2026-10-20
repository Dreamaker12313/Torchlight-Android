#include "core/text.h"

namespace tl::core {

// PORT: presentation, a text's `|c`/`|u` markup taken out.
std::string strip_markup(const std::string& text) {
    // One grammar, in one place: the runs concatenate to exactly this.
    std::string out;
    out.reserve(text.size());
    for (const MarkupRun& run : markup_runs(text)) out += run.text;
    return out;
}

namespace {

/// Eight hex digits to a colour, in the order `|c` writes them: A R G B.
// PORT: presentation, a `|c` run's colour, A R G B.
bool parse_colour(const std::string& text, size_t at, float& a, float& r, float& g, float& b) {
    unsigned value = 0;
    for (size_t i = 0; i < 8; ++i) {
        const char c = text[at + i];
        unsigned digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a') + 10u;
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A') + 10u;
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }
    a = static_cast<float>((value >> 24) & 0xffu) / 255.0f;
    r = static_cast<float>((value >> 16) & 0xffu) / 255.0f;
    g = static_cast<float>((value >> 8) & 0xffu) / 255.0f;
    b = static_cast<float>(value & 0xffu) / 255.0f;
    return true;
}

}  // namespace

// PORT: presentation, a text cut into runs of one colour by its `|c`/`|u` markup.
std::vector<MarkupRun> markup_runs(const std::string& text) {
    std::vector<MarkupRun> runs;
    MarkupRun current;

    const auto flush = [&]() {
        if (!current.text.empty()) runs.push_back(current);
        current.text.clear();
    };

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '|' || i + 1 >= text.size()) {
            current.text.push_back(text[i]);
            continue;
        }
        if (text[i + 1] == 'c' && i + 9 < text.size()) {
            float a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
            if (parse_colour(text, i + 2, a, r, g, b)) {
                flush();
                current.coloured = true;
                current.a = a;
                current.r = r;
                current.g = g;
                current.b = b;
                i += 1 + 8;
                continue;
            }
        }
        if (text[i + 1] == 'u') {
            flush();
            current = MarkupRun{};
            i += 1;
            continue;
        }
        current.text.push_back(text[i]);
    }
    flush();
    return runs;
}

}  // namespace tl::core
