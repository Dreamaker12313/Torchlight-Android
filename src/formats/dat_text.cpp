#include "formats/dat_text.h"

#include "formats/byte_reader.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace tl::formats {
namespace {

/// The file as UTF-8, whatever it was written in.
std::string decode(const uint8_t* data, size_t size) {
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        return utf16le_to_utf8(data + 2, (size - 2) & ~size_t{1});
    }
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        std::vector<uint8_t> swapped(data + 2, data + 2 + ((size - 2) & ~size_t{1}));
        for (size_t i = 0; i + 1 < swapped.size(); i += 2) std::swap(swapped[i], swapped[i + 1]);
        return utf16le_to_utf8(swapped.data(), swapped.size());
    }
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return std::string(reinterpret_cast<const char*>(data + 3), size - 3);
    }
    return std::string(reinterpret_cast<const char*>(data), size);
}

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) {
        --b;
    }
    return s.substr(a, b - a);
}

bool same_ci(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'a' && x <= 'z') x = static_cast<char>(x - 32);
        if (y >= 'a' && y <= 'z') y = static_cast<char>(y - 32);
        if (x != y) return false;
    }
    return i == a.size() && b[i] == '\0';
}

bool parse_record(const std::string& line, AdmRecord& out) {
    const size_t close = line.find('>');
    if (line.empty() || line[0] != '<' || close == std::string::npos) return false;
    const std::string tag = line.substr(1, close - 1);
    const size_t colon = line.find(':', close + 1);
    if (colon == std::string::npos) return false;
    out.key = line.substr(close + 1, colon - close - 1);
    const std::string value = line.substr(colon + 1);
    if (same_ci(tag, "STRING")) {
        out.type = AdmType::String;
        out.text = value;
    } else if (same_ci(tag, "TRANSLATE")) {
        out.type = AdmType::Translate;
        out.text = value;
    } else if (same_ci(tag, "FLOAT") || same_ci(tag, "DOUBLE")) {
        out.type = AdmType::Float;
        out.number = static_cast<float>(std::strtod(value.c_str(), nullptr));
    } else if (same_ci(tag, "INTEGER")) {
        out.type = AdmType::Integer;
        out.integer =
            static_cast<uint32_t>(static_cast<int32_t>(std::strtol(value.c_str(), nullptr, 10)));
    } else if (same_ci(tag, "UNSIGNED INT")) {
        out.type = AdmType::Unsigned;
        out.integer = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    } else if (same_ci(tag, "BOOL")) {
        out.type = AdmType::Bool;
        out.integer = (same_ci(value, "true") || same_ci(value, "1")) ? 1u : 0u;
    } else if (same_ci(tag, "INTEGER64")) {
        out.type = AdmType::Guid;
        out.guid = std::strtoll(value.c_str(), nullptr, 10);
    } else {
        return false;
    }
    return true;
}

}  // namespace

bool looks_like_dat_text(const uint8_t* data, size_t size) {
    const std::string text = decode(data, size < 512 ? size : 512);
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return c == '[';
    }
    return false;
}

AdmFile parse_dat_text(const uint8_t* data, size_t size) {
    AdmFile file;
    const std::string text = decode(data, size);

    // The groups open on the way down; `stack[0]` is a holder whose first child
    // is the file's root group.
    AdmSection holder;
    std::vector<AdmSection*> stack{&holder};
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty()) continue;

        if (line[0] == '[' && line.back() == ']') {
            if (line.size() > 2 && line[1] == '/') {
                if (stack.size() > 1) stack.pop_back();
                continue;
            }
            AdmSection* parent = stack.back();
            parent->children.emplace_back();
            parent->children.back().name = line.substr(1, line.size() - 2);
            stack.push_back(&parent->children.back());
            continue;
        }
        AdmRecord record;
        if (stack.size() > 1 && parse_record(line, record)) {
            stack.back()->records.push_back(std::move(record));
        } else {
            file.undecoded_bytes += line.size();
        }
    }
    if (holder.children.empty()) {
        file.error = "no group in the file";
        return file;
    }
    file.root = std::move(holder.children.front());
    file.ok = true;
    return file;
}

}  // namespace tl::formats
