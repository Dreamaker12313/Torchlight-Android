// The orientations a save record keeps (core/saved_orientation.h).
#include "core/saved_orientation.h"

#include <cmath>
#include <cstring>

namespace tl::core {

// An orientation as `CPositionableObject`'s matrix holds it (save state +0x78,
// 16 floats): a turn about +Y.
std::array<uint8_t, 64> orientation_bytes(float yaw) {
    std::array<float, 16> m{};
    m[0] = std::cos(yaw);
    m[2] = std::sin(yaw);
    m[5] = 1.0f;
    m[8] = -std::sin(yaw);
    m[10] = std::cos(yaw);
    m[15] = 1.0f;
    std::array<uint8_t, 64> out{};
    std::memcpy(out.data(), m.data(), out.size());
    return out;
}

float yaw_of_bytes(const std::array<uint8_t, 64>& bytes) {
    std::array<float, 16> m{};
    std::memcpy(m.data(), bytes.data(), bytes.size());
    return std::atan2(m[2], m[0]);
}

/// `ItemSaveState +0x9c`: the orientation as a 4 x 4 matrix of floats.
void save_orientation(const ogre::Quaternion& turn, std::array<uint8_t, 64>& out) {
    // OGRE's `Matrix4(turn)`: the turn's matrix in the top left, the rest the identity's.
    ogre::Matrix3 rotation;
    turn.ToRotationMatrix(rotation);
    float cells[16] = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) cells[row * 4 + column] = rotation[row][column];
    }
    cells[15] = 1.0f;
    static_assert(sizeof cells == 64, "the saved orientation is sixteen floats");
    std::memcpy(out.data(), cells, sizeof cells);
}

ogre::Quaternion saved_orientation(const std::array<uint8_t, 64>& in) {
    float cells[16];
    std::memcpy(cells, in.data(), sizeof cells);
    ogre::Matrix3 turn;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) turn[row][column] = cells[row * 4 + column];
    }
    const ogre::Quaternion facing(turn);
    // A record that never held one (all zeroes) is no turn at all.
    return facing.Norm() > 0.5f ? facing : ogre::Quaternion::IDENTITY;
}

}  // namespace tl::core
