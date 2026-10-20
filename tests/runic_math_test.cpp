// The console's arithmetic, pinned bit for bit (a non-fused `std::fma` or a reordered
// evaluation goes red).
#include "core/runic_math.h"
#include "core/collision.h"
#include "core/fused.h"
#include "core/ogre_math.h"
#include "core/randomizer.h"
#include "core/route.h"
#include "test_support.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

std::uint64_t bits(double d) {
    std::uint64_t w;
    std::memcpy(&w, &d, 8);
    return w;
}

std::uint32_t bits(float f) {
    std::uint32_t w;
    std::memcpy(&w, &f, 4);
    return w;
}

float from_bits(std::uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

}  // namespace

/// `std::fma` has to round once, or nothing below can hold.
TL_TEST(fma_is_fused) {
    const double a = 1.0 + 0x1p-27;
    const double b = 1.0 + 0x1p-27;
    TL_CHECK(std::fma(a, b, -(1.0 + 0x1p-26)) == 0x1p-54);
    const float fa = 1.0f + 0x1p-12f;
    TL_CHECK(std::fma(fa, fa, -(1.0f + 0x1p-11f)) == 0x1p-24f);
}

/// The console's `sin` and `cos`, not the C library's:
/// sin(pi/2) is one unit short of 1.
TL_TEST(crt_sine_and_cosine_are_the_consoles) {
    struct Case {
        double x;
        std::uint64_t sin;
        std::uint64_t cos;
    };
    const Case cases[] = {
        {0.5, 0x3fdeaee8744b05f0ull, 0x3fec1528065b7d4full},
        {1.0, 0x3feaed548f090ceeull, 0x3fe14a280fb5068cull},
        {2.0, 0x3fed18f6ead1b446ull, 0xbfdaa22657537205ull},
        {100.0, 0xbfe03425b78c4db8ull, 0x3feb981dbf665fe0ull},
        {0.001, 0x3f50624da5218a62ull, 0x3feffffef390876bull},
        {-3.0, 0xbfc210386db6d55bull, 0xbfefae04be85e5d2ull},
        {7.5, 0x3fee041886fcae30ull, 0x3fd62f45e66f5c2full},
        {1.5707963267948966, 0x3fefffffffffffffull, 0x3c91a60000000000ull},
        {3.141592653589793, 0x3ca1a60000000000ull, 0xbff0000000000000ull},
    };
    for (const Case& c : cases) {
        TL_CHECK_EQ(bits(tl::core::crt_sin(c.x)), c.sin);
        TL_CHECK_EQ(bits(tl::core::crt_cos(c.x)), c.cos);
    }
    TL_CHECK_EQ(bits(tl::core::crt_sin(-0.0)), bits(-0.0));
    TL_CHECK(tl::core::crt_cos(0.0) == 1.0);
    TL_CHECK(tl::core::crt_sin(3.0e8) != tl::core::crt_sin(3.0e8));  // a NaN past 2.2e8
}

/// `MATH::rotateY`: fused, so these differ from the unfused
/// sum in the last place.
TL_TEST(rotate_y_is_fused_as_the_consoles) {
    const tl::core::Vec3 v{3.25f, 1.0f, -7.125f};
    struct Case {
        std::uint32_t radians, x, z;
    };
    const Case cases[] = {
        {0x3dcccccdu, 0x40216fd5u, 0xc0ed3e5eu},
        {0x3ef0a3d7u, 0xbea88f39u, 0xc0fa60b4u},
        {0x3ff9999au, 0xc0fa4d37u, 0xbec35e7eu},
    };
    for (const Case& c : cases) {
        const tl::core::Vec3 r = tl::core::rotate_y(v, from_bits(c.radians));
        TL_CHECK_EQ(bits(r.x), c.x);
        TL_CHECK_EQ(bits(r.y), bits(1.0f));
        TL_CHECK_EQ(bits(r.z), c.z);
    }
}

/// `CRandomizer`'s normalisation multiplies by the
/// reciprocal: a lone choice of 83 is 0.99999994, not 1.
TL_TEST(randomizer_normalises_by_the_reciprocal) {
    tl::core::Randomizer lone;
    lone.add_choice(5, 83);
    lone.compute_odds();
    TL_CHECK_EQ(bits(lone.normalised_at(0)), 0x3f7fffffu);
}

