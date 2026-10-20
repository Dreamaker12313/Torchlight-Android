// The layout node kinds' property tables, transcribed from the Xbox image's
// descriptor constructors (`formats/layout_schema.h`).
#include "formats/layout_schema.h"
#include "test_support.h"

#include <string>

namespace ls = tl::formats::layout_schema;

// What the console registers, checked at compile time.
static_assert(ls::count() == 60, "60 node kinds");
static_assert(ls::field("Light", "VISIBLE") == 0 && ls::field("Light", "ANGLE") == 6,
              "Light: VISIBLE, POSITION, FILE, BRIGHTNESS, SCALE X, SCALE Z, ANGLE");
// `CPositionableObjectDescriptor` entered with `[1, 1, 1, 1, 0]`: no VISIBLE.
static_assert(ls::field("Unit Trigger", "POSITION") == 0, "a Unit Trigger has no VISIBLE");
static_assert(ls::field("unit trigger", "loop style") == 6, "names match case-insensitively");
// `[1, 0, 0, 0, 0]`: POSITION only, before the controller's own.
static_assert(ls::field("Skill Controller", "LEARN ON START") == 1, "after its POSITION");
// `PIECE` carries 0x101: not in the binary, so `VISUAL` is field 9.
static_assert(ls::field("Room Piece", "PIECE") == ls::npos, "PIECE is not in the binary");
static_assert(ls::field("Room Piece", "VISUAL") == 9, "VISUAL follows GUID");
// An Emitter registers WIDTH, HEIGHT and DEPTH twice.
static_assert(ls::field("Emitter", "WIDTH", 1) == 26, "the box's WIDTH");
// `CLayoutDescriptor`'s variants.
static_assert(ls::field("Layout Link", "START ON LOAD") == ls::npos, "variant 0");
static_assert(ls::field("Layout Link Particle", "START ON LOAD") == 7, "variant 1");
static_assert(ls::field("Layout Link Timeline", "DURATION MODIFY") == 8, "variant 2");

TL_TEST(layout_schema_finds_every_kind_by_its_name) {
    for (size_t i = 0; i < ls::count(); ++i) {
        const ls::Descriptor& d = ls::begin()[i];
        TL_CHECK(ls::find(d.name) == &d);
    }
    // The console's too: its factory is unnamed and registered last.
    const ls::Descriptor* waypoint = ls::find("Waypoint Activator");
    TL_CHECK(waypoint != nullptr && ls::field_count(*waypoint) == 5);
    TL_CHECK(waypoint != nullptr && waypoint->input_count == 3 && waypoint->inputs[2] == 0x0f);
}

TL_TEST(layout_schema_takes_the_unit_triggers_helper_events) {
    // CUnitTriggerDescriptor::AddInputsAndOutputs: seven inputs, five outputs after the base's.
    const ls::Descriptor* trigger = ls::find("Unit Trigger");
    TL_CHECK(trigger != nullptr && trigger->input_count == 9 && trigger->output_count == 7);
    TL_CHECK(trigger != nullptr && trigger->inputs[6] == 0x2e);
}

TL_TEST(layout_schema_marks_the_choices_stored_as_words) {
    const ls::Descriptor* trigger = ls::find("Unit Trigger");
    TL_CHECK(trigger != nullptr);
    const ls::Property* loop =
        ls::field_property(*trigger, ls::field("Unit Trigger", "LOOP STYLE"));
    TL_CHECK(loop != nullptr && loop->interpreted && (loop->flags & ls::kStoredAsWord) != 0);
    TL_CHECK(loop->type == ls::VariableType::UInt);
    const ls::Descriptor* room = ls::find("Room Piece");
    TL_CHECK(room != nullptr && ls::field_count(*room) == 14);
    TL_CHECK(std::string(ls::field_property(*room, 9)->name) == "VISUAL");
}
