// Float to integer as the x86 and PowerPC builds convert it, defined for every input
// (an out-of-range C++ cast is undefined and differs per CPU). In range: the plain cast.
// Use wherever the float comes from data rather than already-bounded code.
#pragma once

#include <cstdint>
#include <limits>

namespace tl::formats {

/// `cvttss2si` to 32 bits.
inline int32_t x86_float_to_int32(float v) {
    if (!(v >= -2147483648.0f && v < 2147483648.0f)) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(v);
}

/// `cvttss2si` to 64 bits.
inline int64_t x86_float_to_int64(float v) {
    if (!(v >= -9223372036854775808.0f && v < 9223372036854775808.0f)) {
        return std::numeric_limits<int64_t>::min();
    }
    return static_cast<int64_t>(v);
}

/// GCC's `(uint32_t)float` on x86-64: the 64-bit conversion's low half.
inline uint32_t x86_float_to_uint32(float v) {
    return static_cast<uint32_t>(static_cast<uint64_t>(x86_float_to_int64(v)));
}

/// PowerPC's `fctiwz` (the Xbox 360's float to int32): toward zero, and out of
/// range it saturates - NaN and anything below the minimum to the minimum,
/// anything above the maximum to the maximum.
inline int32_t ppc_fctiwz(double v) {
    if (v != v || v <= -2147483649.0) return std::numeric_limits<int32_t>::min();
    if (v >= 2147483648.0) return std::numeric_limits<int32_t>::max();
    return static_cast<int32_t>(v);
}

/// GCC's `(int16_t)float` on x86-64: the 32-bit conversion's low half.
inline int16_t x86_float_to_int16(float v) {
    return static_cast<int16_t>(
        static_cast<uint16_t>(static_cast<uint32_t>(x86_float_to_int32(v))));
}

}  // namespace tl::formats
