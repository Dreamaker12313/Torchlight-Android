#include "core/layout_text.h"

#include "core/logic_events.h"
#include "formats/byte_reader.h"
#include "formats/dat_text.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string_view>

namespace tl::core {
namespace {

namespace schema = tl::formats::layout_schema;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
bool same_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'a' && x <= 'z') x = static_cast<char>(x - 32);
        if (y >= 'a' && y <= 'z') y = static_cast<char>(y - 32);
        if (x != y) return false;
    }
    return true;
}

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    }
    return text;
}

/// `CDataGroup::GetDataValueByName`: the first value of that name.
const AdmRecord* value_named(const AdmSection& group, std::string_view name) {
    for (const AdmRecord& r : group.records) {
        if (same_ci(r.key, name)) return &r;
    }
    return nullptr;
}

/// `CDataGroup::GetDataGroupByName`: the first group of that name.
const AdmSection* group_named(const AdmSection& group, std::string_view name) {
    for (const AdmSection& child : group.children) {
        if (same_ci(child.name, name)) return &child;
    }
    return nullptr;
}

// `CDataGroup::GetDataValue` with a default, by the type asked for. A value
// stored as another type is converted.
int64_t value_i64(const AdmSection& group, std::string_view name, int64_t fallback) {
    const AdmRecord* r = value_named(group, name);
    if (r == nullptr) return fallback;
    switch (r->type) {
        case AdmType::Guid: return r->guid;
        case AdmType::Float: return static_cast<int64_t>(r->number);
        case AdmType::Integer: return static_cast<int32_t>(r->integer);
        case AdmType::String:
        case AdmType::Translate: return std::strtoll(r->text.c_str(), nullptr, 10);
        default: return r->integer;
    }
}

// PORT: asset parsing, a field read with its fallback.
float value_float(const AdmRecord& r) {
    switch (r.type) {
        case AdmType::Float: return r.number;
        case AdmType::Guid: return static_cast<float>(r.guid);
        case AdmType::Integer: return static_cast<float>(static_cast<int32_t>(r.integer));
        case AdmType::String:
        case AdmType::Translate: return static_cast<float>(std::strtod(r.text.c_str(), nullptr));
        default: return static_cast<float>(r.integer);
    }
}

// PORT: asset parsing, a field read with its fallback.
uint32_t value_u32(const AdmRecord& r) {
    switch (r.type) {
        case AdmType::Float: return static_cast<uint32_t>(static_cast<int64_t>(r.number));
        case AdmType::Guid: return static_cast<uint32_t>(r.guid);
        case AdmType::String:
        case AdmType::Translate:
            return static_cast<uint32_t>(std::strtoll(r.text.c_str(), nullptr, 10));
        default: return r.integer;
    }
}

// PORT: asset parsing, a text layout's value as text.
std::string value_text(const AdmRecord& r) {
    switch (r.type) {
        case AdmType::String:
        case AdmType::Translate: return r.text;
        case AdmType::Float: return std::to_string(r.number);
        case AdmType::Guid: return std::to_string(r.guid);
        case AdmType::Integer: return std::to_string(static_cast<int32_t>(r.integer));
        case AdmType::Bool: return r.integer != 0 ? "true" : "false";
        default: return std::to_string(r.integer);
    }
}

// PORT: a float's bits.
uint32_t float_bits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

/// `CDescriptorProp::setData(const wchar_t*, ...)` through `convertDataTo32bit`:
/// the text with its terminator, two UTF-16 units a word, the low one first.
std::vector<uint32_t> string_words(const std::string& text) {
    // An empty text is two zero words in compiled layouts, where the rule would give one.
    if (text.empty()) return {0u, 0u};
    std::vector<uint16_t> units = tl::formats::utf8_to_utf16(text);
    units.push_back(0);
    if (units.size() % 2 != 0) units.push_back(0);
    std::vector<uint32_t> words(units.size() / 2);
    for (size_t i = 0; i < words.size(); ++i) {
        words[i] =
            static_cast<uint32_t>(units[2 * i]) | (static_cast<uint32_t>(units[2 * i + 1]) << 16);
    }
    return words;
}

