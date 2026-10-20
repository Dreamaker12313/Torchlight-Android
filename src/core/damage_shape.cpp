#include "core/damage_shape.h"

#include "core/unit_types.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

namespace tl::core {

namespace {

namespace df = tl::formats::cmp_field::damage_shape;

/// A word's index in one of the original's string tables, compared as
/// `wmemcmp` does (case and all), or `fallback` past the end.
// PORT: asset parsing, a field read with its fallback.
int32_t word_index(const std::string& word, std::initializer_list<const char*> names,
                   int32_t fallback = 0) {
    int32_t i = 0;
    for (const char* name : names) {
        if (word == name) return i;
        ++i;
    }
    return fallback;
}

// PORT: asset parsing, a field read with its fallback.
std::string text_of(const tl::formats::CmpNode& node, size_t index) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr ? f->as_text() : std::string();
}

// PORT: asset parsing, a field read with its fallback.
float float_of(const tl::formats::CmpNode& node, size_t index, float fallback) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr && f->count() == 1 ? f->as_float() : fallback;
}

// PORT: asset parsing, a field read with its fallback.
int32_t int_of(const tl::formats::CmpNode& node, size_t index, int32_t fallback) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr && f->count() == 1 ? static_cast<int32_t>(f->as_uint()) : fallback;
}

// PORT: asset parsing, a field read with its fallback.
bool bool_of(const tl::formats::CmpNode& node, size_t index, bool fallback) {
    const tl::formats::CmpField* f = node.field(index);
    return f != nullptr && f->count() == 1 ? f->as_bool() : fallback;
}

}  // namespace

DamageShape read_damage_shape(const tl::formats::CmpNode& node,
                              const UnitTypeHierarchy* unit_types) {
    DamageShape s;
    // `CShapeDescriptor`'s properties (`CShape`'s static init, for
    // the words).
    s.shape.shape =
        word_index(text_of(node, df::kShape), {"Angle", "Line", "Sphere", "Point", "Box"});
    s.shape.release_order =
        word_index(text_of(node, df::kReleaseOrder), {"Clockwise", "Counter Clockwise", "Random"});
    s.shape.direction =
        word_index(text_of(node, df::kDirection),
                   {"Forward", "Down", "Up", "Outward From Center", "Inward to Center"});
    if (const tl::formats::CmpField* size = node.field(df::kBoxSize);
        size != nullptr && size->count() == 3) {
        s.shape.box_size = {size->as_float(0), size->as_float(1), size->as_float(2)};
    }
    s.shape.use_max_only = bool_of(node, df::kUseMaxOnly, false);
    // `CShape::setMinRadius`, `CShape::setMaxRadius`,
    // `CShape::setAngleOfRelease` and
    // `CShape::setAngleOffset`, inlined on the console:
    // each takes `getDynPropFromArray` of the property's floats; an
    // unauthored one keeps `CShape::CShape`'s.
    if (const tl::formats::CmpField* f = node.field(df::kMinRadius))
        s.shape.min_radius = dynamic_attribute(f);
    if (const tl::formats::CmpField* f = node.field(df::kMaxRadius))
        s.shape.max_radius = dynamic_attribute(f);
    if (const tl::formats::CmpField* f = node.field(df::kAngle))
        s.shape.angle = dynamic_attribute(f);
    if (const tl::formats::CmpField* f = node.field(df::kAngleOffset))
        s.shape.angle_offset = dynamic_attribute(f);

    // `CDamageShapeDescriptor::GetDamageTypeIDByString` over
    // `gDAMAGE_TYPES`; an unknown word is 0.
    if (node.field(df::kDamageType) != nullptr) {
        s.damage_type =
            word_index(text_of(node, df::kDamageType),
                       {"Physical", "Magical", "Fire", "Ice", "Electric", "Poison", "All"});
    }
    // `getUnitTypeIDByString`, then
    // `CDamageShape::setDamageOnlyUnitTypeByUnitTypeLoadIndex`: a known
    // name keeps its id, an unknown one gets load index 0's, `any`.
    if (unit_types != nullptr && node.field(df::kUnitType) != nullptr) {
        const uint32_t id = unit_types->id_of(text_of(node, df::kUnitType));
        s.unit_type = id == UnitTypeHierarchy::kUnknown ? 0u : id;
    }
    // `getAlignmentIDByString` over `KALIGNMENT_STRINGS`.
    if (node.field(df::kAlignment) != nullptr) {
        s.alignment =
            word_index(text_of(node, df::kAlignment),
                       {"NEUTRAL", "GOOD", "EVIL", "ALL", "BERSERK", "EVILBERSERK", "GOODBERSERK"});
    }
    s.update_in_editor = bool_of(node, df::kUpdateInEditor, s.update_in_editor);
    s.enabled = bool_of(node, df::kEnabled, s.enabled);
    s.dot = bool_of(node, df::kDot, s.dot);
    s.target_corpse = bool_of(node, df::kTargetCorpse, s.target_corpse);
    s.max_units_to_hit = int_of(node, df::kMaxUnitsToHit, s.max_units_to_hit);
    s.min_percent = float_of(node, df::kMin, s.min_percent);
    s.max_percent = float_of(node, df::kMax, s.max_percent);
    s.loops = bool_of(node, df::kLoops, s.loops);
    // `Set_setNumLoops` sets the count left too, never below 0.
    s.loop_count = int_of(node, df::kLoopCount, s.loop_count);
    s.loops_left = std::max(0, s.loop_count);
    // `Set_setTimer`: the countdown with it.
    s.timer = float_of(node, df::kTimer, s.timer);
    s.time_left = s.timer;
    // `Set_setDelayTimer` zeroes the pause left.
    s.pause_timer = float_of(node, df::kPauseTimer, s.pause_timer);
    s.pause_left = 0.0f;
    return s;
}

