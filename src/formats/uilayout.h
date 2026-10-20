// `[UI]`: Runic's interface-editor text format, the source of the Xbox 360 screens
// (`[UI]` > `[OBJECTS]` > `[BASEOBJECT]` with `[PROPERTIES]` and `[CHILDREN]`).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

/// One `[BASEOBJECT]`: its properties, in file order, and its children.
/// Values are kept as raw text; the type word (`<FLOAT>`) is ignored.
struct UiObject {
    std::vector<std::pair<std::string, std::string>> properties;
    std::vector<UiObject> children;

    /// A property's raw text, or null when it is absent. Matched exactly (keys are upper case).
    const std::string* property(const std::string& key) const;
    bool has(const std::string& key) const;

    std::string string_or(const std::string& key, const std::string& fallback) const;
    float float_or(const std::string& key, float fallback) const;
    bool bool_or(const std::string& key, bool fallback) const;

    /// `DESCRIPTOR` - the widget kind: `Image`, `Text`, `Text Outlined`,
    /// `Window`, `Button`, `Check Box`, `Group`, `CEGUI Object`.
    std::string descriptor() const { return string_or("DESCRIPTOR", std::string()); }

    /// `NAME` - the editor's label, not unique; the engine's handle is `WIDGET NAME`.
    std::string name() const { return string_or("NAME", std::string()); }
};

struct UiLayout {
    /// The `[OBJECTS]` list, in file order. That order is the draw order.
    std::vector<UiObject> objects;

    int version = 0;
    /// The file's own `COUNT`, every object at every depth; checked against the parse.
    uint32_t count = 0;

    bool ok = false;
    std::string error;

    /// Depth-first search for the object whose `WIDGET NAME` is this. Null when
    /// absent. Matched exactly, because these are engine handles.
    const UiObject* find_widget(const std::string& widget_name) const;
};

/// Parse a `[UI]` file from bytes the caller owns. Never throws. UTF-16LE or UTF-8,
/// decided by the byte-order mark.
UiLayout parse_ui_layout(const uint8_t* data, size_t size);

}  // namespace tl::formats
