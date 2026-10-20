// A layout from its text form: `CEditorScene::loadScene`'s data-group half,
// `CDescriptor::loadObject`, `CDescriptorProp::loadProperty` / `setPropertyValueByString`,
// the timeline and logic-group readers, and the compiled writer's order.

#include "core/layout_text.h"
#include "formats/cmp.h"
#include "formats/dat_text.h"
#include "formats/layout_schema.h"
#include "test_support.h"

#include <cstring>
#include <string>

namespace {

namespace schema = tl::formats::layout_schema;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;

CmpScene read(const std::string& text) {
    return tl::core::parse_layout_file(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

const CmpNode* named(const CmpScene& scene, const std::string& name) { return scene.find(name); }

float as_float(uint32_t word) {
    float f = 0.0f;
    std::memcpy(&f, &word, sizeof f);
    return f;
}

const schema::Property& property(const char* kind, const char* name) {
    const schema::Descriptor* d = schema::find(kind);
    for (size_t i = 0; i < d->property_count; ++i) {
        if (std::string(d->properties[i].name) == name) return d->properties[i];
    }
    return d->properties[0];
}

const char* const kPortal = R"([Layout]
<INTEGER>VERSION:3
<UNSIGNED INT>COUNT:5
[OBJECTS]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Warper
<STRING>NAME:Loose Warper
<INTEGER64>ID:50
<INTEGER64>PARENTID:-1
<STRING>DUNGEON NAME:SOMEWHERE
[/PROPERTIES]
[CHILDREN]
[/CHILDREN]
[/BASEOBJECT]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Group
<STRING>NAME:Portal
<INTEGER64>ID:10
<INTEGER64>PARENTID:-1
[/PROPERTIES]
[CHILDREN]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Unit Trigger
<STRING>NAME:Lever
<INTEGER64>ID:11
<INTEGER64>PARENTID:10
<FLOAT>POSITIONX:-2.5
<FLOAT>POSITIONY:1
<FLOAT>POSITIONZ:28
<STRING>MODEL:Portal
<STRING>LOOP STYLE:Cycle
<STRING>REQUIRED ITEM:
[/PROPERTIES]
[CHILDREN]
[/CHILDREN]
[/BASEOBJECT]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Logic Group
<STRING>NAME:Wires
<INTEGER64>ID:12
<INTEGER64>PARENTID:10
[LOGICGROUP]
[LOGICOBJECT]
<UNSIGNED INT>ID:7
<INTEGER64>OBJECTID:11
<FLOAT>X:54
[LOGICLINK]
<INTEGER>LINKINGTO:9
<STRING>OUTPUTNAME:Triggered
<STRING>INPUTNAME:Activate Warper
[/LOGICLINK]
[LOGICLINK]
<INTEGER>LINKINGTO:3
<STRING>OUTPUTNAME:Triggered
<STRING>INPUTNAME:Activate Warper
[/LOGICLINK]
[/LOGICOBJECT]
[LOGICOBJECT]
<UNSIGNED INT>ID:9
<INTEGER64>OBJECTID:50
[/LOGICOBJECT]
[/LOGICGROUP]
[/PROPERTIES]
[CHILDREN]
[/CHILDREN]
[/BASEOBJECT]
[/CHILDREN]
[/BASEOBJECT]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Timeline
<STRING>NAME:Show
<INTEGER64>ID:60
<INTEGER64>PARENTID:-1
[TIMELINEDATA]
<INTEGER64>ID:60
[TIMELINEOBJECT]
<INTEGER64>OBJECTID:11
[TIMELINEOBJECTPROPERTY]
<STRING>OBJECTPROPERTYNAME:Position
[TIMELINEPOINT]
<FLOAT>TIMEPERCENT:0.5
<STRING>INTERPOLATION:No Interpolation
<STRING>VALUE:1,2,3
[/TIMELINEPOINT]
[/TIMELINEOBJECTPROPERTY]
[TIMELINEOBJECTEVENT]
<STRING>OBJECTEVENTNAME:No Such Input
[TIMELINEPOINT]
<FLOAT>TIMEPERCENT:0
[/TIMELINEPOINT]
[/TIMELINEOBJECTEVENT]
[/TIMELINEOBJECT]
[/TIMELINEDATA]
[/PROPERTIES]
[CHILDREN]
[/CHILDREN]
[/BASEOBJECT]
[/OBJECTS]
[/Layout]
)";

}  // namespace

// `CDescriptorController`'s order is the compiled type; the port's named
// constants agree with it.
TL_TEST(layout_text_created_order_is_the_compiled_type) {
    namespace t = tl::formats::cmp_type;
    TL_CHECK_EQ(schema::created_index("Group"), static_cast<int>(t::kGroup));
    TL_CHECK_EQ(schema::created_index("layout link particle"), static_cast<int>(t::kParticleLink));
    TL_CHECK_EQ(schema::created_index("Generic Model"), static_cast<int>(t::kModel));
    TL_CHECK_EQ(schema::created_index("Missile"), static_cast<int>(t::kMissile));
    TL_CHECK_EQ(schema::created_index("Emitter"), 18);
    TL_CHECK_EQ(schema::created_index("Box Collision"), static_cast<int>(t::kBoxCollision));
    TL_CHECK_EQ(schema::created_index("Timeline"), static_cast<int>(t::kTimeline));
    TL_CHECK_EQ(schema::created_index("Animation Controller"),
                static_cast<int>(t::kAnimationController));
    TL_CHECK_EQ(schema::created_index("Money Taker"), static_cast<int>(t::kMoneyTaker));
    TL_CHECK_EQ(schema::created_index("Waypoint Activator"),
                static_cast<int>(t::kWaypointActivator));
    TL_CHECK_EQ(schema::created_index("Nothing"), -1);
}

TL_TEST(dat_text_reads_groups_and_typed_values) {
    const std::string text =
        "\xEF\xBB\xBF[ROOT]\r\n<INTEGER>A:-1\r\n<UNSIGNED INT>B:7\r\n<FLOAT>C:0.5\r\n"
        "<BOOL>D:true\r\n<INTEGER64>E:-9000000000\r\n<STRING>F:x: y\r\n[KID]\r\n"
        "<BOOL>G:0\r\n[/KID]\r\nnonsense\r\n[/ROOT]\r\n";
    const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
    TL_CHECK(tl::formats::looks_like_dat_text(bytes, text.size()));
    const tl::formats::AdmFile file = tl::formats::parse_dat_text(bytes, text.size());
    TL_CHECK(file.ok);
    TL_CHECK_EQ(file.root.name, std::string("ROOT"));
    TL_CHECK_EQ(file.root.integer_or("A", 0), 0xffffffffu);
    TL_CHECK_EQ(file.root.integer_or("B", 0), 7u);
    TL_CHECK(file.root.float_or("C", 0.0f) == 0.5f);
    TL_CHECK(file.root.bool_or("D", false));
    TL_CHECK_EQ(file.root.guid_or("E", 0), int64_t{-9000000000});
    TL_CHECK_EQ(file.root.string_or("F", ""), std::string("x: y"));
    TL_CHECK_EQ(file.root.children.size(), size_t{1});
    TL_CHECK(!file.root.children[0].bool_or("G", true));
    TL_CHECK(file.undecoded_bytes > 0);
}

// `setPropertyValueByString`: commas split a value into the property's type; an empty text
// is two zero words.
TL_TEST(layout_text_property_values_from_text) {
    const auto life =
        tl::core::property_words_from_string(property("Emitter", "PARTICLE LIFE"), "1,0.6,0.7");
    TL_CHECK(life.has_value());
    TL_CHECK_EQ(life->size(), size_t{3});
    TL_CHECK(as_float((*life)[1]) == 0.6f);
    // No comma: the whole text is the one piece; a trailing comma adds none.
    TL_CHECK_EQ(
        TL_VALUE(tl::core::property_words_from_string(property("Emitter", "PARTICLE LIFE"), "2"))
            .size(),
        size_t{1});
    TL_CHECK_EQ(
        TL_VALUE(tl::core::property_words_from_string(property("Emitter", "PARTICLE LIFE"), "1,2,"))
            .size(),
        size_t{2});
    // An integer is `wcstol` with base 0, stopping at the console's 32-bit limit.
    TL_CHECK_EQ(
        TL_VALUE(tl::core::property_words_from_string(property("Emitter", "NUM LOOPS"), "0x10"))[0],
        16u);
    TL_CHECK_EQ(TL_VALUE(tl::core::property_words_from_string(property("Emitter", "NUM LOOPS"),
                                                              "9999999999"))[0],
                0x7fffffffu);
    // A flag is false for nothing, F..., f... or 0...
    TL_CHECK_EQ(TL_VALUE(tl::core::property_words_from_string(property("Emitter", "KEEP LOCAL"),
                                                              "False"))[0],
                0u);
    TL_CHECK_EQ(
        TL_VALUE(tl::core::property_words_from_string(property("Emitter", "KEEP LOCAL"), "yes"))[0],
        1u);
    const auto word =
        tl::core::property_words_from_string(property("Unit Trigger", "LOOP STYLE"), "Cycle");
    TL_CHECK(word.has_value());
    CmpField field;
    field.words = *word;
    TL_CHECK_EQ(field.as_text(), std::string("Cycle"));
    TL_CHECK_EQ(word->size(), size_t{3});  // five characters and the terminator
    TL_CHECK_EQ(
        TL_VALUE(tl::core::property_words_from_string(property("Unit Trigger", "MODEL"), ""))
            .size(),
        size_t{2});
}

TL_TEST(layout_text_reads_objects_fields_and_parents) {
    const CmpScene scene = read(kPortal);
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{5});
    const CmpNode* lever = named(scene, "Lever");
    TL_CHECK(lever != nullptr);
    TL_CHECK_EQ(lever->type, tl::formats::cmp_type::kUnitTrigger);
    TL_CHECK_EQ(lever->parent_id, int64_t{10});
    TL_CHECK(!lever->root);
    namespace f = tl::formats::cmp_field::unit_trigger;
    const CmpField* position = lever->field(f::kPosition);
    TL_CHECK(position != nullptr && position->count() == 3);
    TL_CHECK(position->as_float(0) == -2.5f && position->as_float(2) == 28.0f);
    TL_CHECK(lever->has_position() && lever->position().z == 28.0f);
    TL_CHECK_EQ(lever->field(f::kModel)->as_text(), std::string("Portal"));
    TL_CHECK_EQ(lever->field(f::kLoopStyle)->as_text(), std::string("Cycle"));
    // Not in the text: the object's own default - the transform's from
    // `CPositionableObject`'s constructor, any other left to the reader (null).
    TL_CHECK(lever->field(f::kForward) != nullptr &&
             lever->field(f::kForward)->as_float(2) == 1.0f);
    TL_CHECK(lever->field(f::kRight) != nullptr && lever->field(f::kRight)->as_float(0) == 1.0f);
    TL_CHECK(lever->field(f::kEnabled) == nullptr);
    TL_CHECK_EQ(lever->fields.size(), schema::field_count(*schema::find("Unit Trigger")));
}