/// `STRINGS::TokenizeString(queue&, text, L",")`:
/// a piece before every comma, empty ones
/// included, and what follows the last comma when it is not empty. No comma, no
/// pieces.
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> pieces;
    size_t comma = text.find(',');
    if (comma == std::string::npos) return pieces;
    size_t start = 0;
    while (comma != std::string::npos) {
        pieces.push_back(text.substr(start, comma - start));
        start = comma + 1;
        comma = text.find(',', start);
    }
    if (start < text.size()) pieces.push_back(text.substr(start));
    return pieces;
}

/// `STRINGS::GetInt`: `wcstol` with base 0, whose `long` is
/// 32 bits on the console - a number out of range stops at the limit.
int32_t get_int(const std::string& piece) {
    const long long value = std::strtoll(piece.c_str(), nullptr, 0);
    if (value > INT_MAX) return INT_MAX;
    if (value < INT_MIN) return INT_MIN;
    return static_cast<int32_t>(value);
}

/// `STRINGS::GetBool`: false when empty or
/// when it starts with `F`, `f` or `0`.
bool get_bool(const std::string& piece) {
    if (piece.empty()) return false;
    const char c = piece[0];
    return c != 'F' && c != 'f' && c != '0';
}

/// The word index 0 of a word choice is, by node kind and property: what a version 1 or 2 layout
/// that leaves the property out ends with (`CDescriptorProp::loadProperty`).
/// Null when the words come from the object itself.
const char* zero_word(std::string_view kind, std::string_view property) {
    struct Entry {
        const char* kind;
        const char* property;
        const char* word;
    };
    static constexpr Entry kWords[] = {
        {"Group", "CHOICE", "ALL"},
        {"Group", "DIFFICULTY MODE", ""},  // asked on a group, the interpreter gives no word for 0
        {"Unit Trigger", "LOOP STYLE", "No Loop"},
        {"Unit Spawner", "SHAPE", "Angle"},
        {"Unit Spawner", "RELEASE ORDER", "Clockwise"},
        {"Unit Spawner", "DIRECTION", "Forward"},
        {"Sound", "CATEGORY", "ALCHEMISTVOICE"},
        {"Missile", "DAMAGE TYPE", "Physical"},
        {"Particle", "RENDER TYPE", "Billboard"},
        {"Particle", "SORT INDEX", "0"},
        {"Particle", "RENDER STYLE", "Alpha"},
        {"Particle", "ROTATION TYPE", "Geometry"},
        {"Particle", "ORIGIN", "Top Left"},
        {"Emitter", "TYPE OF EMITTER", "Point"},
        {"Linear Force", "FORCE APPLICATION", "Average"},
        {"Sine Force", "FORCE APPLICATION", "Average"},
        {"Box Collision", "TYPE", "Stop"},
        {"Box Collision", "INTERSECTION TYPE", "Fast"},
        {"Plane Collision", "TYPE", "Stop"},
        {"Plane Collision", "INTERSECTION TYPE", "Fast"},
        {"Sphere Collision", "TYPE", "Stop"},
        {"Sphere Collision", "INTERSECTION TYPE", "Fast"},
        {"Timeline", "DEFAULT INTERPOLATION TYPE", "Linear"},
        {"Timeline", "TYPE", "None"},
        {"Counter", "LOGIC", "Activate only once"},
        {"Random Choice", "TYPE", "WEIGHT"},
        {"Property Node", "TYPE", "Point of Interest"},
        {"Camera Controller", "TYPE", "Position"},
        {"Game State Controller", "STATE", "NONE"},
        {"Game State Controller", "TIP", "TIP_INVENTORY"},
        {"Damage Shape", "SHAPE", "Angle"},
        {"Damage Shape", "RELEASE ORDER", "Clockwise"},
        {"Damage Shape", "DIRECTION", "Forward"},
        {"Damage Shape", "DAMAGE TYPE", "Physical"},
        {"Damage Shape", "UNITTYPE", "ANY"},
        {"Damage Shape", "ALIGNMENT", "NEUTRAL"},
    };
    for (const Entry& e : kWords) {
        if (same_ci(e.kind, kind) && e.property == property) return e.word;
    }
    return nullptr;
}

