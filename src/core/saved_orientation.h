// The orientations a save record keeps: `CPositionableObject`'s matrix, sixteen floats
// (`CItemSaveState +0x9c`, a character's +0x78), from a yaw or a turn and back.
#pragma once

#include "core/ogre_types.h"

#include <array>
#include <cstdint>

namespace tl::core {

/// A turn about +Y as the matrix.
std::array<uint8_t, 64> orientation_bytes(float yaw);
/// The yaw a matrix of `orientation_bytes` holds.
float yaw_of_bytes(const std::array<uint8_t, 64>& bytes);
/// `ItemSaveState +0x9c`: any turn as the matrix.
void save_orientation(const ogre::Quaternion& turn, std::array<uint8_t, 64>& out);
/// The turn back; a record that never held one is no turn at all.
ogre::Quaternion saved_orientation(const std::array<uint8_t, 64>& in);

}  // namespace tl::core
