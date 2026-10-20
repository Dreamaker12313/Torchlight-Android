#pragma once

// How a timeline property reaches its value between keys: the value half of
// `CTimelineProperty`. The clock, events and applying a value are `core/script.cpp`'s.

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "formats/cmp.h"

namespace tl::core {

/// `gTIMELINE_INTERP_TYPES`, in the original's order.
enum class TimelineInterpolation : uint32_t {
    Linear = 0,
    LinearRound = 1,
    LinearRoundDown = 2,
    LinearRoundUp = 3,
    Spline = 4,
    Quaternion = 5,
    NoInterpolation = 6,
    UseTimelineDefault = 7,
};

/// `CDescriptorProp::getVariableType` codes a timeline can animate.
enum class TimelineValueType : uint32_t {
    Integer = 2,
    Float = 3,
    Unsigned = 4,
    String = 5,
    Bool = 6,
    Vector2 = 7,
    Vector3 = 8,
    Vector4 = 9,
};

/// Kinds of properties driven by timeline value tracks.
enum class TimelinePropertyKind : uint32_t {
    Position,
    Forward,
    Right,
    Up,
    Visible,
    Other,
};

/// A property resolved from a target descriptor's tag (`CDescriptor::GetPropertyByIndex`).
struct TimelineProperty {
    TimelinePropertyKind kind = TimelinePropertyKind::Other;
    TimelineValueType value_type = TimelineValueType::Vector3;
    std::string_view name;
};

/// A property's value as the raw dwords `CDescriptorProp` holds.
struct TimelineValue {
    std::vector<uint32_t> words;

    float as_float(size_t index = 0) const;
    int32_t as_int(size_t index = 0) const;
    tl::formats::CmpVec3 as_vec3() const;
};

/// Resolves which property a timeline value track drives on a given descriptor.
/// Transcribed from `CTimeline::AddProperty`
/// and `CDescriptor::GetPropertyByIndex`.
std::optional<TimelineProperty> resolve_timeline_property(uint32_t descriptor, uint32_t tag);

/// The node fields holding the three columns of `CPositionableObject +0xc0`
/// (the orientation a `RIGHT`, `UP` or `FORWARD` key replaces one at a time),
/// for the descriptors whose tracks resolve to them above.
struct OrientationFields {
    size_t right = 0;
    size_t up = 0;
    size_t forward = 0;
};
std::optional<OrientationFields> orientation_fields(uint32_t descriptor);

/// A timeline's `DEFAULT INTERPOLATION TYPE` name to its value. Exact match
/// against the first seven names only, so "Use Timeline Default" is Linear.
TimelineInterpolation parse_timeline_interpolation(std::string_view name);

/// The type a property keeps once its keys are loaded: the last key's tag,
/// through `SetInterpolationType`'s two overrides.
TimelineInterpolation timeline_property_interpolation(uint32_t last_key_tag,
                                                      TimelineValueType type);

/// One component between two keys, `u` of the way, rounded by `rounding`.
double timeline_component(TimelineInterpolation rounding, double u,
                          const tl::formats::CmpTimelineKey& a,
                          const tl::formats::CmpTimelineKey& b, size_t index,
                          TimelineValueType type);

/// The value between `a` and `b` at `percent`, or nothing for a type the
/// original does not blend.
std::optional<TimelineValue> timeline_linear(const tl::formats::CmpTimelineKey& a,
                                             const tl::formats::CmpTimelineKey& b, float percent,
                                             TimelineInterpolation property,
                                             TimelineInterpolation timeline_default,
                                             TimelineValueType type);

/// The keys as an `Ogre::SimpleSpline` of vectors, sampled at `percent`.
std::optional<TimelineValue> timeline_spline(const std::vector<tl::formats::CmpTimelineKey>& keys,
                                             float percent, TimelineValueType type);

/// The value half of `CTimelineProperty::Update`: what, if anything, the
/// property is set to at `percent`, `last` being the percent last applied
/// (after `Update`'s wrap adjustments) and `percent` already clamped to 0..1.
std::optional<TimelineValue> timeline_value(const std::vector<tl::formats::CmpTimelineKey>& keys,
                                            TimelineInterpolation property,
                                            TimelineInterpolation timeline_default, float percent,
                                            float last, bool backwards, TimelineValueType type);

}  // namespace tl::core