// `saveCompressedLayout`'s order: every group, then the root's non-group
// objects depth first, then each group's non-group children.
TL_TEST(layout_text_takes_the_compiled_order) {
    const CmpScene scene = read(kPortal);
    TL_CHECK_EQ(scene.nodes[0].name, std::string("Portal"));
    TL_CHECK_EQ(scene.nodes[1].name, std::string("Loose Warper"));
    TL_CHECK_EQ(scene.nodes[2].name, std::string("Show"));
    TL_CHECK_EQ(scene.nodes[3].name, std::string("Lever"));
    TL_CHECK_EQ(scene.nodes[4].name, std::string("Wires"));
}

// `loadLogicGroupToDataGroup`: members numbered in file order; a link to an id
// no member has is dropped; `X`/`Y` default to 100.
TL_TEST(layout_text_reads_logic_groups) {
    const CmpScene scene = read(kPortal);
    const tl::formats::CmpLogicGroup* group = scene.logic_group(12);
    TL_CHECK(group != nullptr);
    TL_CHECK_EQ(group->objects.size(), size_t{2});
    TL_CHECK_EQ(group->objects[0].object_id, int64_t{11});
    TL_CHECK(group->objects[0].editor_x == 54.0f && group->objects[0].editor_y == 100.0f);
    TL_CHECK_EQ(group->objects[0].links.size(), size_t{1});
    TL_CHECK_EQ(group->objects[0].links[0].to, 1u);
    TL_CHECK_EQ(group->resolve(1), int64_t{50});
    TL_CHECK_EQ(group->objects[0].links[0].input, std::string("Activate Warper"));
}