/// The name each property of a descriptor is stored and read under:
/// `CDescriptor::AddProperty` appends `_`
/// to a name until no earlier property of the chain has it, so an `Emitter`'s
/// box `WIDTH` is `WIDTH_`.
std::vector<std::string> stored_names(const schema::Descriptor& descriptor) {
    std::vector<std::string> names;
    std::set<std::string> taken;
    for (size_t i = 0; i < descriptor.property_count; ++i) {
        std::string name = descriptor.properties[i].name;
        while (taken.count(name) != 0) name += "_";
        taken.insert(name);
        names.push_back(name);
    }
    return names;
}

/// A transform property's value before anything sets it, or null.
// PORT: asset parsing, a transform property's value before anything sets it.
const float* transform_default(const std::string& name) {
    static constexpr float kOrigin[] = {0.0f, 0.0f, 0.0f};
    static constexpr float kForward[] = {0.0f, 0.0f, 1.0f};
    static constexpr float kRight[] = {1.0f, 0.0f, 0.0f};
    static constexpr float kUp[] = {0.0f, 1.0f, 0.0f};
    if (name == "POSITION") return kOrigin;
    if (name == "FORWARD") return kForward;
    if (name == "RIGHT") return kRight;
    if (name == "UP") return kUp;
    return nullptr;
}

/// The property, its type and whether it carries a word interpreter.
// PORT: asset parsing, whether a property carries a word interpreter.
bool interpreted(const schema::Property& p) { return p.interpreted; }

/// `CTimeline` interpolation names (`gTIMELINE_INTERP_TYPES`), in the order of
/// `formats::cmp_interpolation`.
constexpr const char* kInterpolationNames[] = {
    "Linear", "Linear Round", "Linear Round Down", "Linear Round Up",
    "Spline", "Quaternion",   "No Interpolation",  "Use Timeline Default",
};

/// The load's configuration:
/// `CDescriptorLoadConfiguration::CDescriptorLoadConfiguration`.
/// The file's ids are kept as they are, so the
/// remap is the index of the objects made.
class SceneLoader {
public:
    // PORT: asset parsing, the text layout reader (`CEditorScene::loadObject`'s order, below).
    explicit SceneLoader(int version) : version_(version) {}

    CmpScene scene;

    // `CEditorScene::loadObject`: a
    // `BASEOBJECT` group's `PROPERTIES` names its descriptor; the first pass
    // makes the object (`CDescriptor::loadObject`), the second sets its parent
    // and reads what it carries besides properties
    // (`CDescriptor::postProcessObject`); both then go on to the children.
    void load_object(const AdmSection& group, bool post) {
        if (!same_ci(group.name, "BASEOBJECT")) return;
        const AdmSection* properties = group_named(group, "PROPERTIES");
        if (properties == nullptr) return;
        const AdmRecord* descriptor_name = value_named(*properties, "DESCRIPTOR");
        if (descriptor_name == nullptr || value_text(*descriptor_name).empty()) return;
        const schema::Descriptor* descriptor = schema::find(value_text(*descriptor_name));
        if (descriptor == nullptr) return;
        if (!post) {
            load_descriptor_object(*descriptor, *properties);
        } else {
            post_process_object(*descriptor, *properties);
        }
        // `CEditorScene::loadObjects` through
        // `configureGroupsToParse`: the
        // object groups under `CHILDREN`, in order.
        const AdmSection* children = group_named(group, "CHILDREN");
        if (children == nullptr) return;
        for (const AdmSection& child : children->children) load_object(child, post);
    }