// `CDamageShape::setEnabled`.
DamageShapeSwitch damage_shape_set_enabled(DamageShape& s, bool on) {
    s.pause_left = 0.0f;
    s.enabled = on;
    s.loops_left = std::max(0, s.loop_count);
    s.time_left = s.timer;
    if (!on) return DamageShapeSwitch::Stop;  // the hit list is kept
    s.hit.clear();
    if (0.001f < s.timer || std::isnan(s.timer) || s.loops_left != 0) {
        s.done = false;
        s.time_left = s.timer;
        s.pause_left = 0.0f;
        s.ticked = false;
        return DamageShapeSwitch::Listen;
    }
    return DamageShapeSwitch::Instant;
}

// `CDamageShape::updateLevelObject`.
bool damage_shape_finished(const DamageShape& s) {
    return s.done && s.pause_left <= 0.0f && s.ticked && !s.loops;
}

// `CDamageShape::updateLevelObject`. A shape
// that loops for ever with a negative `TIMER` sits at 1 and is done at once,
// but never finishes.
void damage_shape_step(DamageShape& s, float delta) {
    if (s.loops && !(0.0f < s.timer || s.timer == 0.0f)) {
        s.percent = 1.0f;
        s.done = true;
        return;
    }
    s.pause_left -= delta;
    if (!(s.pause_left < 0.0f && (s.loops || 0 < s.loops_left))) return;
    s.time_left -= delta;
    if (s.time_left <= 0.0f) {
        s.time_left += s.timer;
        if (!s.loops && 0 < s.loops_left) {
            s.pause_left = s.pause_timer;
            const int32_t left = s.loops_left - 1;
            s.loops_left = left;
            if (s.loop_count < 0) {
                s.loops_left = 1;
            } else if (left == 0) {
                s.done = true;
                s.time_left = 0.0f;
            }
        }
    }
    float divisor = s.timer;
    if (divisor == 0.0f) divisor = 1.0f;
    s.percent = 1.0f - s.time_left / divisor;
}