// `loadTimelineFromDataGroup`: a track by its upper-cased name; unknown inputs are skipped.
TL_TEST(layout_text_reads_timelines) {
    const CmpScene scene = read(kPortal);
    const tl::formats::CmpTimeline* timeline = scene.timeline(60);
    TL_CHECK(timeline != nullptr);
    TL_CHECK_EQ(timeline->targets.size(), size_t{1});
    TL_CHECK_EQ(timeline->targets[0].object_id, int64_t{11});
    TL_CHECK_EQ(timeline->targets[0].tracks.size(), size_t{1});
    const tl::formats::CmpTimelineTrack& track = timeline->targets[0].tracks[0];
    TL_CHECK(!track.is_event);
    TL_CHECK_EQ(std::string(schema::find("Unit Trigger")->properties[track.tag].name),
                std::string("POSITION"));
    TL_CHECK_EQ(track.keys.size(), size_t{1});
    TL_CHECK(track.keys[0].time == 0.5f);
    TL_CHECK_EQ(track.keys[0].interpolation, tl::formats::cmp_interpolation::kNoInterpolation);
    TL_CHECK(track.keys[0].vec3().y == 2.0f);
}

// Version 2 keeps each property in a group of its name with `VALUE` as text;
// a repeated property name is stored with `_` (`CDescriptor::AddProperty`).
TL_TEST(layout_text_version_two_and_repeated_names) {
    const std::string text = R"([LAYOUT]
<INTEGER>VERSION:2
[OBJECTS]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Emitter
<STRING>NAME:Box
<INTEGER64>ID:5
[POSITION]
<STRING>VALUE:1,2,3
[/POSITION]
[WIDTH]
<STRING>VALUE:0,4
[/WIDTH]
[WIDTH_]
<STRING>VALUE:9
[/WIDTH_]
[/PROPERTIES]
[/BASEOBJECT]
[/OBJECTS]
[/LAYOUT]
)";
    const CmpScene scene = read(text);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});
    const CmpNode& box = scene.nodes[0];
    TL_CHECK(box.root);
    const schema::Descriptor& emitter = *schema::find("Emitter");
    const CmpField* curve = box.field(schema::field_index(emitter, "WIDTH", 0));
    const CmpField* plain = box.field(schema::field_index(emitter, "WIDTH", 1));
    TL_CHECK(curve != nullptr && curve->count() == 2 && curve->as_float(1) == 4.0f);
    TL_CHECK(plain != nullptr && plain->count() == 1 && plain->as_float() == 9.0f);
    TL_CHECK(box.field(schema::field_index(emitter, "POSITION"))->as_float(2) == 3.0f);
}