    // `CDescriptor::loadObject`: the group
    // must be the version's own (`BASEOBJECT` for version 1, `PROPERTIES`
    // after), carry an `ID`, and becomes an object of that id and `NAME` whose
    // properties are read one by one (`CDescriptorProp::loadProperty`). The id
    // is remembered (`CDescriptorLoadConfiguration::addRemap`)
    // for the second pass to find.
    void load_descriptor_object(const schema::Descriptor& descriptor, const AdmSection& group) {
        const int64_t id = value_i64(group, "ID", -1);
        if (id == -1) return;
        CmpNode node;
        node.type = static_cast<uint32_t>(schema::created_index(descriptor.name));
        node.id = id;
        node.parent_id = -1;
        node.root = true;
        const AdmRecord* name = value_named(group, "NAME");
        if (name != nullptr) node.name = value_text(*name);
        const std::vector<std::string> names = stored_names(descriptor);
        for (size_t i = 0; i < descriptor.property_count; ++i) {
            const schema::Property& property = descriptor.properties[i];
            // `loadProperty` passes over a property flagged 0x100 (`+0x3d & 1`),
            // as the compiled form does.
            if (!property.in_binary()) continue;
            CmpField field;
            std::optional<std::vector<uint32_t>> words =
                load_property(descriptor, property, names[i], group);
            if (words.has_value() && !words->empty()) {
                field.words = std::move(*words);
            } else {
                field.present = false;
            }
            node.fields.push_back(std::move(field));
        }
        node.rebuild_views();
        index_[id] = scene.nodes.size();
        scene.nodes.push_back(std::move(node));
    }

    // `CDescriptor::postProcessObject`: the
    // object the id remapped to takes `PARENTID`'s object as its parent (none
    // when that is not in the scene), then the descriptor's `loadAdditionalInfo`
    // reads the rest: a timeline's keys, a logic group's graph.
    void post_process_object(const schema::Descriptor& descriptor, const AdmSection& group) {
        const int64_t id = value_i64(group, "ID", -1);
        const auto found = index_.find(id);
        if (id == -1 || found == index_.end()) return;
        CmpNode& node = scene.nodes[found->second];
        const int64_t parent = value_i64(group, "PARENTID", -1);
        node.parent_id = index_.count(parent) != 0 ? parent : -1;
        node.root = node.parent_id == -1;
        // `loadAdditionalInfo`: `CTimelineDescriptor`'s reads the keys,
        // `CLogicGroupDescriptor::loadAdditionalInfo` the
        // graph; `CPositionableObjectDescriptor::loadAdditionalInfo`
        // does nothing for a data group.
        const std::string_view kind = descriptor.name;
        if (kind == "Timeline") load_timeline(node.id, group);
        if (kind == "Logic Group") load_logic_group(node.id, group);
    }

private:
    // `CDescriptorProp::loadProperty`. Nothing
    // means the property keeps the object's default.
    std::optional<std::vector<uint32_t>> load_property(const schema::Descriptor& descriptor,
                                                       const schema::Property& property,
                                                       const std::string& stored_name,
                                                       const AdmSection& group) const {
        if (version_ < 3) {
            // The property is a group of its own name holding `VALUE` as text.
            const AdmSection* own = group_named(group, stored_name);
            if (own == nullptr && interpreted(property) &&
                property.type != schema::VariableType::String) {
                // The object's value written back as its first byte's word (`zero_word`).
                if (const char* word = zero_word(descriptor.name, stored_name))
                    return string_words(word);
                return std::nullopt;
            }
            if (own == nullptr) {
                const float* fallback = property.type == schema::VariableType::Vector3
                                            ? transform_default(stored_name)
                                            : nullptr;
                if (fallback == nullptr) return std::nullopt;
                return std::vector<uint32_t>{float_bits(fallback[0]), float_bits(fallback[1]),
                                             float_bits(fallback[2])};
            }
            const AdmRecord* value = value_named(*own, "VALUE");
            return property_words_from_string(
                property, value != nullptr ? value_text(*value) : std::string());
        }
        const AdmRecord* value = value_named(group, stored_name);
        if (value != nullptr && value->type == AdmType::String) {
            return property_words_from_string(property, value->text);
        }
        if (!interpreted(property)) {
            switch (property.type) {
                case schema::VariableType::Int:
                case schema::VariableType::UInt:
                    if (value == nullptr) return std::nullopt;
                    return std::vector<uint32_t>{value_u32(*value)};
                case schema::VariableType::Float:
                    if (value == nullptr) return std::nullopt;
                    return std::vector<uint32_t>{float_bits(value_float(*value))};
                case schema::VariableType::Bool:
                    if (value == nullptr) return std::nullopt;
                    return std::vector<uint32_t>{value_u32(*value) != 0 ? 1u : 0u};
                case schema::VariableType::Vector3: {
                    // `name` + X, Y and Z, each defaulting to the object's own
                    // (`CPositionableObject::CPositionableObject`).
                    //
                    static constexpr const char* kAxes[] = {"X", "Y", "Z"};
                    const float* fallback = transform_default(stored_name);
                    std::vector<uint32_t> words;
                    bool any = false;
                    for (int axis = 0; axis < 3; ++axis) {
                        const AdmRecord* c = value_named(group, stored_name + kAxes[axis]);
                        any = any || c != nullptr;
                        const float own = fallback != nullptr ? fallback[axis] : 0.0f;
                        words.push_back(float_bits(c != nullptr ? value_float(*c) : own));
                    }
                    if (!any && fallback == nullptr) return std::nullopt;
                    return words;
                }
                default: break;
            }
        }
        if (value == nullptr) return std::nullopt;
        return string_words(value_text(*value));
    }

