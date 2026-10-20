// Float-to-integer conversions give x86-64's answers on any CPU. Rows marked `*` differ from
// AArch64's plain cast (it saturates, and NaN goes to 0).
#include "formats/float_convert.h"
#include "test_support.h"

#include <cmath>
#include <cstdint>
#include <limits>

using tl::formats::x86_float_to_int16;
using tl::formats::x86_float_to_int32;
using tl::formats::x86_float_to_int64;
using tl::formats::x86_float_to_uint32;

namespace {
constexpr int32_t kI32Min = std::numeric_limits<int32_t>::min();
constexpr int64_t kI64Min = std::numeric_limits<int64_t>::min();
const float kInf = std::numeric_limits<float>::infinity();
const float kNan = std::numeric_limits<float>::quiet_NaN();
}  // namespace

TL_TEST(in_range_conversions_are_the_plain_cast) {
    TL_CHECK_EQ(x86_float_to_int32(-2.5f), -2);
    TL_CHECK_EQ(x86_float_to_int32(70000.9f), 70000);
    TL_CHECK_EQ(x86_float_to_int64(3e9f), int64_t{3000000000});
    TL_CHECK_EQ(x86_float_to_uint32(4294967040.0f), 4294967040u);
    TL_CHECK_EQ(x86_float_to_int16(-2.5f), int16_t{-2});
}

// `cvttss2si`: anything out of range, and NaN, is the type's minimum.
TL_TEST(out_of_range_signed_conversions_are_x86_indefinite) {
    TL_CHECK_EQ(x86_float_to_int32(3e9f), kI32Min);  // *
    TL_CHECK_EQ(x86_float_to_int32(-3e9f), kI32Min);
    TL_CHECK_EQ(x86_float_to_int32(kInf), kI32Min);           // *
    TL_CHECK_EQ(x86_float_to_int32(kNan), kI32Min);           // *
    TL_CHECK_EQ(x86_float_to_int32(2147483648.0f), kI32Min);  // * 2^31, one past
    TL_CHECK_EQ(x86_float_to_int64(3.4e38f), kI64Min);        // *
    TL_CHECK_EQ(x86_float_to_int64(-kInf), kI64Min);
    TL_CHECK_EQ(x86_float_to_int64(kNan), kI64Min);  // *
}

// Unsigned through the 64-bit conversion: -1 is 2^32 - 1, -3e9 is 2^32 - 3e9, 5e9 is
// 5e9 - 2^32, and the 64-bit indefinite's low half is 0.
TL_TEST(uint32_conversion_keeps_the_low_half_of_the_64_bit_one) {
    TL_CHECK_EQ(x86_float_to_uint32(-1.0f), 4294967295u);  // *
    TL_CHECK_EQ(x86_float_to_uint32(-2.5f), 4294967294u);  // *
    TL_CHECK_EQ(x86_float_to_uint32(-3e9f), 1294967296u);  // *
    TL_CHECK_EQ(x86_float_to_uint32(5e9f), 705032704u);    // *
    TL_CHECK_EQ(x86_float_to_uint32(3.4e38f), 0u);         // *
    TL_CHECK_EQ(x86_float_to_uint32(kNan), 0u);
}

// int16 through the 32-bit conversion: 70000 - 65536 = 4464; out of range is
// the 32-bit indefinite's low half, 0.
TL_TEST(int16_conversion_keeps_the_low_half_of_the_32_bit_one) {
    TL_CHECK_EQ(x86_float_to_int16(70000.0f), int16_t{4464});
    TL_CHECK_EQ(x86_float_to_int16(3e9f), int16_t{0});  // *
    TL_CHECK_EQ(x86_float_to_int16(kNan), int16_t{0});
}