// A node kind the console does not register, or an object with no id, is not
// made.
TL_TEST(layout_text_skips_unknown_kinds_and_missing_ids) {
    const std::string text = R"([Layout]
[OBJECTS]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:No Such Kind
<INTEGER64>ID:1
[/PROPERTIES]
[/BASEOBJECT]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Light
[/PROPERTIES]
[/BASEOBJECT]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:light
<INTEGER64>ID:2
<UNSIGNED INT>BRIGHTNESS:2
[/PROPERTIES]
[/BASEOBJECT]
[/OBJECTS]
[/Layout]
)";
    const CmpScene scene = read(text);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});
    TL_CHECK_EQ(scene.nodes[0].type, tl::formats::cmp_type::kLight);
    TL_CHECK_EQ(scene.nodes[0].field(tl::formats::cmp_field::light::kBrightness)->as_uint(), 2u);
}

TL_TEST(layout_text_reads_a_waypoint_activator) {
    // The console registers it too: a positionable node.
    const std::string text = R"([Layout]
[OBJECTS]
[BASEOBJECT]
[PROPERTIES]
<STRING>DESCRIPTOR:Waypoint Activator
<INTEGER64>ID:7
<FLOAT>POSITIONX:1.5
[/PROPERTIES]
[/BASEOBJECT]
[/OBJECTS]
[/Layout]
)";
    const CmpScene scene = read(text);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});
    TL_CHECK_EQ(scene.nodes[0].type, tl::formats::cmp_type::kWaypointActivator);
}