// `CDamageShape::characterUpdatedInLevel`.
bool damage_shape_reaches(const DamageShape& s, const ShapeFrame& frame, bool hidden,
                          bool targetable, bool alive, const Vec3& at, float radius) {
    if (!s.enabled || hidden || !targetable) return false;
    if (s.target_corpse == alive) return false;
    return shape_contains_position(s.shape, frame, s.percent, at, radius);
}

DamageShapeUnit damage_shape_item_unit(int64_t id) {
    // One address no machine has: an item is told from a character by it.
    static const char kItems = 0;
    return DamageShapeUnit{&kItems, id};
}

// `CDamageShape::itemUpdatedInLevel` -> `doDamageToItem`.
//
bool damage_shape_do_damage_to_item(DamageShape& s, const ShapeFrame& frame,
                                    const DamageShapeItem& item, bool isa) {
    if (!shape_contains_position(s.shape, frame, s.percent, item.at, item.radius)) return false;
    if (!s.enabled || !isa) return false;
    if (!s.dot) {
        const DamageShapeUnit entry = damage_shape_item_unit(item.id);
        if (std::find(s.hit.begin(), s.hit.end(), entry) != s.hit.end()) return false;
        s.hit.push_back(entry);
    }
    // Every listener is asked, whatever the ones before it said.
    bool all = true;
    const std::vector<DamageShapeListener*> listeners = s.listeners;
    for (DamageShapeListener* listener : listeners) {
        if (listener != nullptr && !listener->affect_item(s, item)) all = false;
    }
    return all;
}

// `CDamageShape::doDamageToCharacter`: the
// amount.
float damage_shape_amount(const DamageShape& s, float graph_value, float delta, RunicRandom& rng) {
    // `MAX` scales the curve; with a different `MIN` the draw runs from 0 to
    // that less `MIN / 100`, and `MIN / 100` goes back on - a percentage of the
    // curve beside a bare fraction, as the binary has it.
    // each percent times 0.01f, the maximum's then
    // times the curve.
    float amount = (s.max_percent * 0.01f) * graph_value;
    if (s.max_percent != s.min_percent) {
        const float floor = s.min_percent * 0.01f;
        amount = rng.between(0.0f, amount - floor) + floor;
    }
    if (s.dot) amount *= delta;
    return amount;
}

// `CDamageShape::doDamageToCharacter`.
DamageShapeHit damage_shape_do_damage_to_character(DamageShape& s, const DamageShapeVictim& victim,
                                                   const StatGraph* damage_graph, int32_t level,
                                                   float delta, RunicRandom& rng) {
    DamageShapeHit out;
    if (!victim.isa) return out;
    if (s.alignment != 3 && victim.alignment != s.alignment) return out;
    if (victim.alive == s.target_corpse) return out;
    if (!s.dot) {
        if (0 < s.max_units_to_hit && s.max_units_to_hit <= static_cast<int32_t>(s.hit.size())) {
            return out;
        }
        if (std::find(s.hit.begin(), s.hit.end(), victim.unit) != s.hit.end()) return out;
    }
    // The listeners: any one that declines, or that acts on the character
    // itself, keeps the shape's own damage off; any that was asked to act puts
    // the character on a non-DOT shape's list at once.
    if (!s.listeners.empty()) {
        bool own = true;
        bool asked = false;
        for (size_t i = 0; i < s.listeners.size(); ++i) {  // the count re-read every step
            DamageShapeListener* listener = s.listeners[i];
            if (!listener->can_affect(s, victim.unit)) {
                own = false;
            } else {
                asked = true;
                if (listener->affect(s, victim.unit)) own = false;
            }
        }
        if (asked && !s.dot) s.hit.push_back(victim.unit);
        if (!own) return out;
        out.record = !asked && !s.dot;
    } else {
        out.record = !s.dot;
    }
    if (damage_graph == nullptr) {
        out.record = false;
        return out;
    }
    out.amount = damage_shape_amount(s, damage_graph->at(static_cast<float>(level)), delta, rng);
    // `CDamageShape::getCanDoDamage` always agrees.
    out.deal = true;
    return out;
}

}  // namespace tl::core
