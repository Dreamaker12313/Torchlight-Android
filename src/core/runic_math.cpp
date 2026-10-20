#include "core/runic_math.h"
#include "core/fused.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

namespace tl::core {

namespace {

// The constants both functions load onward.
constexpr double kHalfPi = 1.5707963267948966;     // +0x00
constexpr double kInversePi = 0.3183098861837907;  // +0x08
constexpr double kTooLarge = 220000000.0;          // +0x10
constexpr double kPiHigh = 3.1415926534682512;     // +0x28
constexpr double kPiLow = 1.2154201012607932e-10;  // +0x30
constexpr double kC1 = -0.16666666666666666;       // +0x38
constexpr double kC2 = 0.008333333333333165;       // +0x40
constexpr double kC3 = -0.0001984126984120184;     // +0x48
constexpr double kC4 = 2.7557319210152756e-06;     // +0x50
constexpr double kC5 = -2.5052106798274583e-08;    // +0x58
constexpr double kC6 = 1.605893649037159e-10;      // +0x60
constexpr double kC7 = -7.642917806891047e-13;     // +0x68
constexpr double kC8 = 2.7204790957888847e-15;     // +0x70

/// what a too large argument gives, a quiet NaN with the sign set.
double too_large() {
    const std::uint64_t bits = 0xfff8000000000000ull;
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

// The odd polynomial `crt_sin` and `crt_cos` share.
double polynomial(double r) {
    const double z = r * r;
    double p = std::fma(kC8, z, kC7);
    p = std::fma(p, z, kC6);
    p = std::fma(p, z, kC5);
    p = std::fma(p, z, kC4);
    p = std::fma(p, z, kC3);
    p = std::fma(p, z, kC2);
    p = std::fma(p, z, kC1);
    p = std::fma(p, z, 1.0);
    return p * r;
}

}  // namespace

// `sin`.
double crt_sin(double x) {
    const double a = std::fabs(x);
    // The bit test: the argument itself is returned.
    if (a == 0.0) return x;
    // `fsel` on a - 2.2e8.
    if (a - kTooLarge >= 0.0) return too_large();
    // A NaN goes through the arithmetic and comes out a NaN.
    if (a != a) return a;
    const double sign = x >= 0.0 ? 1.0 : -1.0;                 // fsel
    const double q = std::nearbyint(kInversePi * a);           // fctid, fcfid: to nearest
    double r = fnmsub(kPiHigh, q, a);                          // fnmsub
    const bool odd = (static_cast<std::int64_t>(q) & 1) != 0;  // fctidz, clrldi
    r = fnmsub(kPiLow, q, r);                                  // fnmsub
    double s = polynomial(r);
    if (odd) s = -s;
    return s * sign;
}

// `cos`.
double crt_cos(double x) {
    const double a = std::fabs(x);
    const double shifted = kHalfPi + a;                  // fadd
    if (a == 0.0) return 1.0;
    if (shifted - kTooLarge >= 0.0) return too_large();  // fsel
    if (a != a) return a;
    const double q = std::nearbyint(kInversePi * shifted);
    const double half = q - 0.5;  // fsub
    const bool odd = (static_cast<std::int64_t>(q) & 1) != 0;
    double r = fnmsub(kPiHigh, half, a);
    r = fnmsub(kPiLow, half, r);
    double s = polynomial(r);
    if (odd) s = -s;
    return s;
}

// `atan2`, constants. Reduction by sixths of a turn
// past tan(pi/12) (the product fused), a rational polynomial in t^2, then the half plane
// by `fsel` on x and the sign of y by its bit. Doubles throughout.
double crt_atan2(double y, double x) {
    constexpr double kAtanHalfPi = 0x1.921fb54442d18p+0;  // +0x08
    constexpr double kAtanPi = 0x1.921fb54442d18p+1;      // +0x10
    constexpr double kTanTwelfth = 0x1.126145e9ecd56p-2;  // +0x18
    constexpr double kSqrt3 = 0x1.bb67ae8584caap+0;       // +0x28
    constexpr double kP0 = -0x1.b60a651061ce2p+3;         // +0x38
    constexpr double kP1 = -0x1.4817fb9e2bccbp+4;         // +0x40
    constexpr double kP2 = -0x1.0fd3f5c8d6a63p+3;         // +0x48
    constexpr double kP3 = -0x1.acd7ad9b187bdp-1;         // +0x50
    constexpr double kQ0 = 0x1.4887cbcc495a9p+5;          // +0x58
    constexpr double kQ1 = 0x1.58a12040b6da5p+6;          // +0x60
    constexpr double kQ2 = 0x1.dca0a320da3d7p+5;          // +0x68
    constexpr double kQ3 = 0x1.e0c49e14ac710p+3;          // +0x70
    static const double kBase[4] = {0.0, 0x1.0c152382d7366p-1, kAtanHalfPi,
                                    0x1.0c152382d7366p+0};  // +0x80
    const bool y_negative = std::signbit(y);
    double angle;
    if (x == 0.0) {
        if (y == 0.0) {
            if (!std::signbit(x)) return y;
            return y_negative ? -kAtanPi : kAtanPi;
        }
        angle = kAtanHalfPi;
    } else {
        double num = std::fabs(y);
        double den = std::fabs(x);
        int part = 0;
        if (num > den) {
            std::swap(num, den);
            part = 2;
        }
        double t = num / den;
        if (t > kTanTwelfth) {
            ++part;
            const double sum = kSqrt3 + t;
            t = std::fma(kSqrt3, t, -1.0) / sum;
        }
        const double t2 = t * t;
        double p = std::fma(kP3, t2, kP2);
        p = std::fma(p, t2, kP1);
        p = std::fma(p, t2, kP0);
        p = p * t2 * t;
        double q = kQ3 + t2;
        q = std::fma(q, t2, kQ2);
        q = std::fma(q, t2, kQ1);
        q = std::fma(q, t2, kQ0);
        angle = p / q + t;
        if (part > 1) angle = -angle;
        angle = kBase[part] + angle;
    }
    double result = x >= 0.0 ? angle : kAtanPi - angle;  // fsel: -0 counts as not negative
    if (y_negative) result = -result;
    return result;
}

// `MATH::rotateY`.
Vec3 rotate_y(const Vec3& v, float radians) {
    const float c = static_cast<float>(crt_cos(radians));  // frsp
    const float s = static_cast<float>(crt_sin(radians));
    return {std::fma(s, v.z, c * v.x), v.y, std::fma(c, v.z, -(s * v.x))};
}

// `MATH::expandBounds`.
void expand_bounds(Vec3& low, Vec3& high, const Vec3& p) {
    if (p.x < low.x) low.x = p.x;
    if (high.x < p.x) high.x = p.x;
    if (p.y < low.y) low.y = p.y;
    if (high.y < p.y) high.y = p.y;
    if (p.z < low.z) low.z = p.z;
    if (high.z < p.z) high.z = p.z;
}

// `MATH::boundsIntersect`.
bool bounds_intersect(const Vec3& a_low, const Vec3& a_high, const Vec3& b_low,
                      const Vec3& b_high) {
    if (b_high.x < a_low.x) return false;
    if (b_high.z < a_low.z) return false;
    if (b_low.x > a_high.x) return false;
    if (b_low.z > a_high.z) return false;
    if (b_high.y < a_low.y) return false;
    return !(b_low.y > a_high.y);
}

}  // namespace tl::core
