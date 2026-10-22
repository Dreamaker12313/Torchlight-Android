// A layout node's pose as the editor wrote it (core/layout_maths.h).
#include "core/layout_maths.h"

#include <cmath>

namespace tl::core {

/// The orientation a layout node carries, as a rotation.
///
/// A placeable node stores a position and then three unit vectors, and the
/// TorchED source names them: `FORWARD`, `RIGHT`, `UP`, in that order. **The
/// basis matrix wants its columns in the order (RIGHT, UP, FORWARD)**, not in
/// the order the file stores them - OGRE's convention is that a rotation
/// matrix's columns are the local X, Y and Z axes in world space, and FORWARD
/// is the local +Z.
///
/// Written that way it is right-handed and checks out on every case that looked
/// absurd under the two orderings tried in earlier sessions:
///
///  * the default room piece - FORWARD (0,0,1), RIGHT (1,0,0), UP (0,1,0) - is
///    the **identity**, rather than the 120-degree turn about (1,1,1) that
///    using the file's order as the column order produces;
///  * a turned room piece - FORWARD (0,0,-1), RIGHT (-1,0,0), UP (0,1,0) - is a
///    half-turn about the vertical, which is what a tileset needs;
///  * the menu camera's basis has determinant 1 and agrees with its own look
///    direction.
///
/// A node shorter than four vectors, or one whose vectors are not a rotation,
/// gets the identity. That is not paranoia: the basis is checked for
/// orthonormality and for a positive determinant, because a reflection handed to
/// `ogre::Quaternion(Matrix3)` yields an arbitrary rotation rather than an
/// error, and a silently arbitrary orientation is the worst of the three
/// outcomes.
ogre::Quaternion orientation_from_basis(const ogre::Vector3& forward, const ogre::Vector3& right,
                                        const ogre::Vector3& up) {
    // Unit length, to within the precision the editor wrote them at.
    for (const ogre::Vector3& axis : {forward, right, up}) {
        const float length = axis.length();
        if (length < 0.99f || length > 1.01f) return ogre::Quaternion::IDENTITY;
    }

    // Columns are the local axes: X = RIGHT, Y = UP, Z = FORWARD.
    ogre::Matrix3 basis;
    basis.SetColumn(0, right);
    basis.SetColumn(1, up);
    basis.SetColumn(2, forward);

    // A left-handed triple is a reflection, and no rotation represents it.
    if (right.crossProduct(up).dotProduct(forward) < 0.9f) return ogre::Quaternion::IDENTITY;

    return ogre::Quaternion(basis);
}

ogre::Quaternion node_orientation(const tl::formats::CmpNode& node) {
    if (node.vec3s.size() < 4) return ogre::Quaternion::IDENTITY;

    auto vec = [&](size_t i) {
        const tl::formats::CmpVec3& v = node.vec3s[i];
        return ogre::Vector3(v.x, v.y, v.z);
    };
    return orientation_from_basis(vec(1), vec(2), vec(3));
}

/// The authored per-axis scale of a placeable node, or unit scale.
///
/// `Generic Model` and `Room Piece` each carry `SCALE X/Y/Z` at their own
/// indices; both default to 1, so a node shorter than its schema, or one whose
/// scale fields are absent, keeps unit scale rather than collapsing to zero.
///
/// A room piece also carries a uniform `SCALE` at index 10, which is the
/// editor's slider and drives the three per-axis values. The per-axis three are
/// read because they are the ones that can disagree; throughout the shipped
/// data they do not.
ogre::Vector3 layout_scale(const tl::formats::CmpNode& node, float unit_scale) {
    // A `Monster` carries one uniform `SCALE` rather than three per-axis ones -
    // creatures are resized as wholes - so it is read on its own and applied to
    // all three axes. 151 of the archive's monsters author it.
    //
    // **A unit has a `SCALE` of its own, in its `.DAT`**, and the node's is an
    // override rather than the whole answer: every merchant in the town authors
    // `SCALE: 1.1` in its unit file and nothing in the layout repeats it, so a
    // node with no scale of its own has to fall back to the unit's or the whole
    // cast stands a tenth too short.
    if (node.type == tl::formats::cmp_type::kMonster) {
        const tl::formats::CmpField* f = node.field(tl::formats::cmp_field::monster::kScale);
        float value = unit_scale;
        if (f != nullptr && f->count() == 1) {
            const float authored = f->as_float();
            // 1 is the descriptor's default and means "nothing said here", not
            // "exactly life size": a node that authors it is not overriding the
            // unit, which is why the unit's own value still wins.
            if (authored > 0.0f && authored < 1000.0f && std::fabs(authored - 1.0f) > 1e-4f) {
                value = authored;
            }
        }
        if (!(value > 0.0f) || value >= 1000.0f) return ogre::Vector3::UNIT_SCALE;
        return ogre::Vector3(value, value, value);
    }

    size_t first = 0;
    if (node.type == tl::formats::cmp_type::kModel) {
        first = tl::formats::cmp_field::model::kScaleX;
    } else if (node.type == tl::formats::cmp_type::kRoomPiece) {
        first = tl::formats::cmp_field::room_piece::kScaleX;
    } else {
        return ogre::Vector3::UNIT_SCALE;
    }

    ogre::Vector3 scale = ogre::Vector3::UNIT_SCALE;
    for (size_t axis = 0; axis < 3; ++axis) {
        const tl::formats::CmpField* f = node.field(first + axis);
        if (!f || f->count() != 1) continue;
        const float value = f->as_float();
        // A scale of zero would draw nothing at all, and a negative one turns
        // the mesh inside out. Neither occurs in the archive, so either is a
        // misread field rather than an authored intent.
        if (value > 0.0f && value < 1000.0f) scale[axis] = value;
    }
    return scale;
}

}  // namespace tl::core
