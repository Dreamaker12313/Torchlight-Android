// The layout node kinds and their properties, in the order a compiled layout (`.cmp`)
// stores them: each descriptor constructor's registrations, after its base's.
// `CDescriptor::AddProperty`,
// `CDescriptor::AddPropertyWithInterpreterFunctions`,
// `CDescriptor::AddInputLogic`, `CDescriptor::AddOutputLogic`;
// read back by `CDescriptor::loadObjectFromBinaryFile`,
// which skips `kNotInBinary`.
// `Waypoint Activator`'s factory:.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tl::formats::layout_schema {

/// `EVARIABLE_TYPES` as the constructors pass them: 2 a signed integer, 3 a
/// float, 4 an unsigned integer (a choice's index too), 5 a string, 6 a flag,
/// 8 a three-float vector.
enum class VariableType : uint8_t {
    Unknown = 0,
    Int = 2,
    Float = 3,
    UInt = 4,
    String = 5,
    Bool = 6,
    Vector3 = 8,
};

/// Flags a property is registered with that decide how it is stored.
/// Not in the binary: `loadObjectFromBinaryFile` passes over it.
inline constexpr uint32_t kNotInBinary = 0x100;
/// A choice stored as its word (`Cycle`, `RANDOM`), turned into its index by the
/// property's interpreter (`GetLoopTypeIDByString` and the like).
inline constexpr uint32_t kStoredAsWord = 0x4;
/// A curve of any length: a ParticleUniverse dynamic attribute (0x10) or a
/// colour over time (0x20).
inline constexpr uint32_t kCurve = 0x10;
inline constexpr uint32_t kColourCurve = 0x20;
/// A list of values of any length (`Pathing`'s `POINTS`).
inline constexpr uint32_t kList = 0x8000;

struct Property {
    const char* name;
    const char* group;
    VariableType type;
    uint32_t flags;
    /// Registered through `AddPropertyWithInterpreterFunctions`: the value
    /// goes through the descriptor's word-to-index pair.
    bool interpreted;

    constexpr bool in_binary() const { return (flags & kNotInBinary) == 0; }
};

struct Descriptor {
    const char* name;   ///< what a layout's `DESCRIPTOR` says
    const char* klass;  ///< the original's class
    const Property* properties;
    size_t property_count;
    const uint32_t* inputs;  ///< `EINPUT_EVENTS`, in registration order
    size_t input_count;
    const uint32_t* outputs;  ///< `EOUTPUT_EVENTS`
    size_t output_count;
};

#include "formats/layout_schema_table.h"

namespace detail {

constexpr char upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c; }

constexpr bool same(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (upper(a[i]) != upper(b[i])) return false;
    }
    return true;
}

}  // namespace detail

/// The node kinds in registration order, which is a compiled node's type number.
/// `CDescriptorController::CDescriptorController`;
/// `CDescriptorManager::GetDescriptorByCreated`;
/// `Waypoint Activator` last.
inline constexpr const char* kCreatedOrder[] = {
    "Group",
    "Monster",
    "Item",
    "Unit Trigger",
    "Unit Spawner",
    "Layout Link",
    "Layout Link Particle",
    "Layout Link Timeline",
    "Layout Link Trigger",
    "Generic Model",
    "Room Piece",
    "Light",
    "Sound",
    "Music",
    "Missile",
    "Image",
    "Button",
    "Particle",
    "Emitter",
    "Align",
    "Line",
    "Linear Force",
    "Sine Force",
    "Vortext",
    "Gravity Well",
    "Scale",
    "Jet",
    "Geometry Rotator",
    "Texture Rotate",
    "Texture Animation",
    "Color",
    "Box Collision",
    "Plane Collision",
    "Sphere Collision",
    "Timeline",
    "Logic Group",
    "Player Sphere Trigger",
    "Player Box Trigger",
    "Counter",
    "Random Choice",
    "Warper",
    "Teleport",
    "Timer",
    "Output Incrementor",
    "Camera Shake",
    "Property Node",
    "Puzzle Input",
    "Camera Controller",
    "Game State Controller",
    "Quest Controller",
    "Skill Controller",
    "Pathing",
    "Path Node",
    "Animation Controller",
    "Skip Cutscene",
    "Damage Shape",
    "Cinematic",
    "Money Taker",
    "Dungeon Object",
    "Waypoint Activator",
};

/// The compiled type of the node kind `name` (case-insensitive), or -1.
constexpr int created_index(std::string_view name);

/// Every node kind, sorted by name.
constexpr const Descriptor* begin() { return kDescriptors; }
constexpr size_t count() { return sizeof(kDescriptors) / sizeof(kDescriptors[0]); }

/// The node kind a layout's `DESCRIPTOR` names (case-insensitive), or null.
constexpr const Descriptor* find(std::string_view name) {
    for (size_t i = 0; i < count(); ++i) {
        if (detail::same(kDescriptors[i].name, name)) return &kDescriptors[i];
    }
    return nullptr;
}

constexpr int created_index(std::string_view name) {
    for (size_t i = 0; i < sizeof(kCreatedOrder) / sizeof(kCreatedOrder[0]); ++i) {
        if (detail::same(kCreatedOrder[i], name)) return static_cast<int>(i);
    }
    return -1;
}

/// How many fields a compiled node of this kind has: its properties less the
/// ones not in the binary.
constexpr size_t field_count(const Descriptor& d) {
    size_t n = 0;
    for (size_t i = 0; i < d.property_count; ++i) {
        if (d.properties[i].in_binary()) ++n;
    }
    return n;
}

/// The field index of `property` in a compiled node of this kind - its place
/// among the properties in the binary - or `npos`. `occurrence` picks among
/// properties registered under the same name (an `Emitter` has two `WIDTH`s).
inline constexpr size_t npos = static_cast<size_t>(-1);
constexpr size_t field_index(const Descriptor& d, std::string_view property, int occurrence = 0) {
    size_t field = 0;
    for (size_t i = 0; i < d.property_count; ++i) {
        const Property& p = d.properties[i];
        if (!p.in_binary()) continue;
        if (detail::same(p.name, property) && occurrence-- == 0) return field;
        ++field;
    }
    return npos;
}

/// The same by the kind's name; `npos` for an unknown kind or property.
/// Not through `find`: UBSan cannot fold a null test in a constant expression.
constexpr size_t field(std::string_view descriptor, std::string_view property, int occurrence = 0) {
    for (size_t i = 0; i < count(); ++i) {
        if (detail::same(kDescriptors[i].name, descriptor)) {
            return field_index(kDescriptors[i], property, occurrence);
        }
    }
    return npos;
}

namespace detail {
/// Not `constexpr`: reached only when `must` is handed `npos`, which makes the
/// constant being initialised fail to compile.
inline size_t no_such_property() { return npos; }
}  // namespace detail

/// `field`'s result for a constant that has to name a real property: a name the
/// table does not have stops the build.
constexpr size_t must(size_t index) { return index != npos ? index : detail::no_such_property(); }

/// The property behind field `index` of a compiled node of this kind, or null.
constexpr const Property* field_property(const Descriptor& d, size_t index) {
    size_t field = 0;
    for (size_t i = 0; i < d.property_count; ++i) {
        if (!d.properties[i].in_binary()) continue;
        if (field == index) return &d.properties[i];
        ++field;
    }
    return nullptr;
}

}  // namespace tl::formats::layout_schema
