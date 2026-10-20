#include "formats/uilayout.h"

#include <cstdlib>

namespace tl::formats {
namespace {

/// Decode the file to UTF-8: UTF-16LE when it has that byte-order mark, else UTF-8.
std::string to_utf8(const uint8_t* data, size_t size) {
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return std::string(reinterpret_cast<const char*>(data) + 3, size - 3);
    }
    if (size < 2 || data[0] != 0xFF || data[1] != 0xFE) {
        return std::string(reinterpret_cast<const char*>(data), size);
    }

    std::string out;
    out.reserve(size / 2);
    for (size_t i = 2; i + 1 < size; i += 2) {
        uint32_t cp = static_cast<uint32_t>(data[i]) | (static_cast<uint32_t>(data[i + 1]) << 8);
        // A surrogate pair: the four bytes are consumed together or not at all.
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < size) {
            const uint32_t low =
                static_cast<uint32_t>(data[i + 2]) | (static_cast<uint32_t>(data[i + 3]) << 8);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::string trim(const std::string& text) {
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// How many objects a subtree holds, for the `COUNT` check.
size_t count_objects(const std::vector<UiObject>& objects) {
    size_t total = objects.size();
    for (const UiObject& object : objects) total += count_objects(object.children);
    return total;
}

const UiObject* find_widget_in(const std::vector<UiObject>& objects, const std::string& wanted) {
    for (const UiObject& object : objects) {
        const std::string* widget = object.property("WIDGET NAME");
        if (widget != nullptr && *widget == wanted) return &object;
        if (const UiObject* deeper = find_widget_in(object.children, wanted)) return deeper;
    }
    return nullptr;
}

}  // namespace

const std::string* UiObject::property(const std::string& key) const {
    for (const auto& entry : properties) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

bool UiObject::has(const std::string& key) const { return property(key) != nullptr; }

std::string UiObject::string_or(const std::string& key, const std::string& fallback) const {
    const std::string* found = property(key);
    return found != nullptr ? *found : fallback;
}

float UiObject::float_or(const std::string& key, float fallback) const {
    const std::string* found = property(key);
    if (found == nullptr || found->empty()) return fallback;
    char* end = nullptr;
    const double value = std::strtod(found->c_str(), &end);
    if (end == found->c_str()) return fallback;
    return static_cast<float>(value);
}

bool UiObject::bool_or(const std::string& key, bool fallback) const {
    const std::string* found = property(key);
    if (found == nullptr || found->empty()) return fallback;
    // Case-insensitive, as the game's other data references are.
    const char first = static_cast<char>(
        (*found)[0] >= 'A' && (*found)[0] <= 'Z' ? (*found)[0] + ('a' - 'A') : (*found)[0]);
    if (first == 't' || first == '1') return true;
    if (first == 'f' || first == '0') return false;
    return fallback;
}

const UiObject* UiLayout::find_widget(const std::string& widget_name) const {
    return find_widget_in(objects, widget_name);
}

UiLayout parse_ui_layout(const uint8_t* data, size_t size) {
    UiLayout layout;
    if (data == nullptr || size == 0) {
        layout.error = "empty";
        return layout;
    }

    const std::string text = to_utf8(data, size);

    // The stack of objects currently open; a new object goes into the one above it.
    std::vector<UiObject*> open;
    bool seen_header = false;
    bool in_properties = false;

    size_t pos = 0;
    size_t line_number = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++line_number;
        if (line.empty()) continue;

        if (line[0] == '[') {
            if (line == "[UI]") {
                seen_header = true;
            } else if (line == "[BASEOBJECT]") {
                std::vector<UiObject>& into = open.empty() ? layout.objects : open.back()->children;
                into.emplace_back();
                // Pushed after the emplace: growing `children` moves its elements.
                open.push_back(&into.back());
            } else if (line == "[/BASEOBJECT]") {
                if (open.empty()) {
                    layout.error =
                        "unbalanced [/BASEOBJECT] at line " + std::to_string(line_number);
                    return layout;
                }
                open.pop_back();
            } else if (line == "[PROPERTIES]") {
                in_properties = true;
            } else if (line == "[/PROPERTIES]") {
                in_properties = false;
            }
            // `[OBJECTS]`, `[CHILDREN]` and their closers are implied by the stack.
            continue;
        }

        if (line[0] != '<') continue;
        const size_t type_end = line.find('>');
        if (type_end == std::string::npos) continue;
        const size_t colon = line.find(':', type_end + 1);
        if (colon == std::string::npos) continue;

        const std::string key = line.substr(type_end + 1, colon - type_end - 1);
        const std::string value = line.substr(colon + 1);

        if (!open.empty() && in_properties) {
            open.back()->properties.emplace_back(key, value);
        } else if (open.empty()) {
            // The file header, before `[OBJECTS]`.
            if (key == "VERSION") layout.version = std::atoi(value.c_str());
            else if (key == "COUNT") layout.count = static_cast<uint32_t>(std::atol(value.c_str()));
        }
    }

    if (!seen_header) {
        layout.error = "not a [UI] layout";
        return layout;
    }
    if (!open.empty()) {
        layout.error = "unterminated [BASEOBJECT]";
        return layout;
    }

    // The header's tally: a mismatch means the nesting was misread.
    const size_t parsed = count_objects(layout.objects);
    if (layout.count != 0 && parsed != layout.count) {
        layout.error =
            "COUNT says " + std::to_string(layout.count) + ", parsed " + std::to_string(parsed);
        return layout;
    }

    layout.ok = true;
    return layout;
}

}  // namespace tl::formats
