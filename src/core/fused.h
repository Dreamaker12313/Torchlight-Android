#pragma once

// PowerPC's negated fused multiply-adds, as the console computes them:
// `fnmadds`/`fnmadd` is -(a*c + b) and `fnmsubs`/`fnmsub` is -(a*c - b), one
// rounding, then the sign flipped, so an exact zero comes out -0. Written as
// `-std::fma(a, c, b)` a compiler may fold the negation into the product (x86's
// `vfnmsub`, AArch64's `fnmadd`) and answer +0; the empty `asm` keeps the
// rounded result and its negation apart. No OGRE, no I/O.

#include <cmath>

namespace tl::core {

/// The sign of `r` flipped after it has been rounded, whatever the compiler
/// would like to fold.
template <class T>
inline T negated(T r) {
    __asm__("" : "+m"(r));
    return -r;
}

/// `fnmadds` (and the double `fnmadd`): -(a*c + b), one rounding.
template <class T>
inline T fnmadd(T a, T c, T b) {
    return negated(std::fma(a, c, b));
}

/// `fnmsubs` (and the double `fnmsub`): -(a*c - b), one rounding.
template <class T>
inline T fnmsub(T a, T c, T b) {
    return negated(std::fma(a, c, -b));
}

}  // namespace tl::core