    // `CTimeline::loadTimelineFromDataGroup`.
    void load_timeline(int64_t timeline_id, const AdmSection& group) {
        track_interpolation_.clear();
        const AdmSection* data = group_named(group, "TIMELINEDATA");
        if (data == nullptr || value_i64(*data, "ID", -1) != timeline_id) return;
        tl::formats::CmpTimeline timeline;
        timeline.node_id = timeline_id;
        for (const AdmSection& object : data->children) {
            const int64_t target_id = value_i64(object, "OBJECTID", -1);
            const auto target = index_.find(target_id);
            if (target_id == -1 || target == index_.end() || object.children.empty()) continue;
            const CmpNode& target_node = scene.nodes[target->second];
            const schema::Descriptor* descriptor =
                schema::find(schema::kCreatedOrder[target_node.type]);
            if (descriptor == nullptr) continue;
            for (const AdmSection& track_group : object.children) {
                const bool is_event_group = same_ci(track_group.name, "TIMELINEOBJECTEVENT");
                // Any other group ends the whole load.
                if (!is_event_group && !same_ci(track_group.name, "TIMELINEOBJECTPROPERTY")) {
                    finish_timeline(std::move(timeline));
                    return;
                }
                const AdmRecord* property_name = value_named(track_group, "OBJECTPROPERTYNAME");
                const AdmRecord* event_name = value_named(track_group, "OBJECTEVENTNAME");
                const std::string property_text =
                    upper(property_name != nullptr ? value_text(*property_name) : std::string());
                const std::string event_text =
                    event_name != nullptr ? value_text(*event_name) : std::string();
                uint32_t tag = 0;
                bool is_event = false;
                if (property_text.empty()) {
                    if (event_text.empty()) continue;
                    // `CDescriptor::GetInputLogicWrapper` in turn, by exact name.
                    bool found = false;
                    for (size_t i = 0; i < descriptor->input_count; ++i) {
                        if (event_text == logic::input_name(descriptor->inputs[i])) {
                            tag = static_cast<uint32_t>(i);
                            found = true;
                            break;
                        }
                    }
                    if (!found) continue;
                    is_event = true;
                } else {
                    // `CDescriptor::GetPropertyID`: the property's place in the
                    // descriptor's whole list.
                    bool found = false;
                    const std::vector<std::string> names = stored_names(*descriptor);
                    for (size_t i = 0; i < descriptor->property_count; ++i) {
                        if (property_text == names[i]) {
                            tag = static_cast<uint32_t>(i);
                            found = true;
                            break;
                        }
                    }
                    if (!found) continue;
                }
                // `CTimeline::AddProperty`: one track per target and property.
                tl::formats::CmpTimelineTarget* into = nullptr;
                for (tl::formats::CmpTimelineTarget& t : timeline.targets) {
                    if (t.object_id == target_id) into = &t;
                }
                if (into == nullptr) {
                    timeline.targets.push_back({});
                    into = &timeline.targets.back();
                    into->object_id = target_id;
                }
                tl::formats::CmpTimelineTrack* track = nullptr;
                for (tl::formats::CmpTimelineTrack& t : into->tracks) {
                    if (t.tag == tag && t.is_event == is_event) track = &t;
                }
                if (track == nullptr) {
                    into->tracks.push_back({});
                    track = &into->tracks.back();
                    track->tag = tag;
                    track->is_event = is_event;
                }
                for (const AdmSection& point : track_group.children) {
                    tl::formats::CmpTimelineKey key;
                    const AdmRecord* time = value_named(point, "TIMEPERCENT");
                    key.time = time != nullptr ? value_float(*time) : 0.0f;
                    const AdmRecord* interpolation = value_named(point, "INTERPOLATION");
                    const std::string word = interpolation != nullptr
                                                 ? value_text(*interpolation)
                                                 : std::string("Use Timeline Default");
                    uint32_t interpolation_index =
                        tl::formats::cmp_interpolation::kUseTimelineDefault;
                    for (uint32_t i = 0; i < 8; ++i) {
                        if (word == kInterpolationNames[i]) {
                            interpolation_index = i;
                            break;
                        }
                    }
                    // A track keeps one interpolation, not one a point:
                    // each point, an event's too, sets it on the property track of its
                    // target and number, and the last wins.
                    for (const tl::formats::CmpTimelineTrack& t : into->tracks) {
                        if (t.tag == tag && !t.is_event) {
                            // `CTimelineProperty::setInterpolationType`.
                            const schema::Property& property = descriptor->properties[tag];
                            uint32_t effective = interpolation_index;
                            if (!property.interpreted &&
                                property.type == schema::VariableType::Bool) {
                                effective = 6;
                            } else if (effective == 5 &&
                                       (property.interpreted ||
                                        property.type != schema::VariableType::Vector3)) {
                                effective = 0;
                            }
                            track_interpolation_[{target_id, tag}] = effective;
                            break;
                        }
                    }
                    key.interpolation = interpolation_index;
                    if (!is_event) {
                        // `CTimeline::SetPropertyPointValueByString`.
                        const AdmRecord* value = value_named(point, "VALUE");
                        std::optional<std::vector<uint32_t>> words = property_words_from_string(
                            descriptor->properties[tag],
                            value != nullptr ? value_text(*value) : std::string());
                        if (words.has_value()) key.words = std::move(*words);
                    }
                    track->keys.push_back(std::move(key));
                }
            }
        }
        finish_timeline(std::move(timeline));
    }

