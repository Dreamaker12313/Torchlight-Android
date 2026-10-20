// The console's arithmetic helpers with more than one user: the C runtime's sine, cosine
// and atan2 as the Xbox 360 build links them, and Runic's `MATH::` functions.
// No OGRE, no file I/O.
#pragma once

#include "core/collision.h"

namespace tl::core {

/// The console's `sin(double)`: Microsoft's C runtime, not a correctly
/// rounded sine. +-0 comes back as itself, |x| >= 2.2e8 as a NaN. A `float` sine is this,
/// rounded.
double crt_sin(double x);

/// The console's `cos(double)`: the same reduction of
/// |x| + pi/2 by pi, offset by a half; 0 gives exactly 1.
double crt_cos(double x);

/// The console's `atan2(double y, double x)`, Microsoft's C runtime's;
/// `atan2f` rounds it.
double crt_atan2(double y, double x);

/// `MATH::rotateY(Vector3*, float)`: a turn about the up
/// axis, each term fused, with the console's sine and cosine.
Vec3 rotate_y(const Vec3& v, float radians);

/// `MATH::expandBounds(Vector3&, Vector3&, Vector3)`:
/// `low` and `high` widened to take in `p`, axis by axis.
void expand_bounds(Vec3& low, Vec3& high, const Vec3& p);

/// `MATH::boundsIntersect`: whether two boxes overlap,
/// touching counting. Tested x, z, x, z, y, y; a NaN on either side does not separate them.
bool bounds_intersect(const Vec3& a_low, const Vec3& a_high, const Vec3& b_low, const Vec3& b_high);

}  // namespace tl::core
