#pragma once

// The pieces of OGRE's maths the game's own logic goes through, as the
// console's OGRE has them: `Math::fDeg2Rad`, `Vector3::normalise`, a node's
// orientation from three axes, a vector turned by a quaternion, and a point
// through a node's transform. The console's arithmetic (`fmadds`/`fmsubs`,
// single `fsqrts` and `fdivs`) is kept, so a position the game works out
// through a node lands on the same float as the console's.

#include "core/collision.h"
#include "core/fused.h"

#include <cmath>

namespace tl::core {

/// `Ogre::Math::fDeg2Rad` as the console holds it:
/// 0x3c8efa36, one unit above `float(PI) / 180`. Not the game's own literal
/// (0x3c8efa35).
constexpr float kOgreDegToRad = 0x1.1df46cp-6f;

/// `Ogre::Quaternion`, w first as OGRE stores it.
struct OgreQuaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// `Ogre::Vector3::normalise`: the length's squares in two
/// `fmadds`, left alone at or below 1e-08 (a double on the console).
inline Vec3 ogre_normalised(const Vec3& v) {
    const float length = std::sqrt(std::fma(v.z, v.z, std::fma(v.x, v.x, v.y * v.y)));
    if (!(static_cast<double>(length) > 1e-08)) return v;
    const float inverse = 1.0f / length;
    return {v.x * inverse, v.y * inverse, v.z * inverse};
}

/// `Ogre::Quaternion::FromRotationMatrix`: `m[row][column]`.
inline OgreQuaternion quaternion_from_rotation_matrix(const float m[3][3]) {
    OgreQuaternion q;
    const float trace = (m[0][0] + m[1][1]) + m[2][2];
    if (trace > 0.0f) {
        float root = std::sqrt(trace + 1.0f);
        q.w = root * 0.5f;
        root = 0.5f / root;
        q.x = (m[2][1] - m[1][2]) * root;
        q.y = (m[0][2] - m[2][0]) * root;
        q.z = (m[1][0] - m[0][1]) * root;
        return q;
    }
    static const int kNext[3] = {1, 2, 0};
    int i = 0;
    if (m[1][1] > m[0][0]) i = 1;
    if (m[2][2] > m[i][i]) i = 2;
    const int j = kNext[i];
    const int k = kNext[j];
    float* xyz[3] = {&q.x, &q.y, &q.z};
    float root = std::sqrt(((m[i][i] - m[j][j]) - m[k][k]) + 1.0f);
    *xyz[i] = root * 0.5f;
    root = 0.5f / root;
    q.w = (m[k][j] - m[j][k]) * root;
    *xyz[j] = (m[j][i] + m[i][j]) * root;
    *xyz[k] = (m[k][i] + m[i][k]) * root;
    return q;
}

/// `Ogre::Quaternion::FromAxes`: the axes are the matrix's
/// columns.
inline OgreQuaternion quaternion_from_axes(const Vec3& x_axis, const Vec3& y_axis,
                                           const Vec3& z_axis) {
    const float m[3][3] = {{x_axis.x, y_axis.x, z_axis.x},
                           {x_axis.y, y_axis.y, z_axis.y},
                           {x_axis.z, y_axis.z, z_axis.z}};
    return quaternion_from_rotation_matrix(m);
}

/// `Ogre::Node::setOrientation(const Quaternion&)` keeps the
/// quaternion normalised: its norm in three `fmadds`, then each part times
/// `1 / sqrt(norm)`.
inline OgreQuaternion quaternion_normalised(const OgreQuaternion& q) {
    const float norm = std::fma(q.z, q.z, std::fma(q.y, q.y, std::fma(q.w, q.w, q.x * q.x)));
    const float factor = 1.0f / std::sqrt(norm);
    return {q.w * factor, q.x * factor, q.y * factor, q.z * factor};
}

/// A node's derived orientation, `parent * own`, as
/// `Node::_updateFromParentImpl` inlines the product: each
/// part a chain of `fmadds`, ending in `fnmsubs` - which negates its rounded
/// `a * c - b`, so an exact zero comes out -0.
inline OgreQuaternion quaternion_derived(const OgreQuaternion& p, const OgreQuaternion& q) {
    const auto fnmsubs = [](float a, float c, float b) { return fnmsub(a, c, b); };
    OgreQuaternion out;
    out.w = fnmsubs(q.z, p.z, fnmsubs(q.y, p.y, std::fma(q.w, p.w, -(q.x * p.x))));
    out.x = fnmsubs(q.y, p.z, std::fma(q.x, p.w, std::fma(q.z, p.y, q.w * p.x)));
    out.y = fnmsubs(q.z, p.x, std::fma(q.y, p.w, std::fma(q.x, p.z, q.w * p.y)));
    out.z = fnmsubs(q.x, p.y, std::fma(q.z, p.w, std::fma(q.w, p.z, q.y * p.x)));
    return out;
}

/// A positionable object's node orientation from its own axes:
/// `CPositionableObject::setOrientation(const Matrix4&, bool)`
/// (`CPositionableObject::setRight`/`setUp`/`setForward`),
/// normalised, then derived from the parent (`Node::_getDerivedOrientation`).
/// The layout node is unturned but derived once, so its zeros are -0.
constexpr OgreQuaternion kLayoutNodeOrientation{1.0f, -0.0f, -0.0f, -0.0f};
inline OgreQuaternion node_orientation(const Vec3& right, const Vec3& up, const Vec3& forward,
                                       const OgreQuaternion& parent = kLayoutNodeOrientation) {
    return quaternion_derived(parent,
                              quaternion_normalised(quaternion_from_axes(right, up, forward)));
}

/// `Ogre::Quaternion::operator*(const Vector3&)`:
/// v + 2w(q x v) + 2(q x (q x v)), each cross product's terms in one `fmsubs`.
inline Vec3 quaternion_rotate(const OgreQuaternion& q, const Vec3& v) {
    const float two_w = q.w * 2.0f;
    const float uv_x = std::fma(v.z, q.y, -(v.y * q.z));
    const float uv_y = std::fma(v.x, q.z, -(v.z * q.x));
    const float uv_z = std::fma(v.y, q.x, -(v.x * q.y));
    const float uuv_x = std::fma(uv_z, q.y, -(uv_y * q.z));
    const float uuv_y = std::fma(uv_x, q.z, -(uv_z * q.x));
    const float uuv_z = std::fma(uv_y, q.x, -(q.y * uv_x));
    return {(v.x + uv_x * two_w) + uuv_x * 2.0f, (v.y + uv_y * two_w) + uuv_y * 2.0f,
            (v.z + uv_z * two_w) + uuv_z * 2.0f};
}

/// `Ogre::Quaternion::xAxis`, `yAxis`, `zAxis`, as a positionable object's absolute right, up
/// and forward inline them.
inline Vec3 quaternion_x_axis(const OgreQuaternion& q) {
    const float ty = q.y * 2.0f, tz = q.z * 2.0f;
    const float tyy = q.y * ty, txy = q.x * ty, twy = q.w * ty;
    return {1.0f - std::fma(q.z, tz, tyy), std::fma(q.w, tz, txy), std::fma(q.x, tz, -twy)};
}
inline Vec3 quaternion_y_axis(const OgreQuaternion& q) {
    const float tx = q.x * 2.0f, ty = q.y * 2.0f, tz = q.z * 2.0f;
    const float txx = q.x * tx, twz = q.w * tz, tyz = q.y * tz;
    return {std::fma(ty, q.x, -twz), 1.0f - std::fma(q.z, tz, txx), std::fma(q.w, tx, tyz)};
}
inline Vec3 quaternion_z_axis(const OgreQuaternion& q) {
    const float tx = q.x * 2.0f, ty = q.y * 2.0f, tz = q.z * 2.0f;
    const float txx = q.x * tx, twx = q.w * tx, txz = q.x * tz;
    return {std::fma(q.w, ty, txz), std::fma(q.y, tz, -twx), 1.0f - std::fma(q.y, ty, txx)};
}

/// `Ogre::Matrix4`, `m[row][column]`.
struct OgreMatrix4 {
    float m[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                     {0.0f, 1.0f, 0.0f, 0.0f},
                     {0.0f, 0.0f, 1.0f, 0.0f},
                     {0.0f, 0.0f, 0.0f, 1.0f}};
};

/// A positionable object's transform (its vtable +0x7c):
/// `Matrix4(Quaternion)` (through
/// `Quaternion::ToRotationMatrix`) of the node's derived
/// orientation, and the world position in the last column. No scale.
inline OgreMatrix4 object_transform(const OgreQuaternion& q, const Vec3& position) {
    const float tx = q.x * 2.0f, ty = q.y * 2.0f, tz = q.z * 2.0f;
    const float twx = q.w * tx, twy = q.w * ty, twz = q.w * tz;
    const float txx = q.x * tx, txy = q.x * ty, txz = q.x * tz;
    const float tyy = q.y * ty, tyz = q.y * tz, tzz = q.z * tz;
    OgreMatrix4 out;
    out.m[0][0] = 1.0f - (tzz + tyy);
    out.m[0][1] = txy - twz;
    out.m[0][2] = txz + twy;
    out.m[1][0] = txy + twz;
    out.m[1][1] = 1.0f - (tzz + txx);
    out.m[1][2] = tyz - twx;
    out.m[2][0] = txz - twy;
    out.m[2][1] = tyz + twx;
    out.m[2][2] = 1.0f - (tyy + txx);
    out.m[0][3] = position.x;
    out.m[1][3] = position.y;
    out.m[2][3] = position.z;
    return out;
}

/// `Ogre::Matrix4::inverse`, every product pair fused. `fused` is the
/// fused multiply-add (`std::fma`; a test can pass a twice-rounded one).
template <class Fused>
OgreMatrix4 matrix_inverse_with(const OgreMatrix4& in, Fused fused) {
    const auto& m = in.m;
    const auto fms = [&](float a, float b, float c) { return fused(a, b, -c); };
    const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    float v0 = fms(m31, m20, m30 * m21);
    float v1 = fms(m32, m20, m30 * m22);
    float v2 = fms(m33, m20, m30 * m23);
    float v3 = fms(m32, m21, m31 * m22);
    float v4 = fms(m33, m21, m31 * m23);
    float v5 = fms(m33, m22, m32 * m23);

    const float t00 = fused(v3, m13, fms(v5, m11, v4 * m12));
    const float t10 = negated(fused(v1, m13, fms(v5, m10, v2 * m12)));  // fnmadds
    const float t20 = fused(v0, m13, fms(v4, m10, v2 * m11));
    const float t30 = negated(fused(v0, m12, fms(v3, m10, v1 * m11)));  // fnmadds
    const float inv_det = 1.0f / fused(t00, m00, fused(t10, m01, fused(t30, m03, t20 * m02)));

    OgreMatrix4 out;
    auto& d = out.m;
    d[0][0] = inv_det * t00;
    d[1][0] = inv_det * t10;
    d[2][0] = inv_det * t20;
    d[3][0] = inv_det * t30;
    d[0][1] = -(fused(v3, m03, fms(v5, m01, v4 * m02)) * inv_det);
    d[1][1] = fused(v1, m03, fms(v5, m00, v2 * m02)) * inv_det;
    d[2][1] = -(fused(v0, m03, fms(v4, m00, v2 * m01)) * inv_det);
    d[3][1] = fused(v0, m02, fms(v3, m00, v1 * m01)) * inv_det;

    v0 = fms(m31, m10, m30 * m11);
    v1 = fms(m32, m10, m30 * m12);
    v2 = fms(m33, m10, m30 * m13);
    v3 = fms(m32, m11, m31 * m12);
    v4 = fms(m33, m11, m31 * m13);
    v5 = fms(m33, m12, m32 * m13);
    d[0][2] = fused(v3, m03, fms(v5, m01, v4 * m02)) * inv_det;
    d[1][2] = -(fused(v1, m03, fms(v5, m00, v2 * m02)) * inv_det);
    d[2][2] = fused(v0, m03, fms(v4, m00, v2 * m01)) * inv_det;
    d[3][2] = -(fused(v0, m02, fms(v3, m00, v1 * m01)) * inv_det);

    v0 = fms(m21, m10, m20 * m11);
    v1 = fms(m22, m10, m20 * m12);
    v2 = fms(m23, m10, m20 * m13);
    v3 = fms(m22, m11, m21 * m12);
    v4 = fms(m23, m11, m21 * m13);
    v5 = fms(m23, m12, m22 * m13);
    d[0][3] = -(fused(v3, m03, fms(v5, m01, v4 * m02)) * inv_det);
    d[1][3] = fused(v1, m03, fms(v5, m00, v2 * m02)) * inv_det;
    d[2][3] = -(fused(v0, m03, fms(v4, m00, v2 * m01)) * inv_det);
    d[3][3] = fused(v0, m02, fms(v3, m00, v1 * m01)) * inv_det;
    return out;
}

inline OgreMatrix4 matrix_inverse(const OgreMatrix4& in) {
    return matrix_inverse_with(in, [](float a, float b, float c) { return std::fma(a, b, c); });
}

/// `Ogre::AxisAlignedBox`, finite: its corners.
struct OgreBox {
    Vec3 min;
    Vec3 max;
};

/// `AxisAlignedBox(minX, minY, minZ, maxX, maxY, maxZ)` as the console builds
/// it: a minimum past its maximum is put 0.01 under it,
/// axis by axis.
inline OgreBox ogre_box(const Vec3& min, const Vec3& max) {
    OgreBox b{min, max};
    if (b.min.x > b.max.x) b.min.x = b.max.x - 0.01f;
    if (b.min.y > b.max.y) b.min.y = b.max.y - 0.01f;
    if (b.min.z > b.max.z) b.min.z = b.max.z - 0.01f;
    return b;
}

/// `AxisAlignedBox::intersects(const AxisAlignedBox&)`,
/// both finite: apart only where a comparison says so (a NaN does not).
inline bool box_intersects(const OgreBox& a, const OgreBox& b) {
    if (a.max.x < b.min.x || a.max.y < b.min.y || a.max.z < b.min.z) return false;
    return !(a.min.x > b.max.x || a.min.y > b.max.y || a.min.z > b.max.z);
}

/// `AxisAlignedBox::contains(const AxisAlignedBox&)`, both
/// finite.
inline bool box_contains(const OgreBox& a, const OgreBox& b) {
    if (a.min.x > b.min.x || a.min.y > b.min.y || a.min.z > b.min.z) return false;
    return !(b.max.x > a.max.x || b.max.y > a.max.y || b.max.z > a.max.z);
}

/// `Ogre::Matrix4::operator*(const Vector3&)`: each row in
/// two `fmadds` and an add, then divided through by the fourth.
template <class Fused>
Vec3 matrix_transform_with(const OgreMatrix4& t, const Vec3& v, Fused fused) {
    const auto& m = t.m;
    const float w = fused(m[3][2], v.z, fused(m[3][0], v.x, m[3][1] * v.y)) + m[3][3];
    const float x = fused(v.x, m[0][0], fused(m[0][1], v.y, m[0][2] * v.z)) + m[0][3];
    const float y = fused(m[1][2], v.z, fused(m[1][0], v.x, m[1][1] * v.y)) + m[1][3];
    const float z = fused(m[2][2], v.z, fused(m[2][0], v.x, m[2][1] * v.y)) + m[2][3];
    const float inverse = 1.0f / w;
    return {x * inverse, y * inverse, z * inverse};
}
inline Vec3 matrix_transform(const OgreMatrix4& t, const Vec3& v) {
    return matrix_transform_with(t, v, [](float a, float b, float c) { return std::fma(a, b, c); });
}

}  // namespace tl::core