/// `CPath`: fused lengths, the console's spline.
TL_TEST(route_is_the_consoles_path) {
    tl::core::Route route;
    route.add_point({0.0f, 0.0f, 0.0f});
    route.add_point({1.1f, 0.0f, 2.3f});
    route.add_point({-4.7f, 0.5f, 3.3f});
    route.add_point({-4.7f, 0.5f, 3.3f});
    route.add_point({9.0f, -1.0f, 0.25f});
    TL_CHECK_EQ(bits(route.length()), 0x41b492aau);
    struct Case {
        float d;
        std::uint32_t px, py, pz, sx, sy, sz;
    };
    const Case cases[] = {
        {0.5f, 0x3e5ce7bcu, 0x00000000u, 0x3ee6f244u, 0x3e8160dfu, 0xbbfd4907u, 0x3ea69d2au},
        {2.0f, 0x3f5ce7bcu, 0x00000000u, 0x3fe6f244u, 0x3fa675acu, 0xbd07d20cu, 0x3fec9e0eu},
        {7.25f, 0xc060fe4au, 0x3ecbb833u, 0x40462140u, 0xc071ab04u, 0x3ed80669u, 0x404ba621u},
        {11.0f, 0xc00ecadbu, 0x3e6b32cau, 0x403005ecu, 0xc0383744u, 0x3e99e1eeu, 0x40393ec2u},
        {20.0f, 0x40d02136u, 0xbf3a0a66u, 0x3f4e4030u, 0x40e4e394u, 0xbf4c3954u, 0x3f29472cu},
    };
    for (const Case& c : cases) {
        const tl::core::Vec3 p = route.position_at_distance(c.d);
        const tl::core::Vec3 s = route.spline_position_at_distance(c.d);
        TL_CHECK_EQ(bits(p.x), c.px);
        TL_CHECK_EQ(bits(p.y), c.py);
        TL_CHECK_EQ(bits(p.z), c.pz);
        TL_CHECK_EQ(bits(s.x), c.sx);
        TL_CHECK_EQ(bits(s.y), c.sy);
        TL_CHECK_EQ(bits(s.z), c.sz);
    }
    tl::core::Vec3 nearest;
    float along = 0.0f;
    TL_CHECK(route.find_nearest_point({-1.0f, 0.0f, 3.0f}, nearest, along));
    TL_CHECK_EQ(bits(nearest.x), 0xbf8543b9u);
    TL_CHECK_EQ(bits(nearest.y), 0x3e3d0292u);
    TL_CHECK_EQ(bits(nearest.z), 0x402ad385u);
    TL_CHECK_EQ(bits(along), 0x40975c9fu);
}

/// The console's `atan2`, each reduction branch and the
/// signed-zero and half-plane cases.
TL_TEST(crt_atan2_pinned) {
    struct Case {
        double y, x;
        std::uint64_t want;
    };
    const Case cases[] = {
        {1.0, 1.0, 0x3fe921fb54442d18ull},     {0.5, -2.0, 0x40072c43f4b1650aull},
        {-3.0, 0.25, 0xbff7cd6f6dc59db4ull},   {0.2679491924311227, 1.0, 0x3fd0c152382d7365ull},
        {-0.0, -1.0, 0xc00921fb54442d18ull},   {7.0, 0.0, 0x3ff921fb54442d18ull},
        {-1e-5, -40.0, 0xc00921fb32b63dd7ull},
    };
    for (const Case& c : cases) TL_CHECK_EQ(bits(tl::core::crt_atan2(c.y, c.x)), c.want);
    // +-0 over +0 is the numerator itself.
    TL_CHECK_EQ(bits(tl::core::crt_atan2(-0.0, 0.0)), bits(-0.0));
}

/// OGRE's maths as the console's: an orientation from turned axes, an inverse transform, a
/// quaternion turn and a closest point.
TL_TEST(ogre_math_pinned) {
    const tl::core::OgreQuaternion q =
        tl::core::node_orientation({0.422934592f, 0.0f, -0.906160176f}, {0.0f, 1.0f, 0.0f},
                                   {0.906160176f, 0.0f, 0.422934592f});
    TL_CHECK_EQ(bits(q.w), 0x3f57eea5u);
    TL_CHECK_EQ(bits(q.x), 0x00000000u);
    TL_CHECK_EQ(bits(q.y), 0x3f0982d2u);
    TL_CHECK_EQ(bits(q.z), 0x00000000u);
    const tl::core::Vec3 local = tl::core::matrix_transform(
        tl::core::matrix_inverse(tl::core::object_transform(q, {0.4f, 1.4f, -3.0f})),
        {2.5f, 0.0f, 1.25f});
    TL_CHECK_EQ(bits(local.x), 0xc03da218u);
    TL_CHECK_EQ(bits(local.y), 0xbfb33333u);
    TL_CHECK_EQ(bits(local.z), 0x406cd380u);
    const tl::core::Vec3 r = tl::core::quaternion_rotate(q, {0.4f, 0.0f, -1.7f});
    TL_CHECK_EQ(bits(r.x), 0xbfaf86b6u);
    TL_CHECK_EQ(bits(r.y), 0x00000000u);
    TL_CHECK_EQ(bits(r.z), 0xbf8a6d0du);
    const tl::core::Vec3 c = tl::core::closest_point_on_line({0.0f, 0.0f, 0.0f}, {3.0f, 1.0f, 2.0f},
                                                             {1.5f, -2.0f, 0.75f});
    TL_CHECK_EQ(bits(c.x), 0x3f5b6db8u);
    TL_CHECK_EQ(bits(c.y), 0x3e924925u);
    TL_CHECK_EQ(bits(c.z), 0x3f124925u);
}

/// PowerPC's negated fused forms negate after the rounding, so an exact zero is -0. The
/// arguments are `volatile` so the compiler cannot fold the call.
TL_TEST(negated_fused_forms_keep_the_sign_of_an_exact_zero) {
    volatile float a = 2.0f, c = 3.0f, six = 6.0f, minus_six = -6.0f;
    TL_CHECK_EQ(bits(tl::core::fnmsub(static_cast<float>(a), static_cast<float>(c),
                                      static_cast<float>(six))),
                0x80000000u);
    TL_CHECK_EQ(bits(tl::core::fnmadd(static_cast<float>(a), static_cast<float>(c),
                                      static_cast<float>(minus_six))),
                0x80000000u);
    volatile double da = 2.0, dc = 3.0, dsix = 6.0;
    TL_CHECK_EQ(bits(tl::core::fnmsub(static_cast<double>(da), static_cast<double>(dc),
                                      static_cast<double>(dsix))),
                0x8000000000000000ull);
}