    // PORT: asset parsing, a timeline the reader finished.
    void finish_timeline(tl::formats::CmpTimeline&& timeline) {
        // An event track's interpolation is never set: `CTimelineProperty::CTimelineProperty`'s
        // `Linear`.
        for (tl::formats::CmpTimelineTarget& target : timeline.targets) {
            for (tl::formats::CmpTimelineTrack& track : target.tracks) {
                const auto found = track_interpolation_.find({target.object_id, track.tag});
                const uint32_t value = !track.is_event && found != track_interpolation_.end()
                                           ? found->second
                                           : tl::formats::cmp_interpolation::kLinear;
                for (tl::formats::CmpTimelineKey& key : track.keys) key.interpolation = value;
            }
        }
        scene.timelines.push_back(std::move(timeline));
    }

    // `CLogicGroup::loadLogicGroupToDataGroup`,
    // its data group half: every `LOGICOBJECT`
    // becomes a member in turn - its number in the group is its place - and its
    // `ID` is filed to that number; then each reads its links
    // (`CLogicObject::loadLogicObjectByDataGroup`),
    // `X` and `Y` defaulting to 100, a link to an `ID` no member
    // has, or with no output or input name, dropped.
    void load_logic_group(int64_t group_id, const AdmSection& group) {
        const AdmSection* logic = group_named(group, "LOGICGROUP");
        if (logic == nullptr) return;
        // `CLogicGroup::CLogicGroup`; each member
        // `CLogicObject::CLogicObject(scene, id, number)`.
        tl::formats::CmpLogicGroup out;
        out.node_id = group_id;
        std::map<int32_t, uint32_t> local;
        std::vector<const AdmSection*> members;
        for (const AdmSection& member : logic->children) {
            if (!same_ci(member.name, "LOGICOBJECT")) continue;
            const int32_t file_id = static_cast<int32_t>(value_i64(member, "ID", -1));
            const int64_t object_id = value_i64(member, "OBJECTID", -1);
            tl::formats::CmpLogicObject object;
            object.local = static_cast<uint32_t>(out.objects.size());
            object.object_id = index_.count(object_id) != 0 ? object_id : -1;
            local[file_id] = object.local;
            out.objects.push_back(std::move(object));
            members.push_back(&member);
        }
        for (size_t i = 0; i < members.size(); ++i) {
            const AdmSection& member = *members[i];
            tl::formats::CmpLogicObject& object = out.objects[i];
            const AdmRecord* x = value_named(member, "X");
            const AdmRecord* y = value_named(member, "Y");
            object.editor_x = x != nullptr ? value_float(*x) : 100.0f;
            object.editor_y = y != nullptr ? value_float(*y) : 100.0f;
            for (const AdmSection& link : member.children) {
                if (!same_ci(link.name, "LOGICLINK")) continue;
                const auto to = local.find(static_cast<int32_t>(value_i64(link, "LINKINGTO", -1)));
                if (to == local.end()) continue;
                const AdmRecord* output = value_named(link, "OUTPUTNAME");
                const AdmRecord* input = value_named(link, "INPUTNAME");
                tl::formats::CmpLogicLink wire;
                wire.to = to->second;
                wire.output = output != nullptr ? value_text(*output) : std::string();
                wire.input = input != nullptr ? value_text(*input) : std::string();
                if (wire.output.empty() || wire.input.empty()) continue;
                object.links.push_back(std::move(wire));
            }
        }
        scene.logic_groups.push_back(std::move(out));
    }

    int version_;
    std::map<int64_t, size_t> index_;
    /// The interpolation each property track (target, number) holds, as the points set it.
    std::map<std::pair<int64_t, uint32_t>, uint32_t> track_interpolation_;
};

/// The order `CEditorScene::saveCompressedLayout` writes the
/// scene in, which the game reloads: every `Group` first, then the root's non-group objects each
/// with its non-group children depth first, then each group's non-group children the same way.
/// `getChildren`, `saveBinaryObjects`,
/// `saveBinaryGroupChildren`.
std::vector<CmpNode> compiled_order(std::vector<CmpNode> loaded) {
    const uint32_t group_kind = static_cast<uint32_t>(schema::created_index("Group"));
    std::vector<CmpNode> out;
    out.reserve(loaded.size());
    std::vector<bool> taken(loaded.size(), false);
    std::multimap<int64_t, size_t> children;  // parent id -> node, in load order
    for (size_t i = 0; i < loaded.size(); ++i) children.emplace(loaded[i].parent_id, i);
    std::vector<int64_t> groups;
    for (size_t i = 0; i < loaded.size(); ++i) {
        if (loaded[i].type != group_kind) continue;
        groups.push_back(loaded[i].id);
        taken[i] = true;
        out.push_back(loaded[i]);
    }
    const auto save_children = [&](int64_t parent, const auto& self) -> void {
        const auto range = children.equal_range(parent);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t i = it->second;
            if (taken[i] || loaded[i].type == group_kind) continue;
            taken[i] = true;
            out.push_back(loaded[i]);
            self(loaded[i].id, self);
        }
    };
    save_children(-1, save_children);
    for (int64_t group : groups) save_children(group, save_children);
    return out;
}

}  // namespace

// PORT: asset parsing, a property's words from its text in a `.layout` file.
std::optional<std::vector<uint32_t>> property_words_from_string(const schema::Property& property,
                                                                const std::string& text) {
    if (interpreted(property) || property.type == schema::VariableType::String) {
        return string_words(text);
    }
    std::vector<std::string> pieces = tokenize(text);
    if (pieces.empty()) pieces.push_back(text);
    std::vector<uint32_t> words;
    for (const std::string& piece : pieces) {
        switch (property.type) {
            case schema::VariableType::Float:
            case schema::VariableType::Vector3:
                words.push_back(
                    float_bits(static_cast<float>(std::strtod(piece.c_str(), nullptr))));
                break;
            case schema::VariableType::Int:
            case schema::VariableType::UInt:
                words.push_back(static_cast<uint32_t>(get_int(piece)));
                break;
            case schema::VariableType::Bool: words.push_back(get_bool(piece) ? 1u : 0u); break;
            default: return std::nullopt;
        }
    }
    return words;
}

CmpScene scene_from_layout_group(const AdmSection& layout) {
    // `VERSION` (3 when absent); `COUNT` is read and not used.
    const AdmRecord* version_record = value_named(layout, "VERSION");
    const int version =
        version_record != nullptr ? static_cast<int32_t>(value_u32(*version_record)) : 3;
    SceneLoader loader(version);
    if (version == 1) {
        // `CDescriptorManager::loadDescriptorObjects`: each
        // descriptor in turn takes the group of its own name, an object per
        // `BASEOBJECT` in it (`CDescriptor::loadObjects`); the
        // second pass is `postProcessDescriptorObjects`.
        for (int pass = 0; pass < 2; ++pass) {
            for (const char* kind : schema::kCreatedOrder) {
                const schema::Descriptor* descriptor = schema::find(kind);
                const AdmSection* objects = group_named(layout, kind);
                if (descriptor == nullptr || objects == nullptr) continue;
                for (const AdmSection& object : objects->children) {
                    if (!same_ci(object.name, "BASEOBJECT")) continue;
                    if (pass == 0) {
                        loader.load_descriptor_object(*descriptor, object);
                    } else {
                        loader.post_process_object(*descriptor, object);
                    }
                }
            }
        }
    } else if (version == 2 || version == 3) {
        const AdmSection* objects = group_named(layout, "OBJECTS");
        if (objects != nullptr) {
            for (const AdmSection& object : objects->children) loader.load_object(object, false);
            for (const AdmSection& object : objects->children) loader.load_object(object, true);
        }
    }
    CmpScene scene = std::move(loader.scene);
    scene.nodes = compiled_order(std::move(scene.nodes));
    scene.ok = !scene.nodes.empty();
    if (!scene.ok) scene.error = "no objects in the layout's data group";
    return scene;
}

// PORT: asset parsing, a layout file, binary or text, as the port's scene.
CmpScene parse_layout_file(const uint8_t* data, size_t size) {
    if (tl::formats::looks_like_dat_text(data, size)) {
        const tl::formats::AdmFile file = tl::formats::parse_dat_text(data, size);
        if (!file.ok) {
            CmpScene scene;
            scene.error = file.error;
            return scene;
        }
        return scene_from_layout_group(file.root);
    }
    CmpScene compiled = tl::formats::parse_cmp(data, size);
    if (compiled.ok) return compiled;
    const tl::formats::AdmFile file = tl::formats::parse_adm(data, size);
    if (file.ok && group_named(file.root, "OBJECTS") != nullptr)
        return scene_from_layout_group(file.root);
    return compiled;
}

}  // namespace tl::core
